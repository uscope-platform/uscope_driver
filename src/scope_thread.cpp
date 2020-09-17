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

    if(log){
        std::cout << "scope_thread initialization started"<< std::endl;
    }

    multichannel_mode = false;
    writeback_done = true;
    thread_should_exit = false;
    n_buffers_left = 0;
    scope_data_ready = false;
    scope_mode = SCOPE_MODE_RUN;
    internal_buffer_size = buffer_size;
    sc_scope_data_buffer.reserve(internal_buffer_size);

    log_enabled = log;
    debug_mode = debug;

    if(!debug_mode){
        //mmap buffer
        fd_data = open(driver_file.c_str(), O_RDWR| O_SYNC);

        dma_buffer = (int32_t* ) mmap(nullptr, buffer_size*sizeof(uint32_t), PROT_READ, MAP_SHARED, fd_data, 0);
        if(dma_buffer == MAP_FAILED) {
            std::cerr << "Cannot mmap uio device: " << strerror(errno) <<std::endl;
        }
        scope_service_thread = std::thread(&scope_thread::service_scope,this);

    } else {
        dma_buffer = (int32_t* ) mmap(nullptr, buffer_size*sizeof(uint32_t), PROT_READ, MAP_ANONYMOUS, -1, 0);
    }

    uint32_t write_val = 1;
    write(fd_data, &write_val, sizeof(write_val));

    if(log){
        std::cout << "scope_thread initialization ended"<< std::endl;
    }
}

void scope_thread::service_scope() {
    if(log_enabled){
        std::cout << "scope_thread::service_scope started"<< std::endl;
    }

    struct pollfd poll_file;
    poll_file.fd = fd_data;
    poll_file.events = POLLIN;
    poll(&poll_file, 1, 0);
    while(1){
        if(thread_should_exit) return;
        if((poll_file.revents&POLLIN) == POLLIN){
            wait_for_Interrupt();
            shunt_data(dma_buffer);
        } else{
            usleep(100);
        }

    }
    if(log_enabled){
        std::cout << "scope_thread::service_scope stopped"<< std::endl;
    }
}

void scope_thread::shunt_data(volatile int32_t * buffer_in) {

    std::copy(mc_scope_data_buffer[acquired_channels].begin(), mc_scope_data_buffer[acquired_channels].begin() + internal_buffer_size * sizeof(int32_t), buffer_in);

    acquired_channels++;
    
    if(acquired_channels==n_channels){
        acquired_channels = 0;
        scope_data_ready = true;
    }

}

void scope_thread::set_channel_status(std::vector<bool> status) {
    acquired_channels = 0;
    n_channels = 0;
    for(int i = 0; i< status.size(); i++){
        channel_status[i] = status[i];
        n_channels++;
    }


}

void scope_thread::stop_thread() {
    thread_should_exit = true;
    scope_service_thread.join();
    munmap((void*)dma_buffer, internal_buffer_size* sizeof(uint32_t));
    close(fd_data);
}


/// Wait for data to be ready to be read. This is signaled through an hardware interrupt. Since a UIO driver is
/// used the interrupt is waited upon with a read to the driver file
/// \return 0
void scope_thread::wait_for_Interrupt() const {
    uint32_t read_val;
    uint32_t write_val = 1;
    read(fd_data, &read_val, sizeof(read_val));
    write(fd_data, &write_val, sizeof(write_val));
}

void scope_thread::start_capture(unsigned int n_buffers) {
    n_buffers_left = n_buffers;
    scope_mode = SCOPE_MODE_CAPTURE;
    scope_data_ready = false;
    captured_data.clear();
}

/// This function returns the number of data buffers left to capture
/// \return Number of buffers still to capture
unsigned int scope_thread::check_capture_progress() const {
    return n_buffers_left;
}

bool scope_thread::is_data_ready() const {
    return scope_data_ready;
}

void scope_thread::read_data(std::vector<uint32_t> &data_vector) {
    if(debug_mode){
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
    } else{
        if(!scope_data_ready) return;
        if(scope_mode==SCOPE_MODE_CAPTURE){
            captured_data.clear();
        }
        data_vector.insert(data_vector.begin(), captured_data.begin(), captured_data.end());
        scope_data_ready = false;
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