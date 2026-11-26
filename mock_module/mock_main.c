/*
 * Module setup: device nodes, the scope platform device that carries the
 * sysfs files, and teardown.
 */

#include "mock_module.h"

static dev_t device_number;
static struct class *uCube_class;
struct scope_device_data *dev_data;

/* the platform device that carries the sysfs files, where the real uScope device has them */
static struct platform_device *scope_dev;

/* Make the device nodes accessible to non root users */
static char *mock_devnode(const struct device *dev, umode_t *mode)
{
    if (mode)
        *mode = 0666;
    return NULL;
}

void mock_make_world_writable(struct device *dev, const struct attribute *attr)
{
    if (sysfs_chmod_file(&dev->kobj, attr, 0666))
        pr_err("%s: Failed to open up the permissions of %s\n", __func__, attr->name);
}

static void free_device_data(struct device *d)
{
    pr_info("%s: In free_device_data\n", __func__);
}


/* Register the scope device and populate its sysfs files */
static int mock_scope_dev_init(void)
{
    struct device *dev;
    int rc;

    scope_dev = platform_device_register_simple("fffc000000008000.uScope", -1, NULL, 0);
    if (IS_ERR(scope_dev)) {
        rc = PTR_ERR(scope_dev);
        pr_err("%s: Failed to register the scope device\nError:%d\n", __func__, rc);
        scope_dev = NULL;
        return rc;
    }
    dev = &scope_dev->dev;

    rc = mock_dma_init(dev);
    if (rc)
        goto err_unregister;
    rc = mock_clk_add_files(dev);
    if (rc)
        goto err_dma;
    rc = mock_rpu_init(dev);
    if (rc)
        goto err_clk;
    return 0;

err_clk:
    mock_clk_remove_files(dev);
err_dma:
    mock_dma_free(dev);
err_unregister:
    platform_device_unregister(scope_dev);
    scope_dev = NULL;
    return rc;
}

static void mock_scope_dev_free(void)
{
    if (!scope_dev)
        return;
    mock_rpu_free(&scope_dev->dev);
    mock_clk_remove_files(&scope_dev->dev);
    mock_dma_free(&scope_dev->dev);
    platform_device_unregister(scope_dev);
    scope_dev = NULL;
}


static void mock_chrdevs_free(void)
{
    int major = MAJOR(device_number);

    for (int i = 0; i < N_MINOR_NUMBERS; i++) {
        cdev_del(&dev_data->cdevs[i]);
        device_destroy(uCube_class, MKDEV(major, i));
    }
    class_destroy(uCube_class);
    unregister_chrdev_region(device_number, N_MINOR_NUMBERS);
}

static int __init mock_init(void)
{
    int rc;
    int major;
    const char * const device_names[] = {
        "uscope_data", "uscope_BUS_0", "uscope_BUS_1", "uscope_bitstream", "uscope_shm_0", "uscope_shm_1"
    };

    pr_info("%s: In init\n", __func__);

    dev_data = kzalloc(sizeof(*dev_data), GFP_KERNEL);
    if (!dev_data)
        return -ENOMEM;
    init_waitqueue_head(&dev_data->poll_queue);
    for (int i = 0; i < N_RPU_CORES; i++)
        init_waitqueue_head(&dev_data->rpu[i].ipi_wq);

    rc = mock_fpga_init();
    if (rc)
        goto err_data;

    /* DYNAMICALLY ALLOCATE DEVICE NUMBERS, CLASSES, ETC.*/
    rc = alloc_chrdev_region(&device_number, 0, N_MINOR_NUMBERS, "uCube DMA");
    if (rc) {
        pr_err("%s: Failed to obtain major/minors\nError:%d\n", __func__, rc);
        goto err_fpga;
    }

    major = MAJOR(device_number);
    uCube_class = class_create("uCube_scope");
    uCube_class->devnode = mock_devnode;

    for (int i = 0; i < N_MINOR_NUMBERS; i++) {
        dev_t devt = MKDEV(major, i);

        dev_data->devs[i].devt = devt;
        dev_data->devs[i].class = uCube_class;
        dev_data->devs[i].release = free_device_data;
        dev_set_name(&dev_data->devs[i], device_names[i]);
        device_initialize(&dev_data->devs[i]);
        cdev_init(&dev_data->cdevs[i], &mock_file_ops);
        rc = cdev_add(&dev_data->cdevs[i], devt, 1);
        if (rc)
            pr_info("%s: Failed in adding cdev[%d] to subsystem retval:%d\n", __func__, i, rc);
        else
            device_create(uCube_class, NULL, devt, NULL, device_names[i]);
        pr_info("%s: finished setup for endpoint: %s\n", __func__, device_names[i]);
    }

    rc = mock_scope_dev_init();
    if (rc)
        goto err_chrdevs;
    return 0;

err_chrdevs:
    mock_chrdevs_free();
err_fpga:
    mock_fpga_free();
err_data:
    kfree(dev_data);
    dev_data = NULL;
    return rc;
}

static void __exit mock_exit(void)
{
    pr_info("%s: In exit\n", __func__);

    mock_scope_dev_free();
    mock_chrdevs_free();
    mock_chrdev_free();
    mock_fpga_free();
    kfree(dev_data);
}


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Filippo Savi");
MODULE_DESCRIPTION("uScope dma handler");
MODULE_VERSION("0.01");

module_init(mock_init);
module_exit(mock_exit);
