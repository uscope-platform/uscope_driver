/*
 * FPGA programming through /dev/uscope_bitstream: the bitstream is stored and
 * IOCTL_PROGRAM_FPGA marks the FPGA as loaded.
 */

#include "mock_module.h"

static char *bitstream_buffer;
static bool fpga_loaded;

int mock_fpga_init(void)
{
    fpga_loaded = false;
    bitstream_buffer = vmalloc(BITSTREAM_SIZE);
    return bitstream_buffer ? 0 : -ENOMEM;
}

void mock_fpga_free(void)
{
    vfree(bitstream_buffer);
    bitstream_buffer = NULL;
}

ssize_t mock_fpga_write_bitstream(const char __user *buffer, size_t len, loff_t *offset)
{
    if (*offset + len > BITSTREAM_SIZE)
        return -EINVAL;
    if (copy_from_user(bitstream_buffer + *offset, buffer, len))
        return -EFAULT;
    *offset += len;
    if (dev_data->bitstream_len < *offset)
        dev_data->bitstream_len = *offset;
    return len;
}

void mock_program_fpga(void)
{
    pr_info("%s: FPGA BITSTREAM LENGTH: %zu\n", __func__, dev_data->bitstream_len);
    fpga_loaded = true;
}

bool mock_fpga_loaded(void)
{
    return fpga_loaded;
}
