

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

#include "options_repository.hpp"
#include "server_frontend/infrastructure/command.hpp"
#include "server_frontend/infrastructure/response.hpp"
#include "hw_interface/fpga_bridge.hpp"
#include "hw_interface/toolchain_manager.hpp"
#include "deployment/hil_emulator.hpp"

#include "deployment/hil_deployer.hpp"
#include "deployment/custom_deployer.hpp"

class cores_endpoints {
public:
    cores_endpoints();
    void set_options_repository(std::shared_ptr<options_repository> &rep) {
        options_rep = rep;
        emulator.set_options_repository(options_rep);
        hil.set_options_repository(options_rep);
    };
    void set_accessor(const std::shared_ptr<bus_accessor> &ba);
    nlohmann::json process_command(const std::string& command_string, const nlohmann::json &arguments);
private:
    nlohmann::json process_apply_program(const nlohmann::json &arguments);
    nlohmann::json process_emulate_hil(const nlohmann::json &arguments);
    nlohmann::json process_compile_program(const nlohmann::json &arguments);
    nlohmann::json process_hil_disassemble(const nlohmann::json &arguments);
    nlohmann::json process_hil_hardware_sim(const nlohmann::json &arguments);
    nlohmann::json process_deploy_hil(const nlohmann::json &arguments);
    nlohmann::json process_hil_set_in(const nlohmann::json &arguments);
    nlohmann::json process_hil_select_out(const nlohmann::json &arguments);
    nlohmann::json process_hil_debug(const nlohmann::json &arguments);
    nlohmann::json process_hil_stop();
    nlohmann::json process_hil_start();

    nlohmann::json process_set_hil_address_map(const nlohmann::json &arguments);
    nlohmann::json process_get_hil_address_map(const nlohmann::json &arguments);

    nlohmann::json process_set_debugger_option(const nlohmann::json &arguments);
    nlohmann::json process_get_debugger_option(const nlohmann::json &arguments);



    bool check_float_intness(double d){
        uint64_t rounded_addr = round(d);
        if(ceil(d) == rounded_addr && floor(d) == rounded_addr){
            return true;
        } else {
            return false;
        }
    };

    fpga_bridge hw;
    std::shared_ptr<options_repository> options_rep;
    hil_deployer hil;
    custom_deployer custom;

    toolchain_manager toolchain;
    hil_emulator emulator;
};


#endif //USCOPE_DRIVER_CORES_ENDPOINTS_HPP
