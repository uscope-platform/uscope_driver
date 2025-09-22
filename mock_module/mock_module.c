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

#define N_MINOR_NUMBERS	3

#define KERNEL_BUFFER_LENGTH 6144
#define KERNEL_BUFFER_SIZE KERNEL_BUFFER_LENGTH*4


#define IOCTL_NEW_DATA_AVAILABLE 1



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
static struct scope_device_data *dev_data;
static struct platform_device *fclk_devs[4];

/* STRUCTURE FOR THE DEVICE SPECIFIC DATA*/
struct scope_device_data {
    struct device devs[N_MINOR_NUMBERS];
    struct cdev cdevs[N_MINOR_NUMBERS];
    u32 *read_data_buffer;
    dma_addr_t physaddr;
    int new_data_available;
    struct clk *fclk[4];
    bool is_zynqmp;
};


static char *mock_page;

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


static ssize_t fclk_show(struct device *dev, struct device_attribute *mattr, char *data) {
    unsigned long freq = 1e6;
    return sprintf(data, "%lu\n", freq);
}

static ssize_t fclk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len) {
    return len;
}

static DEVICE_ATTR(set_rate, S_IRUGO|S_IWUSR, fclk_show, fclk_store);


static __poll_t ucube_lkm_poll(struct file *flip , struct poll_table_struct * poll_struct){
    int minor = MINOR(flip->f_inode->i_rdev);
    if(minor == 0){
        __poll_t mask = 0;
        mask |= POLLIN | POLLRDNORM;
        return mask;
    }
    return 0;
}


static int ucube_lkm_mmap(struct file *filp, struct vm_area_struct *vma){
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
        size_t datalen = KERNEL_BUFFER_SIZE;

        pr_info("%s: In read\n", __func__);

        if (count > datalen) {
            count = datalen;
        }

        if (copy_to_user(buffer, dev_data->read_data_buffer, count)) {
            return -EFAULT;
        }
        dev_data->new_data_available = 0;
        return count;
    }
    return 0;
}


static ssize_t ucube_lkm_write(struct file *flip, const char *buffer, size_t len, loff_t *offset) {
    int minor = iminor(flip->f_inode);
    size_t to_copy;
    pr_info("%s: In write with minor number %d\n", __func__, minor);
    if(minor >0) return len;


    if (len > KERNEL_BUFFER_SIZE)
        len = KERNEL_BUFFER_SIZE;

    to_copy = len;

    if (copy_from_user(dev_data->read_data_buffer, buffer, to_copy))
        return -EFAULT;

    return to_copy;
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



static int __init ucube_lkm_init(void) {
    int dev_rc;
    int major;
    int cdev_rcs[N_MINOR_NUMBERS];
    dev_t devices[N_MINOR_NUMBERS];
    const char* const device_names[] = { "uscope_data_new", "uscope_BUS_0", "uscope_BUS_1"};

    /* DYNAMICALLY ALLOCATE DEVICE NUMBERS, CLASSES, ETC.*/
    pr_info("%s: In init\n", __func__);\

    dev_rc = alloc_chrdev_region(&device_number, 0, N_MINOR_NUMBERS,"uCube DMA");

    if (dev_rc) {
        pr_err("%s: Failed to obtain major/minors\nError:%d\n", __func__, dev_rc);
        return dev_rc;
    }

    major = MAJOR(device_number);
    uCube_class = class_create("uCube_scope");


    for(int i = 0; i< N_MINOR_NUMBERS; i++){
        devices[i] = MKDEV(major, i);
    }

    dev_data = kzalloc(sizeof(*dev_data), GFP_KERNEL);
    dev_data->new_data_available = 0;

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
        pr_info("%s: finished setup for endpoint: %s\n", __func__, device_names[i]);
    }

    char name[16];

	for(int i = 0; i< 4; i++){
        snprintf(name, sizeof(name), "fclk%d", i);
	    fclk_devs[i] = platform_device_register_simple(name, -1, NULL, 0);
        if (IS_ERR(fclk_devs[0])) {
            int rc = PTR_ERR(fclk_devs[0]);
            class_destroy(uCube_class);
            unregister_chrdev_region(device_number, N_MINOR_NUMBERS);
            return rc;
        }

        int rc = device_create_file(&fclk_devs[i]->dev, &dev_attr_set_rate);
	}

    /*SETUP AND ALLOCATE DMA BUFFER*/
    dev_data->read_data_buffer = vmalloc(KERNEL_BUFFER_SIZE);
    pr_warn("%s: Allocated dma buffer at: %u\n", __func__, dev_data->physaddr);;

    return 0;
}

static void __exit ucube_lkm_exit(void) {

	int major = MAJOR(device_number);

    pr_info("%s: In exit\n", __func__);

    vfree(dev_data->read_data_buffer);
    for (int i = 0; i < 4; i++) {
		device_remove_file(&fclk_devs[i]->dev, &dev_attr_set_rate);
        if (!IS_ERR_OR_NULL(fclk_devs[i])) {
            platform_device_unregister(fclk_devs[i]);
        }
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