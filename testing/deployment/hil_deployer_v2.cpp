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

#include "../deployment/fpga_bridge_mock.hpp"


nlohmann::json get_addr_map_v2(){

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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

    ASSERT_EQ(ops.size(), 13);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA

    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[4].data[0], 3);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[5].data[0], 0x41f9999a);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'3008);
    ASSERT_EQ(ops[6].data[0], 2);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'3000);
    ASSERT_EQ(ops[7].data[0], 0x40800000);

    // SEQUENCER

    ASSERT_EQ(ops[8].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[8].data[0], 0);

    ASSERT_EQ(ops[9].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[9].data[0], 2);

    ASSERT_EQ(ops[10].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[10].data[0], 100'000'000);

    ASSERT_EQ(ops[11].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[11].data[0], 1);

    // CORES

    ASSERT_EQ(ops[12].address[0], 0x443c20000);
    ASSERT_EQ(ops[12].data[0],11);

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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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

    ASSERT_EQ(ops.size(), 13);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[4].data[0], 3);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[5].data[0], 0x41f9999a);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'3008);
    ASSERT_EQ(ops[6].data[0], 2);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'3000);
    ASSERT_EQ(ops[7].data[0], 4);

    // SEQUENCER

    ASSERT_EQ(ops[8].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[8].data[0], 0);

    ASSERT_EQ(ops[9].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[9].data[0], 2);

    ASSERT_EQ(ops[10].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[10].data[0], 100'000'000);

    ASSERT_EQ(ops[11].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[11].data[0], 1);

    // CORES

    ASSERT_EQ(ops[12].address[0], 0x443c20000);
    ASSERT_EQ(ops[12].data[0],11);

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
                "build_settings": {
                    "io": {
                        "inputs": [
                        ],
                        "memories": [
                            "mem",
                            "mem_2"
                        ],
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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x40003);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[3].data[0], 0x50002);

    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'100c);
    ASSERT_EQ(ops[5].data[0], 0x60001);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'104c);
    ASSERT_EQ(ops[6].data[0], 0x18);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[7].data[0], 3);

    // MEMORIES INITIALIZATION

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'0008);
    ASSERT_EQ(ops[8].data[0], 0x41600000);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'0004);
    ASSERT_EQ(ops[9].data[0], 12);

    // INPUTS

    // SEQUENCER

    ASSERT_EQ(ops[10].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[11].data[0], 2);

    ASSERT_EQ(ops[12].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[12].data[0], 100'000'000);

    ASSERT_EQ(ops[13].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[13].data[0], 1);

    // CORES

    ASSERT_EQ(ops[14].address[0], 0x443c20000);
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




    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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

    ASSERT_EQ(ops.size(), 19);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[3].data[0], 0x10031001);

    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'100c);
    ASSERT_EQ(ops[5].data[0], 0x20042001);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'104c);
    ASSERT_EQ(ops[6].data[0], 0x38);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'1010);
    ASSERT_EQ(ops[7].data[0], 0x30053001);

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'1050);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[9].data[0], 4);

    // INPUTS
    ASSERT_EQ(ops[10].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[10].data[0], 3);

    ASSERT_EQ(ops[11].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[11].data[0], 0x41f9999a);

    ASSERT_EQ(ops[12].address[0], 0x4'43c2'3008);
    ASSERT_EQ(ops[12].data[0], 2);

    ASSERT_EQ(ops[13].address[0], 0x4'43c2'3000);
    ASSERT_EQ(ops[13].data[0], 0x40800000);

    // SEQUENCER

    ASSERT_EQ(ops[14].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[14].data[0], 0);

    ASSERT_EQ(ops[15].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[15].data[0], 2);

    ASSERT_EQ(ops[16].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[16].data[0], 100'000'000);

    ASSERT_EQ(ops[17].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[17].data[0], 1);

    // CORES

    ASSERT_EQ(ops[18].address[0], 0x443c20000);
    ASSERT_EQ(ops[18].data[0],11);

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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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
    ASSERT_EQ(ops.size(), 24);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
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
    ASSERT_EQ(ops[1].address[0], 0x5'1000'0000);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[2].data[0], 0x30002);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x38);

    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[4].data[0], 1);

    // DMA 2
    ASSERT_EQ(ops[5].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[5].data[0], 0x40001);

    ASSERT_EQ(ops[6].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[6].data[0], 0x38);

    ASSERT_EQ(ops[7].address[0], 0x4'43c3'1000);
    ASSERT_EQ(ops[7].data[0], 1);

    // INPUTS 1
    ASSERT_EQ(ops[8].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[8].data[0], 4);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[9].data[0], 0x41f9999a);

    ASSERT_EQ(ops[10].address[0], 0x4'43c2'3008);
    ASSERT_EQ(ops[10].data[0], 3);

    ASSERT_EQ(ops[11].address[0], 0x4'43c2'3000);
    ASSERT_EQ(ops[11].data[0], 0x40800000);


    // INPUTS 2
    ASSERT_EQ(ops[12].address[0], 0x4'43c3'2008);
    ASSERT_EQ(ops[12].data[0], 4);

    ASSERT_EQ(ops[13].address[0], 0x4'43c3'2000);
    ASSERT_EQ(ops[13].data[0], 0x41f9999a);

    ASSERT_EQ(ops[14].address[0], 0x4'43c3'3008);
    ASSERT_EQ(ops[14].data[0], 3);

    ASSERT_EQ(ops[15].address[0], 0x4'43c3'3000);
    ASSERT_EQ(ops[15].data[0], 0x40800000);


    // SEQUENCER

    ASSERT_EQ(ops[16].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[16].data[0], 0);

    ASSERT_EQ(ops[17].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[17].data[0], 2);

    ASSERT_EQ(ops[18].address[0], 0x4'43c1'1008);
    ASSERT_EQ(ops[18].data[0], 0);

    ASSERT_EQ(ops[19].address[0], 0x4'43c1'000C);
    ASSERT_EQ(ops[19].data[0], 0x4e);

    ASSERT_EQ(ops[20].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[20].data[0], 100'000'000);

    ASSERT_EQ(ops[21].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[21].data[0], 3);

    // CORES

    ASSERT_EQ(ops[22].address[0], 0x443c20000);
    ASSERT_EQ(ops[22].data[0],11);

    ASSERT_EQ(ops[23].address[0], 0x443c30000);
    ASSERT_EQ(ops[23].data[0],11);

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
                "build_settings": {
                    "io": {
                        "inputs": [
                            "input_1",
                            "input_2"
                        ],
                        "outputs": [
                            "out",
                            "out2"
                        ],
                        "memories": []
                    }
                },
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
                    "source":{"type": "external"}
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
                "build_settings": {
                    "io": {
                        "inputs": [
                            "input"
                        ],
                        "outputs": [
                            "out"
                        ],
                        "memories": []
                    }
                },
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
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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


    ASSERT_EQ(ops.size(), 22);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
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

    ASSERT_EQ(ops[1].address[0], 0x5'1000'0000);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[2].data[0], 0x10001);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x18);

    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1008);
     ASSERT_EQ(ops[4].data[0], 0x40003);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[6].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[7].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[7].data[0], 0x50002);

    ASSERT_EQ(ops[8].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[8].data[0], 0x38);



    ASSERT_EQ(ops[9].address[0], 0x4'43c3'1000);
    ASSERT_EQ(ops[9].data[0], 1);

    // INPUTS


    ASSERT_EQ(ops[10].address[0], 0x443c22008);
    ASSERT_EQ(ops[10].data[0], 5);

    ASSERT_EQ(ops[11].address[0], 0x443c22000);
    ASSERT_EQ(ops[11].data[0], 0x41f9999a);


    ASSERT_EQ(ops[12].address[0], 0x443c23008);
    ASSERT_EQ(ops[12].data[0], 4);

    ASSERT_EQ(ops[13].address[0], 0x443c23000);
    ASSERT_EQ(ops[13].data[0], 0x4202cccd);
    // SEQUENCER

    ASSERT_EQ(ops[14].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[14].data[0], 0);

    ASSERT_EQ(ops[15].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[15].data[0], 2);

    ASSERT_EQ(ops[16].address[0], 0x4'43c1'1008);
    ASSERT_EQ(ops[16].data[0], 0);

    ASSERT_EQ(ops[17].address[0], 0x4'43c1'000C);
    ASSERT_EQ(ops[17].data[0], 0x59);

    ASSERT_EQ(ops[18].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[18].data[0], 100'000'000);

    ASSERT_EQ(ops[19].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[19].data[0], 3);

    // CORES

    ASSERT_EQ(ops[20].address[0], 0x443c20000);
    ASSERT_EQ(ops[20].data[0],11);

    ASSERT_EQ(ops[21].address[0], 0x443c30000);
    ASSERT_EQ(ops[21].data[0],11);

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
                    "build_settings":{"io":{"inputs":[],"outputs":["out"],"memories":[]}},
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
                        "source":{"type": "external"}
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
                    "build_settings":{"io":{"inputs":["input"],"outputs":["out"],"memories":[]}},
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
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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

    ASSERT_EQ(ops.size(), 20);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
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
    ASSERT_EQ(ops[1].address[0], 0x5'1000'0000);
    ASSERT_EQ(ops[1].data, reference_program);


    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[2].data[0], 0x10001);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x38);


    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[4].data[0], 0x10010002);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[5].data[0], 0x38);


    ASSERT_EQ(ops[6].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[6].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[7].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[7].data[0], 0x40003);

    ASSERT_EQ(ops[8].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], 0x4'43c3'1008);
    ASSERT_EQ(ops[9].data[0], 0x10051003);

    ASSERT_EQ(ops[10].address[0], 0x4'43c3'1048);
    ASSERT_EQ(ops[10].data[0], 0x38);


    ASSERT_EQ(ops[11].address[0], 0x4'43c3'1000);
    ASSERT_EQ(ops[11].data[0], 2);


    // SEQUENCER

    ASSERT_EQ(ops[12].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[12].data[0], 0);

    ASSERT_EQ(ops[13].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[13].data[0], 2);

    ASSERT_EQ(ops[14].address[0], 0x4'43c1'1008);
    ASSERT_EQ(ops[14].data[0], 0);

    ASSERT_EQ(ops[15].address[0], 0x4'43c1'000C);
    ASSERT_EQ(ops[15].data[0], 0x5d);

    ASSERT_EQ(ops[16].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[16].data[0], 100'000'000);

    ASSERT_EQ(ops[17].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[17].data[0], 3);

    // CORES

    ASSERT_EQ(ops[18].address[0], 0x443c20000);
    ASSERT_EQ(ops[18].data[0],11);

    ASSERT_EQ(ops[19].address[0], 0x443c30000);
    ASSERT_EQ(ops[19].data[0],11);

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
                "build_settings":{"io":{"inputs":["input_data"],"outputs":["out"],"memories":[]}},
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
                        "source":{"type": "external"}
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
                "build_settings":{"io":{"inputs":["input_1", "input_2"],"outputs":["out"],"memories":[]}},
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
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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

    ASSERT_EQ(ops.size(), 22);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
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
    ASSERT_EQ(ops[1].address[0], 0x5'1000'0000);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[2].data[0], 0x10001);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x38);


    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[4].data[0], 0x21001);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[6].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[7].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[7].data[0], 0x40003);

    ASSERT_EQ(ops[8].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], 0x4'43c3'1000);
    ASSERT_EQ(ops[9].data[0], 1);

    // MEMORY INITIALIZATIONS

    ASSERT_EQ(ops[10].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[10].data[0], 0x5);

    ASSERT_EQ(ops[11].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[11].data[0], 0x41f9999a);

    ASSERT_EQ(ops[12].address[0], 0x4'43c2'3008);
    ASSERT_EQ(ops[12].data[0], 0x4);

    ASSERT_EQ(ops[13].address[0], 0x4'43c2'3000);
    ASSERT_EQ(ops[13].data[0], 0x41f9999a);

    // SEQUENCER

    ASSERT_EQ(ops[14].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[14].data[0], 0);

    ASSERT_EQ(ops[15].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[15].data[0], 2);

    ASSERT_EQ(ops[16].address[0], 0x4'43c1'1008);
    ASSERT_EQ(ops[16].data[0], 0);

    ASSERT_EQ(ops[17].address[0], 0x4'43c1'000C);
    ASSERT_EQ(ops[17].data[0], 78);

    ASSERT_EQ(ops[18].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[18].data[0], 100'000'000);

    ASSERT_EQ(ops[19].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[19].data[0], 3);

    // CORES

    ASSERT_EQ(ops[20].address[0], 0x443c20000);
    ASSERT_EQ(ops[20].data[0],11);

    ASSERT_EQ(ops[21].address[0], 0x443c30000);
    ASSERT_EQ(ops[21].data[0],11);

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
                    "build_settings":{"io":{"inputs":["input_1", "input_2"],"outputs":["out"],"memories":[]}},
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
                        "source":{"type": "external"}
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
                    "build_settings":{"io":{"inputs":["input"],"outputs":["out"],"memories":[]}},
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
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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
    ASSERT_EQ(ops.size(), 24);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
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
    ASSERT_EQ(ops[1].address[0], 0x5'1000'0000);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[2].data[0], 0x10001);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x38);


    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[4].data[0], 0x10011001);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[6].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[7].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[7].data[0], 0x30002);

    ASSERT_EQ(ops[8].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], 0x4'43c3'1008);
    ASSERT_EQ(ops[9].data[0], 0x10041002);

    ASSERT_EQ(ops[10].address[0], 0x4'43c3'1048);
    ASSERT_EQ(ops[10].data[0], 0x38);

    ASSERT_EQ(ops[11].address[0], 0x4'43c3'1000);
    ASSERT_EQ(ops[11].data[0], 2);

    // MEMORY INITIALIZATIONS

    ASSERT_EQ(ops[12].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[12].data[0], 0x4);

    ASSERT_EQ(ops[13].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[13].data[0], 0x41f9999a);

    ASSERT_EQ(ops[14].address[0], 0x4'43c2'3008);
    ASSERT_EQ(ops[14].data[0], 0x3);

    ASSERT_EQ(ops[15].address[0], 0x4'43c2'3000);
    ASSERT_EQ(ops[15].data[0], 0x41f9999a);

    // SEQUENCER

    ASSERT_EQ(ops[16].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[16].data[0], 0);

    ASSERT_EQ(ops[17].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[17].data[0], 2);

    ASSERT_EQ(ops[18].address[0], 0x4'43c1'1008);
    ASSERT_EQ(ops[18].data[0], 0);

    ASSERT_EQ(ops[19].address[0], 0x4'43c1'000C);
    ASSERT_EQ(ops[19].data[0], 78);

    ASSERT_EQ(ops[20].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[20].data[0], 100'000'000);

    ASSERT_EQ(ops[21].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[21].data[0], 3);

    // CORES

    ASSERT_EQ(ops[22].address[0], 0x443c20000);
    ASSERT_EQ(ops[22].data[0],11);

    ASSERT_EQ(ops[23].address[0], 0x443c30000);
    ASSERT_EQ(ops[23].data[0],11);

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
                    "build_settings":{"io":{"inputs":[],"outputs":["out"],"memories":[]}},
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
                            "source":{"type": "external"}
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
                    "build_settings":{"io":{"inputs":["input"],"outputs":["consumer_out"],"memories":[]}},
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
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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
    ASSERT_EQ(ops.size(), 28);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
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
    ASSERT_EQ(ops[1].address[0], 0x5'1000'0000);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[2].data[0], 0x10001);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x38);

    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[4].data[0], 0x10011001);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'100c);
    ASSERT_EQ(ops[6].data[0], 0x20002);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'104c);
    ASSERT_EQ(ops[7].data[0], 0x38);

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'1010);
    ASSERT_EQ(ops[8].data[0], 0x10021002);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'1050);
    ASSERT_EQ(ops[9].data[0], 0x38);

    ASSERT_EQ(ops[10].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[10].data[0], 4);

    // DMA 2
    ASSERT_EQ(ops[11].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[11].data[0], 0x50003);

    ASSERT_EQ(ops[12].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[12].data[0], 0x38);

    ASSERT_EQ(ops[13].address[0], 0x4'43c3'1008);
    ASSERT_EQ(ops[13].data[0], 0x10061003);

    ASSERT_EQ(ops[14].address[0], 0x4'43c3'1048);
    ASSERT_EQ(ops[14].data[0], 0x38);

    ASSERT_EQ(ops[15].address[0], 0x4'43c3'100c);
    ASSERT_EQ(ops[15].data[0], 0x70004);

    ASSERT_EQ(ops[16].address[0], 0x4'43c3'104c);
    ASSERT_EQ(ops[16].data[0], 0x38);

    ASSERT_EQ(ops[17].address[0], 0x4'43c3'1010);
    ASSERT_EQ(ops[17].data[0], 0x10081004);

    ASSERT_EQ(ops[18].address[0], 0x4'43c3'1050);
    ASSERT_EQ(ops[18].data[0], 0x38);

    ASSERT_EQ(ops[19].address[0], 0x4'43c3'1000);
    ASSERT_EQ(ops[19].data[0], 4);

    // MEMORY INITIALIZATIONS

    // SEQUENCER

    ASSERT_EQ(ops[20].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[20].data[0], 0);

    ASSERT_EQ(ops[21].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[21].data[0], 2);

    ASSERT_EQ(ops[22].address[0], 0x4'43c1'1008);
    ASSERT_EQ(ops[22].data[0], 0);

    ASSERT_EQ(ops[23].address[0], 0x4'43c1'000C);
    ASSERT_EQ(ops[23].data[0], 93);

    ASSERT_EQ(ops[24].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[24].data[0], 100'000'000);

    ASSERT_EQ(ops[25].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[25].data[0], 3);

    // CORES

    ASSERT_EQ(ops[26].address[0], 0x443c20000);
    ASSERT_EQ(ops[26].data[0],11);

    ASSERT_EQ(ops[27].address[0], 0x443c30000);
    ASSERT_EQ(ops[27].data[0],11);

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
            "channels": 2,
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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);
    output_specs_t out;
    out.address = 1,
    out.channel = 1,
    out.core_name = "test";
    out.source_output = "out";
    d.select_output(0, out);


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

    ASSERT_EQ(ops.size(), 16);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[3].data[0], 0x10031001);

    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[5].data[0], 2);

    // INPUTS
    ASSERT_EQ(ops[6].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[6].data[0], 3);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[7].data[0], 0x41f9999a);

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'3008);
    ASSERT_EQ(ops[8].data[0], 2);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'3000);
    ASSERT_EQ(ops[9].data[0], 0x40800000);

    // SEQUENCER

    ASSERT_EQ(ops[10].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[11].data[0], 2);

    ASSERT_EQ(ops[12].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[12].data[0], 100'000'000);

    ASSERT_EQ(ops[13].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[13].data[0], 1);

    // CORES

    ASSERT_EQ(ops[14].address[0], 0x443c20000);
    ASSERT_EQ(ops[14].data[0],11);

    // select_output
    ASSERT_EQ(ops[15].address[0], 0x443c50004);
    ASSERT_EQ(ops[15].data[0],0x10003);

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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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

    ASSERT_EQ(ops.size(), 15);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[4].data[0], 3);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[5].data[0], 0x41f9999a);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'3008);
    ASSERT_EQ(ops[6].data[0], 2);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'3000);
    ASSERT_EQ(ops[7].data[0], 0x40800000);

    // SEQUENCER

    ASSERT_EQ(ops[8].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[8].data[0], 0);

    ASSERT_EQ(ops[9].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[9].data[0], 2);

    ASSERT_EQ(ops[10].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[10].data[0], 100'000'000);

    ASSERT_EQ(ops[11].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[11].data[0], 1);

    // CORES

    ASSERT_EQ(ops[12].address[0], 0x443c20000);
    ASSERT_EQ(ops[12].data[0],11);

    ASSERT_EQ(ops[13].address[0], 0x443c23008);
    ASSERT_EQ(ops[13].data[0],2);

    ASSERT_EQ(ops[14].address[0], 0x443c23000);
    ASSERT_EQ(ops[14].data[0],90);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
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
    ASSERT_EQ(ops.size(), 15);

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x20001);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[4].data[0], 3);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[5].data[0], 0x41f9999a);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'3008);
    ASSERT_EQ(ops[6].data[0], 2);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'3000);
    ASSERT_EQ(ops[7].data[0], 0x40800000);

    // SEQUENCER

    ASSERT_EQ(ops[8].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[8].data[0], 0);

    ASSERT_EQ(ops[9].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[9].data[0], 2);

    ASSERT_EQ(ops[10].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[10].data[0], 100'000'000);

    ASSERT_EQ(ops[11].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[11].data[0], 1);

    // CORES

    ASSERT_EQ(ops[12].address[0], 0x443c20000);
    ASSERT_EQ(ops[12].data[0],11);

    ASSERT_EQ(ops[13].address[0], 0x4'43c0'0000);
    ASSERT_EQ(ops[13].data[0],1);

    ASSERT_EQ(ops[14].address[0], 0x4'43c0'0000);
    ASSERT_EQ(ops[14].data[0],0);

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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map_v2();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);

    auto ops = ba->get_operations();


    auto files = ba->get_hardware_simulation_data();
    auto control_ref = "18316660740:131073\n18316660804:56\n18316660736:1\n18316664840:3\n18316664832:1106876826\n18316668936:2\n18316668928:1082130432\n18316595204:0\n18316591112:2\n18316591108:100000000\n18316595200:1\n18316656640:11\n";
    auto rom_ref = "21474836480:131076\n21474836484:12\n21474836488:196609\n21474836492:65538\n21474836496:131075\n21474836500:12\n21474836504:12\n21474836508:395329\n21474836512:12\n";
    EXPECT_EQ(files.second, control_ref);
    EXPECT_EQ(files.first, rom_ref);
}