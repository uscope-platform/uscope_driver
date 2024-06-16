

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

#ifndef USCOPE_DRIVER_TOOLCHAIN_MANAGER_HPP
#define USCOPE_DRIVER_TOOLCHAIN_MANAGER_HPP

#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <utility>
#include <cstdint>

#include "fcore_cc.hpp"


struct io_metadata {
    std::string associated_io;
    uint32_t address;
    std::string type;
};


class toolchain_manager {
public:
    std::vector<uint32_t> compile(std::string content, nlohmann::json h, nlohmann::json io);

};


#endif //USCOPE_DRIVER_TOOLCHAIN_MANAGER_HPP
