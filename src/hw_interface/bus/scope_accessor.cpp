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

    std::array<uint64_t, scope_accessor::n_channels*scope_accessor::buffer_size> ret_val{};
    ssize_t read_data = read(fd_data,ret_val.data(), ret_val.size()*sizeof(uint64_t));
    if(read_data==0) throw std::runtime_error("nothing to read");
    if(read_data<0) {
        spdlog::critical(std::strerror(errno));
    }
    spdlog::trace("READ_DATA: COPIED DATA FROM KERNEL ({0} words transferred)", read_data/sizeof(uint64_t));
    return ret_val;
}

