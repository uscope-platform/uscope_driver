#include "hw_emulation/kernel_emulator.hpp"

struct fuse_session *se[3];

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<> distrib(0, 2<<16);

std::vector<uint32_t> generate_random_unsigned_data(uint32_t size, uint32_t average_value, uint32_t noise_ampl){
    std::vector<uint32_t> ret;
    for(int i = 0; i<size; i++){
        ret.push_back(average_value + distrib(gen) % noise_ampl);
    }
    return ret;
}

kernel_emulator::kernel_emulator(const std::string &intf, int dev_order, int ti) {
    const char *argv[] = {
            "progname",
            "-f"
    };

    char **fake_args = (char **) &argv;

    struct fuse_args args = FUSE_ARGS_INIT(2, fake_args);
    struct cuse_info control_dev_ci;

    const char *control_dev_info_argv[] = { intf.c_str()};

    memset(&control_dev_ci, 0, sizeof(control_dev_ci));
    control_dev_ci.dev_major = dev_order;
    control_dev_ci.dev_minor = dev_order;
    control_dev_ci.dev_info_argc = 1;
    control_dev_ci.dev_info_argv = control_dev_info_argv;
    control_dev_ci.flags = CUSE_UNRESTRICTED_IOCTL;


    int multithreaded;
    int res;

    int user_index = ti;
    se[ti] = cuse_lowlevel_setup(args.argc, args.argv, &control_dev_ci,  &kernel_emulator_ops, &multithreaded,
                                 &user_index);
    if (se[ti] == nullptr)
        throw std::runtime_error("Error while setting up the cuse session");

    if (multithreaded) {
        res = fuse_session_loop_mt(se[ti], 0);
    }
    else
        res = fuse_session_loop(se[ti]);

    if(ti ==2){
        fuse_remove_signal_handlers(se[ti]);
    }
    fuse_session_destroy(se[ti]);
    if (res == -1)
        throw std::runtime_error("Error from the fuse session loop");

    fuse_opt_free_args(&args);
}

void kernel_emulator::open(fuse_req_t req, struct fuse_file_info *fi) {
    fuse_reply_open(req, fi);
}

void kernel_emulator::read(fuse_req_t req, size_t size, off_t off, struct fuse_file_info *fi) {
    (void)fi;
    const int n_channels = 7;
    const int n_datapoints = 1024;
    uint32_t data_array[n_channels*n_datapoints];
    // Generate channel_data
    std::array<std::vector<uint32_t>, n_channels> channel_data;
    for(int j = 0; j<n_channels;j++){
        channel_data[j] = generate_random_unsigned_data(n_datapoints, j*100, 20);
    }
    // Interleave channels and add id to MSB
    for(int i = 0; i<n_datapoints;i++){
        for(int j = 0; j<n_channels;j++){
            uint32_t channel_id = (j<<24);
            data_array[i*n_channels+j] =  channel_id | channel_data[j][i];
        }
    }
    char *data_array_raw = (char *) &data_array;
    fuse_reply_buf(req, data_array_raw, size > n_channels*n_datapoints*sizeof(uint32_t) ?  n_channels*n_datapoints*sizeof(uint32_t) : size);
}

void kernel_emulator::write(fuse_req_t req, const char *buf, size_t size, off_t off, struct fuse_file_info *fi) {
    fuse_reply_write(req, size);
}

void kernel_emulator::ioctl(fuse_req_t req, int cmd, void *arg, struct fuse_file_info *fi, unsigned int flags,
                            const void *in_buf, size_t in_bufsz, size_t out_bufsz) {

    if (flags & FUSE_IOCTL_COMPAT) {
        fuse_reply_err(req, ENOSYS);
        return;
    }

    if(cmd==4){
        int* data = (int*) fuse_req_userdata(req);
        std::cout<< "Received termination signal for thread number: " << data[0] << std::endl;
        fuse_session_exit(se[data[0]]);
    }

    fuse_reply_ioctl(req, 0, nullptr, 0);
}
