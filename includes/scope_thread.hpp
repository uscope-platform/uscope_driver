// Copyright 2021 University of Nottingham Ningbo China
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

#ifndef USCOPE_DRIVER_SCOPE_THREAD_HPP
#define USCOPE_DRIVER_SCOPE_THREAD_HPP

#include <string>
#include <iostream>

#include <cstring>

#include <random>
#include <functional>
#include <utility>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>

#include "emulated_data_generator.hpp"

#define SCOPE_MODE_RUN 1
#define SCOPE_MODE_CAPTURE 2


#define GET_CHANNEL(NUMBER) ((NUMBER >> 24) & 0xff)

static int32_t sign_extend(uint32_t value, uint32_t bits) {

    int32_t result;

    int x;      // sign extend this b-bit number to r
    int r;      // resulting sign-extended number
    int const mask = 1U << (bits - 1); // mask can be pre-computed if b is fixed

    value = value & ((1U << bits) - 1);  // (Skip this if bits in x above position b are already zero.)
    result = (value ^ mask) - mask;
    return result;
}

class scope_thread {

public:
    scope_thread(const std::string& driver_file, int32_t buffer_size, bool emulate_control, bool log);
    void start_capture(unsigned int n_buffers);
    [[nodiscard]] unsigned int check_capture_progress() const;
    [[nodiscard]] bool is_data_ready();
    void read_data(std::vector<nlohmann::json> &data_vector);
    void stop_thread();
    void set_channel_widths(std::vector<uint32_t> &widths);
    void set_scaling_factors(std::vector<float> &sf);
    void set_channel_status(std::unordered_map<int, bool>status);

private:
    static constexpr int n_channels = 7;
    void read_data_hw(std::vector<std::vector<float>> &data_vector);
    void read_data_debug(std::vector<std::vector<float>> &data_vector);
    std::vector<std::vector<float>> shunt_data(const volatile int32_t * buffer_in);
    float scale_data(uint32_t raw_sample, unsigned int size, float scaling_factor);

    std::vector<uint32_t> channel_sizes;
    std::vector<float> scaling_factors;
    int internal_buffer_size;
    unsigned int n_buffers_left;
    std::vector<uint32_t> sc_scope_data_buffer;
    bool debug_mode;
    bool log_enabled;
    bool emulate_scope;
    std::atomic_bool scope_data_ready;
    volatile int32_t* dma_buffer;  ///mmapped buffer
    volatile int fd_data; /// Scope driver file descriptor
    std::array<uint32_t, n_channels*1024> captured_data;
    //MULTICHANNEL SUPPORT
    std::vector<uint32_t> data_holding_buffer;
    std::array<uint32_t, n_channels*1024> mc_data_buffer;
    std::unordered_map<int, bool> channel_status;

     emulated_data_generator data_gen;

};


#endif //USCOPE_DRIVER_SCOPE_THREAD_HPP


