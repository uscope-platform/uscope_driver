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

#include "scope_thread.hpp"

/// Initializes the scope_handler infrastructure, opening the UIO driver file and writing to it to clear any outstanding
/// interrupts
/// \param driver_file Path of the driver file
/// \param buffer_size Size of the capture buffer
scope_thread::scope_thread(const std::string& driver_file, int32_t buffer_size, bool emulate_control, bool log) : data_gen(buffer_size){
    std::cout << "scope_thread emulate_control mode: " << std::boolalpha << emulate_control << std::endl;
    std::cout<< "scope_thread logging: "<< std::boolalpha <<log << std::endl;

    if(log){
        std::cout << "scope_thread initialization started"<< std::endl;
    }
    int max_channels = 6;
    n_buffers_left = 0;
    scope_data_ready = false;
    internal_buffer_size = buffer_size;
    sc_scope_data_buffer.reserve(internal_buffer_size);
    data_holding_buffer.reserve(max_channels*internal_buffer_size);
    scaling_factors = {1,1,1,1,1,1};
    log_enabled = log;
    debug_mode = emulate_control;

    if(!debug_mode){
        //mmap buffer
        fd_data = open(driver_file.c_str(), O_RDWR| O_SYNC);
        dma_buffer = (int32_t* ) malloc(buffer_size*sizeof(int32_t));
    } else {
        if(std::filesystem::exists(driver_file)){
            data_gen.set_data_file(driver_file);
        }
        dma_buffer = (int32_t* ) mmap(nullptr, max_channels*buffer_size*sizeof(uint32_t), PROT_READ, MAP_ANONYMOUS, -1, 0);
    }

    if(log){
        std::cout << "scope_thread initialization ended"<< std::endl;
    }
}

void scope_thread::stop_thread() {
    if(debug_mode) {
        munmap((void *) dma_buffer, 6 * internal_buffer_size * sizeof(uint32_t));
    }
    close(fd_data);
}

void scope_thread::start_capture(unsigned int n_buffers) {
    n_buffers_left = n_buffers;
    scope_data_ready = false;
}

/// This function returns the number of data buffers left to capture
/// \return Number of buffers still to capture
unsigned int scope_thread::check_capture_progress() const {
    return n_buffers_left;
}

bool scope_thread::is_data_ready() {
    bool result = scope_data_ready;
    return result;

}

void scope_thread::read_data(std::vector<nlohmann::json> &data_vector) {

    std::vector<std::vector<float>> data;
    if(debug_mode){
        read_data_debug(data);
    } else{
        read_data_hw(data);
    }
    for(int i = 0; i<n_channels; i++){
        nlohmann::json ch_obj;
        if(channel_status[i]){
            ch_obj["channel"] = i;
            ch_obj["data"] = data[i];
            data_vector.push_back(ch_obj);
        }
    }
}


void scope_thread::read_data_debug(std::vector<std::vector<float>> &data_vector) {
    data_vector = data_gen.get_data(scaling_factors);
}

void scope_thread::read_data_hw(std::vector<std::vector<float>> &data_vector) {
    read(fd_data, (void *) dma_buffer, internal_buffer_size * sizeof(unsigned int));
    data_vector = shunt_data(dma_buffer);

}


std::vector<std::vector<float>> scope_thread::shunt_data(const volatile int32_t * buffer_in) {
    std::vector<std::vector<float>> ret_data;
    ret_data.reserve(n_channels);
    for(int i = 0; i<internal_buffer_size; i++){
        int channel_base = GET_CHANNEL(buffer_in[i]);
        unsigned int sample_size = channel_sizes[channel_base];

        float data_sample = scale_data(buffer_in[i],sample_size, scaling_factors[channel_base]);

        ret_data[channel_base].push_back(data_sample);
    }
    return ret_data;
}

float scope_thread::scale_data(uint32_t raw_sample, unsigned int size, float scaling_factor) {
    int32_t sample;
    if(size>100){ // USE CHANNELS > 100 to signal signedness
        auto true_size = size-100;
        sample = raw_sample & ((1<<true_size)-1);
    } else {
        auto masked_sample = raw_sample & ((1<<size)-1);
        sample = sign_extend(masked_sample, size);
    }

    return scaling_factor*(float)sample;
}

void scope_thread::set_channel_widths(std::vector<uint32_t> &widths) {
    channel_sizes = widths;
}

void scope_thread::set_scaling_factors(std::vector<float> &sf) {
    scaling_factors = sf;
}

