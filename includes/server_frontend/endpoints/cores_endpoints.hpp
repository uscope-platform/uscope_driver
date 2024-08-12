

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

#ifndef USCOPE_DRIVER_CORES_ENDPOINTS_HPP
#define USCOPE_DRIVER_CORES_ENDPOINTS_HPP

#include <nlohmann/json.hpp>

#include "server_frontend/infrastructure/command.hpp"
#include "server_frontend/infrastructure/response.hpp"
#include "hw_interface/fpga_bridge.hpp"
#include "hw_interface/toolchain_manager.hpp"
#include "hil/hil_deployer.hpp"
#include "hil/hil_emulator.hpp"

class cores_endpoints {
public:
    explicit cores_endpoints(std::shared_ptr<fpga_bridge> &h);
    nlohmann::json process_command(const std::string& command_string, nlohmann::json &arguments);
private:
    nlohmann::json process_apply_program(nlohmann::json &arguments);
    nlohmann::json process_emulate_hil(nlohmann::json &arguments);
    nlohmann::json process_compile_program(nlohmann::json &arguments);
    nlohmann::json process_deploy_hil(nlohmann::json &arguments);
    nlohmann::json process_hil_set_in(nlohmann::json &arguments);
    nlohmann::json process_hil_select_out(nlohmann::json &arguments);
    nlohmann::json process_hil_stop();
    nlohmann::json process_hil_start();
    nlohmann::json process_set_layout_map(nlohmann::json &arguments);
    nlohmann::json process_set_hil_address_map(nlohmann::json &arguments);
    nlohmann::json process_get_hil_address_map(nlohmann::json &arguments);

    std::shared_ptr<fpga_bridge> hw;
    hil_deployer hil;
    toolchain_manager toolchain;
    hil_emulator emulator;
};


#endif //USCOPE_DRIVER_CORES_ENDPOINTS_HPP
