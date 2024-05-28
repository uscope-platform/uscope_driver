

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

#ifndef USCOPE_DRIVER_PLATFORM_ENDPOINTS_HPP
#define USCOPE_DRIVER_PLATFORM_ENDPOINTS_HPP


#include <nlohmann/json.hpp>

#include "hw_interface/fpga_bridge.hpp"
#include "hw_interface/timing_manager.hpp"
#include "driver_version.h"

class platform_endpoints {
public:
    explicit platform_endpoints(std::shared_ptr<fpga_bridge> &hw);
    nlohmann::json process_command(const std::string& command_string, nlohmann::json &arguments);
private:
    nlohmann::json process_set_clock(nlohmann::json &arguments);
    nlohmann::json process_get_clock(nlohmann::json &arguments);
    nlohmann::json process_get_version(nlohmann::json &arguments);

    std::shared_ptr<fpga_bridge> hw;
    timing_manager tm;

};


#endif //USCOPE_DRIVER_PLATFORM_ENDPOINTS_HPP
