#ifndef USCOPE_DRIVER_KERNEL_EMULATOR_HPP
#define USCOPE_DRIVER_KERNEL_EMULATOR_HPP

#include <string>
#include <cstring>
#include <stdexcept>
#include <random>
#include <vector>
#include <array>

#include <cuse_lowlevel.h>
#include <fuse_opt.h>



class kernel_emulator {
public:
    explicit kernel_emulator(const std::string &intf, int dev_order, int ti);


    static void open(fuse_req_t req, struct fuse_file_info *fi);
    static void read(fuse_req_t req, size_t size, off_t off,
                             struct fuse_file_info *fi);
    static void write(fuse_req_t req, const char *buf, size_t size,
                              off_t off, struct fuse_file_info *fi);
    static void ioctl(fuse_req_t req, int cmd, void *arg,
                              struct fuse_file_info *fi, unsigned flags,
                              const void *in_buf, size_t in_bufsz, size_t out_bufsz);


private:
};

static const struct cuse_lowlevel_ops kernel_emulator_ops = {
        .open		= kernel_emulator::open,
        .read		= kernel_emulator::read,
        .write		= kernel_emulator::write,
        .ioctl		= kernel_emulator::ioctl,
};

#endif //USCOPE_DRIVER_KERNEL_EMULATOR_HPP
