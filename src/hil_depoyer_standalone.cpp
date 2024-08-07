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

#include "hil/hil_deployer.hpp"


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

    uint64_t dma_base =   0x4'43c2'1000;
    uint64_t dma_offset =      0x1'0000;

    uint64_t controller_base =   0x4'43c1'0000;
    uint64_t controller_tb_offset =   0x1000;
    uint64_t scope_mux_base = 0x4'43c5'0000;

    uint64_t hil_control_base = 0x4'43c0'0000;
    auto hw_bridge = std::make_shared<fpga_bridge>();
    hil_deployer d(hw_bridge);



    d.set_cores_rom_location(cores_rom_base, cores_rom_offset);
    d.set_cores_control_location(cores_control_base, cores_control_offset);
    d.set_cores_inputs_location(cores_inputs_base_address, cores_inputs_offset);
    d.set_dma_location(dma_base, dma_offset);
    d.set_scope_mux_base(scope_mux_base);
    d.set_controller_location(controller_base, controller_tb_offset);
    d.set_hil_control_location(hil_control_base);
    d.deploy(spec);


    return 0;
}
