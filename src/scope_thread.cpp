// Copyright (C) 2020 Filippo Savi - All Rights Reserved

// This file is part of uscope_driver.

// uscope_driver is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 3 of the
// License.

// uscope_driver is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with uscope_driver.  If not, see <https://www.gnu.org/licenses/>.

#include "scope_thread.hpp"

/// Initializes the scope_handler infrastructure, opening the UIO driver file and writing to it to clear any outstanding
/// interrupts
/// \param driver_file Path of the driver file
/// \param buffer_size Size of the capture buffer
scope_thread::scope_thread(const std::string& driver_file, int32_t buffer_size, bool debug, bool log) {
    std::cout<< "scope_thread debug mode: "<< std::boolalpha <<debug;
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
    debug_mode = debug;

    if(!debug_mode){
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


void scope_thread::shunt_data(const volatile int32_t * buffer_in) {
    std::vector<uint32_t> tmp_data;
    int channel_offset = 0;
    for(int i = 0; i<internal_buffer_size; i++){
        int channel_base = GET_CHANNEL(buffer_in[i]);
        uint32_t raw_data = sign_extend(buffer_in[i] & 0x3fff, 14);
        ch_data[channel_base].push_back(raw_data);
    }
}

void scope_thread::set_channel_status(std::vector<bool> status) {
    n_channels = 0;
    for(int i = 0; i< status.size(); i++){
        channel_status[i] = status[i];
        n_channels++;
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

void scope_thread::read_data(std::vector<uint32_t> &data_vector) {
    //if(log_enabled){
    //    std::cout << "scope_thread::read_data start"<< std::endl;
    //}
    if(debug_mode){
        read_data_debug(data_vector);
    } else{
        read_data_hw(data_vector);
    }
}


void scope_thread::read_data_debug(std::vector<uint32_t> &data_vector) {
    std::vector<uint32_t> data = emulate_scope_data();
    int progress[6] = {0};
    for(int i = 0; i< internal_buffer_size*n_channels; i++){
        int channel_idx = i%n_channels;
        mc_scope_data_buffer[channel_idx][progress[channel_idx]] = data[i];
        progress[channel_idx]++;
    }
    for(int i = 0; i<n_channels; i++){
        data_vector.insert(data_vector.end(),mc_scope_data_buffer[i].begin(), mc_scope_data_buffer[i].end());
    }
}

void scope_thread::read_data_hw(std::vector<uint32_t> &data_vector) {
    read(fd_data, (void *) dma_buffer, internal_buffer_size * sizeof(unsigned int));
    for(int i = 0; i<6; i++){
        ch_data[i].clear();
    }
    shunt_data(dma_buffer);
    for(int i = 0; i< 6; i++){
        data_vector.insert(data_vector.end(), ch_data[i].begin(), ch_data[i].end());
    }
}

std::vector<uint32_t> scope_thread::emulate_scope_data() const {

    std::vector<uint32_t> data;

    data.reserve(internal_buffer_size*n_channels);

    for(int i = 0; i< internal_buffer_size*n_channels; i++) {
        data[i] = std::rand()%1000+1000*(i%n_channels);
    }

    return data;
}
