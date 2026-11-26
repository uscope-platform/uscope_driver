/*
 * Shared memory windows and IPI doorbells for the RPU cores, one per core,
 * served through /dev/uscope_shm_<core>.
 *
 * The windows are plain kernel memory with fake physical addresses, so a test
 * that maps them can play the R5 side. The scope device has shm_addr_<n> and
 * shm_size_<n> as on the real device, plus the mock only ipi_<n>: reading it
 * gives how many times Linux rang core <n>, writing anything to it simulates
 * core <n> ringing Linux.
 */

#include "mock_module.h"

static unsigned long shm_size = SZ_1M;
module_param(shm_size, ulong, 0444);
MODULE_PARM_DESC(shm_size, "Size in bytes of each core's shared memory window (default 1 MiB)");

/* core <n> rang Linux: wake the readers of /dev/uscope_shm_<n> */
static void mock_ipi_rx(struct mock_rpu *r)
{
    atomic_inc(&r->ipi_count);
    wake_up_interruptible(&r->ipi_wq);
}

#define MOCK_RPU_ATTRS(n)                                                               \
static ssize_t shm_addr_##n##_show(struct device *dev, struct device_attribute *attr,  \
                                   char *buf)                                           \
{                                                                                       \
    return sysfs_emit(buf, "0x%llx\n", (unsigned long long)dev_data->rpu[n].shm_phys);  \
}                                                                                       \
static ssize_t shm_size_##n##_show(struct device *dev, struct device_attribute *attr,  \
                                   char *buf)                                           \
{                                                                                       \
    return sysfs_emit(buf, "%zu\n", dev_data->rpu[n].shm_size);                         \
}                                                                                       \
static ssize_t ipi_##n##_show(struct device *dev, struct device_attribute *attr,       \
                              char *buf)                                                \
{                                                                                       \
    return sysfs_emit(buf, "%d\n", atomic_read(&dev_data->rpu[n].ring_count));          \
}                                                                                       \
static ssize_t ipi_##n##_store(struct device *dev, struct device_attribute *attr,      \
                               const char *buf, size_t len)                             \
{                                                                                       \
    mock_ipi_rx(&dev_data->rpu[n]);                                                     \
    return len;                                                                         \
}                                                                                       \
static DEVICE_ATTR_RO(shm_addr_##n);                                                    \
static DEVICE_ATTR_RO(shm_size_##n);                                                    \
static DEVICE_ATTR(ipi_##n, 0644, ipi_##n##_show, ipi_##n##_store)

MOCK_RPU_ATTRS(0);
MOCK_RPU_ATTRS(1);

static struct attribute *mock_rpu_attrs[] = {
    &dev_attr_shm_addr_0.attr,
    &dev_attr_shm_size_0.attr,
    &dev_attr_ipi_0.attr,
    &dev_attr_shm_addr_1.attr,
    &dev_attr_shm_size_1.attr,
    &dev_attr_ipi_1.attr,
    NULL,
};

static const struct attribute_group mock_rpu_attr_group = {
    .attrs = mock_rpu_attrs,
};

int mock_rpu_init(struct device *dev)
{
    int rc;

    /* shm_size = 0 disables the windows, like on the real device */
    if (shm_size) {
        shm_size = PAGE_ALIGN(shm_size);
        for (int i = 0; i < N_RPU_CORES; i++) {
            struct mock_rpu *r = &dev_data->rpu[i];

            r->shm_va = vmalloc_user(shm_size);
            if (!r->shm_va) {
                pr_err("%s: unable to allocate %lu bytes of shared memory for core %d\n", __func__, shm_size, i);
                continue;
            }
            r->shm_size = shm_size;
            r->shm_phys = MOCK_SHM_PHYS_BASE + i * shm_size;
        }
    }

    rc = sysfs_create_group(&dev->kobj, &mock_rpu_attr_group);
    if (rc) {
        pr_err("%s: Failed to create the rpu interface\nError:%d\n", __func__, rc);
        return rc;
    }
    mock_make_world_writable(dev, &dev_attr_ipi_0.attr);
    mock_make_world_writable(dev, &dev_attr_ipi_1.attr);
    return 0;
}

void mock_rpu_free(struct device *dev)
{
    sysfs_remove_group(&dev->kobj, &mock_rpu_attr_group);
    for (int i = 0; i < N_RPU_CORES; i++) {
        vfree(dev_data->rpu[i].shm_va);
        dev_data->rpu[i].shm_va = NULL;
    }
}

int mock_shm_mmap(struct mock_rpu *r, struct vm_area_struct *vma)
{
    if (!r->shm_va)
        return -ENODEV;
    /* checks vm_pgoff and the length against the window, like dma_mmap_coherent() on the real device */
    return remap_vmalloc_range(vma, r->shm_va, vma->vm_pgoff);
}

int mock_ipi_ring(struct mock_rpu *r)
{
    atomic_inc(&r->ring_count);
    return 0;
}
