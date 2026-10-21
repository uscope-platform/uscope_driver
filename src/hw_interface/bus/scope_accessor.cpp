//   Copyright 2024 Filippo Savi <filssavi@gmail.com>
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "hw_interface/bus/scope_accessor.hpp"

#include <poll.h>
#include <thread>
#include <chrono>
#include <sys/ioctl.h>

scope_accessor::scope_accessor() {
    std::string data_file = if_dict.get_data_bus();
    fd_data = open(data_file.c_str(), O_RDWR| O_SYNC);
    if(fd_data == -1){
        spdlog::error("Error while mapping the scope data buffer: {0}", std::strerror(errno));
    }

}

scope_accessor::~scope_accessor() {
    close(fd_data);
}

std::array<uint64_t, scope_accessor::n_channels*scope_accessor::buffer_size>  scope_accessor::get_scope_data() {

    std::array<uint64_t, n_channels*buffer_size> ret_val{};
    auto read_size = ret_val.size()*sizeof(uint64_t);
    ssize_t read_data = read(fd_data,ret_val.data(), read_size);
    if(read_data==0) throw std::runtime_error("nothing to read");
    if(read_data<0) {
        spdlog::critical(std::strerror(errno));
    }
    spdlog::trace("READ_DATA: COPIED DATA FROM KERNEL ({0} words transferred)", read_data/sizeof(uint64_t));
    return ret_val;
}


bool scope_accessor::wait_for_data(int timeout_ms) {
    pollfd pfd{fd_data, POLLIN, 0};
    if(poll(&pfd, 1, timeout_ms) <= 0) return false;

    // the poll of the kernel module can report readiness without a new buffer, so confirm it
    if(ioctl(fd_data, ioctl_new_data_available) > 0) return true;

    // avoid spinning if poll keeps returning immediately
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return false;
}
