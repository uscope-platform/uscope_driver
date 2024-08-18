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


#include <gtest/gtest.h>
#include "deployment/hil_deployer.hpp"
#include "emulator/emulator_manager.hpp"

#include "../deployment/fpga_bridge_mock.hpp"


nlohmann::json get_addr_map(){

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
    return addr_map;
}

TEST(deployer, simple_deployment) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "cores": [
        {
            "id": "test",
            "order": 0,
            "input_data": [],
            "inputs": [
                {
                    "name": "input_1",
                    "type": "float",
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2,
                            32.7,
                            62.1,
                            64
                        ]
                    },
                    "reg_n": 3,
                    "channel": [
                        0,
                        1,
                        2,
                        3
                    ]
                },
                {
                    "name": "input_2",
                    "type": "float",
                    "source": {
                        "type": "constant",
                        "value": [
                            4,
                            2,
                            6,
                            12
                        ]
                    },
                    "reg_n": 4,
                    "channel": [
                        0,
                        1,
                        2,
                        3
                    ]
                }
            ],
            "outputs": [
                {
                    "name": "out",
                    "type": "float",
                    "reg_n": [
                        5
                    ]
                }
            ],
            "memory_init": [],
            "channels": 4,
            "options": {
                "comparators": "reducing",
                "efi_implementation": "none"
            },
            "program": {
                "content": "int main(){\n  float input_1;\n  float input_2;\n  float out = input_1 + input_2;\n}",
                "build_settings": {
                    "io": {
                        "inputs": [
                            "input_1",
                            "input_2"
                        ],
                        "memories": [],
                        "outputs": [
                            "out"
                        ]
                    }
                },
                "headers": []
            },
            "sampling_frequency": 1,
            "deployment": {
                "has_reciprocal": false,
                "control_address": 18316525568,
                "rom_address": 17179869184
            }
        }
    ],
    "interconnect": [],
    "emulation_time": 2,
    "deployment_mode": false
})");
    std::string s_f = SCHEMAS_FOLDER;
    auto specs = fcore::emulator::emulator_specs(spec_json, s_f + "/emulator_spec_schema.json" );
    fcore::emulator_manager em(spec_json, false, SCHEMAS_FOLDER);
    auto programs = em.get_programs();


auto mock_hw = std::make_shared<fpga_bridge_mock>();
hil_deployer<fpga_bridge_mock> d(mock_hw);
auto addr_map = get_addr_map();
d.set_layout_map(addr_map);
d.deploy(specs, programs);

ASSERT_TRUE(true);
}


