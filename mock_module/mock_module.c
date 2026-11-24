#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/poll.h>
#include <asm/pgtable.h>
#include <linux/clk.h>
#include <linux/device.h>

#define N_MINOR_NUMBERS	4

#define N_CHANNELS 6
#define KERNEL_BUFFER_LENGTH 1024
#define KERNEL_BUFFER_SIZE (KERNEL_BUFFER_LENGTH*N_CHANNELS*8)
#define BITSTREAM_SIZE 32000000

#define IOCTL_NEW_DATA_AVAILABLE 1
#define IOCTL_GET_BUFFER_ADDRESS 2
#define IOCTL_PROGRAM_FPGA 3



#define ZYNQ_BUS_0_ADDRESS_BASE 0x40000000
#define ZYNQ_BUS_0_ADDRESS_TOP 0x7fffffff

#define ZYNQ_BUS_1_ADDRESS_BASE 0x80000000
#define ZYNQ_BUS_1_ADDRESS_TOP 0xBfffffff

#define ZYNQMP_BUS_0_ADDRESS_BASE 0x400000000
#define ZYNQMP_BUS_0_ADDRESS_TOP  0x4FFFFFFFF

#define ZYNQMP_BUS_1_ADDRESS_BASE 0x500000000
#define ZYNQMP_BUS_1_ADDRESS_TOP  0x5FFFFFFFF



#define FCLK_0_DEFAULT_FREQ 100000000
#define FCLK_1_DEFAULT_FREQ 40000000
#define FCLK_2_DEFAULT_FREQ 40000000
#define FCLK_3_DEFAULT_FREQ 40000000

/* Prototypes for device functions */
static int ucube_lkm_open(struct inode *, struct file *);
static int ucube_lkm_release(struct inode *, struct file *);
static ssize_t ucube_lkm_read(struct file *, char *, size_t, loff_t *);
static ssize_t ucube_lkm_write(struct file *, const char *, size_t, loff_t *);
static long ucube_lkm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int ucube_lkm_mmap(struct file *filp, struct vm_area_struct *vma);
static __poll_t ucube_lkm_poll(struct file *, struct poll_table_struct *);


static dev_t device_number;
static struct class *uCube_class;

/* Make the device nodes accessible to non root users */
static char *ucube_devnode(const struct device *dev, umode_t *mode)
{
    if (mode)
        *mode = 0666;
    return NULL;
}
static struct scope_device_data *dev_data;
#define N_FCLK 4
static struct device_attribute fclk_attrs[N_FCLK];
static struct device_attribute fclk_en_attrs[N_FCLK];
static const char * const fclk_names[N_FCLK] = { "fclk_0", "fclk_1", "fclk_2", "fclk_3" };
static const char * const fclk_en_names[N_FCLK] = { "fclk_0_en", "fclk_1_en", "fclk_2_en", "fclk_3_en" };
static bool fclk_on[N_FCLK] = { true, true, true, true };
static unsigned long fclk_rates[N_FCLK] = {
    FCLK_0_DEFAULT_FREQ, FCLK_1_DEFAULT_FREQ, FCLK_2_DEFAULT_FREQ, FCLK_3_DEFAULT_FREQ
};
/* the platform device that exposes the physical address of the dma buffer, where the real uScope device has it */
static struct platform_device *scope_dev;
static u64 mock_dma_addr;

/* STRUCTURE FOR THE DEVICE SPECIFIC DATA*/
struct scope_device_data {
    struct device devs[N_MINOR_NUMBERS];
    struct cdev cdevs[N_MINOR_NUMBERS];
    u32 *read_data_buffer;
    u32 dma_buf_size;
    size_t bitstream_len;
    dma_addr_t physaddr;
    wait_queue_head_t poll_queue;
    int new_data_available;
    struct clk *fclk[4];
    bool is_zynqmp;
};

static char *bitstream_buffer;

static char *mock_page;

static bool fpga_loaded;

static vm_fault_t ucube_lkm_fault(struct vm_fault *vmf)
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

static const struct vm_operations_struct mock_vm_ops = {
    .fault = ucube_lkm_fault,
};


/* the attribute tells us which clock it is: fclk_attrs[i] belongs to fclk_<i> */
static ssize_t fclk_show(struct device *dev, struct device_attribute *attr, char *data) {
    int idx = attr - fclk_attrs;
    return sprintf(data, "%lu\n", fclk_rates[idx]);
}

static ssize_t fclk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len) {
    int idx = attr - fclk_attrs;
    if (kstrtoul(buf, 0, &fclk_rates[idx]))
        return -EINVAL;
    return len;
}

static ssize_t fclk_en_show(struct device *dev, struct device_attribute *attr, char *data) {
    int idx = attr - fclk_en_attrs;
    return sprintf(data, "%d\n", fclk_on[idx]);
}

static ssize_t fclk_en_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len) {
    int idx = attr - fclk_en_attrs;
    bool on;
    int rc = kstrtobool(buf, &on);
    if (rc)
        return rc;
    fclk_on[idx] = on;
    return len;
}

static ssize_t dma_buf_size_show(struct device *dev, struct device_attribute *mattr, char *data) {
    return sprintf(data, "%u\n", dev_data->dma_buf_size);
}

/* like the real device the read buffer is reallocated (and its content lost) when the size changes */
static ssize_t dma_buf_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len) {
    unsigned long size;
    u32 *new_buf;
    if (kstrtoul(buf, 0, &size) || !size || size > U32_MAX)
        return -EINVAL;
    new_buf = vzalloc(size);
    if (!new_buf)
        return -ENOMEM;
    vfree(dev_data->read_data_buffer);
    dev_data->read_data_buffer = new_buf;
    dev_data->dma_buf_size = size;
    return len;
}

static ssize_t dma_addr_show(struct device *dev, struct device_attribute *mattr, char *data) {
    return sprintf(data, "%llu\n", mock_dma_addr);
}

/* the address is settable so that the tests can choose the one the driver reads back */
static ssize_t dma_addr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len) {
    int rc = kstrtou64(buf, 0, &mock_dma_addr);
    if (rc)
        return rc;
    return len;
}

static DEVICE_ATTR(dma_addr, S_IRUGO|S_IWUSR, dma_addr_show, dma_addr_store);
static DEVICE_ATTR(dma_buf_size, S_IRUGO|S_IWUSR, dma_buf_size_show, dma_buf_size_store);


static __poll_t ucube_lkm_poll(struct file *flip , struct poll_table_struct * poll_struct){
    int minor = MINOR(flip->f_inode->i_rdev);
    if(minor == 0){
        __poll_t mask = 0;
        poll_wait(flip, &dev_data->poll_queue, poll_struct);
        if (READ_ONCE(dev_data->new_data_available))
            mask |= EPOLLIN | EPOLLRDNORM;
        return mask;
    }
    return 0;
}


static int ucube_lkm_mmap(struct file *filp, struct vm_area_struct *vma){

    pr_info("%s: In mmap\n", __func__);
    vma->vm_ops = &mock_vm_ops;
    return 0;
}



static long ucube_lkm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    int minor = MINOR(filp->f_inode->i_rdev);
    if(minor == 0){
        pr_info("%s: In ioctl\n CMD: %u\n ARG: %lu\n", __func__, cmd, arg);
        switch (cmd){
        case IOCTL_NEW_DATA_AVAILABLE:
            return dev_data->new_data_available;
            break;
        default:
            return -EINVAL;
            break;
        }
        return 0;
    } else if(minor == 3){
        switch (cmd){
        case IOCTL_PROGRAM_FPGA:
            pr_info("%s: FPGA BITSTREAM LENGTH: %zu\n", __func__, dev_data->bitstream_len);
            fpga_loaded = true;
            break;
        default:
            return -EINVAL;
        }
        return 0;
    } else{
        return 0;
    }

}


static int ucube_lkm_open(struct inode *inode, struct file *file) {

    pr_info("%s: In open\n", __func__);

    return 0;
}

static int ucube_lkm_release(struct inode *inode, struct file *file) {
    pr_info("%s: In release\n", __func__);
    return 0;
}

static ssize_t ucube_lkm_read(struct file *flip, char *buffer, size_t count, loff_t *offset) {
    int minor = iminor(flip->f_inode);
    if(minor == 0){
        size_t datalen = dev_data->dma_buf_size;

        if (count > datalen) {
            count = datalen;
        }

        if (copy_to_user(buffer, dev_data->read_data_buffer, count)) {
            return -EFAULT;
        }
        dev_data->new_data_available = 0;
        return count;
    } else if(minor == 3){
        char result = fpga_loaded ?'1' : '0';
        if (copy_to_user(buffer, &result, 1)) return -EFAULT;
        return 1;
    }
    return 0;
}


static ssize_t ucube_lkm_write(struct file *flip, const char *buffer, size_t len, loff_t *offset) {
    int minor = iminor(flip->f_inode);
    size_t to_copy;
	if(minor == 0){
 		if (len > dev_data->dma_buf_size)
			len = dev_data->dma_buf_size;

    	to_copy = len;


    	if (copy_from_user(dev_data->read_data_buffer, buffer, to_copy))
        	return -EFAULT;
	    WRITE_ONCE(dev_data->new_data_available,1);
	    wake_up_interruptible(&dev_data->poll_queue);
    	return to_copy;
	}else if(minor ==3){
		size_t needed = *offset + len;
			if (needed > BITSTREAM_SIZE)
				return -EINVAL;
			if (copy_from_user(bitstream_buffer + *offset, buffer, len))
				return -EFAULT;
			*offset += len;
			if (dev_data->bitstream_len < *offset)
				dev_data->bitstream_len = *offset;
			return len;
	} else {
		return len;
	}
}


static void free_device_data(struct device *d)
{

	struct scope_device_data *dev_data;

    pr_info("%s: In free_device_data\n", __func__);

    for(int i = 0; i< N_MINOR_NUMBERS; i++){
       dev_data = container_of(d, struct scope_device_data, devs[i]);
    }

	kfree(dev_data);
}


/* This structure points to all of the device functions */
static struct file_operations file_ops = {
    .read = ucube_lkm_read,
    .write = ucube_lkm_write,
    .open = ucube_lkm_open,
    .unlocked_ioctl = ucube_lkm_ioctl,
    .poll = ucube_lkm_poll,
    .release = ucube_lkm_release,
    .mmap = ucube_lkm_mmap
};



static int __init ucube_lkm_init(void)
{
    int dev_rc;
    int major;
    int cdev_rcs[N_MINOR_NUMBERS];
    dev_t devices[N_MINOR_NUMBERS];
    const char* const device_names[] = { "uscope_data", "uscope_BUS_0", "uscope_BUS_1", "uscope_bitstream"};
    fpga_loaded = false;
    /* DYNAMICALLY ALLOCATE DEVICE NUMBERS, CLASSES, ETC.*/
    pr_info("%s: In init\n", __func__);\

    dev_rc = alloc_chrdev_region(&device_number, 0, N_MINOR_NUMBERS,"uCube DMA");

    if (dev_rc) {
        pr_err("%s: Failed to obtain major/minors\nError:%d\n", __func__, dev_rc);
        return dev_rc;
    }

    major = MAJOR(device_number);
    uCube_class = class_create("uCube_scope");
    uCube_class->devnode = ucube_devnode;


    for(int i = 0; i< N_MINOR_NUMBERS; i++){
        devices[i] = MKDEV(major, i);
    }

    dev_data = kzalloc(sizeof(*dev_data), GFP_KERNEL);
    init_waitqueue_head(&dev_data->poll_queue);
    dev_data->new_data_available = 0;
    dev_data->dma_buf_size = KERNEL_BUFFER_SIZE;

    for(int i = 0; i< N_MINOR_NUMBERS; i++){
        dev_data->devs[i].devt =  devices[i];
        dev_data->devs[i].class = uCube_class;
        dev_data->devs[i].release = free_device_data;
        dev_set_name(&dev_data->devs[i], device_names[i]);
        device_initialize(&dev_data->devs[i]);
        cdev_init(&dev_data->cdevs[i], &file_ops);
        cdev_rcs[i] = cdev_add(&dev_data->cdevs[i], devices[i], 1);
        if (cdev_rcs[i]) {
            pr_info("%s: Failed in adding cdev[%d] to subsystem "
                    "retval:%d\n", __func__, i, cdev_rcs[i]);
        }
        else {
            device_create(uCube_class, NULL, devices[i], NULL, device_names[i]);
        }
        pr_info("%s: finished setup for endpoint: %s\n", __func__, device_names[i]);
    }

    scope_dev = platform_device_register_simple("fffc000000008000.uScope", -1, NULL, 0);
    if (IS_ERR(scope_dev)) {
        int rc = PTR_ERR(scope_dev);
        pr_err("%s: Failed to register the scope device\nError:%d\n", __func__, rc);
        scope_dev = NULL;
        return rc;
    }
    int scope_rc = device_create_file(&scope_dev->dev, &dev_attr_dma_addr);
    if (!scope_rc)
        scope_rc = device_create_file(&scope_dev->dev, &dev_attr_dma_buf_size);
    if (scope_rc) {
        pr_err("%s: Failed to create the dma_addr interface\nError:%d\n", __func__, scope_rc);
        device_remove_file(&scope_dev->dev, &dev_attr_dma_addr);
        device_remove_file(&scope_dev->dev, &dev_attr_dma_buf_size);
        platform_device_unregister(scope_dev);
        scope_dev = NULL;
        return scope_rc;
    }
    /* like the clock files, opened up to let the driver and the tests use it without being root */
    scope_rc = sysfs_chmod_file(&scope_dev->dev.kobj, &dev_attr_dma_buf_size.attr, 0666);
    if (scope_rc)
        pr_err("%s: Failed to open up the permissions of dma_buf_size\nError:%d\n", __func__, scope_rc);
    scope_rc = sysfs_chmod_file(&scope_dev->dev.kobj, &dev_attr_dma_addr.attr, 0666);
    if (scope_rc) {
        pr_err("%s: Failed to open up the permissions of dma_addr\nError:%d\n", __func__, scope_rc);
    }

    /* the clock files live on the scope device, as fclk_<i> (rate) and fclk_<i>_en (enable), like on the real uScope device */
    for (int i = 0; i < N_FCLK; i++) {
        sysfs_attr_init(&fclk_attrs[i].attr);
        fclk_attrs[i].attr.name = fclk_names[i];
        fclk_attrs[i].attr.mode = S_IRUGO | S_IWUSR;
        fclk_attrs[i].show = fclk_show;
        fclk_attrs[i].store = fclk_store;
        sysfs_attr_init(&fclk_en_attrs[i].attr);
        fclk_en_attrs[i].attr.name = fclk_en_names[i];
        fclk_en_attrs[i].attr.mode = S_IRUGO | S_IWUSR;
        fclk_en_attrs[i].show = fclk_en_show;
        fclk_en_attrs[i].store = fclk_en_store;
        scope_rc = device_create_file(&scope_dev->dev, &fclk_attrs[i]);
        if (!scope_rc) {
            scope_rc = device_create_file(&scope_dev->dev, &fclk_en_attrs[i]);
            if (scope_rc)
                device_remove_file(&scope_dev->dev, &fclk_attrs[i]);
        }
        if (scope_rc) {
            pr_err("%s: Failed to create %s\nError:%d\n", __func__, fclk_names[i], scope_rc);
            while (--i >= 0) {
                device_remove_file(&scope_dev->dev, &fclk_attrs[i]);
                device_remove_file(&scope_dev->dev, &fclk_en_attrs[i]);
            }
            device_remove_file(&scope_dev->dev, &dev_attr_dma_buf_size);
            device_remove_file(&scope_dev->dev, &dev_attr_dma_addr);
            platform_device_unregister(scope_dev);
            scope_dev = NULL;
            return scope_rc;
        }
        /* world writable, so the driver can set the clocks without being root */
        if (sysfs_chmod_file(&scope_dev->dev.kobj, &fclk_attrs[i].attr, 0666) ||
            sysfs_chmod_file(&scope_dev->dev.kobj, &fclk_en_attrs[i].attr, 0666))
            pr_err("%s: Failed to open up the permissions of %s\n", __func__, fclk_names[i]);
    }

    /*SETUP AND ALLOCATE DMA BUFFER*/
    dev_data->read_data_buffer = vzalloc(dev_data->dma_buf_size);
    bitstream_buffer = vmalloc(BITSTREAM_SIZE);
    pr_warn("%s: Allocated dma buffer at: %llu\n", __func__, dev_data->physaddr);;
    /*SETUP POLL WAIT QUEUE*/
    return 0;
}

static void __exit ucube_lkm_exit(void) {

	int major = MAJOR(device_number);

    pr_info("%s: In exit\n", __func__);

    vfree(dev_data->read_data_buffer);
	vfree(bitstream_buffer);
    if (scope_dev) {
        for (int i = 0; i < N_FCLK; i++) {
            device_remove_file(&scope_dev->dev, &fclk_attrs[i]);
            device_remove_file(&scope_dev->dev, &fclk_en_attrs[i]);
        }
        device_remove_file(&scope_dev->dev, &dev_attr_dma_buf_size);
        device_remove_file(&scope_dev->dev, &dev_attr_dma_addr);
        platform_device_unregister(scope_dev);
    }

    for(int i = 0; i< N_MINOR_NUMBERS; i++){
        int device = MKDEV(major, i);

        cdev_del(&dev_data->cdevs[i]);
        device_destroy(uCube_class, device);
    }


	class_destroy(uCube_class);
	unregister_chrdev_region(device_number, N_MINOR_NUMBERS);
    if (mock_page)
        free_page((unsigned long)mock_page);

}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Filippo Savi");
MODULE_DESCRIPTION("uScope dma handler");
MODULE_VERSION("0.01");

module_init(ucube_lkm_init);
module_exit(ucube_lkm_exit);