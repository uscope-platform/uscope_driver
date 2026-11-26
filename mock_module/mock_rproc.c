/*
 * Fake R5 cores registered with the kernel remoteproc framework, so that the
 * driver uses the real /sys/class/remoteproc/remoteproc<n>/{firmware,state}
 * interface, as on a ZynqMP with the zynqmp_r5 remoteproc driver in split mode.
 *
 * Starting a core makes remoteproc read the firmware file for real; loading it
 * only records its size (rpu_fw_size_<n> on the scope device, mock only) and
 * starting or stopping it does nothing.
 */

#include <linux/remoteproc.h>
#include <linux/firmware.h>

#include "mock_module.h"

struct mock_rproc {
    struct rproc *rproc;
    size_t fw_size;
};

static struct mock_rproc rprocs[N_RPU_CORES];

static int mock_rproc_load(struct rproc *rproc, const struct firmware *fw)
{
    struct mock_rproc *m = rproc->priv;

    m->fw_size = fw->size;
    pr_info("%s: %s loaded %zu bytes of firmware\n", __func__, rproc->name, fw->size);
    return 0;
}

static int mock_rproc_start(struct rproc *rproc)
{
    pr_info("%s: %s started\n", __func__, rproc->name);
    return 0;
}

static int mock_rproc_stop(struct rproc *rproc)
{
    pr_info("%s: %s stopped\n", __func__, rproc->name);
    return 0;
}

/* a load op of our own also keeps remoteproc from installing its ELF loader */
static const struct rproc_ops mock_rproc_ops = {
    .start = mock_rproc_start,
    .stop = mock_rproc_stop,
    .load = mock_rproc_load,
};

#define MOCK_RPROC_ATTRS(n)                                                             \
static ssize_t rpu_fw_size_##n##_show(struct device *dev, struct device_attribute *attr, \
                                      char *buf)                                        \
{                                                                                       \
    return sysfs_emit(buf, "%zu\n", rprocs[n].fw_size);                                 \
}                                                                                       \
static DEVICE_ATTR_RO(rpu_fw_size_##n)

MOCK_RPROC_ATTRS(0);
MOCK_RPROC_ATTRS(1);

static struct attribute *mock_rproc_attrs[] = {
    &dev_attr_rpu_fw_size_0.attr,
    &dev_attr_rpu_fw_size_1.attr,
    NULL,
};

static const struct attribute_group mock_rproc_attr_group = {
    .attrs = mock_rproc_attrs,
};

/* the remoteproc files are root only: open them up, they are found by name like any sysfs file */
static void mock_rproc_open_up(struct rproc *rproc)
{
    static const char * const names[] = { "firmware", "state" };

    for (int i = 0; i < ARRAY_SIZE(names); i++) {
        struct attribute attr = { .name = names[i] };
        mock_make_world_writable(&rproc->dev, &attr);
    }
}

static void mock_rproc_remove(int n)
{
    for (int i = 0; i < n; i++) {
        rproc_del(rprocs[i].rproc);
        rproc_free(rprocs[i].rproc);
        rprocs[i].rproc = NULL;
    }
}

int mock_rproc_init(struct device *dev)
{
    int rc;

    for (int i = 0; i < N_RPU_CORES; i++) {
        char name[8];
        struct rproc *rproc;

        snprintf(name, sizeof(name), "r5f_%d", i);
        rproc = rproc_alloc(dev, name, &mock_rproc_ops, NULL, 0);
        if (!rproc) {
            rc = -ENOMEM;
            goto err;
        }
        rproc->priv = &rprocs[i];
        rproc->auto_boot = false;
        rc = rproc_add(rproc);
        if (rc) {
            rproc_free(rproc);
            goto err;
        }
        rprocs[i].rproc = rproc;
        rprocs[i].fw_size = 0;
        mock_rproc_open_up(rproc);
        dev_info(dev, "fake R5 core %d registered as %s\n", i, dev_name(&rproc->dev));
        continue;
err:
        pr_err("%s: Failed to register fake R5 core %d\nError:%d\n", __func__, i, rc);
        mock_rproc_remove(i);
        return rc;
    }

    rc = sysfs_create_group(&dev->kobj, &mock_rproc_attr_group);
    if (rc) {
        pr_err("%s: Failed to create the rproc interface\nError:%d\n", __func__, rc);
        mock_rproc_remove(N_RPU_CORES);
    }
    return rc;
}

void mock_rproc_free(struct device *dev)
{
    sysfs_remove_group(&dev->kobj, &mock_rproc_attr_group);
    mock_rproc_remove(N_RPU_CORES);
}
