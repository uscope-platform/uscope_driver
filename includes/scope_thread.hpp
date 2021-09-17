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

#define SCOPE_MODE_RUN 1
#define SCOPE_MODE_CAPTURE 2

#define N_CHANNELS 6


#define GET_CHANNEL(NUMBER) (NUMBER >> 24) & 0xff

static uint32_t sign_extend(uint32_t value, uint32_t bits) {
    uint32_t sign = (1 << (bits - 1)) & value;
    uint32_t mask = ((~0U) >> (bits - 1)) << (bits - 1);
    if (sign != 0)
        value |= mask;
    else
        value &= ~mask;
    return value;
}

class scope_thread {

public:
    scope_thread(const std::string& driver_file, int32_t buffer_size, bool debug, bool log);
    void start_capture(unsigned int n_buffers);
    [[nodiscard]] unsigned int check_capture_progress() const;
    [[nodiscard]] bool is_data_ready();
    void read_data(std::vector<uint32_t> &data_vector);
    void stop_thread();
    int set_channel_widths( std::vector<uint32_t> widths);

private:
    void read_data_hw(std::vector<uint32_t> &data_vector);
    void read_data_debug(std::vector<uint32_t> &data_vector);
    void shunt_data(const volatile int32_t * buffer_in);

    std::vector<uint32_t> channel_sizes;
    int internal_buffer_size;
    unsigned int n_buffers_left;
    std::vector<uint32_t> sc_scope_data_buffer;
    bool debug_mode;
    bool log_enabled;
    std::atomic_bool scope_data_ready;
    volatile int32_t* dma_buffer;  ///mmapped buffer
    volatile uint32_t fd_data; /// Scope driver file descriptor
    std::array<uint32_t, 6*1024> captured_data;
    std::array<std::vector<uint32_t>, 6> ch_data;
    //MULTICHANNEL SUPPORT
    std::vector<uint32_t> data_holding_buffer;
    std::array<uint32_t, 6*1024> mc_data_buffer;
    std::vector<uint32_t> mc_scope_data_buffer[6]; //TODO: make internal array dynamic

};


#endif //USCOPE_DRIVER_SCOPE_THREAD_HPP


