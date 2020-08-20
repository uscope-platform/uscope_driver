//
// Created by fils on 20/08/2020.
//


#include "scope_thread.hpp"

/// Initializes the scope_handler infrastructure, opening the UIO driver file and writing to it to clear any outstanding
/// interrupts
/// \param driver_file Path of the driver file
/// \param buffer_size Size of the capture buffer
scope_thread::scope_thread(const std::string& driver_file, int32_t buffer_size, bool debug) {
    writeback_done = true;
    thread_should_exit = false;
    n_buffers_left = 0;
    scope_data_ready = false;
    scope_mode = SCOPE_MODE_RUN;
    internal_buffer_size = buffer_size;
    scope_data_buffer = (int32_t *) malloc(internal_buffer_size* sizeof(int32_t));
    debug_mode = debug;

    if(!debug_mode){
        //mmap buffer
        fd_data = open(driver_file.c_str(), O_RDWR| O_SYNC);

        dma_buffer = (int32_t* ) mmap(nullptr, buffer_size*sizeof(uint32_t), PROT_READ, MAP_SHARED, fd_data, 0);
        if(dma_buffer == MAP_FAILED) {
            std::cerr << "Cannot mmap uio device: " << strerror(errno) <<std::endl;
        }
    } else {
        fd_data = open("/dev/zero", O_RDWR| O_SYNC);
        dma_buffer = (int32_t* ) mmap(nullptr, buffer_size*sizeof(uint32_t), PROT_READ, MAP_SHARED, fd_data, 0);
    }


    uint32_t write_val = 1;
    write(fd_data, &write_val, sizeof(write_val));

    scope_service_thread = std::thread(&scope_thread::service_scope,this);

}

void scope_thread::service_scope() {
    while(!thread_should_exit){
        wait_for_Interrupt();
        memcpy(scope_data_buffer, (void *)dma_buffer,internal_buffer_size*sizeof(int32_t));

        if(scope_mode==SCOPE_MODE_CAPTURE){

            captured_data.insert(captured_data.end(), scope_data_buffer,scope_data_buffer+internal_buffer_size);
            n_buffers_left--;
        }
        scope_data_ready = true;
    }

}

scope_thread::~scope_thread() {
    munmap((void*)dma_buffer, internal_buffer_size* sizeof(uint32_t));
    close(fd_data);
    thread_should_exit = true;
    scope_service_thread.join();
    free(scope_data_buffer);
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

std::vector <uint32_t> scope_thread::read_data() {
    std::vector<uint32_t> return_val;

    if(!scope_data_ready) return return_val;
    if(scope_mode==SCOPE_MODE_CAPTURE){
        captured_data.clear();
    }
    return_val = captured_data;
    scope_data_ready = false;
    return captured_data;
}
