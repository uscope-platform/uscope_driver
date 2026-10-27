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
#include <unistd.h>
#include <fcntl.h>
#include <array>
#include <memory>
#include "errors/result.hpp"
#include "hw_interface/interfaces_dictionary.hpp"

class scope_accessor {
public:
    /// Opens the data buffer of the kernel module
    static fcore::result<std::shared_ptr<scope_accessor>> create();

    /// Only create() can build the key, which keeps the constructor out of the reach of the callers
    class construction_key {
        friend class scope_accessor;
        construction_key() = default;
    };
    explicit scope_accessor(construction_key) {}
    ~scope_accessor();
    scope_accessor(const scope_accessor &) = delete;
    scope_accessor &operator=(const scope_accessor &) = delete;
    static constexpr int n_channels = 6;
    static constexpr int buffer_size = 1024;
    [[nodiscard]] fcore::result<std::array<uint64_t, n_channels*buffer_size>> get_scope_data();
    /// Blocks for at most timeout_ms waiting for the kernel module to signal a new buffer
    /// \return true if a new buffer is available to be read
    bool wait_for_data(int timeout_ms);
private:
    static constexpr unsigned long ioctl_new_data_available = 1;
    static constexpr  int internal_buffer_size = n_channels*buffer_size;
    volatile int fd_data = -1;
};


#endif //USCOPE_DRIVER_SCOPE_ACCESSOR_HPP
