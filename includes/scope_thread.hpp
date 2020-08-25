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

#ifndef USCOPE_DRIVER_SCOPE_THREAD_HPP
#define USCOPE_DRIVER_SCOPE_THREAD_HPP

#include <string>
#include <iostream>

#include <cstring>

#include <vector>
#include <thread>
#include <atomic>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>

#define SCOPE_MODE_RUN 1
#define SCOPE_MODE_CAPTURE 2



class scope_thread {

public:
    scope_thread(const std::string& driver_file, int32_t buffer_size, bool debug);
    void start_capture(unsigned int n_buffers);
    [[nodiscard]] unsigned int check_capture_progress() const;
    [[nodiscard]] bool is_data_ready() const;
    void read_data(std::vector<uint32_t> &data_vector);
    void stop_thread();
private:
    void service_scope();
    void wait_for_Interrupt() const;

    bool writeback_done;
    int scope_mode;
    int internal_buffer_size;
    unsigned int n_buffers_left;
    int32_t *scope_data_buffer;
    bool debug_mode;
    std::atomic_bool scope_data_ready;
    volatile int32_t* dma_buffer;  ///mmapped buffer
    volatile uint32_t fd_data; /// Scope driver file descriptor
    std::vector<uint32_t> captured_data;
    std::thread scope_service_thread;
    bool thread_should_exit;
};


#endif //USCOPE_DRIVER_SCOPE_THREAD_HPP
