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
#include <sys/ioctl.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>

#include <spdlog/spdlog.h>

#include "emulated_data_generator.hpp"
#include "channel_metadata.hpp"
#include "interfaces_dictionary.hpp"
#include "configuration.hpp"
#include "server_frontend/infrastructure/response.hpp"


#define GET_DATA(NUMBER) (NUMBER & 0xffffffff)
#define GET_CHANNEL(NUMBER) ((NUMBER & 0x0000ffff00000000)>>32)
#define GET_METADATA(NUMBER) ((NUMBER & 0xffff000000000000)>>48)

static int32_t sign_extend(uint32_t value, uint32_t bits) {

    int32_t result;

    int x;      // sign extend this b-bit number to r
    int r;      // resulting sign-extended number
    int const mask = 1U << (bits - 1); // mask can be pre-computed if b is fixed

    value = value & ((1U << bits) - 1);  // (Skip this if bits in x above position b are already zero.)
    result = (value ^ mask) - mask;
    return result;
}

struct acquisition_metadata{
    std::string mode;
    std::string trigger_mode;
    uint32_t trigger_source;
    float trigger_level;
    std::string level_type;
};


class scope_manager {

public:
    scope_manager();
    responses::response_code start_capture(unsigned int n_buffers);
    [[nodiscard]] unsigned int check_capture_progress() const;
    responses::response_code read_data(std::vector<nlohmann::json> &data_vector);
    responses::response_code set_channel_widths(std::vector<uint32_t> &widths);
    responses::response_code set_scaling_factors(std::vector<float> &sf);
    responses::response_code set_channel_status(std::unordered_map<int, bool>status);
    responses::response_code set_channel_signed(std::unordered_map<int, bool>signed_status);
    responses::response_code enable_manual_metadata();
    std::string get_acquisition_status();
    responses::response_code set_acquisition(const acquisition_metadata &data);

private:
    static constexpr int n_channels = 6;
    static constexpr int buffer_size = 1024;

    std::vector<std::vector<float>> shunt_data(const volatile uint64_t * buffer_in);
    float scale_data(uint32_t raw_sample, unsigned int size, float scaling_factor, bool is_signed, bool is_float);

    std::vector<uint32_t> channel_sizes;
    std::vector<float> scaling_factors;
    int internal_buffer_size;
    unsigned int n_buffers_left;
    std::vector<uint32_t> sc_scope_data_buffer;
    volatile uint64_t * dma_buffer;  ///mmapped buffer
    //MULTICHANNEL SUPPORT
    std::vector<uint32_t> data_holding_buffer;
    std::unordered_map<int, bool> channel_status;
    std::unordered_map<int, bool> signed_status;

    emulated_data_generator data_gen;
    bool manual_metadata = false;

};


#endif //USCOPE_DRIVER_SCOPE_THREAD_HPP


