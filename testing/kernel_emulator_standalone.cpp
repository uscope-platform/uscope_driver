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

#include <csignal>
#include <vector>
#include <random>
#include <iostream>
#include <array>

#include <unistd.h>
#include <fcntl.h>

#include "hw_interface/channel_metadata.hpp"

std::vector<int32_t> generate_white_noise(int32_t size, int32_t average_value, int32_t noise_ampl);
std::vector<int32_t> generate_sinusoid(uint32_t size, uint32_t amplitude, float frequency, uint32_t offset, uint32_t phase);
void fill_buffer();

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<> distrib(0, 2<<16);

int buf_fd;

/// main is the entry point of the program, as per standard C conventions
/// \param argc standard C arguments count
/// \param argv standard C arguments array
/// \return Program exit value
int main (int argc, char **argv) {


    bool emulate_hw = false;
    bool log_command = false;
    bool test_untriggered_scope = false;
    std::string scope_data_source;


    usleep(10000);

    std::cout<< "debug mode: "<< std::boolalpha <<emulate_hw<<std::endl;
    std::cout<< "logging: "<< std::boolalpha <<log_command<<std::endl;

    buf_fd = open("/dev/uscope_data", O_RDWR| O_SYNC);

    while(true) {
        fill_buffer();
        usleep(9000);
    }
}


void fill_buffer(void) {
    const int n_channels = 6;
    const int n_datapoints = 1024;
    uint64_t data_array[n_channels*n_datapoints];
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
    channel_metadata m;
    m.set_signed(true);
    m.set_size(16);


    // Interleave channels and add id to MSB
    for(int i = 0; i<n_datapoints;i++){
        for(int j = 0; j<n_channels;j++){
            uint64_t metadata = m.get_high_side_field(j);
            uint64_t data = 0xFFFFFFFF & channel_data[j][i];
            data_array[i*n_channels+j] =  metadata | data;
        }
    }

    char *data_array_raw = (char *) &data_array;

    write(buf_fd, data_array_raw,sizeof(data_array[0])*n_channels*n_datapoints);
    fsync(buf_fd);
}

std::vector<int32_t> generate_white_noise(int32_t size, int32_t average_value, int32_t noise_ampl) {
    std::vector<int32_t> ret;
    for(int i = 0; i<size; i++){
        ret.push_back(average_value + distrib(gen) % noise_ampl);
    }
    return ret;
}

std::vector<int32_t> generate_sinusoid(uint32_t size, uint32_t amplitude, float frequency, uint32_t offset, uint32_t phase) {
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