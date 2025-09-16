// Copyright 2021 University of Nottingham Ningbo China
// Author: Filippo Savi <filssavi@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include <nlohmann/json.hpp>
#include <CLI/CLI.hpp>

#include "deployment/hil_deployer.hpp"
#include "deployment/custom_deployer.hpp"


interfaces_dictionary if_dict;
configuration runtime_config;

int main (int argc, char **argv) {

    std::string input_file;
    CLI::App app{"fCore C compiler"};

    app.add_option("--file", input_file, "Path for the deployment artifact json");

    CLI11_PARSE(app, argc, argv);

    runtime_config.emulate_hw = true;
    runtime_config.debug_hil = true;
    spdlog::set_level(spdlog::level::trace);
    runtime_config.server_port = 6666;

    auto arch = std::getenv("ARCH");
    if_dict.set_arch(arch);

    std::ifstream ifs(input_file);
    nlohmann::json spec = nlohmann::json::parse(ifs);

    uint64_t cores_rom_base = 0x5'0000'0000;
    uint64_t cores_rom_offset = 0x1000'0000;

    uint64_t cores_control_base = 0x4'43c2'0000;
    uint64_t cores_control_offset =    0x1'0000;

    uint64_t cores_inputs_base_address = 0x2000;
    uint64_t cores_inputs_offset = 0x1000;

    uint64_t dma_offset =   0x1000;

    uint64_t controller_base =   0x4'43c1'0000;
    uint64_t controller_tb_offset =   0x1000;
    uint64_t scope_mux_base = 0x4'43c5'0000;

    uint64_t hil_control_base = 0x4'43c0'0000;


    nlohmann::json addr_map, offsets, bases;
    bases["cores_rom"] = cores_rom_base;
    bases["cores_control"] = cores_control_base;
    bases["cores_inputs"] = cores_inputs_base_address;
    bases["controller"] = controller_base;

    bases["scope_mux"] = scope_mux_base;
    bases["hil_control"] = hil_control_base;


    offsets["cores_rom"] = cores_rom_offset;
    offsets["cores_control"] =cores_control_offset;
    offsets["controller"] = controller_tb_offset;
    offsets["cores_inputs"] = cores_inputs_offset;
    offsets["dma"] = dma_offset;
    offsets["hil_tb"] = 0;
    addr_map["bases"] = bases;
    addr_map["offsets"] = offsets;



    fcore::emulator_dispatcher em;
    em.set_specs(spec);
    bool custom_deploy = false;
    if(spec.contains("deployment_mode")) {
        custom_deploy = spec["deployment_mode"];
    }

    auto ba = std::make_shared<bus_accessor>(true);
    if(custom_deploy){
        custom_deployer c;
        c.set_accessor(ba);
        c.set_layout_map(addr_map);
        c.deploy(spec);
    } else {
        hil_deployer d;
        d.set_accessor(ba);
        d.set_layout_map(addr_map);
        d.deploy(spec);
    }

    return 0;
}
