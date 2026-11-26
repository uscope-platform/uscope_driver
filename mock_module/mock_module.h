/*
 * Mock of the uScope kernel module (software/kernel_dev/uscope_module), so the
 * driver and its tests can run on a regular PC. It exposes the same device
 * nodes and sysfs files as the real module, with plain memory behind them.
 */

#ifndef MOCK_MODULE_H
#define MOCK_MODULE_H

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/sizes.h>

#define N_MINOR_NUMBERS	6

/* One shared memory + doorbell device per RPU core: /dev/uscope_shm_<core> */
#define N_RPU_CORES 2
#define SHM_MINOR_BASE 4        /* minor of /dev/uscope_shm_0 */

#define N_CHANNELS 6
#define KERNEL_BUFFER_LENGTH 1024
#define KERNEL_BUFFER_SIZE (KERNEL_BUFFER_LENGTH*N_CHANNELS*8)
#define BITSTREAM_SIZE 32000000

#define IOCTL_NEW_DATA_AVAILABLE 1
#define IOCTL_GET_BUFFER_ADDRESS 2
#define IOCTL_PROGRAM_FPGA 3
#define IOCTL_RING_RPU 4        /* on /dev/uscope_shm_<core>: ring that core */

/* R5 clock = RPLL / RPU_CLK_DIV; rpu_clk accepts RPU_CLK_MIN..RPU_CLK_MAX */
#define RPU_CLK_DIV 2
#define RPU_CLK_MIN 375000000U
#define RPU_CLK_MAX 500000000U
#define RPU_CLK_DEFAULT_FREQ 500000000U

/* fake physical base of the shared memory windows, below 4 GB like the real CMA allocations */
#define MOCK_SHM_PHYS_BASE 0x70000000ULL

#define N_FCLK 4
#define FCLK_0_DEFAULT_FREQ 100000000
#define FCLK_1_DEFAULT_FREQ 40000000
#define FCLK_2_DEFAULT_FREQ 40000000
#define FCLK_3_DEFAULT_FREQ 40000000

/* Per RPU core: shared memory window and IPI doorbell (mock_rpu.c) */
struct mock_rpu {
    void *shm_va;
    u64 shm_phys;
    size_t shm_size;
    wait_queue_head_t ipi_wq;
    atomic_t ipi_count;         /* doorbells received from this core */
    atomic_t ring_count;        /* mock only: doorbells sent to this core */
};

/* STRUCTURE FOR THE DEVICE SPECIFIC DATA*/
struct scope_device_data {
    struct device devs[N_MINOR_NUMBERS];
    struct cdev cdevs[N_MINOR_NUMBERS];
    u32 *read_data_buffer;
    u32 dma_buf_size;
    size_t bitstream_len;
    wait_queue_head_t poll_queue;
    int new_data_available;
    struct mock_rpu rpu[N_RPU_CORES];
};

/* Shared module state */
extern struct scope_device_data *dev_data;

/* RPU core served by a /dev/uscope_shm_<core> minor, or NULL for other minors */
static inline struct mock_rpu *mock_minor_rpu(int minor)
{
    if (minor < SHM_MINOR_BASE || minor >= SHM_MINOR_BASE + N_RPU_CORES)
        return NULL;
    return &dev_data->rpu[minor - SHM_MINOR_BASE];
}

/* Open up a sysfs file so the driver and the tests can use it without being root */
void mock_make_world_writable(struct device *dev, const struct attribute *attr);

/* Character device operations (mock_chrdev.c) */
extern struct file_operations mock_file_ops;
void mock_chrdev_free(void);

/* Data buffer and its sysfs interface (mock_dma.c) */
int mock_dma_init(struct device *dev);
void mock_dma_free(struct device *dev);

/* PL and R5 clocks (mock_fclk.c) */
int mock_clk_add_files(struct device *dev);
void mock_clk_remove_files(struct device *dev);

/* FPGA programming (mock_fpga.c) */
int mock_fpga_init(void);
void mock_fpga_free(void);
ssize_t mock_fpga_write_bitstream(const char __user *buffer, size_t len, loff_t *offset);
void mock_program_fpga(void);
bool mock_fpga_loaded(void);

/* Shared memory windows and IPI doorbells (mock_rpu.c) */
int mock_rpu_init(struct device *dev);
void mock_rpu_free(struct device *dev);
int mock_shm_mmap(struct mock_rpu *r, struct vm_area_struct *vma);
int mock_ipi_ring(struct mock_rpu *r);

/* Fake R5 cores in the remoteproc framework (mock_rproc.c) */
int mock_rproc_init(struct device *dev);
void mock_rproc_free(struct device *dev);

#endif /* MOCK_MODULE_H */
