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

#ifndef USCOPE_DRIVER_SCOPE_ACCESSOR_HPP
#define USCOPE_DRIVER_SCOPE_ACCESSOR_HPP

#include <spdlog/spdlog.h>
#include <fcntl.h>
#include "hw_interface/interfaces_dictionary.hpp"

class scope_accessor {
public:
    scope_accessor();
    ~scope_accessor();
    static constexpr int n_channels = 6;
    static constexpr int buffer_size = 1024;
    std::array<uint64_t, n_channels*buffer_size> get_scope_data();
private:
    static constexpr  int internal_buffer_size = n_channels*buffer_size;
    volatile int fd_data;
};


#endif //USCOPE_DRIVER_SCOPE_ACCESSOR_HPP
