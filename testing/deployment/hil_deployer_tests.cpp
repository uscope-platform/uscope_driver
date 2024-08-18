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


TEST(deployer, simple_single_core_deployment) {

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
                            31.2
                        ]
                    },
                    "reg_n": 3,
                    "channel": [
                        0
                    ]
                },
                {
                    "name": "input_2",
                    "type": "float",
                    "source": {
                        "type": "constant",
                        "value": [
                            4
                        ]
                    },
                    "reg_n": 4,
                    "channel": [
                        0
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
            "channels": 1,
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

    auto rom_writes = mock_hw->rom_writes;
    auto control_writes = mock_hw->control_writes;

    std::vector<uint32_t> reference_program = {
            0x20004,
            0xc,
            0x20003,
            0x10004,
            0x30005,
            0xc,
            0x60841,
            0xc,
    };

    ASSERT_EQ(rom_writes[0].first, 0x5'0000'0000);
    ASSERT_EQ(rom_writes[0].second, reference_program);

    // DMA
    ASSERT_EQ(control_writes[0].first, 0x4'43c2'1004);
    ASSERT_EQ(control_writes[0].second, 0x50005);

    ASSERT_EQ(control_writes[1].first, 0x4'43c2'1044);
    ASSERT_EQ(control_writes[1].second, 0x38);

    ASSERT_EQ(control_writes[2].first, 0x4'43c2'1000);
    ASSERT_EQ(control_writes[2].second, 1);

    // INPUTS
    ASSERT_EQ(control_writes[3].first, 0x4'43c2'2008);
    ASSERT_EQ(control_writes[3].second, 3);

    ASSERT_EQ(control_writes[4].first, 0x4'43c2'2000);
    ASSERT_EQ(control_writes[4].second, 0x41f9999a);

    ASSERT_EQ(control_writes[5].first, 0x4'43c2'3008);
    ASSERT_EQ(control_writes[5].second, 4);

    ASSERT_EQ(control_writes[6].first, 0x4'43c2'3000);
    ASSERT_EQ(control_writes[6].second, 0x40800000);

    // SEQUENCER

    ASSERT_EQ(control_writes[7].first, 0x4'43c1'1004);
    ASSERT_EQ(control_writes[7].second, 0);

    ASSERT_EQ(control_writes[8].first, 0x4'43c1'0008);
    ASSERT_EQ(control_writes[8].second, 2);

    ASSERT_EQ(control_writes[9].first, 0x4'43c1'0004);
    ASSERT_EQ(control_writes[9].second, 100'000'000);

    ASSERT_EQ(control_writes[10].first, 0x4'43c1'1000);
    ASSERT_EQ(control_writes[10].second, 1);

    // CORES

    ASSERT_EQ(control_writes[11].first, 0x443c20000);
    ASSERT_EQ(control_writes[11].second,11);

}

