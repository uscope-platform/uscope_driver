

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

#ifndef USCOPE_DRIVER_SCOPE_ENDPOINTS_HPP
#define USCOPE_DRIVER_SCOPE_ENDPOINTS_HPP

#include <nlohmann/json.hpp>
#include "server_frontend/infrastructure/command.hpp"
#include "server_frontend/infrastructure/response.hpp"
#include "hw_interface/fpga_bridge.hpp"
#include "hw_interface/scope_manager.hpp"

class scope_endpoints {
public:
    scope_endpoints() = default;
    void set_accessor(const std::shared_ptr<bus_accessor> &ba, const std::shared_ptr<scope_accessor> &sa);
    nlohmann::json process_command(std::string command_string, nlohmann::json &arguments);
private:
    nlohmann::json process_read_data();
    nlohmann::json process_set_scaling_factors(nlohmann::json &arguments);
    nlohmann::json process_set_channel_status(nlohmann::json &arguments);
    nlohmann::json process_disable_dma(nlohmann::json &arguments);
    nlohmann::json process_get_acquisition_status();
    nlohmann::json process_set_scope_address(nlohmann::json &arguments);
    nlohmann::json process_set_acquisition(nlohmann::json &arguments);
    scope_manager scope;
};




#endif //USCOPE_DRIVER_SCOPE_ENDPOINTS_HPP
