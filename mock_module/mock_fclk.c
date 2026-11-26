/*
 * Clock controls on the scope device, like on the real uScope device:
 * fclk_<i> (rate in Hz) and fclk_<i>_en (0/1) for the PL clocks, rpu_clk
 * (R5 frequency in Hz, range checked) and rpu_clk_en for the R5 clock.
 */

#include "mock_module.h"

static const char * const fclk_names[N_FCLK] = { "fclk_0", "fclk_1", "fclk_2", "fclk_3" };
static const char * const fclk_en_names[N_FCLK] = { "fclk_0_en", "fclk_1_en", "fclk_2_en", "fclk_3_en" };
static struct device_attribute fclk_attrs[N_FCLK];
static struct device_attribute fclk_en_attrs[N_FCLK];
static bool fclk_on[N_FCLK] = { true, true, true, true };
static unsigned long fclk_rates[N_FCLK] = {
    FCLK_0_DEFAULT_FREQ, FCLK_1_DEFAULT_FREQ, FCLK_2_DEFAULT_FREQ, FCLK_3_DEFAULT_FREQ
};

/* the R5 clock, held as the RPLL rate like the real module does */
static unsigned long rpll_rate = RPU_CLK_DEFAULT_FREQ * RPU_CLK_DIV;
static bool rpll_on = true;


/* the attribute tells us which clock it is: fclk_attrs[i] belongs to fclk_<i> */
static ssize_t fclk_show(struct device *dev, struct device_attribute *attr, char *data)
{
    return sysfs_emit(data, "%lu\n", fclk_rates[attr - fclk_attrs]);
}

static ssize_t fclk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
    if (kstrtoul(buf, 0, &fclk_rates[attr - fclk_attrs]))
        return -EINVAL;
    return len;
}

static ssize_t fclk_en_show(struct device *dev, struct device_attribute *attr, char *data)
{
    return sysfs_emit(data, "%d\n", fclk_on[attr - fclk_en_attrs]);
}

static ssize_t fclk_en_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
    bool on;
    int rc = kstrtobool(buf, &on);
    if (rc)
        return rc;
    fclk_on[attr - fclk_en_attrs] = on;
    return len;
}


/* R5 clock: the file shows and takes the R5 frequency (RPLL / RPU_CLK_DIV), RPU_CLK_MIN..RPU_CLK_MAX only */
static ssize_t rpu_clk_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%lu\n", rpll_rate / RPU_CLK_DIV);
}

static ssize_t rpu_clk_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
    unsigned long r5_hz;
    if (kstrtoul(buf, 0, &r5_hz))
        return -EINVAL;
    if (r5_hz < RPU_CLK_MIN || r5_hz > RPU_CLK_MAX)
        return -ERANGE;
    rpll_rate = r5_hz * RPU_CLK_DIV;
    return len;
}

static ssize_t rpu_clk_en_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%d\n", rpll_on);
}

static ssize_t rpu_clk_en_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t len)
{
    bool on;
    int rc = kstrtobool(buf, &on);
    if (rc)
        return rc;
    rpll_on = on;
    return len;
}

static DEVICE_ATTR(rpu_clk, 0644, rpu_clk_show, rpu_clk_store);
static DEVICE_ATTR(rpu_clk_en, 0644, rpu_clk_en_show, rpu_clk_en_store);


static void init_attr(struct device_attribute *a, const char *name,
                      ssize_t (*show)(struct device *, struct device_attribute *, char *),
                      ssize_t (*store)(struct device *, struct device_attribute *, const char *, size_t))
{
    sysfs_attr_init(&a->attr);
    a->attr.name = name;
    a->attr.mode = 0644;
    a->show = show;
    a->store = store;
}

static struct attribute *mock_clk_attrs[2 * N_FCLK + 3];

static const struct attribute_group mock_clk_attr_group = {
    .attrs = mock_clk_attrs,
};

int mock_clk_add_files(struct device *dev)
{
    int n = 0;
    int rc;

    for (int i = 0; i < N_FCLK; i++) {
        init_attr(&fclk_attrs[i], fclk_names[i], fclk_show, fclk_store);
        init_attr(&fclk_en_attrs[i], fclk_en_names[i], fclk_en_show, fclk_en_store);
        mock_clk_attrs[n++] = &fclk_attrs[i].attr;
        mock_clk_attrs[n++] = &fclk_en_attrs[i].attr;
    }
    mock_clk_attrs[n++] = &dev_attr_rpu_clk.attr;
    mock_clk_attrs[n++] = &dev_attr_rpu_clk_en.attr;
    mock_clk_attrs[n] = NULL;

    rc = sysfs_create_group(&dev->kobj, &mock_clk_attr_group);
    if (rc) {
        pr_err("%s: Failed to create the clock interface\nError:%d\n", __func__, rc);
        return rc;
    }
    for (int i = 0; mock_clk_attrs[i]; i++)
        mock_make_world_writable(dev, mock_clk_attrs[i]);
    return 0;
}

void mock_clk_remove_files(struct device *dev)
{
    sysfs_remove_group(&dev->kobj, &mock_clk_attr_group);
}
