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
#include "emulator/emulator_dispatcher.hpp"

#include "../hil_addresses.hpp"


TEST(deployer_v2, simple_single_core_deployment) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "inputs": [
                {
                    "name": "input_1",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2
                        ]
                    }
                },
                {
                    "name": "input_2",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            4
                        ]
                    }
                }
            ],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    }
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



    auto ba = std::make_shared<bus_accessor>();
    hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);

    auto ops = ba->get_operations();

    std::vector<u_int64_t> reference_program = {
            0x80004,
            0xc,
            0x30001,
            0x10002,
            0x20003,
            0xc,
            0xc,
            0x61021,
            0,
            0,
            0,
            0,
            0,
            0,
            0xc,
    };


    std::vector<uint32_t> ref_p;
    for(auto i:reference_program) ref_p.push_back(i);
    fcore::fcore_dis dis(ref_p);
    auto ref_dis = dis.get_disassembled_program_text();
    std::vector<uint32_t> actual_program;
    for(auto i:ops[0].data) actual_program.push_back(i);
    dis = fcore::fcore_dis(actual_program);
    auto actual_dis = dis.get_disassembled_program_text();

    EXPECT_EQ(ops.size(), 15);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    // DMA

    EXPECT_EQ(ops[1].type, control_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x20001);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[3].data[0], 1);

    // INPUTS
    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[4].data[0], 0);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[5].data[0], 2);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[6].data[0], 0x41f9999a);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[7].data[0], 1);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[8].data[0], 3);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[9].data[0], 0x40800000);

    // SEQUENCER

    EXPECT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[10].data[0], 0);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[11].data[0], 2);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[12].data[0], 100'000'000);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[13].data[0], 1);

    // CORES

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[14].data[0],11);
}

TEST(deployer_v2, simple_single_core_integer_input) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "inputs": [
                {
                    "name": "input_1",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2
                        ]
                    }
                },
                {
                    "name": "input_2",
                    "is_vector": false,
                    "metadata":{
                        "type": "integer",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            4
                        ]
                    }
                }
            ],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    }
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


    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);


    auto ops = ba->get_operations();


    std::vector<u_int64_t> reference_program = {
        0x80004,
        0xc,
        0x30001,
        0x10002,
        0x20003,
        0xc,
        0xc,
        0x61021,
        0,
        0,
        0,
        0,
        0,
        0,
        0xc,
    };

    EXPECT_EQ(ops.size(), 15);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    // DMA
    EXPECT_EQ(ops[1].type, control_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x20001);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[3].data[0], 1);

    // INPUTS
    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[4].data[0], 0);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[5].data[0], 2);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[6].data[0], 0x41f9999a);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[7].data[0], 1);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[8].data[0], 3);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[9].data[0], 4);

    // SEQUENCER

    EXPECT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[10].data[0], 0);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[11].data[0], 2);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[12].data[0], 100'000'000);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[13].data[0], 1);

    // CORES

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[14].data[0],11);

}

TEST(deployer_v2, simple_single_core_memory_init) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "inputs": [
            ],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true,
                        "common_io":false
                    }
                }
            ],
            "memory_init": [
                {
                    "name": "mem",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true
                    },
                    "is_output": true,
                    "is_input": false,
                    "value": 14
                },
                {
                    "name": "mem_2",
                    "is_vector": false,
                    "metadata":{
                        "type": "integer",
                        "width":16,
                        "signed":true
                    },
                    "is_output": true,
                    "is_input": false,
                    "value": 12
                }
            ],
            "channels": 1,
            "options": {
                "comparators": "reducing",
                "efi_implementation": "none"
            },
            "program": {
                "content": "int main(){\n  float mem;\n  float mem_2;\n  float out = mem + mem_2;\n}",
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



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x80004,
            0xc,
            0x3f0001,
            0x3e0002,
            0x10003,
            0xc,
            0xc,
            0x3f7e1,
            0,
            0,
            0,
            0,
            0,
            0,
            0xc

    };

    EXPECT_EQ(ops.size(), 15);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    // DMA
    EXPECT_EQ(ops[1].type, control_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x40003);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[3].data[0], 0x50001);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[4].data[0], 0x38);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    EXPECT_EQ(ops[5].data[0], 0x60002);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    EXPECT_EQ(ops[6].data[0], 0x18);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[7].data[0], 3);

    // MEMORIES INITIALIZATION

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 4);
    EXPECT_EQ(ops[8].data[0], 0x41600000);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 8);
    EXPECT_EQ(ops[9].data[0], 12);

    // INPUTS

    // SEQUENCER

    EXPECT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[10].data[0], 0);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[11].data[0], 2);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[12].data[0], 100'000'000);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[13].data[0], 1);

    // CORES

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[14].data[0],11);

}

TEST(deployer_v2, multichannel_single_core_deployment) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
    {
        "id": "test",
        "order": 0,
        "inputs": [
            {
                "name": "input_1",
                "is_vector": false,
                "metadata":{
                    "type": "float",
                    "width":16,
                    "signed":true,
                    "common_io":false
                },
                "source": {
                    "type": "constant",
                    "value": [
                        31.2,
                        32.7,
                        62.1,
                        64
                    ]
                }
            },
            {
                "name": "input_2",
                "is_vector": false,
                "metadata":{
                    "type": "float",
                    "width":16,
                    "signed":true,
                    "common_io":false
                },
                "source": {
                    "type": "constant",
                    "value": [
                        4,
                        2,
                        6,
                        12
                    ]
                }
            }
        ],
        "outputs": [
            {
                "name": "out",
                "is_vector": false,
                "metadata":{
                    "type": "float",
                    "width":16,
                    "signed":true,
                    "common_io":false
                }
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




    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x30004,
            0xc,
            0x30001,
            0x10002,
            0x20003,
            0xc,
            0xc,
            0x61021,
            0,
            0xc,
    };

    EXPECT_EQ(ops.size(), 39);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    // DMA
    EXPECT_EQ(ops[1].type, control_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x20001);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[3].data[0], 0x10031001);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[4].data[0], 0x38);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    EXPECT_EQ(ops[5].data[0], 0x20042001);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    EXPECT_EQ(ops[6].data[0], 0x38);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    EXPECT_EQ(ops[7].data[0], 0x30053001);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    EXPECT_EQ(ops[8].data[0], 0x38);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[9].data[0], 4);

    // INPUTS

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[10].data[0], 0);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[11].data[0], 2);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[12].data[0], 0x41f9999a);


    EXPECT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[13].data[0], 0x10000);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[14].data[0], 0x10002);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[15].data[0], 0x4202CCCD);


    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[16].data[0], 0x20000);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[17].data[0], 0x20002);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[18].data[0], 0x42786666);


    EXPECT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[19].data[0], 0x30000);

    EXPECT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[20].data[0], 0x30002);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[21].data[0], 0x42800000);


    EXPECT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[22].data[0], 1);

    EXPECT_EQ(ops[23].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[23].data[0], 3);

    EXPECT_EQ(ops[24].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[24].data[0], 0x40800000);


    EXPECT_EQ(ops[25].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[25].data[0], 0x10001);

    EXPECT_EQ(ops[26].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[26].data[0], 0x10003);

    EXPECT_EQ(ops[27].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[27].data[0], 0x40000000);


    EXPECT_EQ(ops[28].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[28].data[0], 0x20001);

    EXPECT_EQ(ops[29].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[29].data[0], 0x20003);

    EXPECT_EQ(ops[30].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[30].data[0], 0x40C00000);


    EXPECT_EQ(ops[31].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[31].data[0], 0x30001);

    EXPECT_EQ(ops[32].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[32].data[0], 0x30003);

    EXPECT_EQ(ops[33].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[33].data[0], 0x41400000);

    // SEQUENCER

    EXPECT_EQ(ops[34].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[34].data[0], 0);

    EXPECT_EQ(ops[35].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[35].data[0], 2);

    EXPECT_EQ(ops[36].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[36].data[0], 100'000'000);

    EXPECT_EQ(ops[37].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[37].data[0], 1);

    // CORES

    EXPECT_EQ(ops[38].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[38].data[0],11);

}

TEST(deployer_v2, simple_multi_core_deployment) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "inputs": [
                {
                    "name": "input_1",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2
                        ]
                    }
                },
                {
                    "name": "input_2",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            4
                        ]
                    }
                }
            ],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true,
                        "common_io":false
                    }
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
                "headers": []
            },
            "sampling_frequency": 1,
            "deployment": {
                "has_reciprocal": false,
                "control_address": 18316525568,
                "rom_address": 17179869184
            }
        },
        {
            "id": "test_2",
            "order": 1,
            "inputs": [
                {
                    "name": "input_1",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2
                        ]
                    }
                },
                {
                    "name": "input_2",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            4
                        ]
                    }
                }
            ],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true,
                        "common_io":false
                    }
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


    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);

    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x80004,
            0xc,
            0x30001,
            0x10003,
            0x20004,
            0xc,
            0xc,
            0x61021,
            0,
            0,
            0,
            0,
            0,
            0,
            0xc,
    };
    EXPECT_EQ(ops.size(), 28);

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    reference_program = {
        0x80004,
        0xc,
        0x30002,
        0x10003,
        0x20004,
        0xc,
        0xc,
        0x61021,
        0,
        0,
        0,
        0,
        0,
        0,
        0xc,
};

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    EXPECT_EQ(ops[1].data, reference_program);

    // DMA 1
    EXPECT_EQ(ops[2].type, control_plane_write);
    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[2].data[0], 0x30001);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[3].data[0], 0x38);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[4].data[0], 1);

    // DMA 2
    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[5].data[0], 0x40002);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1+ addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[6].data[0], 0x38);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[7].data[0], 1);

    // INPUTS 1
    EXPECT_EQ(ops[8].address[0],  addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[8].data[0], 0);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[9].data[0], 3);

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[10].data[0], 0x41f9999a);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[11].data[0], 1);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[12].data[0], 4);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[13].data[0], 0x40800000);


    // INPUTS 2

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[14].data[0], 0);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[15].data[0], 3);

    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[16].data[0], 0x41f9999a);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[17].data[0], 1);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[18].data[0], 4);

    EXPECT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[19].data[0], 0x40800000);


    // SEQUENCER

    EXPECT_EQ(ops[20].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[20].data[0], 0);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[21].data[0], 2);

    EXPECT_EQ(ops[22].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    EXPECT_EQ(ops[22].data[0], 0);

    EXPECT_EQ(ops[23].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    EXPECT_EQ(ops[23].data[0], 108);

    EXPECT_EQ(ops[24].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[24].data[0], 100'000'000);

    EXPECT_EQ(ops[25].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[25].data[0], 3);

    // CORES

    EXPECT_EQ(ops[26].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[26].data[0],11);

    EXPECT_EQ(ops[27].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    EXPECT_EQ(ops[27].data[0],11);

}

TEST(deployer_v2, scalar_interconnect_test) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "id": "test",
            "inputs": [
                {
                    "name": "input_1",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2
                        ]
                    }
                },
                {
                    "name": "input_2",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            32.7
                        ]
                    }
                }
            ],
            "outputs": [
                {
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "is_vector": false,
                    "name": "out"
                },
                {
                    "metadata": {
                        "type": "integer",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "is_vector": false,
                    "name": "out2"
                }
            ],
            "memory_init": [],
            "program": {
                "content": "int main(){float input_1; float input_2; float out; out = input_1 + input_2 ; out2=fti(out);}",
                "headers": []
            },
            "order": 0,
            "options": {
                "comparators": "reducing",
                "efi_implementation": "efi_sort"
            },
            "channels": 1,
            "sampling_frequency": 1,
            "deployment": {
                "has_reciprocal": false,
                "control_address": 18316525568,
                "rom_address": 17179869184
            }
        },
        {
            "id": "test_move",
            "inputs": [
                {
                    "name": "input",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "source":{
                            "type": "external",
                            "value": [0]
                        }
                }
            ],
            "outputs": [
                {
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "name": "out"
                }
            ],
            "memory_init": [],
            "program": {
                "content": "int main(){float input; float out; float val = itf(input); out = fti(val+1.0);}",
                "headers": []
            },
            "order": 1,
            "options": {
                "comparators": "reducing",
                "efi_implementation": "efi_sort"
            },
            "channels": 1,
            "sampling_frequency": 1,
            "deployment": {
                "has_reciprocal": false,
                "control_address": 18316525568,
                "rom_address": 17179869184
            }
        }
    ],
    "interconnect": [
        {
            "source": "test.out2",
            "destination": "test_move.input"
        }
    ],
    "emulation_time": 2,
    "deployment_mode": false
}
)");


    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0xD0005,
            0xc,
            0x10001,
            0x30002,
            0x10004,
            0x20005,
            0xc,
            0xc,
            0x61021,
            0,
            0,
            0,
            0,
            0,
            0,
            0x865,
            0,
            0,
            0,
            0,
            0xc

    };


    EXPECT_EQ(ops.size(), 26);

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x130003,
            0xc,
            0x10001,
            0x10003,
            0xc,
            0xc,
            0x1024,
            0x26,
            0x3f800000,
            0, 0, 0,
            0x60841,
            0, 0, 0, 0, 0, 0,
            0x865,
            0, 0, 0, 0,
            0xc
    };

    EXPECT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    EXPECT_EQ(ops[1].data, reference_program);

    // DMA 1
    EXPECT_EQ(ops[2].type, control_plane_write);
    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[2].data[0], 0x10001);

    EXPECT_EQ(ops[3].address[0],  addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[3].data[0], 0x0);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[4].data[0], 0x0);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[5].data[0], 0x18);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
     EXPECT_EQ(ops[6].data[0], 0x40002);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[7].data[0], 0x38);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[8].data[0], 2);

    // DMA 2
    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[9].data[0], 0x50003);

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[10].data[0], 0x38);



    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[11].data[0], 1);

    // INPUTS

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[12].data[0], 0);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[13].data[0], 4);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[14].data[0], 0x41f9999a);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[15].data[0], 1);

    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[16].data[0], 5);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[17].data[0], 0x4202cccd);
    // SEQUENCER

    EXPECT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[18].data[0], 0);

    EXPECT_EQ(ops[19].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[19].data[0], 2);

    EXPECT_EQ(ops[20].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    EXPECT_EQ(ops[20].data[0], 0);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    EXPECT_EQ(ops[21].data[0], 185);

    EXPECT_EQ(ops[22].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[22].data[0], 100'000'000);

    EXPECT_EQ(ops[23].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[23].data[0], 3);

    // CORES

    EXPECT_EQ(ops[24].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[24].data[0],11);

    EXPECT_EQ(ops[25].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    EXPECT_EQ(ops[25].data[0],11);

}

TEST(deployer_v2, scatter_interconnect_test) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
        "version":2,
        "cores": [
            {
                "order": 0,
                "id": "test_producer",
                "channels":1,
                "options":{
                    "comparators": "full",
                    "efi_implementation":"none"
                },
                "sampling_frequency":1,
                "inputs":[],
                "outputs":[
                    {
                        "name":"out",
                        "is_vector":true,
                        "vector_size": 2,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io":false
                        }
                    }
                    ],
                "memory_init":[],
                "program": {
                    "content": "int main(){\n  float out[2] = {15.6, 17.2};\n}",
                    "headers": []
                },
                "deployment": {
                    "has_reciprocal": false,
                    "control_address": 18316525568,
                    "rom_address": 17179869184
                }
            },
            {
                "order": 1,
                "id": "test_consumer",
                "channels":2,
                "options":{
                    "comparators": "full",
                    "efi_implementation":"none"
                },
                "sampling_frequency":1,
                "inputs":[
                    {
                        "name": "input",
                        "is_vector": false,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io":false
                        },
                        "source":{
                            "type": "external",
                            "value": [0]
                        }
                    }
                ],
                "outputs":[
                    {
                        "name":"out",
                        "is_vector": false,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io":false
                        }
                    }
                ],
                "memory_init":[],
                "program": {
                    "content": "int main(){\n  float input;float out = input*3.5;\n}",
                    "headers": []
                },
                "deployment": {
                    "has_reciprocal": false,
                    "control_address": 18316525568,
                    "rom_address": 17179869184
                }
            }
        ],
        "interconnect": [
            {
                "source": "test_producer.out",
                "destination": "test_consumer.input"
            }
    ],
        "emulation_time": 1,
    "deployment_mode": false
    })");


    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x60003,
            0xc,
            0x10001,
            0x20002,
            0xc,
            0xc,
            0x26,
            0x4179999a,
            0x46,
            0x4189999a,
            0,
            0xc
    };

    EXPECT_EQ(ops.size(), 24);

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x70003,
            0xc,
            0x10001,
            0x30003,
            0xc,
            0xc,
            0x46,
            0x40600000,
            0x61023,
            0,0,0,
            0xc
    };

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    EXPECT_EQ(ops[1].data, reference_program);


    // DMA 1
    EXPECT_EQ(ops[2].type, control_plane_write);
    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[2].data[0], 0x10001);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[3].data[0], 0x0);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[4].data[0], 0x0);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[5].data[0], 0x38);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[6].data[0], 0x10010002);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[7].data[0], 0x1);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[8].data[0], 0x0);


    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[9].data[0], 0x38);


    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[10].data[0], 2);

    // DMA 2
    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[11].data[0], 0x40003);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[12].data[0], 0x38);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[13].data[0], 0x10051003);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[14].data[0], 0x38);


    EXPECT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[15].data[0], 2);


    // SEQUENCER

    EXPECT_EQ(ops[16].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[16].data[0], 0);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[17].data[0], 2);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    EXPECT_EQ(ops[18].data[0], 0);

    EXPECT_EQ(ops[19].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    EXPECT_EQ(ops[19].data[0], 123);

    EXPECT_EQ(ops[20].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[20].data[0], 100'000'000);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[21].data[0], 3);

    // CORES

    EXPECT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[22].data[0],11);

    EXPECT_EQ(ops[23].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    EXPECT_EQ(ops[23].data[0],11);

}

TEST(deployer_v2, gather_interconnect_test) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "order": 0,
            "id": "test_producer",
            "channels":2,
            "options":{
                "comparators": "full",
                "efi_implementation":"none"
            },
            "sampling_frequency":1,
            "inputs":[
                {
                    "name": "input_1",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "source":{"type": "constant","value": [31.2, 32.7]}
                },
                {
                    "name": "input_2",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "source":{"type": "constant","value": [31.2, 32.7]}
                }
            ],
            "outputs":[
                {
                    "name":"out",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    }
                }
            ],
            "memory_init":[],
            "program": {
                "content": "int main(){\n  float input_1;\n  float input_2;\n  float out = input_1 + input_2;\n}",
                "headers": []
            },
            "deployment": {
                "has_reciprocal": false,
                "control_address": 18316525568,
                "rom_address": 17179869184
            }
        },
        {
            "order": 1,
            "id": "test_reducer",
            "channels":1,
            "options":{
                "comparators": "full",
                "efi_implementation":"none"
            },
            "sampling_frequency":1,
            "inputs":[
                {
                        "name": "input_data",
                        "is_vector": true,
                        "vector_size": 2,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io":false
                        },
                        "source":{
                            "type": "external",
                            "value": [0, 0]
                        }
                }
            ],
            "outputs":[
                {
                    "name":"out",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    }
                }
            ],
            "memory_init":[],
            "program": {
                "content": "int main(){\n    float input_data[2];\n    float out = input_data[0] + input_data[1];\n}\n",
                "headers": []
            },
            "deployment": {
                "has_reciprocal": false,
                "control_address": 18316525568,
                "rom_address": 17179869184
            }
        }
    ],
    "interconnect": [
        {
            "source": "test_producer.out",
            "destination": "test_reducer.input_data"
        }
],
    "emulation_time": 1,
    "deployment_mode": false
})");


    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x50004,
            0xc,
            0x30001,
            0x10004,
            0x20005,
            0xc,
            0xc,
            0x61021,
            0,
            0,
            0,
            0xc
    };

    EXPECT_EQ(ops.size(), 34);

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x80004,
            0xc,
            0x10001,
            0x20002,
            0x30003,
            0xc,
            0xc,
            0x61021,
            0, 0, 0, 0, 0, 0,
            0xc

    };

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    EXPECT_EQ(ops[1].data, reference_program);

    // DMA 1
    EXPECT_EQ(ops[2].type, control_plane_write);
    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[2].data[0], 0x10001);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[3].data[0], 0x0);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[4].data[0], 0x0);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[5].data[0], 0x38);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[6].data[0], 0x21001);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[7].data[0], 0x1);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[8].data[0], 0x0);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[9].data[0], 0x38);

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[10].data[0], 2);

    // DMA 2
    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[11].data[0], 0x40003);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[12].data[0], 0x38);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[13].data[0], 1);

    // INPUTS

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[14].data[0], 0);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[15].data[0], 0x4);

    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[16].data[0], 0x41f9999a);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[17].data[0], 0x10000);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[18].data[0], 0x10004);

    EXPECT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[19].data[0], 0x4202CCCD);


    EXPECT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[20].data[0], 1);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[21].data[0], 0x5);

    EXPECT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[22].data[0], 0x41f9999a);

    EXPECT_EQ(ops[23].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[23].data[0], 0x10001);

    EXPECT_EQ(ops[24].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[24].data[0], 0x10005);

    EXPECT_EQ(ops[25].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[25].data[0], 0x4202CCCD);

    // SEQUENCER

    EXPECT_EQ(ops[26].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[26].data[0], 0);

    EXPECT_EQ(ops[27].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[27].data[0], 2);

    EXPECT_EQ(ops[28].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    EXPECT_EQ(ops[28].data[0], 0);

    EXPECT_EQ(ops[29].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    EXPECT_EQ(ops[29].data[0], 108);

    EXPECT_EQ(ops[30].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[30].data[0], 100'000'000);

    EXPECT_EQ(ops[31].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[31].data[0], 3);

    // CORES

    EXPECT_EQ(ops[32].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[32].data[0],11);

    EXPECT_EQ(ops[33].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    EXPECT_EQ(ops[33].data[0],11);

}

TEST(deployer_v2, vector_interconnect_test) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
        "version":2,
        "cores": [
            {
                "order": 0,
                "id": "test_producer",
                "channels":2,
                "options":{
                    "comparators": "full",
                    "efi_implementation":"none"
                },
                "sampling_frequency":1,
                "inputs":[
                    {
                        "name": "input_1",
                        "is_vector": false,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io":false
                        },
                        "source":{"type": "constant","value": [31.2, 32.7]}
                    },
                    {
                        "name": "input_2",
                        "is_vector": false,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io":false
                        },
                        "source":{"type": "constant","value": [31.2, 32.7]}
                    }
                ],
                "outputs":[
                    {
                        "name":"out",
                        "is_vector": false,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io": false
                        }
                    }
                ],
                "memory_init":[],
                "program": {
                    "content": "int main(){\n  float input_1;\n  float input_2;\n  float out = input_1 + input_2;\n}",
                    "headers": []
                },
                "deployment": {
                    "has_reciprocal": false,
                    "control_address": 18316525568,
                    "rom_address": 17179869184
                }
            },
            {
                "order": 1,
                "id": "test_consumer",
                "channels":2,
                "options":{
                    "comparators": "full",
                    "efi_implementation":"none"
                },
                "sampling_frequency":1,
                "inputs":[
                    {
                        "name": "input",
                        "is_vector": false,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io":false
                        },
                        "source":{
                            "type": "external",
                            "value": [0]
                        }
                    }
                ],
                "outputs":[
                    {
                        "name":"out",
                        "is_vector": false,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io": false
                        }
                    }
                ],
                "memory_init":[],
                "program": {
                    "content": "int main(){\n  float input;float out = input*3.5;\n}",
                    "headers": []
                },
                "deployment": {
                    "has_reciprocal": false,
                    "control_address": 18316525568,
                    "rom_address": 17179869184
                }
            }
        ],
        "interconnect": [
            {
                "source": "test_producer.out",
                "destination": "test_consumer.input"
            }
        ],
        "emulation_time": 1,
    "deployment_mode": false
    })");



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x50004,
            0xc,
            0x30001,
            0x10003,
            0x20004,
            0xc,
            0xc,
            0x61021,
            0,0,0,
            0xc

    };
    EXPECT_EQ(ops.size(), 36);

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x70003,
            0xc,
            0x10001,
            0x30002,
            0xc,
            0xc,
            0x46,
            0x40600000,
            0x61023,
            0,0,0,
            0xc
    };

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    EXPECT_EQ(ops[1].data, reference_program);

    // DMA 1
    EXPECT_EQ(ops[2].type, control_plane_write);
    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[2].data[0], 0x10001);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[3].data[0], 0x0);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[4].data[0], 0x0);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[5].data[0], 0x38);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[6].data[0], 0x10011001);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[7].data[0], 0x1);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[8].data[0], 0x0);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[9].data[0], 0x38);

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[10].data[0], 2);

    // DMA 2
    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[11].data[0], 0x30002);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[12].data[0], 0x38);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[13].data[0], 0x10041002);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[14].data[0], 0x38);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[15].data[0], 2);

    // INPUTS

    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[16].data[0], 0);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[17].data[0], 0x3);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[18].data[0], 0x41f9999a);


    EXPECT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[19].data[0], 0x10000);

    EXPECT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[20].data[0], 0x10003);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[21].data[0], 0x4202CCCD);


    EXPECT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[22].data[0], 1);

    EXPECT_EQ(ops[23].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[23].data[0], 0x4);

    EXPECT_EQ(ops[24].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[24].data[0], 0x41f9999a);


    EXPECT_EQ(ops[25].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[25].data[0], 0x10001);

    EXPECT_EQ(ops[26].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[26].data[0], 0x10004);

    EXPECT_EQ(ops[27].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[27].data[0], 0x4202CCCD);

    // SEQUENCER

    EXPECT_EQ(ops[28].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[28].data[0], 0);

    EXPECT_EQ(ops[29].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[29].data[0], 2);

    EXPECT_EQ(ops[30].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    EXPECT_EQ(ops[30].data[0], 0);

    EXPECT_EQ(ops[31].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    EXPECT_EQ(ops[31].data[0], 108);

    EXPECT_EQ(ops[32].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[32].data[0], 100'000'000);

    EXPECT_EQ(ops[33].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[33].data[0], 3);

    // CORES

    EXPECT_EQ(ops[34].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[34].data[0],11);

    EXPECT_EQ(ops[35].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    EXPECT_EQ(ops[35].data[0],11);

}

TEST(deployer_v2, 2d_vector_interconnect_test) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
        "version":2,
        "cores": [
            {
                "order": 0,
                "id": "test_producer",
                "channels":2,
                "options":{
                    "comparators": "full",
                    "efi_implementation":"none"
                },
                "sampling_frequency":1,
                "inputs":[],
                "outputs":[
                    {
                        "name":"out",
                        "is_vector": true,
                        "vector_size":2,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": false,
                            "common_io": false
                        }
                    }
                ],
                "memory_init":[],
                "program": {
                    "content": "int main(){\n  float out[2] = {15.6, 17.2};\n}",
                    "headers": []
                },
                "deployment": {
                    "has_reciprocal": false,
                    "control_address": 18316525568,
                    "rom_address": 17179869184
                }
            },
            {
                "order": 1,
                "id": "test_consumer",
                "channels":2,
                "options":{
                    "comparators": "full",
                    "efi_implementation":"none"
                },
                "sampling_frequency":1,
                "inputs":[
                    {
                            "name": "input",
                            "is_vector": true,
                            "vector_size":2,
                            "metadata": {
                                "type": "float",
                                "width": 32,
                                "signed": true,
                                "common_io":false
                            },
                            "source":{
                            "type": "external",
                            "value": [0, 0]
                        }
                    }
                ],
                "outputs":[
                    {
                        "name":"consumer_out",
                        "is_vector": true,
                        "vector_size":2,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": false,
                            "common_io": false
                        }
                    }
                ],
                "memory_init":[],
                "program": {
                    "content": "int main(){\n  float input[2]; \n  float consumer_out[2]; \n  consumer_out[0] = input[0]*3.5; \n  consumer_out[1] = input[1]*3.5;\n}",

                    "headers": []
                },
                "deployment": {
                    "has_reciprocal": false,
                    "control_address": 18316525568,
                    "rom_address": 17179869184
                }
            }
        ],
        "interconnect": [
            {
                "source": "test_producer.out",
                "destination": "test_consumer.input"
            }
        ],
        "emulation_time": 2,
    "deployment_mode": false
    })");


    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x50003,
            0xc,
            0x10001,
            0x20002,
            0xc,
            0xc,
            0x26,
            0x4179999a,
            0x46,
            0x4189999a,
            0xc
    };
    EXPECT_EQ(ops.size(), 36);

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x80005,
            0xc,
            0x10001,
            0x20002,
            0x40003,
            0x10004,
            0xc,
            0xc,
            0x66,
            0x40600000,
            0x81823,
            0x21843,
            0,0,0,
            0xc

    };

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    EXPECT_EQ(ops[1].data, reference_program);

    // DMA 1
    EXPECT_EQ(ops[2].type, control_plane_write);
    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[2].data[0], 0x10001);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[3].data[0], 0x0);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[4].data[0], 0x0);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[5].data[0], 0x38);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[6].data[0], 0x10011001);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[7].data[0], 0x1);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[8].data[0], 0x0);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[9].data[0], 0x38);

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    EXPECT_EQ(ops[10].data[0], 0x20002);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[11].data[0], 0x2);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[12].data[0], 0x0);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    EXPECT_EQ(ops[13].data[0], 0x38);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    EXPECT_EQ(ops[14].data[0], 0x10021002);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[15].data[0], 0x3);

    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[16].data[0], 0x0);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    EXPECT_EQ(ops[17].data[0], 0x38);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[18].data[0], 4);

    // DMA 2
    EXPECT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[19].data[0], 0x50003);

    EXPECT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[20].data[0], 0x38);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[21].data[0], 0x10061003);

    EXPECT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[22].data[0], 0x38);

    EXPECT_EQ(ops[23].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    EXPECT_EQ(ops[23].data[0], 0x70004);

    EXPECT_EQ(ops[24].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    EXPECT_EQ(ops[24].data[0], 0x38);

    EXPECT_EQ(ops[25].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    EXPECT_EQ(ops[25].data[0], 0x10081004);

    EXPECT_EQ(ops[26].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    EXPECT_EQ(ops[26].data[0], 0x38);

    EXPECT_EQ(ops[27].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[27].data[0], 4);

    // MEMORY INITIALIZATIONS

    // SEQUENCER

    EXPECT_EQ(ops[28].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[28].data[0], 0);

    EXPECT_EQ(ops[29].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[29].data[0], 2);

    EXPECT_EQ(ops[30].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    EXPECT_EQ(ops[30].data[0], 0);

    EXPECT_EQ(ops[31].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    EXPECT_EQ(ops[31].data[0], 123);

    EXPECT_EQ(ops[32].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[32].data[0], 100'000'000);

    EXPECT_EQ(ops[33].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[33].data[0], 3);

    // CORES

    EXPECT_EQ(ops[34].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[34].data[0],11);

    EXPECT_EQ(ops[35].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    EXPECT_EQ(ops[35].data[0],11);

}

TEST(deployer_v2, simple_single_core_output_select) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "inputs": [
                {
                    "name": "input_1",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":true
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2
                        ]
                    }
                },
                {
                    "name": "input_2",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":true
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            4
                        ]
                    }
                }
            ],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    }
                }
            ],
            "memory_init": [],
            "channels": 2,
            "options": {
                "comparators": "reducing",
                "efi_implementation": "none"
            },
            "program": {
                "content": "int main(){\n  float input_1;\n  float input_2;\n  float out = input_1 + input_2;\n}",
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



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);
    output_specs_t out;
    out.channel = 1,
    out.core_name = "test";
    out.source_output = "out";
    d.select_output(0, out);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x50002,
            0xc,
            0x10001,
            0xc,
            0x10002,
            0x20003,
            0xc,
            0x1821021,
            0,0,0,
            0xc,
    };

    EXPECT_EQ(ops.size(), 24);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    // DMA
    EXPECT_EQ(ops[1].type, control_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x20001);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[3].data[0], 0x10031001);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[4].data[0], 0x38);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[5].data[0], 2);

    // INPUTS

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[6].data[0], 0);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[7].data[0], 2);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[8].data[0], 0x41f9999a);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[9].data[0], 0x10000);

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[10].data[0], 0x10002);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[11].data[0], 0x41F9999A);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[12].data[0], 1);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[13].data[0], 3);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[14].data[0], 0x40800000);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[15].data[0], 65537);

    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[16].data[0], 65539);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[17].data[0], 0x40800000);


    // SEQUENCER

    EXPECT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[18].data[0], 0);

    EXPECT_EQ(ops[19].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[19].data[0], 2);

    EXPECT_EQ(ops[20].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[20].data[0], 100'000'000);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[21].data[0], 1);

    // CORES

    EXPECT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[22].data[0],11);

    // select_output
    EXPECT_EQ(ops[23].address[0], addr_map.bases.scope +  scope_mux_regs.ch_1);
    EXPECT_EQ(ops[23].data[0],0x10003);

}

TEST(deployer_v2, simple_single_core_input_set) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "inputs": [
                {
                    "name": "input_1",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2
                        ]
                    }
                },
                {
                    "name": "input_2",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            4
                        ]
                    }
                }
            ],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    }
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


    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);
    d.set_input("test","input_2",0,90);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x80004,
            0xc,
            0x30001,
            0x10002,
            0x20003,
            0xc,
            0xc,
            0x61021,
            0,0,0,0,0,0,
            0xc,
    };

    EXPECT_EQ(ops.size(), 18);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    // DMA
    EXPECT_EQ(ops[1].type, control_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x20001);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[3].data[0], 1);

    // INPUTS
    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[4].data[0], 0);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[5].data[0], 2);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[6].data[0], 0x41f9999a);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[7].data[0], 1);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[8].data[0], 3);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[9].data[0], 0x40800000);

    // SEQUENCER

    EXPECT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[10].data[0], 0);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[11].data[0], 2);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[12].data[0], 100'000'000);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[13].data[0], 1);

    // CORES

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[14].data[0],11);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[15].data[0],1);

    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[16].data[0],3);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[17].data[0],0x42B40000);

}

TEST(deployer_v2, simple_single_core_start_stop) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "inputs": [
                {
                    "name": "input_1",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2
                        ]
                    }
                },
                {
                    "name": "input_2",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            4
                        ]
                    }
                }
            ],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    }
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



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);
    d.start();
    d.stop();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x80004,
            0xc,
            0x30001,
            0x10002,
            0x20003,
            0xc,
            0xc,
            0x61021,
            0,0,0,0,0,0,
            0xc,
    };
    EXPECT_EQ(ops.size(), 17);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    // DMA
    EXPECT_EQ(ops[1].type, control_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x20001);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[3].data[0], 1);

    // INPUTS
    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[4].data[0], 0);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[5].data[0], 2);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[6].data[0], 0x41f9999a);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[7].data[0], 1);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[8].data[0], 3);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[9].data[0], 0x40800000);

    // SEQUENCER

    EXPECT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[10].data[0], 0);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[11].data[0], 2);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[12].data[0], 100'000'000);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[13].data[0], 1);

    // CORES

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[14].data[0],11);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[15].data[0],1);

    EXPECT_EQ(ops[16].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[16].data[0],0);

}



TEST(deployer_v2, hardware_sim_file_production) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "inputs": [
                {
                    "name": "input_1",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2
                        ]
                    }
                },
                {
                    "name": "input_2",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            4
                        ]
                    }
                }
            ],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    }
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



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);

    auto files = d.get_hardware_sim_data(spec_json);

    auto control_ref = "18316857356:131073\n18316857420:56\n18316857344:1\n18316861452:0\n18316861448:2\n18316861440:1106876826\n18316861452:1\n18316861448:3\n18316861440:1082130432\n18316595204:0\n18316591112:2\n18316591108:100000000\n18316595200:1\n18316853248:11\n18316656640:1\n";
    auto outputs_ref = "2:test.out\n";
    auto rom_ref = "21474836480:524292\n21474836484:12\n21474836488:196609\n21474836492:65538\n21474836496:131075\n21474836500:12\n21474836504:12\n21474836508:397345\n21474836512:0\n21474836516:0\n21474836520:0\n21474836524:0\n21474836528:0\n21474836532:0\n21474836536:12\n";
    auto inputs_ref = "test.input_1,18316861440,2,0,0\ntest.input_2,18316861440,3,1,0\n";
    EXPECT_EQ(files.control, control_ref);
    EXPECT_EQ(files.outputs, outputs_ref);
    EXPECT_EQ(files.code, rom_ref);
    EXPECT_EQ(files.inputs, inputs_ref);
}



TEST(deployer_v2, hardware_sim_file_production_multichannel) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "inputs": [
                {
                    "name": "input_1",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2
                        ]
                    }
                },
                {
                    "name": "input_2",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            4
                        ]
                    }
                }
            ],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    }
                }
            ],
            "memory_init": [],
            "channels": 2,
            "options": {
                "comparators": "reducing",
                "efi_implementation": "none"
            },
            "program": {
                "content": "int main(){\n  float input_1;\n  float input_2;\n  float out = input_1 + input_2;\n}",
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



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);

    auto files = d.get_hardware_sim_data(spec_json);

    auto control_ref = "18316857356:131073\n18316857420:56\n18316857360:268636161\n18316857424:56\n18316857344:2\n18316861452:0\n18316861448:2\n18316861440:1106876826\n18316861452:65536\n18316861448:65538\n18316861440:1106876826\n18316861452:1\n18316861448:3\n18316861440:1082130432\n18316861452:65537\n18316861448:65539\n18316861440:1082130432\n18316595204:0\n18316591112:2\n18316591108:100000000\n18316595200:1\n18316853248:11\n18316656640:1\n";
    auto outputs_ref = "2:test.out[0]\n65539:test.out[1]\n";
    auto rom_ref = "21474836480:327684\n21474836484:12\n21474836488:196609\n21474836492:65538\n21474836496:131075\n21474836500:12\n21474836504:12\n21474836508:397345\n21474836512:0\n21474836516:0\n21474836520:0\n21474836524:12\n";
    auto inputs_ref = "test[0].input_1,18316861440,2,0,0\ntest[0].input_2,18316861440,3,1,0\ntest[1].input_1,18316861440,65538,65536,0\ntest[1].input_2,18316861440,65539,65537,0\n";

    EXPECT_EQ(files.control, control_ref);
    EXPECT_EQ(files.outputs, outputs_ref);
    EXPECT_EQ(files.code, rom_ref);
    EXPECT_EQ(files.inputs, inputs_ref);
}


TEST(deployer_v2, hardware_sim_file_production_mem_out) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "inputs": [],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true,
                        "common_io":false
                    }
                }
            ],
            "memory_init": [
                {
                    "name": "mem",
                    "vector_size": 1,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true
                    },
                    "is_output": true,
                    "is_input": false,
                    "is_vector": false,
                    "value": [
                        0
                    ]
                }
            ],
            "channels": 1,
            "options": {
                "comparators": "reducing",
                "efi_implementation": "none"
            },
            "program": {
                "content": "int main(){\n  float mem = mem + 5.3; \n float out = mem*2.0;\n}",
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



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);

    auto files = d.get_hardware_sim_data(spec_json);

    auto control_ref = "18316857356:196610\n18316857420:56\n18316857360:262145\n18316857424:56\n18316857344:2\n18316853252:0\n18316595204:0\n18316591112:2\n18316591108:100000000\n18316595200:1\n18316853248:11\n18316656640:1\n";
    auto rom_ref = "21474836480:1245187\n21474836484:12\n21474836488:4128769\n21474836492:131074\n21474836496:12\n21474836500:12\n21474836504:38\n21474836508:1084856730\n21474836512:0\n21474836516:8261601\n21474836520:38\n21474836524:1073741824\n21474836528:0\n21474836532:0\n21474836536:0\n21474836540:0\n21474836544:0\n21474836548:266211\n21474836552:0\n21474836556:0\n21474836560:0\n21474836564:0\n21474836568:0\n21474836572:0\n21474836576:12\n";
    auto outputs_ref = "3:test.out\n4:test.mem\n";
    auto inputs_ref = "";
    EXPECT_EQ(files.code, rom_ref);
    EXPECT_EQ(files.outputs, outputs_ref);
    EXPECT_EQ(files.control, control_ref);
    EXPECT_EQ(files.inputs, inputs_ref);
}


TEST(deployer_v2, multichannel_diversified_input_constants) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version": 2,
    "cores": [
        {
            "id": "gen",
            "order": 1,
            "inputs": [
                {
                    "name": "a",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io": false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            1,
                            2
                        ]
                    },
                    "vector_size": 1,
                    "is_vector": false
                },
                {
                    "name": "b",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io": false
                    },
                    "source": {
                        "type": "constant",
                        "value": [
                            1,
                            2
                        ]
                    },
                    "vector_size": 1,
                    "is_vector": false
                }
            ],
            "outputs": [
                {
                    "name": "c",
                    "is_vector": false,
                    "vector_size": 1,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io": false
                    }
                }
            ],
            "memory_init": [
                {
                    "name": "mem",
                    "vector_size": 1,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true
                    },
                    "is_output": true,
                    "is_input": false,
                    "is_vector": false,
                    "value": [
                        0
                    ]
                }
            ],
            "deployment": {
                "rom_address": 0,
                "has_reciprocal": false,
                "control_address": 0
            },
            "channels": 2,
            "program": {
                "content": "int main() {\n  float a, b, mem;\n  float input_data = a + b;\n  mem += input_data;\n  c = mem*2.0;\n}",
                "headers": []
            },
            "options": {
                "comparators": "reducing",
                "efi_implementation": "efi_trig"
            },
            "sampling_frequency": 20000
        }
    ],
    "interconnect": [],
    "emulation_time": 0.001,
    "deployment_mode": false
})");



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);
    d.start();
    d.stop();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0xF0005,
            0xc,
            0x20001,
            0x3F0002,
            0x10003,
            0x20004,
            0xc,
            0xc,
            0x61021,
            0, 0, 0,
            0x7E1FE1,
            0,
            0x26,
            0x40000000,
            0, 0,
            0x40FE3,
            0, 0, 0,
            0xc,
    };
    EXPECT_EQ(ops.size(), 30);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    EXPECT_EQ(ops[1].type, control_plane_write);
    // DMA

    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x30001);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[3].data[0], 0x10041001);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[4].data[0], 0x38);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    EXPECT_EQ(ops[5].data[0], 0x50002);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    EXPECT_EQ(ops[6].data[0], 0x38);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    EXPECT_EQ(ops[7].data[0], 0x10061002);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    EXPECT_EQ(ops[8].data[0], 0x38);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[9].data[0], 4);

    // MEMORY INITIALIZATION

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 8);
    EXPECT_EQ(ops[10].data[0], 0);

    // INPUTS

    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[11].data[0], 0);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[12].data[0], 3);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[13].data[0], 0x3f800000);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[14].data[0], 0x10000);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[15].data[0], 0x10003);

    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[16].data[0], 0x40000000);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[17].data[0], 1);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[18].data[0], 0x4);

    EXPECT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[19].data[0], 0x3f800000);


    EXPECT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[20].data[0], 0x10001);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[21].data[0], 0x10004);

    EXPECT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[22].data[0], 0x40000000);

    // SEQUENCER

    EXPECT_EQ(ops[23].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[23].data[0], 0);

    EXPECT_EQ(ops[24].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[24].data[0], 2);

    EXPECT_EQ(ops[25].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[25].data[0], 5000);

    EXPECT_EQ(ops[26].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[26].data[0], 1);

    // CORES

    EXPECT_EQ(ops[27].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[27].data[0],11);

    EXPECT_EQ(ops[28].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[28].data[0],1);

    EXPECT_EQ(ops[29].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[29].data[0],0);

}


TEST(deployer_v2, multichannel_memory_init) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version": 2,
    "cores": [
        {
            "id": "test",
            "order": 1,
            "inputs": [],
            "outputs": [
                {
                    "name": "out",
                    "is_vector": false,
                    "metadata":{
                        "type": "float",
                        "width": 24,
                        "signed":false,
                        "common_io": false
                    }
                }
            ],
            "memory_init": [
                {
                    "name": "mem",
                    "vector_size": 1,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true
                    },
                    "is_output": true,
                    "is_input": false,
                    "is_vector": false,
                    "value": [
                        100.0,
                        500.0
                    ]
                }
            ],
            "channels": 2,
            "options": {
                "comparators": "reducing",
                "efi_implementation": "none"
            },
            "program": {
                "content": "int main(){\n  float out = mem*2.5;\n mem += 0.1;\n}",
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



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);
    d.start();
    d.stop();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0xA0003,
            0xc,
            0x3F0001,
            0x20002,
            0xc,
            0xc,
            0x26,
            0x40200000,
            0x40FE3,
            0x26,
            0x3DCCCCCD,
            0x7E0FE1,
            0, 0, 0,
            0xc,
    };
    EXPECT_EQ(ops.size(), 19);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    EXPECT_EQ(ops[1].type, control_plane_write);
    // DMA

    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x30002);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[3].data[0], 0x10041002);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[4].data[0], 0x38);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    EXPECT_EQ(ops[5].data[0], 0x50001);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    EXPECT_EQ(ops[6].data[0], 0x38);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    EXPECT_EQ(ops[7].data[0], 0x10061001);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    EXPECT_EQ(ops[8].data[0], 0x38);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[9].data[0], 4);

    // MEMORY INITIALIZATION

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 4);
    EXPECT_EQ(ops[10].data[0], 0x42C80000);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 0x404);
    EXPECT_EQ(ops[11].data[0], 0x43FA0000);

    // SEQUENCER

    EXPECT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[12].data[0], 0);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[13].data[0], 2);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[14].data[0], 100000000);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[15].data[0], 1);

    // CORES

    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[16].data[0],11);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[17].data[0],1);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[18].data[0],0);

}


TEST(deployer_v2, multichannel_random_inputs) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version": 2,
    "cores": [
        {
            "id": "gen",
            "order": 1,
            "inputs": [
                {
                    "name": "a",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io": false
                    },
                    "source": {
                        "type": "random",
                        "value":[0]
                    },
                    "vector_size": 1,
                    "is_vector": false
                },
                {
                    "name": "b",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io": false
                    },
                    "source": {
                        "type": "random",
                        "value":[0]
                    },
                    "vector_size": 1,
                    "is_vector": false
                }
            ],
            "outputs": [
                {
                    "name": "c",
                    "is_vector": false,
                    "vector_size": 1,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io": false
                    }
                }
            ],
            "memory_init": [
                {
                    "name": "mem",
                    "vector_size": 1,
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true
                    },
                    "is_output": true,
                    "is_input": false,
                    "is_vector": false,
                    "value": [
                        0
                    ]
                }
            ],
            "deployment": {
                "rom_address": 0,
                "has_reciprocal": false,
                "control_address": 0
            },
            "channels": 2,
            "program": {
                "content": "int main() {\n  float a, b, mem;\n  float input_data = a + b;\n  mem += input_data;\n  c = mem*2.0;\n}",
                "headers": []
            },
            "options": {
                "comparators": "reducing",
                "efi_implementation": "efi_trig"
            },
            "sampling_frequency": 20000
        }
    ],
    "interconnect": [],
    "emulation_time": 0.001,
    "deployment_mode": false
})");



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);
    d.start();
    d.stop();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0xf0005,
            0xc,
            0x10001,
            0x20002,
            0x20003,
            0x3f0004,
            0xc,
            0xc,
            0x61021,
            0, 0, 0,
            0x7E1FE1,
            0,
            0x26,
            0x40000000,
            0, 0,
            0x40FE3,
            0, 0, 0,
            0xc,
    };
    EXPECT_EQ(ops.size(), 23);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    EXPECT_EQ(ops[1].type, control_plane_write);
    // DMA

    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x50003);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[3].data[0], 0x10061003);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[4].data[0], 0x38);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    EXPECT_EQ(ops[5].data[0], 0x70004);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    EXPECT_EQ(ops[6].data[0], 0x38);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    EXPECT_EQ(ops[7].data[0], 0x10081004);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    EXPECT_EQ(ops[8].data[0], 0x38);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[9].data[0], 4);

    // MEMORY INITIALIZATION

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 0x10);
    EXPECT_EQ(ops[10].data[0], 0);

    // INPUTS

    EXPECT_EQ(ops[11].address[0], addr_map.bases.noise_generator + noise_gen_regs.output_dest_1);
    EXPECT_EQ(ops[11].data[0], 1);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.noise_generator + noise_gen_regs.output_dest_2);
    EXPECT_EQ(ops[12].data[0], 0x10001);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.noise_generator + noise_gen_regs.output_dest_3);
    EXPECT_EQ(ops[13].data[0], 2);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.noise_generator + noise_gen_regs.output_dest_4);
    EXPECT_EQ(ops[14].data[0], 0x10002);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.noise_generator + noise_gen_regs.active_outputs);
    EXPECT_EQ(ops[15].data[0], 4);

    // SEQUENCER

    EXPECT_EQ(ops[16].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[16].data[0], 0);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[17].data[0], 2);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[18].data[0], 5000);

    EXPECT_EQ(ops[19].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[19].data[0], 1);

    // CORES

    EXPECT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[20].data[0],11);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[21].data[0],1);

    EXPECT_EQ(ops[22].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[22].data[0],0);

}

TEST(deployer_v2, memory_to_memory_inteconnect) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
      "version": 2,
      "cores": [
        {
          "id": "test_producer",
          "inputs": [],
          "order": 1,
          "outputs": [
          ],
          "program":{
            "content": "void main(){float mem; mem += 1.2;}",
            "headers": []
          },
          "memory_init": [
            {
              "name": "mem",
              "is_vector": false,
              "metadata": {
                "type": "float",
                "width": 32,
                "signed": false,
                "common_io": false
              },
              "is_output": true,
              "is_input": false,
              "is_input": false,
              "value": 0
            }
          ],
          "options":{
            "comparators":"reducing",
            "efi_implementation":"efi_sort"
          },
          "channels":1,
          "sampling_frequency": 1,
          "deployment": {
            "has_reciprocal": false,
            "control_address": 18316525568,
            "rom_address": 17179869184
          }
        },
        {
          "id": "test_consumer",
          "inputs": [],
          "memory_init": [
            {
              "name": "mem",
              "is_vector": false,
              "metadata": {
                "type": "float",
                "width": 32,
                "signed": false,
                "common_io": false
              },
              "is_output": false,
              "is_input": true,
              "value": 0
            }
          ],
          "outputs": [
            {
              "name":"out",
              "is_vector": false,
              "metadata": {
                "type": "integer",
                "width": 32,
                "signed": false,
                "common_io": false
              }
            }
          ],
          "order": 2,
          "program":{
            "content": "void main(){float mem; float out = mem*2.0;}",
            "headers": []
          },
          "options":{
            "comparators":"reducing",
            "efi_implementation":"efi_sort"
          },
          "channels":1,
          "sampling_frequency": 1,
          "deployment": {
            "has_reciprocal": false,
            "control_address": 18316525568,
            "rom_address": 17179869184
          }
        }
      ],
      "interconnect":[
        {
          "source": "test_producer.mem",
          "destination": "test_consumer.mem"
        }
      ],
      "emulation_time": 2,
      "deployment_mode": false
    })");



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);
    d.start();
    d.stop();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0xB0002,
            0xc,
            0x3f0001,
            0xc,
            0xc,
            0x26,
            0x3f99999a,
            0,
            0x7E0FE1,
            0, 0, 0, 0, 0, 0,
            0xc,
    };
    EXPECT_EQ(ops.size(), 20);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    reference_program = {
        0xB0003,
        0xc,
        0x3f0001,
        0x20002,
        0xc,
        0xc,
        0x26,
        0x40000000,
        0,
        0x40FE3,
        0, 0, 0, 0, 0, 0,
        0xc,
    };

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    EXPECT_EQ(ops[1].data, reference_program);

    // DMA

    EXPECT_EQ(ops[2].type, control_plane_write);
    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[2].data[0], 0x10001);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[3].data[0], 0x38);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[4].data[0], 1);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[5].data[0], 0x30002);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[6].data[0], 0x18);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[7].data[0], 0x1);

    // MEMORY INITIALIZATION

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 4);
    EXPECT_EQ(ops[8].data[0], 0);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + 4);
    EXPECT_EQ(ops[9].data[0], 0);

    // SEQUENCER

    EXPECT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[10].data[0], 0);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[11].data[0], 2);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    EXPECT_EQ(ops[12].data[0], 0);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    EXPECT_EQ(ops[13].data[0], 132);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[14].data[0], 100000000);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    EXPECT_EQ(ops[15].data[0], 3);

    // CORES

    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[16].data[0],11);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    EXPECT_EQ(ops[17].data[0],11);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[18].data[0],1);

    EXPECT_EQ(ops[19].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[19].data[0],0);

}



TEST(deployer_v2, multichannel_partial_transfer) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
      "version": 2,
      "cores": [
        {
          "id": "test_producer",
          "inputs": [
            {
                "name": "in_c",
                "is_vector": false,
                "metadata":{
                    "type": "float",
                    "width": 12,
                    "signed":true,
                    "common_io": false
                },
                "source": {
                    "type": "constant",
                    "value": [5,1]
                }
            }
          ],
          "order": 1,
          "outputs": [
            {
              "name":"out",
              "is_vector": false,
              "metadata": {
                "type": "float",
                "width": 32,
                "signed": false,
                "common_io": false
              }
            }
          ],
          "program":{
            "content": "void main(){float mem; mem += in_c; float out = mem*1.0;}",
            "headers": []
          },
          "memory_init": [
            {
              "name": "mem",
              "is_vector": false,
              "metadata": {
                "type": "float",
                "width": 32,
                "signed": false,
                "common_io": false
              },
              "is_output": true,
              "is_input": true,
              "value": 0
            }
          ],
          "options":{
            "comparators":"reducing",
            "efi_implementation":"efi_sort"
          },
          "channels":2,
          "sampling_frequency": 1,
          "deployment": {
            "has_reciprocal": false,
            "control_address": 18316525568,
            "rom_address": 17179869184
          }
        }
      ],
      "interconnect":[
        {
          "source": "test_producer.mem",
          "source_channel":0,
          "destination": "test_producer.mem",
          "destination_channel": 1
        }
      ],
      "emulation_time": 2,
      "deployment_mode": false
    })");



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);
    d.start();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0xA0004,
            0xc,
            0x3f0001,
            0x20002,
            0x10003,
            0xc,
            0xc,
            0x7E0FE1,
            0x26,
            0x3f800000,
            0, 0,
            0x40FE3,
            0, 0, 0,
            0xc,
    };
    EXPECT_EQ(ops.size(), 21);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);
    // DMA

    EXPECT_EQ(ops[1].type, control_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x10010001);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[3].data[0], 0x30002);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[4].data[0], 0x38);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    EXPECT_EQ(ops[5].data[0], 0x10041002);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    EXPECT_EQ(ops[6].data[0], 0x38);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[7].data[0], 0x3);

    // MEMORY INITIALIZATION

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 4);
    EXPECT_EQ(ops[8].data[0], 0);

    // INPUTS
    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[9].data[0], 0);

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[10].data[0], 3);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[11].data[0], 0x40A00000);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[12].data[0], 0x10000);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[13].data[0], 0x10003);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[14].data[0], 0x3f800000);

    // SEQUENCER


    EXPECT_EQ(ops[15].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[15].data[0], 0);

    EXPECT_EQ(ops[16].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[16].data[0], 2);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[17].data[0], 100000000);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.enable);
    EXPECT_EQ(ops[18].data[0],1);

    // CORES

    EXPECT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[19].data[0],11);

    EXPECT_EQ(ops[20].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[20].data[0],1);

}

TEST(deployer_v2, interconnect_initial_value) {

    nlohmann::json spec_json = nlohmann::json::parse(R"(
        {
            "version": 2,
            "cores": [
                {
                    "id": "fast_core",
                    "order": 2,
                    "inputs": [
                        {
                            "name": "input",
                            "metadata": {
                                "type": "float",
                                "width": 32,
                                "signed": true,
                                "common_io": false
                            },
                            "source": {
                                "type": "external",
                                "value": [
                                    40
                                ]
                            },
                            "vector_size": 1,
                            "is_vector": false
                        }
                    ],
                    "outputs": [
                        {
                            "name": "out",
                            "is_vector": false,
                            "vector_size": 1,
                            "metadata": {
                                "type": "float",
                                "width": 32,
                                "signed": true,
                                "common_io": false
                            }
                        }
                    ],
                    "memory_init": [],
                    "deployment": {
                        "rom_address": 0,
                        "has_reciprocal": false,
                        "control_address": 0
                    },
                    "channels": 1,
                    "program": {
                        "content": "void main(){\n\n    //inputs\n    float random_in;\n    float gain;\n\n    float out = input + 20.4;\n\n      \n}\n",
                        "headers": []
                    },
                    "options": {
                        "comparators": "reducing",
                        "efi_implementation": "efi_trig"
                    },
                    "sampling_frequency": 100
                },
                {
                    "id": "slow_core",
                    "order": 1,
                    "inputs": [
                        {
                            "name": "input",
                            "metadata": {
                                "type": "float",
                                "width": 32,
                                "signed": true,
                                "common_io": false
                            },
                            "source": {
                                "type": "constant",
                                "value": [
                                    50
                                ]
                            },
                            "vector_size": 1,
                            "is_vector": false
                        }
                    ],
                    "outputs": [
                        {
                            "name": "out",
                            "is_vector": false,
                            "vector_size": 1,
                            "metadata": {
                                "type": "float",
                                "width": 32,
                                "signed": true,
                                "common_io": false
                            }
                        }
                    ],
                    "memory_init": [],
                    "deployment": {
                        "rom_address": 0,
                        "has_reciprocal": false,
                        "control_address": 0
                    },
                    "channels": 1,
                    "program": {
                        "content": "void main(){\n\n    //inputs\n    float random_in;\n    float gain;\n\n    float out = input + 20.4;\n\n      \n}\n",
                        "headers": []
                    },
                    "options": {
                        "comparators": "reducing",
                        "efi_implementation": "efi_trig"
                    },
                    "sampling_frequency": 50
                }
            ],
            "interconnect": [
                {
                    "source": "slow_core.out",
                    "source_channel": -1,
                    "destination": "fast_core.input",
                    "destination_channel": -1
                }
            ],
            "emulation_time": 0.025,
            "deployment_mode": false
        }
    )");



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);
    d.start();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0xB0003,
            0xc,
            0x10001,
            0x30002,
            0xc,
            0xc,
            0x46,
            0x41A33333,
            0,
            0x61021,
            0, 0, 0, 0, 0, 0,
            0xc,
    };

    EXPECT_EQ(ops.size(), 22);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    reference_program = {
        0xB0003,
        0xc,
        0x30001,
        0x10003,
        0xc,
        0xc,
        0x46,
        0x41A33333,
        0,
        0x61021,
        0, 0, 0, 0, 0, 0,
        0xc,
};

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    EXPECT_EQ(ops[1].data, reference_program);
    // DMA

    EXPECT_EQ(ops[2].type, control_plane_write);
    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[2].data[0], 0x30002);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[3].data[0], 0x38);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[4].data[0], 0x1);


    // DMA 2
    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[5].data[0], 0x10001);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[6].data[0], 0x0);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[7].data[0], 0x42200000);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[8].data[0], 0x38);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[9].data[0], 0x1);

    // MEMORY INITIALIZATION

    // INPUTS
    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[10].data[0], 0);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[11].data[0], 3);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[12].data[0], 0x42480000);

    // SEQUENCER


    EXPECT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[13].data[0], 0);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[14].data[0], 132);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    EXPECT_EQ(ops[15].data[0], 1);

    EXPECT_EQ(ops[16].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    EXPECT_EQ(ops[16].data[0], 2);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[17].data[0], 1000000);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.enable);
    EXPECT_EQ(ops[18].data[0],3);

    // CORES

    EXPECT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[19].data[0],11);

    EXPECT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    EXPECT_EQ(ops[20].data[0],11);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[21].data[0],1);

}


TEST(deployer_v2, multichannel_interconnect_initial_value) {

    nlohmann::json spec_json = nlohmann::json::parse(R"(
    {
        "version": 2,
        "cores": [
            {
                "id": "fast_core",
                "order": 2,
                "inputs": [
                    {
                        "name": "input",
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io": false
                        },
                        "source": {
                            "type": "external",
                            "value": [
                                32,
                                64
                            ]
                        },
                        "vector_size": 1,
                        "is_vector": false
                    }
                ],
                "outputs": [
                    {
                        "name": "out",
                        "is_vector": false,
                        "vector_size": 1,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io": false
                        }
                    }
                ],
                "memory_init": [],
                "deployment": {
                    "rom_address": 0,
                    "has_reciprocal": false,
                    "control_address": 0
                },
                "channels": 2,
                "program": {
                    "content": "void main(){\n\n    //inputs\n    float random_in;\n    float gain;\n\n    float out = input + 20.4;\n\n      \n}\n",
                    "headers": []
                },
                "options": {
                    "comparators": "reducing",
                    "efi_implementation": "efi_trig"
                },
                "sampling_frequency": 100
            },
            {
                "id": "slow_core",
                "order": 1,
                "inputs": [
                    {
                        "name": "input",
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io": false
                        },
                        "source": {
                            "type": "constant",
                            "value": [
                                50
                            ]
                        },
                        "vector_size": 1,
                        "is_vector": false
                    }
                ],
                "outputs": [
                    {
                        "name": "out",
                        "is_vector": false,
                        "vector_size": 1,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io": false
                        }
                    }
                ],
                "memory_init": [],
                "deployment": {
                    "rom_address": 0,
                    "has_reciprocal": false,
                    "control_address": 0
                },
                "channels": 2,
                "program": {
                    "content": "void main(){\n\n    //inputs\n    float random_in;\n    float gain;\n\n    float out = input + 20.4;\n\n      \n}\n",
                    "headers": []
                },
                "options": {
                    "comparators": "reducing",
                    "efi_implementation": "efi_trig"
                },
                "sampling_frequency": 50
            }
        ],
        "interconnect": [
            {
                "source": "slow_core.out",
                "source_channel": -1,
                "destination": "fast_core.input",
                "destination_channel": -1
            }
        ],
        "emulation_time": 0.025,
        "deployment_mode": false
    }
    )");



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);
    d.start();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x70003,
            0xc,
            0x10001,
            0x30002,
            0xc,
            0xc,
            0x46,
            0x41A33333,
            0x61021,
            0, 0, 0,
            0xc,
    };

    EXPECT_EQ(ops.size(), 31);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    reference_program = {
        0x70003,
        0xc,
        0x30001,
        0x10003,
        0xc,
        0xc,
        0x46,
        0x41A33333,
        0x61021,
        0, 0, 0,
        0xc,
};

    EXPECT_EQ(ops[1].type, rom_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    EXPECT_EQ(ops[1].data, reference_program);
    // DMA

    EXPECT_EQ(ops[2].type, control_plane_write);
    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[2].data[0], 0x30002);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[3].data[0], 0x38);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[4].data[0], 0x10041002);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[5].data[0], 0x38);

    EXPECT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[6].data[0], 0x2);


    // DMA 2
    EXPECT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[7].data[0], 0x10001);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[8].data[0], 0x0);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[9].data[0], 0x42000000);

    EXPECT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[10].data[0], 0x38);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[11].data[0], 0x10011001);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_channel);
    EXPECT_EQ(ops[12].data[0], 0x1);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_data);
    EXPECT_EQ(ops[13].data[0], 0x42800000);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[14].data[0], 0x38);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[15].data[0], 0x2);

    // MEMORY INITIALIZATION

    // INPUTS
    EXPECT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[16].data[0], 0);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[17].data[0], 3);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[18].data[0], 0x42480000);

    EXPECT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.selector);
    EXPECT_EQ(ops[19].data[0], 0x10000);

    EXPECT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_dest);
    EXPECT_EQ(ops[20].data[0], 0x10003);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_lsb);
    EXPECT_EQ(ops[21].data[0], 0x42480000);

    // SEQUENCER


    EXPECT_EQ(ops[22].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[22].data[0], 0);

    EXPECT_EQ(ops[23].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[23].data[0], 121);

    EXPECT_EQ(ops[24].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    EXPECT_EQ(ops[24].data[0], 1);

    EXPECT_EQ(ops[25].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    EXPECT_EQ(ops[25].data[0], 2);

    EXPECT_EQ(ops[26].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[26].data[0], 1000000);

    EXPECT_EQ(ops[27].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.enable);
    EXPECT_EQ(ops[27].data[0],3);

    // CORES

    EXPECT_EQ(ops[28].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[28].data[0],11);

    EXPECT_EQ(ops[29].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    EXPECT_EQ(ops[29].data[0],11);

    EXPECT_EQ(ops[30].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[30].data[0],1);

}




TEST(deployer_v2, waveform_inputs) {

    nlohmann::json spec_json = nlohmann::json::parse(R"(
    {
        "version": 2,
        "cores": [
            {
                "id": "test core",
                "order": 1,
                "inputs": [
                     {
                    "name": "in_1",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io": false
                    },
                    "source": {
                        "ton": [
                            0.1,
                            0.1
                        ],
                        "von": [
                            20,
                            20
                        ],
                        "duty": [
                            0,
                            0
                        ],
                        "type": "waveform",
                        "voff": [
                            10,
                            10
                        ],
                        "phase": [
                            0,
                            0
                        ],
                        "shape": "square",
                        "value": "",
                        "period": [
                            0.2,
                            0.2
                        ],
                        "tdelay": [
                            0.01,
                            0.01
                        ],
                        "amplitude": [
                            0,
                            0
                        ],
                        "dc_offset": [
                            0,
                            0
                        ],
                        "frequency": [
                            0,
                            0
                        ]
                    },
                    "vector_size": 1,
                    "is_vector": false
                },
                {
                    "name": "in_2",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io": false
                    },
                    "source": {
                        "ton": [
                            0.02,
                            0.02
                        ],
                        "von": [
                            1,
                            1
                        ],
                        "duty": [
                            0,
                            0
                        ],
                        "type": "waveform",
                        "voff": [
                            -1,
                            -1
                        ],
                        "phase": [
                            0,
                            3.14
                        ],
                        "shape": "square",
                        "value": "",
                        "period": [
                            0.04,
                            0.04
                        ],
                        "tdelay": [
                            0,
                            0.01
                        ],
                        "amplitude": [
                            5,
                            5
                        ],
                        "dc_offset": [
                            0,
                            0
                        ],
                        "frequency": [
                            100,
                            100
                        ]
                    },
                    "vector_size": 1,
                    "is_vector": false
                }],
                "outputs": [
                    {
                        "name": "out",
                        "is_vector": false,
                        "vector_size": 1,
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io": false
                        }
                    }
                ],
                "memory_init": [],
                "deployment": {
                    "rom_address": 0,
                    "has_reciprocal": false,
                    "control_address": 0
                },
                "channels": 2,
                "program": {
                    "content": "void main(){\n  float in_1, in_2;\n\n  float out = in_1 + in_2;\n  \n}\n",
                    "headers": []
                },
                "options": {
                    "comparators": "reducing",
                    "efi_implementation": "efi_trig"
                },
                "sampling_frequency": 100000
            }
        ],
        "interconnect": [],
        "emulation_time": 1,
        "deployment_mode": false
    }
    )");



    auto ba = std::make_shared<bus_accessor>();
     hil_deployer d(true);
    d.set_accessor(ba);
    d.set_layout_map(addr_map_v2);
    d.deploy(spec_json);
    d.start();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x50004,
            0xc,
            0x10001,
            0x20002,
            0x30003,
            0xc,
            0xc,
            0x61021,
            0, 0, 0,
            0xc,
    };

    EXPECT_EQ(ops.size(), 52);

    EXPECT_EQ(ops[0].type, rom_plane_write);
    EXPECT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    EXPECT_EQ(ops[0].data, reference_program);

    // DMA

    EXPECT_EQ(ops[1].type, control_plane_write);
    EXPECT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    EXPECT_EQ(ops[1].data[0], 0x40003);

    EXPECT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    EXPECT_EQ(ops[2].data[0], 0x38);

    EXPECT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    EXPECT_EQ(ops[3].data[0], 0x10051003);

    EXPECT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    EXPECT_EQ(ops[4].data[0], 0x38);

    EXPECT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    EXPECT_EQ(ops[5].data[0], 0x2);


    // INPUTS
        // wave 1

    EXPECT_EQ(ops[6].address[0], addr_map.bases.waveform_generator + wave_gen_regs.channel_selector);
    EXPECT_EQ(ops[6].data[0], 0);

    EXPECT_EQ(ops[7].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_0);
    EXPECT_EQ(ops[7].data[0], 0x41A00000);

    EXPECT_EQ(ops[8].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_1);
    EXPECT_EQ(ops[8].data[0], 0x41200000);

    EXPECT_EQ(ops[9].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_2);
    EXPECT_EQ(ops[9].data[0], 1000000);

    EXPECT_EQ(ops[10].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_3);
    EXPECT_EQ(ops[10].data[0], 10000000);

    EXPECT_EQ(ops[11].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_4);
    EXPECT_EQ(ops[11].data[0], 20000000);

    EXPECT_EQ(ops[12].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_5);
    EXPECT_EQ(ops[12].data[0], 1);

    EXPECT_EQ(ops[13].address[0], addr_map.bases.waveform_generator + wave_gen_regs.shape);
    EXPECT_EQ(ops[13].data[0], 0);

    EXPECT_EQ(ops[14].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_6);
    EXPECT_EQ(ops[14].data[0], 0x38);

    EXPECT_EQ(ops[15].address[0], addr_map.bases.waveform_generator + wave_gen_regs.active_channels);
    EXPECT_EQ(ops[15].data[0], 0x1);
        // wave 2

    EXPECT_EQ(ops[16].address[0], addr_map.bases.waveform_generator + wave_gen_regs.channel_selector);
    EXPECT_EQ(ops[16].data[0], 1);

    EXPECT_EQ(ops[17].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_0);
    EXPECT_EQ(ops[17].data[0], 0x41A00000);

    EXPECT_EQ(ops[18].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_1);
    EXPECT_EQ(ops[18].data[0], 0x41200000);

    EXPECT_EQ(ops[19].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_2);
    EXPECT_EQ(ops[19].data[0], 1000000);

    EXPECT_EQ(ops[20].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_3);
    EXPECT_EQ(ops[20].data[0], 10000000);

    EXPECT_EQ(ops[21].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_4);
    EXPECT_EQ(ops[21].data[0], 20000000);

    EXPECT_EQ(ops[22].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_5);
    EXPECT_EQ(ops[22].data[0], 0x10001);

    EXPECT_EQ(ops[23].address[0], addr_map.bases.waveform_generator + wave_gen_regs.shape);
    EXPECT_EQ(ops[23].data[0], 0);

    EXPECT_EQ(ops[24].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_6);
    EXPECT_EQ(ops[24].data[0], 0x38);

    EXPECT_EQ(ops[25].address[0], addr_map.bases.waveform_generator + wave_gen_regs.active_channels);
    EXPECT_EQ(ops[25].data[0], 2);

        //wave 3

    EXPECT_EQ(ops[26].address[0], addr_map.bases.waveform_generator + wave_gen_regs.channel_selector);
    EXPECT_EQ(ops[26].data[0], 2);

    EXPECT_EQ(ops[27].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_0);
    EXPECT_EQ(ops[27].data[0], 0x3f800000);

    EXPECT_EQ(ops[28].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_1);
    EXPECT_EQ(ops[28].data[0], 0xbf800000);

    EXPECT_EQ(ops[29].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_2);
    EXPECT_EQ(ops[29].data[0], 0);

    EXPECT_EQ(ops[30].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_3);
    EXPECT_EQ(ops[30].data[0], 2000000);

    EXPECT_EQ(ops[31].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_4);
    EXPECT_EQ(ops[31].data[0], 4000000);

    EXPECT_EQ(ops[32].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_5);
    EXPECT_EQ(ops[32].data[0], 2);

    EXPECT_EQ(ops[33].address[0], addr_map.bases.waveform_generator + wave_gen_regs.shape);
    EXPECT_EQ(ops[33].data[0], 0);

    EXPECT_EQ(ops[34].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_6);
    EXPECT_EQ(ops[34].data[0], 0x38);

    EXPECT_EQ(ops[35].address[0], addr_map.bases.waveform_generator + wave_gen_regs.active_channels);
    EXPECT_EQ(ops[35].data[0], 3);


        //wave 4

    EXPECT_EQ(ops[36].address[0], addr_map.bases.waveform_generator + wave_gen_regs.channel_selector);
    EXPECT_EQ(ops[36].data[0], 3);

    EXPECT_EQ(ops[37].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_0);
    EXPECT_EQ(ops[37].data[0], 0x3f800000);

    EXPECT_EQ(ops[38].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_1);
    EXPECT_EQ(ops[38].data[0], 0xbf800000);

    EXPECT_EQ(ops[39].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_2);
    EXPECT_EQ(ops[39].data[0], 1000000);

    EXPECT_EQ(ops[40].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_3);
    EXPECT_EQ(ops[40].data[0], 2000000);

    EXPECT_EQ(ops[41].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_4);
    EXPECT_EQ(ops[41].data[0], 4000000);

    EXPECT_EQ(ops[42].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_5);
    EXPECT_EQ(ops[42].data[0], 0x10002);

    EXPECT_EQ(ops[43].address[0], addr_map.bases.waveform_generator + wave_gen_regs.shape);
    EXPECT_EQ(ops[43].data[0], 0);

    EXPECT_EQ(ops[44].address[0], addr_map.bases.waveform_generator + wave_gen_regs.parameter_6);
    EXPECT_EQ(ops[44].data[0], 0x38);

    EXPECT_EQ(ops[45].address[0], addr_map.bases.waveform_generator + wave_gen_regs.active_channels);
    EXPECT_EQ(ops[45].data[0], 4);
    // SEQUENCER


    EXPECT_EQ(ops[46].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    EXPECT_EQ(ops[46].data[0], 0);

    EXPECT_EQ(ops[47].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    EXPECT_EQ(ops[47].data[0], 2);

    EXPECT_EQ(ops[48].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    EXPECT_EQ(ops[48].data[0], 1000);

    EXPECT_EQ(ops[49].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.enable);
    EXPECT_EQ(ops[49].data[0],1);

    // CORES

    EXPECT_EQ(ops[50].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    EXPECT_EQ(ops[50].data[0],11);

    EXPECT_EQ(ops[51].address[0], addr_map.bases.gpio + gpio_regs.out);
    EXPECT_EQ(ops[51].data[0],1);

}


