

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

#ifndef USCOPE_DRIVER_CONTROL_ENDPOINTS_HPP
#define USCOPE_DRIVER_CONTROL_ENDPOINTS_HPP

#include <nlohmann/json.hpp>
#include "server_frontend/infrastructure/command.hpp"
#include "server_frontend/infrastructure/response.hpp"
#include "hw_interface/fpga_bridge.hpp"

class control_endpoints {
public:
    explicit control_endpoints(std::shared_ptr<fpga_bridge> &hw);
    nlohmann::json process_command(const std::string& command_string, nlohmann::json &arguments);
private:
    nlohmann::json process_single_write_register(nlohmann::json &arguments);
    nlohmann::json process_single_read_register(nlohmann::json &arguments);
    nlohmann::json process_set_frequency(nlohmann::json &arguments);
    nlohmann::json process_load_bitstream(nlohmann::json &arguments);
    nlohmann::json process_set_scope_data(nlohmann::json &arguments);
    nlohmann::json process_apply_filter(nlohmann::json &arguments);

    std::shared_ptr<fpga_bridge> hw;
};


#endif //USCOPE_DRIVER_CONTROL_ENDPOINTS_HPP