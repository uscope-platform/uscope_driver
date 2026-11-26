/*
 * Scope data buffer and its sysfs files on the scope device: dma_addr and
 * dma_buf_size.
 */

#include "mock_module.h"

static u64 mock_dma_addr;

static ssize_t dma_buf_size_show(struct device *dev, struct device_attribute *attr, char *data)
{
    return sysfs_emit(data, "%u\n", dev_data->dma_buf_size);
}

/* like the real device the read buffer is reallocated (and its content lost) when the size changes */
static ssize_t dma_buf_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
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

static ssize_t dma_addr_show(struct device *dev, struct device_attribute *attr, char *data)
{
    return sysfs_emit(data, "%llu\n", mock_dma_addr);
}

/* mock only: the address is settable so that the tests can choose the one the driver reads back */
static ssize_t dma_addr_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
    int rc = kstrtou64(buf, 0, &mock_dma_addr);
    if (rc)
        return rc;
    return len;
}

static DEVICE_ATTR(dma_addr, 0644, dma_addr_show, dma_addr_store);
static DEVICE_ATTR(dma_buf_size, 0644, dma_buf_size_show, dma_buf_size_store);

static struct attribute *mock_dma_attrs[] = {
    &dev_attr_dma_addr.attr,
    &dev_attr_dma_buf_size.attr,
    NULL,
};

static const struct attribute_group mock_dma_attr_group = {
    .attrs = mock_dma_attrs,
};

int mock_dma_init(struct device *dev)
{
    int rc;

    dev_data->dma_buf_size = KERNEL_BUFFER_SIZE;
    dev_data->read_data_buffer = vzalloc(dev_data->dma_buf_size);
    if (!dev_data->read_data_buffer)
        return -ENOMEM;

    rc = sysfs_create_group(&dev->kobj, &mock_dma_attr_group);
    if (rc) {
        pr_err("%s: Failed to create the dma interface\nError:%d\n", __func__, rc);
        vfree(dev_data->read_data_buffer);
        dev_data->read_data_buffer = NULL;
        return rc;
    }
    mock_make_world_writable(dev, &dev_attr_dma_addr.attr);
    mock_make_world_writable(dev, &dev_attr_dma_buf_size.attr);
    return 0;
}

void mock_dma_free(struct device *dev)
{
    sysfs_remove_group(&dev->kobj, &mock_dma_attr_group);
    vfree(dev_data->read_data_buffer);
    dev_data->read_data_buffer = NULL;
}
