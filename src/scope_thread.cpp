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
scope_thread::scope_thread(const std::string& driver_file, int32_t buffer_size, bool debug) {
    multichannel_mode = false;
    writeback_done = true;
    thread_should_exit = false;
    n_buffers_left = 0;
    scope_data_ready = false;
    scope_mode = SCOPE_MODE_RUN;
    internal_buffer_size = buffer_size;
    sc_scope_data_buffer.reserve(internal_buffer_size);
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

}

void scope_thread::service_scope() {
    struct pollfd poll_file;
    poll_file.fd = fd_data;
    poll_file.events = POLLIN;
    poll(&poll_file, 1, 0);
    while(1){
        if(thread_should_exit) return;
        if((poll_file.revents&POLLIN) == POLLIN){
            wait_for_Interrupt();
            if(multichannel_mode) shunt_data_mc(dma_buffer);
            else shunt_data_sc(dma_buffer);
        } else{
            usleep(100);
        }

    }

}

void scope_thread::shunt_data_sc(volatile int32_t * buffer_in) {
    std::copy(sc_scope_data_buffer.begin(), sc_scope_data_buffer.begin() + internal_buffer_size * sizeof(int32_t), buffer_in);

    if(scope_mode==SCOPE_MODE_CAPTURE){
        captured_data.insert(captured_data.end(), sc_scope_data_buffer.begin(), sc_scope_data_buffer.begin() + internal_buffer_size);
        n_buffers_left--;
    }
    scope_data_ready = true;
}

void scope_thread::shunt_data_mc(volatile int32_t * buffer_in) {

    std::copy(mc_scope_data_buffer[acquired_channels].begin(), mc_scope_data_buffer[acquired_channels].begin() + internal_buffer_size * sizeof(int32_t), buffer_in);

    acquired_channels++;
    
    if(acquired_channels==n_channels){
        acquired_channels = 0;
        scope_data_ready = true;
    }

}

void scope_thread::enable_channel(int channel) {
    if(n_channels== 0) multichannel_mode = false;
    else multichannel_mode = true;
    acquired_channels = 0;
    n_channels++;
    sc_scope_data_buffer.clear();
    for(auto &item:mc_scope_data_buffer){
        item.reserve(internal_buffer_size);
    }
    channel_status[channel] = true;
}

void scope_thread::disable_channel(int channel) {
    n_channels--;
    if(n_channels== 1) multichannel_mode = false;
    else multichannel_mode = true;
    channel_status[channel] = false;
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
        if(multichannel_mode){
            std::vector<uint32_t> data = emulate_scope_data();

        } else {
            data_vector = emulate_scope_data();
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

std::vector<uint32_t> scope_thread::emulate_scope_data() {
    std::vector<uint32_t> data;
    // obtain a seed from the system clock:
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::minstd_rand0 g1 (seed);
    if(multichannel_mode){
        for(int i = 0; i< 1024*n_channels; i++) data.push_back(g1()%1000+1000*i%6);
    } else {
        for(int i = 0; i< 1024; i++) data.push_back(g1()%1000);
    }
    return data;
}
