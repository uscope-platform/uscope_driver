/*
 * Character devices: /dev/uscope_data, /dev/uscope_BUS_0, /dev/uscope_BUS_1,
 * /dev/uscope_bitstream and /dev/uscope_shm_<core>.
 */

#include "mock_module.h"

/* The bus mappings are backed by one zeroed page, as there is no FPGA behind them */
static char *mock_page;

static vm_fault_t mock_bus_fault(struct vm_fault *vmf)
{
    if (!mock_page) {
        mock_page = (char *)__get_free_page(GFP_KERNEL);
        if (!mock_page)
            return VM_FAULT_OOM;
    }
    memset(mock_page, 0, PAGE_SIZE);
    get_page(virt_to_page(mock_page));
    vmf->page = virt_to_page(mock_page);
    return 0;
}

static const struct vm_operations_struct mock_bus_vm_ops = {
    .fault = mock_bus_fault,
};

void mock_chrdev_free(void)
{
    if (mock_page)
        free_page((unsigned long)mock_page);
    mock_page = NULL;
}


static __poll_t mock_poll(struct file *flip, struct poll_table_struct *poll_struct)
{
    int minor = MINOR(flip->f_inode->i_rdev);
    struct mock_rpu *r = mock_minor_rpu(minor);

    if (minor == 0) {
        __poll_t mask = 0;
        poll_wait(flip, &dev_data->poll_queue, poll_struct);
        if (READ_ONCE(dev_data->new_data_available))
            mask |= EPOLLIN | EPOLLRDNORM;
        return mask;
    }
    if (r) {
        /* Readable once this file's R5 has rung since its last read() */
        poll_wait(flip, &r->ipi_wq, poll_struct);
        if ((unsigned int)atomic_read(&r->ipi_count) != (uintptr_t)flip->private_data)
            return EPOLLIN | EPOLLRDNORM;
    }
    return 0;
}


static int mock_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int minor = MINOR(filp->f_inode->i_rdev);
    struct mock_rpu *r = mock_minor_rpu(minor);

    pr_info("%s: In mmap\n", __func__);
    if (r)
        return mock_shm_mmap(r, vma);
    if (minor == 0) {
        pr_err("%s: mmapping of data memory is not supported\n", __func__);
        return -EINVAL;
    }
    vma->vm_ops = &mock_bus_vm_ops;
    return 0;
}


static long mock_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int minor = MINOR(filp->f_inode->i_rdev);
    struct mock_rpu *r = mock_minor_rpu(minor);

    if (minor == 0) {
        pr_info("%s: In ioctl\n CMD: %u\n ARG: %lu\n", __func__, cmd, arg);
        switch (cmd) {
        case IOCTL_NEW_DATA_AVAILABLE:
            return dev_data->new_data_available;
        default:
            return -EINVAL;
        }
    } else if (minor == 3) {
        switch (cmd) {
        case IOCTL_PROGRAM_FPGA:
            mock_program_fpga();
            return 0;
        default:
            return -EINVAL;
        }
    } else if (r) {
        switch (cmd) {
        case IOCTL_RING_RPU:
            return mock_ipi_ring(r);
        default:
            return -EINVAL;
        }
    }
    return 0;
}


static int mock_open(struct inode *inode, struct file *file)
{
    struct mock_rpu *r = mock_minor_rpu(iminor(inode));

    pr_info("%s: In open\n", __func__);

    /* Shared memory: only doorbells arriving after open() count for this file */
    if (r)
        file->private_data = (void *)(uintptr_t)(unsigned int)atomic_read(&r->ipi_count);

    return 0;
}

static int mock_release(struct inode *inode, struct file *file)
{
    pr_info("%s: In release\n", __func__);
    return 0;
}


/* Wait for this file's R5 to ring; return how many doorbells arrived since the last read */
static ssize_t mock_read_doorbell(struct mock_rpu *r, struct file *flip, char __user *buffer, size_t count)
{
    unsigned int seen = (uintptr_t)flip->private_data;
    unsigned int now;
    u32 n;

    if (count < sizeof(n))
        return -EINVAL;
    if ((unsigned int)atomic_read(&r->ipi_count) == seen) {
        if (flip->f_flags & O_NONBLOCK)
            return -EAGAIN;
        if (wait_event_interruptible(r->ipi_wq,
                (unsigned int)atomic_read(&r->ipi_count) != seen))
            return -ERESTARTSYS;
    }
    now = atomic_read(&r->ipi_count);
    n = now - seen;
    flip->private_data = (void *)(uintptr_t)now;
    if (copy_to_user(buffer, &n, sizeof(n)))
        return -EFAULT;
    return sizeof(n);
}

static ssize_t mock_read(struct file *flip, char __user *buffer, size_t count, loff_t *offset)
{
    int minor = iminor(flip->f_inode);
    struct mock_rpu *r = mock_minor_rpu(minor);

    if (minor == 0) {
        if (count > dev_data->dma_buf_size)
            count = dev_data->dma_buf_size;
        if (copy_to_user(buffer, dev_data->read_data_buffer, count))
            return -EFAULT;
        dev_data->new_data_available = 0;
        return count;
    } else if (minor == 3) {
        char result = mock_fpga_loaded() ? '1' : '0';
        if (copy_to_user(buffer, &result, 1))
            return -EFAULT;
        return 1;
    } else if (r) {
        return mock_read_doorbell(r, flip, buffer, count);
    }
    return 0;
}


static ssize_t mock_write(struct file *flip, const char __user *buffer, size_t len, loff_t *offset)
{
    int minor = iminor(flip->f_inode);

    if (minor == 0) {
        /* mock only: what is written here is what the next read returns, as if the scope captured it */
        if (len > dev_data->dma_buf_size)
            len = dev_data->dma_buf_size;
        if (copy_from_user(dev_data->read_data_buffer, buffer, len))
            return -EFAULT;
        WRITE_ONCE(dev_data->new_data_available, 1);
        wake_up_interruptible(&dev_data->poll_queue);
        return len;
    } else if (minor == 3) {
        return mock_fpga_write_bitstream(buffer, len, offset);
    }
    return len;
}


/* This structure points to all of the device functions */
struct file_operations mock_file_ops = {
    .read = mock_read,
    .write = mock_write,
    .open = mock_open,
    .unlocked_ioctl = mock_ioctl,
    .poll = mock_poll,
    .release = mock_release,
    .mmap = mock_mmap
};
