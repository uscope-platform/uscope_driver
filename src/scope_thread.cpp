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
scope_thread::scope_thread(const std::string& driver_file, int32_t buffer_size, bool emulate_control, bool es, bool log) {
    std::cout << "scope_thread emulate_control mode: " << std::boolalpha << emulate_control;
    std::cout<< "scope_thread logging: "<< std::boolalpha <<log;

    if(log){
        std::cout << "scope_thread initialization started"<< std::endl;
    }
    int max_channels = 6;
    n_buffers_left = 0;
    scope_data_ready = false;
    internal_buffer_size = buffer_size;
    sc_scope_data_buffer.reserve(internal_buffer_size);
    data_holding_buffer.reserve(max_channels*internal_buffer_size);
    ch_data = {};
    log_enabled = log;
    debug_mode = emulate_control;
    emulate_scope = es;

    if(!debug_mode | emulate_scope){
        //mmap buffer
        fd_data = open(driver_file.c_str(), O_RDWR| O_SYNC);
        dma_buffer = (int32_t* ) malloc(buffer_size*sizeof(int32_t));
    } else {
        dma_buffer = (int32_t* ) mmap(nullptr, max_channels*buffer_size*sizeof(uint32_t), PROT_READ, MAP_ANONYMOUS, -1, 0);
    }

    if(log){
        std::cout << "scope_thread initialization ended"<< std::endl;
    }
}

void scope_thread::stop_thread() {
    if(debug_mode & !emulate_scope) {
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

void scope_thread::read_data(std::vector<float> &data_vector) {
    //if(log_enabled){
    //    std::cout << "scope_thread::read_data start"<< std::endl;
    //}
    if(debug_mode & !emulate_scope){
        read_data_debug(data_vector);
    } else{
        read_data_hw(data_vector);
    }
}


void scope_thread::read_data_debug(std::vector<float> &data_vector) {
    std::vector<float> mc_scope_data_buffer[6];
    for(auto & i : mc_scope_data_buffer){
        i.clear();
    }
    std::vector<float> tb;
    tb.push_back(0);
    for(int j= 0; j<internal_buffer_size/6-1; j++){
        tb.push_back(tb.back()+(float)1/10e3);
    }
    std::array<float,6> phases = {0, M_PI/3, 2*M_PI/3, M_PI, 3*M_PI/3, 5*M_PI/3};

    for(int i = 0; i<6; i++){
        for(int j= 0; j<internal_buffer_size/6; j++){
            //float sample = std::rand()%1000+1000*(i%6);
            float sample = 4000.0*sin(2*M_PI*50*tb[j]+phases[i]);
            float scaled_sample = sample*scaling_factors[i];
            mc_scope_data_buffer[i].push_back(scaled_sample);
        }
    }
    for(auto & i : mc_scope_data_buffer){
        data_vector.insert(data_vector.end(),i.begin(), i.end());
    }
}

void scope_thread::read_data_hw(std::vector<float> &data_vector) {
    for(int i = 0; i<6; i++){
        ch_data[i].clear();
    }
    read(fd_data, (void *) dma_buffer, internal_buffer_size * sizeof(unsigned int));
    shunt_data(dma_buffer);
    for(int i = 0; i< 6; i++){
        data_vector.insert(data_vector.end(), ch_data[i].begin(), ch_data[i].end());
    }
}


void scope_thread::shunt_data(const volatile int32_t * buffer_in) {
    std::vector<uint32_t> tmp_data;
    for(int i = 0; i<internal_buffer_size; i++){
        int channel_base = GET_CHANNEL(buffer_in[i]);
        unsigned int sample_size = channel_sizes[channel_base];

        float data_sample = scale_data(buffer_in[i],sample_size, scaling_factors[channel_base]);

        ch_data[channel_base].push_back(data_sample);
    }
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

