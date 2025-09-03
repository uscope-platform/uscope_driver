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

#include "../deployment/hil_addresses.hpp"



nlohmann::json get_addr_map_v2() {
    std::string map = R"({
        "bases": {
            "controller": 18316591104,
            "cores_control": 18316853248,
            "cores_inputs": 8192,
            "cores_rom": 21474836480,
            "hil_control": 18316656640,
            "noise_generator": 18316722176,
            "waveform_generator": 18316787712,
            "scope_mux": 18316525568
        },
        "offsets": {
            "controller": 4096,
            "cores_control": 65536,
            "cores_inputs": 4096,
            "cores_rom": 268435456,
            "dma": 4096,
            "hil_tb": 0
        }
    })";

    return nlohmann::json::parse(map);
}

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);

    auto ops = ba->get_operations();

    std::vector<u_int64_t> reference_program = {
            0x20004,
            0xc,
            0x30001,
            0x10002,
            0x20003,
            0xc,
            0xc,
            0x60841,
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

    ASSERT_EQ(ops.size(), 15);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA

    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[4].data[0], 0);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[5].data[0], 3);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[6].data[0], 0x41f9999a);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[7].data[0], 1);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[8].data[0], 2);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[9].data[0], 0x40800000);

    // SEQUENCER

    ASSERT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[11].data[0], 2);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[12].data[0], 100'000'000);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[13].data[0], 1);

    // CORES

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[14].data[0],11);
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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);


    auto ops = ba->get_operations();


    std::vector<u_int64_t> reference_program = {
        0x20004,
        0xc,
        0x30001,
        0x10002,
        0x20003,
        0xc,
        0xc,
        0x60841,
        0xc,
    };

    ASSERT_EQ(ops.size(), 15);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[4].data[0], 0);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[5].data[0], 3);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[6].data[0], 0x41f9999a);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[7].data[0], 1);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[8].data[0], 2);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[9].data[0], 4);

    // SEQUENCER

    ASSERT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[11].data[0], 2);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[12].data[0], 100'000'000);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[13].data[0], 1);

    // CORES

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[14].data[0],11);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x3e0001,
            0x3f0002,
            0x10003,
            0xc,
            0xc,
            0x3f7e1,
            0xc

    };

    ASSERT_EQ(ops.size(), 15);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[1].data[0], 0x40003);

    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[3].data[0], 0x50002);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    ASSERT_EQ(ops[5].data[0], 0x60001);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    ASSERT_EQ(ops[6].data[0], 0x18);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[7].data[0], 3);

    // MEMORIES INITIALIZATION

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 8);
    ASSERT_EQ(ops[8].data[0], 0x41600000);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 4);
    ASSERT_EQ(ops[9].data[0], 12);

    // INPUTS

    // SEQUENCER

    ASSERT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[11].data[0], 2);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[12].data[0], 100'000'000);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[13].data[0], 1);

    // CORES

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[14].data[0],11);

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




    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x30001,
            0x10002,
            0x20003,
            0xc,
            0xc,
            0x60841,
            0xc,
    };

    ASSERT_EQ(ops.size(), 39);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[3].data[0], 0x10031001);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    ASSERT_EQ(ops[5].data[0], 0x20042001);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    ASSERT_EQ(ops[6].data[0], 0x38);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    ASSERT_EQ(ops[7].data[0], 0x30053001);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[9].data[0], 4);

    // INPUTS

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[11].data[0], 3);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[12].data[0], 0x41f9999a);


    ASSERT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[13].data[0], 0x10000);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[14].data[0], 0x10003);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[15].data[0], 0x4202CCCD);


    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[16].data[0], 0x20000);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[17].data[0], 0x20003);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[18].data[0], 0x42786666);


    ASSERT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[19].data[0], 0x30000);

    ASSERT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[20].data[0], 0x30003);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[21].data[0], 0x42800000);


    ASSERT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[22].data[0], 1);

    ASSERT_EQ(ops[23].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[23].data[0], 2);

    ASSERT_EQ(ops[24].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[24].data[0], 0x40800000);


    ASSERT_EQ(ops[25].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[25].data[0], 0x10001);

    ASSERT_EQ(ops[26].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[26].data[0], 0x10002);

    ASSERT_EQ(ops[27].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[27].data[0], 0x40000000);


    ASSERT_EQ(ops[28].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[28].data[0], 0x20001);

    ASSERT_EQ(ops[29].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[29].data[0], 0x20002);

    ASSERT_EQ(ops[30].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[30].data[0], 0x40C00000);


    ASSERT_EQ(ops[31].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[31].data[0], 0x30001);

    ASSERT_EQ(ops[32].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[32].data[0], 0x30002);

    ASSERT_EQ(ops[33].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[33].data[0], 0x41400000);

    // SEQUENCER

    ASSERT_EQ(ops[34].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[34].data[0], 0);

    ASSERT_EQ(ops[35].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[35].data[0], 2);

    ASSERT_EQ(ops[36].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[36].data[0], 100'000'000);

    ASSERT_EQ(ops[37].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[37].data[0], 1);

    // CORES

    ASSERT_EQ(ops[38].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[38].data[0],11);

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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x30002,
            0x10003,
            0x20004,
            0xc,
            0xc,
            0x60841,
            0xc,
    };
    ASSERT_EQ(ops.size(), 28);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
        0x20004,
        0xc,
        0x30001,
        0x10003,
        0x20004,
        0xc,
        0xc,
        0x60841,
        0xc,
};

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[2].data[0], 0x30002);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[3].data[0], 0x38);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[4].data[0], 1);

    // DMA 2
    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[5].data[0], 0x40001);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1+ addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[6].data[0], 0x38);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[7].data[0], 1);

    // INPUTS 1
    ASSERT_EQ(ops[8].address[0],  addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[8].data[0], 0);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[9].data[0], 4);

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[10].data[0], 0x41f9999a);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[11].data[0], 1);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[12].data[0], 3);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[13].data[0], 0x40800000);


    // INPUTS 2

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[14].data[0], 0);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[15].data[0], 4);

    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[16].data[0], 0x41f9999a);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[17].data[0], 1);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[18].data[0], 3);

    ASSERT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[19].data[0], 0x40800000);


    // SEQUENCER

    ASSERT_EQ(ops[20].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[20].data[0], 0);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[21].data[0], 2);

    ASSERT_EQ(ops[22].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    ASSERT_EQ(ops[22].data[0], 0);

    ASSERT_EQ(ops[23].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    ASSERT_EQ(ops[23].data[0], 108);

    ASSERT_EQ(ops[24].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[24].data[0], 100'000'000);

    ASSERT_EQ(ops[25].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[25].data[0], 3);

    // CORES

    ASSERT_EQ(ops[26].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[26].data[0],11);

    ASSERT_EQ(ops[27].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    ASSERT_EQ(ops[27].data[0],11);

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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x30005,
            0xc,
            0x10001,
            0x30003,
            0x10004,
            0x20005,
            0xc,
            0xc,
            0x60841,
            0x865,
            0xc

    };


    ASSERT_EQ(ops.size(), 26);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x60003,
            0xc,
            0x10001,
            0x10002,
            0xc,
            0xc,
            0x1024,
            0x26,
            0x3f800000,
            0x60841,
            0x865,
            0xc
    };

    ASSERT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[2].data[0], 0x10001);

    ASSERT_EQ(ops[3].address[0],  addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[3].data[0], 0x0);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[4].data[0], 0x0);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[5].data[0], 0x18);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
     ASSERT_EQ(ops[6].data[0], 0x40003);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[7].data[0], 0x38);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[8].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[9].data[0], 0x50002);

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[10].data[0], 0x38);



    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[11].data[0], 1);

    // INPUTS

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[12].data[0], 0);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[13].data[0], 5);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[14].data[0], 0x41f9999a);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[15].data[0], 1);

    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[16].data[0], 4);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[17].data[0], 0x4202cccd);
    // SEQUENCER

    ASSERT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[18].data[0], 0);

    ASSERT_EQ(ops[19].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[19].data[0], 2);

    ASSERT_EQ(ops[20].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    ASSERT_EQ(ops[20].data[0], 0);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    ASSERT_EQ(ops[21].data[0], 119);

    ASSERT_EQ(ops[22].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[22].data[0], 100'000'000);

    ASSERT_EQ(ops[23].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[23].data[0], 3);

    // CORES

    ASSERT_EQ(ops[24].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[24].data[0],11);

    ASSERT_EQ(ops[25].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    ASSERT_EQ(ops[25].data[0],11);

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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
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

    ASSERT_EQ(ops.size(), 24);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x40003,
            0xc,
            0x10001,
            0x30003,
            0xc,
            0xc,
            0x46,
            0x40600000,
            0x61023,
            0xc
    };

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    ASSERT_EQ(ops[1].data, reference_program);


    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[2].data[0], 0x10001);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[3].data[0], 0x0);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[4].data[0], 0x0);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[6].data[0], 0x10010002);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[7].data[0], 0x1);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[8].data[0], 0x0);


    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[9].data[0], 0x38);


    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[10].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[11].data[0], 0x40003);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[12].data[0], 0x38);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[13].data[0], 0x10051003);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[14].data[0], 0x38);


    ASSERT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[15].data[0], 2);


    // SEQUENCER

    ASSERT_EQ(ops[16].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[16].data[0], 0);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[17].data[0], 2);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    ASSERT_EQ(ops[18].data[0], 0);

    ASSERT_EQ(ops[19].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    ASSERT_EQ(ops[19].data[0], 123);

    ASSERT_EQ(ops[20].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[20].data[0], 100'000'000);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[21].data[0], 3);

    // CORES

    ASSERT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[22].data[0],11);

    ASSERT_EQ(ops[23].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    ASSERT_EQ(ops[23].data[0],11);

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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x30001,
            0x10004,
            0x20005,
            0xc,
            0xc,
            0x60841,
            0xc
    };

    ASSERT_EQ(ops.size(), 34);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x20004,
            0xc,
            0x20001,
            0x10002,
            0x30003,
            0xc,
            0xc,
            0x60841,
            0xc

    };

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[2].data[0], 0x10001);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[3].data[0], 0x0);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[4].data[0], 0x0);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[6].data[0], 0x21001);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[7].data[0], 0x1);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[8].data[0], 0x0);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[9].data[0], 0x38);

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[10].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[11].data[0], 0x40003);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[12].data[0], 0x38);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[13].data[0], 1);

    // INPUTS

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[14].data[0], 0);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[15].data[0], 0x5);

    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[16].data[0], 0x41f9999a);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[17].data[0], 0x10000);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[18].data[0], 0x10005);

    ASSERT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[19].data[0], 0x4202CCCD);


    ASSERT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[20].data[0], 1);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[21].data[0], 0x4);

    ASSERT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[22].data[0], 0x41f9999a);

    ASSERT_EQ(ops[23].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[23].data[0], 0x10001);

    ASSERT_EQ(ops[24].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[24].data[0], 0x10004);

    ASSERT_EQ(ops[25].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[25].data[0], 0x4202CCCD);

    // SEQUENCER

    ASSERT_EQ(ops[26].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[26].data[0], 0);

    ASSERT_EQ(ops[27].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[27].data[0], 2);

    ASSERT_EQ(ops[28].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    ASSERT_EQ(ops[28].data[0], 0);

    ASSERT_EQ(ops[29].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    ASSERT_EQ(ops[29].data[0], 108);

    ASSERT_EQ(ops[30].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[30].data[0], 100'000'000);

    ASSERT_EQ(ops[31].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[31].data[0], 3);

    // CORES

    ASSERT_EQ(ops[32].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[32].data[0],11);

    ASSERT_EQ(ops[33].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    ASSERT_EQ(ops[33].data[0],11);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x30001,
            0x10003,
            0x20004,
            0xc,
            0xc,
            0x60841,
            0xc

    };
    ASSERT_EQ(ops.size(), 36);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x40003,
            0xc,
            0x10001,
            0x30002,
            0xc,
            0xc,
            0x46,
            0x40600000,
            0x61023,
            0xc
    };

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[2].data[0], 0x10001);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[3].data[0], 0x0);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[4].data[0], 0x0);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[6].data[0], 0x10011001);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[7].data[0], 0x1);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[8].data[0], 0x0);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[9].data[0], 0x38);

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[10].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[11].data[0], 0x30002);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[12].data[0], 0x38);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[13].data[0], 0x10041002);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[14].data[0], 0x38);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[15].data[0], 2);

    // INPUTS

    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[16].data[0], 0);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[17].data[0], 0x4);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[18].data[0], 0x41f9999a);


    ASSERT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[19].data[0], 0x10000);

    ASSERT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[20].data[0], 0x10004);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[21].data[0], 0x4202CCCD);


    ASSERT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[22].data[0], 1);

    ASSERT_EQ(ops[23].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[23].data[0], 0x3);

    ASSERT_EQ(ops[24].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[24].data[0], 0x41f9999a);


    ASSERT_EQ(ops[25].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[25].data[0], 0x10001);

    ASSERT_EQ(ops[26].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[26].data[0], 0x10003);

    ASSERT_EQ(ops[27].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[27].data[0], 0x4202CCCD);

    // SEQUENCER

    ASSERT_EQ(ops[28].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[28].data[0], 0);

    ASSERT_EQ(ops[29].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[29].data[0], 2);

    ASSERT_EQ(ops[30].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    ASSERT_EQ(ops[30].data[0], 0);

    ASSERT_EQ(ops[31].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    ASSERT_EQ(ops[31].data[0], 108);

    ASSERT_EQ(ops[32].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[32].data[0], 100'000'000);

    ASSERT_EQ(ops[33].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[33].data[0], 3);

    // CORES

    ASSERT_EQ(ops[34].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[34].data[0],11);

    ASSERT_EQ(ops[35].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    ASSERT_EQ(ops[35].data[0],11);

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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
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
    ASSERT_EQ(ops.size(), 36);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x50005,
            0xc,
            0x20001,
            0x10002,
            0x40003,
            0x20004,
            0xc,
            0xc,
            0x66,
            0x40600000,
            0x81843,
            0x41823,
            0xc

    };

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[2].data[0], 0x10001);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[3].data[0], 0x0);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[4].data[0], 0x0);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[6].data[0], 0x10011001);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[7].data[0], 0x1);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[8].data[0], 0x0);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[9].data[0], 0x38);

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    ASSERT_EQ(ops[10].data[0], 0x20002);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[11].data[0], 0x2);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[12].data[0], 0x0);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    ASSERT_EQ(ops[13].data[0], 0x38);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    ASSERT_EQ(ops[14].data[0], 0x10021002);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[15].data[0], 0x3);

    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[16].data[0], 0x0);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    ASSERT_EQ(ops[17].data[0], 0x38);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[18].data[0], 4);

    // DMA 2
    ASSERT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[19].data[0], 0x50003);

    ASSERT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[20].data[0], 0x38);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[21].data[0], 0x10061003);

    ASSERT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[22].data[0], 0x38);

    ASSERT_EQ(ops[23].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    ASSERT_EQ(ops[23].data[0], 0x70004);

    ASSERT_EQ(ops[24].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    ASSERT_EQ(ops[24].data[0], 0x38);

    ASSERT_EQ(ops[25].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    ASSERT_EQ(ops[25].data[0], 0x10081004);

    ASSERT_EQ(ops[26].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    ASSERT_EQ(ops[26].data[0], 0x38);

    ASSERT_EQ(ops[27].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[27].data[0], 4);

    // MEMORY INITIALIZATIONS

    // SEQUENCER

    ASSERT_EQ(ops[28].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[28].data[0], 0);

    ASSERT_EQ(ops[29].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[29].data[0], 2);

    ASSERT_EQ(ops[30].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    ASSERT_EQ(ops[30].data[0], 0);

    ASSERT_EQ(ops[31].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    ASSERT_EQ(ops[31].data[0], 123);

    ASSERT_EQ(ops[32].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[32].data[0], 100'000'000);

    ASSERT_EQ(ops[33].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[33].data[0], 3);

    // CORES

    ASSERT_EQ(ops[34].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[34].data[0],11);

    ASSERT_EQ(ops[35].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    ASSERT_EQ(ops[35].data[0],11);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);
    output_specs_t out;
    out.address = 1,
    out.channel = 1,
    out.core_name = "test";
    out.source_output = "out";
    d.select_output(0, out);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20002,
            0xc,
            0x10001,
            0xc,
            0x20002,
            0x10003,
            0xc,
            0x1821021,
            0xc,
    };

    ASSERT_EQ(ops.size(), 24);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[3].data[0], 0x10031001);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[5].data[0], 2);

    // INPUTS

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[6].data[0], 0);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[7].data[0], 3);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[8].data[0], 0x41f9999a);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[9].data[0], 0x10000);

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[10].data[0], 0x10003);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[11].data[0], 0x41F9999A);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[12].data[0], 1);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[13].data[0], 2);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[14].data[0], 0x40800000);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[15].data[0], 65537);

    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[16].data[0], 65538);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[17].data[0], 0x40800000);


    // SEQUENCER

    ASSERT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[18].data[0], 0);

    ASSERT_EQ(ops[19].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[19].data[0], 2);

    ASSERT_EQ(ops[20].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[20].data[0], 100'000'000);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[21].data[0], 1);

    // CORES

    ASSERT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[22].data[0],11);

    // select_output
    ASSERT_EQ(ops[23].address[0], addr_map.bases.scope +  scope_mux_regs.ch_1);
    ASSERT_EQ(ops[23].data[0],0x10003);

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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);
    d.set_input(2,90,"test");


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x30001,
            0x10002,
            0x20003,
            0xc,
            0xc,
            0x60841,
            0xc,
    };

    ASSERT_EQ(ops.size(), 18);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[4].data[0], 0);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[5].data[0], 3);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[6].data[0], 0x41f9999a);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[7].data[0], 1);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[8].data[0], 2);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[9].data[0], 0x40800000);

    // SEQUENCER

    ASSERT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[11].data[0], 2);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[12].data[0], 100'000'000);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[13].data[0], 1);

    // CORES

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[14].data[0],11);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[15].data[0],1);

    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[16].data[0],2);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[17].data[0],90);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);
    d.start();
    d.stop();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x30001,
            0x10002,
            0x20003,
            0xc,
            0xc,
            0x60841,
            0xc,
    };
    ASSERT_EQ(ops.size(), 17);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[4].data[0], 0);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[5].data[0], 3);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[6].data[0], 0x41f9999a);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[7].data[0], 1);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[8].data[0], 2);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[9].data[0], 0x40800000);

    // SEQUENCER

    ASSERT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[11].data[0], 2);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[12].data[0], 100'000'000);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[13].data[0], 1);

    // CORES

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[14].data[0],11);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[15].data[0],1);

    ASSERT_EQ(ops[16].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[16].data[0],0);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());

    auto files = d.get_hardware_sim_data(spec_json);

    auto control_ref = "18316857356:131073\n18316857420:56\n18316857344:1\n18316861452:0\n18316861448:3\n18316861440:1106876826\n18316861452:1\n18316861448:2\n18316861440:1082130432\n18316595204:0\n18316591112:2\n18316591108:100000000\n18316595200:1\n18316853248:11\n18316656640:1\n";
    auto outputs_ref = "2:test.out\n";
    auto rom_ref = "21474836480:131076\n21474836484:12\n21474836488:196609\n21474836492:65538\n21474836496:131075\n21474836500:12\n21474836504:12\n21474836508:395329\n21474836512:12\n";
    auto inputs_ref = "test.input_1,18316861440,3,0,0\ntest.input_2,18316861440,2,1,0\n";
    EXPECT_EQ(files.control, control_ref);
    EXPECT_EQ(files.outputs, outputs_ref);
    EXPECT_EQ(files.cores, rom_ref);
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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());

    auto files = d.get_hardware_sim_data(spec_json);

    auto control_ref = "18316857356:131073\n18316857420:56\n18316857360:268636161\n18316857424:56\n18316857344:2\n18316861452:0\n18316861448:3\n18316861440:1106876826\n18316861452:65536\n18316861448:65539\n18316861440:1106876826\n18316861452:1\n18316861448:2\n18316861440:1082130432\n18316861452:65537\n18316861448:65538\n18316861440:1082130432\n18316595204:0\n18316591112:2\n18316591108:100000000\n18316595200:1\n18316853248:11\n18316656640:1\n";
    auto outputs_ref = "2:test.out[0]\n65539:test.out[1]\n";
    auto rom_ref = "21474836480:131076\n21474836484:12\n21474836488:196609\n21474836492:65538\n21474836496:131075\n21474836500:12\n21474836504:12\n21474836508:395329\n21474836512:12\n";
    auto inputs_ref = "test[0].input_1,18316861440,3,0,0\ntest[0].input_2,18316861440,2,1,0\ntest[1].input_1,18316861440,65539,65536,0\ntest[1].input_2,18316861440,65538,65537,0\n";
    EXPECT_EQ(files.control, control_ref);
    EXPECT_EQ(files.outputs, outputs_ref);
    EXPECT_EQ(files.cores, rom_ref);
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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());

    auto files = d.get_hardware_sim_data(spec_json);

    auto control_ref = "18316857356:196610\n18316857420:56\n18316857360:262145\n18316857424:56\n18316857344:2\n18316853252:0\n18316595204:0\n18316591112:2\n18316591108:100000000\n18316595200:1\n18316853248:11\n18316656640:1\n";
    auto rom_ref = "21474836480:458755\n21474836484:12\n21474836488:4128769\n21474836492:131074\n21474836496:12\n21474836500:12\n21474836504:38\n21474836508:1084856730\n21474836512:8261601\n21474836516:38\n21474836520:1073741824\n21474836524:266211\n21474836528:12\n";
    auto outputs_ref = "3:test.out\n4:test.mem\n";
    auto inputs_ref = "";
    EXPECT_EQ(files.cores, rom_ref);
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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);
    d.start();
    d.stop();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x60005,
            0xc,
            0x3F0001,
            0x20002,
            0x10003,
            0x20004,
            0xc,
            0xc,
            0x60841,
            0x7E1FE1,
            0x26,
            0x40000000,
            0x40FE3,
            0xc,
    };
    ASSERT_EQ(ops.size(), 30);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    ASSERT_EQ(ops[1].type, control_plane_write);
    // DMA

    ASSERT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[1].data[0], 0x30002);

    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[3].data[0], 0x10041002);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    ASSERT_EQ(ops[5].data[0], 0x50001);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    ASSERT_EQ(ops[6].data[0], 0x38);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    ASSERT_EQ(ops[7].data[0], 0x10061001);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[9].data[0], 4);

    // MEMORY INITIALIZATION

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 4);
    ASSERT_EQ(ops[10].data[0], 0);

    // INPUTS

    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[11].data[0], 0);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[12].data[0], 4);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[13].data[0], 0x3f800000);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[14].data[0], 0x10000);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[15].data[0], 0x10004);

    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[16].data[0], 0x40000000);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[17].data[0], 1);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[18].data[0], 0x3);

    ASSERT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[19].data[0], 0x3f800000);


    ASSERT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[20].data[0], 0x10001);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[21].data[0], 0x10003);

    ASSERT_EQ(ops[22].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[22].data[0], 0x40000000);

    // SEQUENCER

    ASSERT_EQ(ops[23].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[23].data[0], 0);

    ASSERT_EQ(ops[24].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[24].data[0], 2);

    ASSERT_EQ(ops[25].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[25].data[0], 5000);

    ASSERT_EQ(ops[26].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[26].data[0], 1);

    // CORES

    ASSERT_EQ(ops[27].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[27].data[0],11);

    ASSERT_EQ(ops[28].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[28].data[0],1);

    ASSERT_EQ(ops[29].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[29].data[0],0);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);
    d.start();
    d.stop();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x70003,
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
            0xc,
    };
    ASSERT_EQ(ops.size(), 19);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    ASSERT_EQ(ops[1].type, control_plane_write);
    // DMA

    ASSERT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[1].data[0], 0x30002);

    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[3].data[0], 0x10041002);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    ASSERT_EQ(ops[5].data[0], 0x50001);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    ASSERT_EQ(ops[6].data[0], 0x38);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    ASSERT_EQ(ops[7].data[0], 0x10061001);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[9].data[0], 4);

    // MEMORY INITIALIZATION

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 4);
    ASSERT_EQ(ops[10].data[0], 0x42C80000);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 0x404);
    ASSERT_EQ(ops[11].data[0], 0x43FA0000);

    // SEQUENCER

    ASSERT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[12].data[0], 0);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[13].data[0], 2);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[14].data[0], 100000000);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[15].data[0], 1);

    // CORES

    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[16].data[0],11);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[17].data[0],1);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[18].data[0],0);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);
    d.start();
    d.stop();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x60005,
            0xc,
            0x10001,
            0x20002,
            0x3f0003,
            0x20004,
            0xc,
            0xc,
            0x60841,
            0x7E1FE1,
            0x26,
            0x40000000,
            0x40FE3,
            0xc,
    };
    ASSERT_EQ(ops.size(), 23);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    ASSERT_EQ(ops[1].type, control_plane_write);
    // DMA

    ASSERT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[1].data[0], 0x50004);

    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[3].data[0], 0x10061004);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    ASSERT_EQ(ops[5].data[0], 0x70003);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    ASSERT_EQ(ops[6].data[0], 0x38);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*3);
    ASSERT_EQ(ops[7].data[0], 0x10081003);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*3);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[9].data[0], 4);

    // MEMORY INITIALIZATION

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 0xc);
    ASSERT_EQ(ops[10].data[0], 0);

    // INPUTS

    ASSERT_EQ(ops[11].address[0], addr_map.bases.noise_generator + noise_gen_regs.output_dest_1);
    ASSERT_EQ(ops[11].data[0], 2);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.noise_generator + noise_gen_regs.output_dest_2);
    ASSERT_EQ(ops[12].data[0], 0x10002);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.noise_generator + noise_gen_regs.output_dest_3);
    ASSERT_EQ(ops[13].data[0], 1);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.noise_generator + noise_gen_regs.output_dest_4);
    ASSERT_EQ(ops[14].data[0], 0x10001);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.noise_generator + noise_gen_regs.active_outputs);
    ASSERT_EQ(ops[15].data[0], 4);

    // SEQUENCER

    ASSERT_EQ(ops[16].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[16].data[0], 0);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[17].data[0], 2);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[18].data[0], 5000);

    ASSERT_EQ(ops[19].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[19].data[0], 1);

    // CORES

    ASSERT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[20].data[0],11);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[21].data[0],1);

    ASSERT_EQ(ops[22].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[22].data[0],0);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);
    d.start();
    d.stop();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x40002,
            0xc,
            0x3f0001,
            0xc,
            0xc,
            0x26,
            0x3f99999a,
            0x7E0FE1,
            0xc,
    };
    ASSERT_EQ(ops.size(), 20);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
        0x40003,
        0xc,
        0x3f0001,
        0x20002,
        0xc,
        0xc,
        0x26,
        0x40000000,
        0x40FE3,
        0xc,
    };

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA

    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[2].data[0], 0x10001);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[3].data[0], 0x38);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[4].data[0], 1);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[5].data[0], 0x30002);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[6].data[0], 0x18);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[7].data[0], 0x1);

    // MEMORY INITIALIZATION

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 4);
    ASSERT_EQ(ops[8].data[0], 0);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + 4);
    ASSERT_EQ(ops[9].data[0], 0);

    // SEQUENCER

    ASSERT_EQ(ops[10].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[11].data[0], 2);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    ASSERT_EQ(ops[12].data[0], 0);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    ASSERT_EQ(ops[13].data[0], 121);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[14].data[0], 100000000);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.controller + addr_map.offsets.sequencer);
    ASSERT_EQ(ops[15].data[0], 3);

    // CORES

    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[16].data[0],11);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    ASSERT_EQ(ops[17].data[0],11);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[18].data[0],1);

    ASSERT_EQ(ops[19].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[19].data[0],0);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);
    d.start();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x50004,
            0xc,
            0x3f0001,
            0x20002,
            0x10003,
            0xc,
            0xc,
            0x7E0FE1,
            0x26,
            0x3f800000,
            0x40FE3,
            0xc,
    };
    ASSERT_EQ(ops.size(), 21);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);
    // DMA

    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[1].data[0], 0x10010001);

    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[3].data[0], 0x30002);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*2);
    ASSERT_EQ(ops[5].data[0], 0x10041002);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*2);
    ASSERT_EQ(ops[6].data[0], 0x38);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[7].data[0], 0x3);

    // MEMORY INITIALIZATION

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + 4);
    ASSERT_EQ(ops[8].data[0], 0);

    // INPUTS
    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[9].data[0], 0);

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[10].data[0], 3);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[11].data[0], 0x40A00000);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[12].data[0], 0x10000);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[13].data[0], 0x10003);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[14].data[0], 0x3f800000);

    // SEQUENCER


    ASSERT_EQ(ops[15].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[15].data[0], 0);

    ASSERT_EQ(ops[16].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[16].data[0], 2);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[17].data[0], 100000000);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.enable);
    ASSERT_EQ(ops[18].data[0],1);

    // CORES

    ASSERT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[19].data[0],11);

    ASSERT_EQ(ops[20].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[20].data[0],1);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);
    d.start();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x40003,
            0xc,
            0x10001,
            0x30002,
            0xc,
            0xc,
            0x46,
            0x41A33333,
            0x61021,
            0xc,
    };

    ASSERT_EQ(ops.size(), 22);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
        0x40003,
        0xc,
        0x30001,
        0x10003,
        0xc,
        0xc,
        0x46,
        0x41A33333,
        0x61021,
        0xc,
};

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    ASSERT_EQ(ops[1].data, reference_program);
    // DMA

    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[2].data[0], 0x30002);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[3].data[0], 0x38);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[4].data[0], 0x1);


    // DMA 2
    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[5].data[0], 0x10001);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[6].data[0], 0x0);

    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[7].data[0], 0x42200000);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[9].data[0], 0x1);

    // MEMORY INITIALIZATION

    // INPUTS
    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[11].data[0], 3);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[12].data[0], 0x42480000);

    // SEQUENCER


    ASSERT_EQ(ops[13].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[13].data[0], 0);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[14].data[0], 121);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    ASSERT_EQ(ops[15].data[0], 1);

    ASSERT_EQ(ops[16].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    ASSERT_EQ(ops[16].data[0], 2);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[17].data[0], 1000000);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.enable);
    ASSERT_EQ(ops[18].data[0],3);

    // CORES

    ASSERT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[19].data[0],11);

    ASSERT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    ASSERT_EQ(ops[20].data[0],11);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[21].data[0],1);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    d.set_layout_map(get_addr_map_v2());
    d.deploy(spec_json);
    d.start();


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x40003,
            0xc,
            0x10001,
            0x30002,
            0xc,
            0xc,
            0x46,
            0x41A33333,
            0x61021,
            0xc,
    };

    ASSERT_EQ(ops.size(), 31);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], addr_map.cores_rom_plane);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
        0x40003,
        0xc,
        0x30001,
        0x10003,
        0xc,
        0xc,
        0x46,
        0x41A33333,
        0x61021,
        0xc,
};

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[1].address[0], addr_map.cores_rom_plane + addr_map.offsets.rom*1);
    ASSERT_EQ(ops[1].data, reference_program);
    // DMA

    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[2].data[0], 0x30002);

    ASSERT_EQ(ops[3].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[3].data[0], 0x38);

    ASSERT_EQ(ops[4].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[4].data[0], 0x10041002);

    ASSERT_EQ(ops[5].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[6].data[0], 0x2);


    // DMA 2
    ASSERT_EQ(ops[7].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base);
    ASSERT_EQ(ops[7].data[0], 0x10001);

    ASSERT_EQ(ops[8].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[8].data[0], 0x0);

    ASSERT_EQ(ops[9].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[9].data[0], 0x42000000);

    ASSERT_EQ(ops[10].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base);
    ASSERT_EQ(ops[10].data[0], 0x38);

    ASSERT_EQ(ops[11].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.addr_base + 4*1);
    ASSERT_EQ(ops[11].data[0], 0x10011001);

    ASSERT_EQ(ops[12].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_channel);
    ASSERT_EQ(ops[12].data[0], 0x1);

    ASSERT_EQ(ops[13].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.fill_data);
    ASSERT_EQ(ops[13].data[0], 0x42800000);

    ASSERT_EQ(ops[14].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.user_base + 4*1);
    ASSERT_EQ(ops[14].data[0], 0x38);

    ASSERT_EQ(ops[15].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.dma + dma_regs.channels);
    ASSERT_EQ(ops[15].data[0], 0x2);

    // MEMORY INITIALIZATION

    // INPUTS
    ASSERT_EQ(ops[16].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[16].data[0], 0);

    ASSERT_EQ(ops[17].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[17].data[0], 3);

    ASSERT_EQ(ops[18].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[18].data[0], 0x42480000);

    ASSERT_EQ(ops[19].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.selector);
    ASSERT_EQ(ops[19].data[0], 0x10000);

    ASSERT_EQ(ops[20].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_dest);
    ASSERT_EQ(ops[20].data[0], 0x10003);

    ASSERT_EQ(ops[21].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1 + addr_map.offsets.inputs + mc_const.data_lsb);
    ASSERT_EQ(ops[21].data[0], 0x42480000);

    // SEQUENCER


    ASSERT_EQ(ops[22].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_1);
    ASSERT_EQ(ops[22].data[0], 0);

    ASSERT_EQ(ops[23].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_1);
    ASSERT_EQ(ops[23].data[0], 121);

    ASSERT_EQ(ops[24].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.divisors_2);
    ASSERT_EQ(ops[24].data[0], 1);

    ASSERT_EQ(ops[25].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.threshold_2);
    ASSERT_EQ(ops[25].data[0], 2);

    ASSERT_EQ(ops[26].address[0], addr_map.bases.controller + addr_map.offsets.timebase + enable_gen_regs.period);
    ASSERT_EQ(ops[26].data[0], 1000000);

    ASSERT_EQ(ops[27].address[0], addr_map.bases.controller + addr_map.offsets.sequencer + sequencer_regs.enable);
    ASSERT_EQ(ops[27].data[0],3);

    // CORES

    ASSERT_EQ(ops[28].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*0);
    ASSERT_EQ(ops[28].data[0],11);

    ASSERT_EQ(ops[29].address[0], addr_map.bases.cores_control + addr_map.offsets.cores*1);
    ASSERT_EQ(ops[29].data[0],11);

    ASSERT_EQ(ops[30].address[0], addr_map.bases.gpio + gpio_regs.out);
    ASSERT_EQ(ops[30].data[0],1);

}