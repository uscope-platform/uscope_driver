// Copyright 2024 Filippo Savi
// Author: Filippo Savi <filssavi@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hw_emulation/kernel_emulator.hpp"

struct fuse_session *se[3];

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<> distrib(0, 2<<16);


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
    std::cout<<"HERE!!"<< std::endl;
    const int n_channels = 7;
    const int n_datapoints = 1024;
    uint32_t data_array[n_channels*n_datapoints];
    // Generate channel_data

    std::array<std::vector<int32_t>, n_channels> channel_data;

    /*
    for(int j = 0; j<n_channels;j++){
        channel_data[j] = generate_white_noise(n_datapoints, j*100, 20);
    }
    */

    for(int j = 0; j<n_channels;j++){
        channel_data[j] = generate_sinusoid(n_datapoints, 20, 500, 0, j+100);
    }

    // Interleave channels and add id to MSB
    for(int i = 0; i<n_datapoints;i++){
        for(int j = 0; j<n_channels;j++){
            uint32_t channel_id = (j<<24);
            uint32_t  raw_data = 0xFFFFFF & channel_data[j][i];
            data_array[i*n_channels+j] =  channel_id | raw_data;
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

std::vector<int32_t>
kernel_emulator::generate_white_noise(int32_t size, int32_t average_value, int32_t noise_ampl) {
    std::vector<int32_t> ret;
    for(int i = 0; i<size; i++){
        ret.push_back(average_value + distrib(gen) % noise_ampl);
    }
    return ret;
}

std::vector<int32_t>
kernel_emulator::generate_sinusoid(uint32_t size, uint32_t amplitude, float frequency, uint32_t offset, uint32_t phase) {
    std::vector<int32_t> ret;
    for(int i = 0; i<size; i++){
        double t = i*1e-6;
        auto omega = 2*M_PI*frequency;
        auto noise = distrib(gen) % amplitude/10.0;
        int32_t val = amplitude*sin(omega*t + phase) + offset + noise;
        ret.push_back(val);
    }
    return ret;
}
