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
    "version":1,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "input_data": [],
            "inputs": [
                {
                    "name": "input_1",
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
                    },
                    "reg_n": 3,
                    "channel": [
                        0
                    ]
                },
                {
                    "name": "input_2",
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
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true
                    },
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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);

    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x20003,
            0x10004,
            0x30005,
            0xc,
            0xc,
            0x60841,
            0xc,
    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA

    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x50005);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[4].data[0], 0);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[5].data[0], 3);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[6].data[0], 0x41f9999a);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[7].data[0], 1);

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[8].data[0], 4);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'2000);
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

}

TEST(deployer, simple_single_core_integer_input) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":1,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "input_data": [],
            "inputs": [
                {
                    "name": "input_1",
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
                    },
                    "reg_n": 3,
                    "channel": [
                        0
                    ]
                },
                {
                    "name": "input_2",
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
                    "metadata":{
                        "type": "float",
                        "width":16,
                        "signed":true
                    },
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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);

    
    auto ops = ba->get_operations();

    
    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x20003,
            0x10004,
            0x30005,
            0xc,
            0xc,
            0x60841,
            0xc,
    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x50005);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[4].data[0], 0);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[5].data[0], 3);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[6].data[0], 0x41f9999a);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[7].data[0], 1);

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[8].data[0], 4);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[9].data[0], 4);

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

TEST(deployer, simple_single_core_memory_init) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":1,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "input_data": [],
            "inputs": [
            ],
            "outputs": [
                {
                    "name": "out",
                    "type": "float",
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true
                    },
                    "reg_n": [
                        5
                    ]
                }
            ],
            "memory_init": [
                {
                    "name": "mem",
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true
                    },
                    "is_output": true,
                    "reg_n": 4,
                    "value": 14
                },
                {
                    "name": "mem_2",
                    "metadata":{
                        "type": "integer",
                        "width":16,
                        "signed":true
                    },
                    "is_output": true,
                    "reg_n": 3,
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
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);

    
    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x3e0003,
            0x3f0004,
            0x10005,
            0xc,
            0xc,
            0x3f7e1,
            0xc

    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x50005);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[3].data[0], 0x40004);

    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'100c);
    ASSERT_EQ(ops[5].data[0], 0x30003);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'104c);
    ASSERT_EQ(ops[6].data[0], 0x18);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[7].data[0], 3);

    // MEMORIES INITIALIZATION

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'0010);
    ASSERT_EQ(ops[8].data[0], 0x41600000);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'000c);
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

TEST(deployer, multichannel_single_core_deployment) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":1,
    "cores": [
    {
        "id": "test",
        "order": 0,
        "input_data": [],
        "inputs": [
            {
                "name": "input_1",
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
                "metadata":{
                    "type": "float",
                    "width":16,
                    "signed":true
                },
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




    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);

    
    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x20003,
            0x10004,
            0x30005,
            0xc,
            0xc,
            0x60841,
            0xc,
    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x50005);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[3].data[0], 0x3ed1005);

    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'100c);
    ASSERT_EQ(ops[5].data[0], 0x7d52005);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'104c);
    ASSERT_EQ(ops[6].data[0], 0x38);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'1010);
    ASSERT_EQ(ops[7].data[0], 0xbbd3005);

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'1050);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[9].data[0], 4);

    // INPUTS
    ASSERT_EQ(ops[10].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[11].data[0], 3);

    ASSERT_EQ(ops[12].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[12].data[0], 0x41f9999a);

    ASSERT_EQ(ops[13].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[13].data[0], 0x10000);

    ASSERT_EQ(ops[14].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[14].data[0], 0x10003);

    ASSERT_EQ(ops[15].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[15].data[0], 0x4202CCCD);


    ASSERT_EQ(ops[16].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[16].data[0], 0x20000);

    ASSERT_EQ(ops[17].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[17].data[0], 0x20003);

    ASSERT_EQ(ops[18].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[18].data[0], 0x42786666);


    ASSERT_EQ(ops[19].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[19].data[0], 0x30000);

    ASSERT_EQ(ops[20].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[20].data[0], 0x30003);

    ASSERT_EQ(ops[21].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[21].data[0], 0x42800000);



    ASSERT_EQ(ops[22].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[22].data[0], 1);

    ASSERT_EQ(ops[23].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[23].data[0], 4);

    ASSERT_EQ(ops[24].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[24].data[0], 0x40800000);


    ASSERT_EQ(ops[25].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[25].data[0], 0x10001);

    ASSERT_EQ(ops[26].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[26].data[0], 0x10004);

    ASSERT_EQ(ops[27].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[27].data[0], 0x40000000);


    ASSERT_EQ(ops[28].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[28].data[0], 0x20001);

    ASSERT_EQ(ops[29].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[29].data[0], 0x20004);

    ASSERT_EQ(ops[30].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[30].data[0], 0x40C00000);


    ASSERT_EQ(ops[31].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[31].data[0], 0x30001);

    ASSERT_EQ(ops[32].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[32].data[0], 0x30004);

    ASSERT_EQ(ops[33].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[33].data[0], 0x41400000);


    // SEQUENCER

    ASSERT_EQ(ops[34].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[34].data[0], 0);

    ASSERT_EQ(ops[35].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[35].data[0], 2);

    ASSERT_EQ(ops[36].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[36].data[0], 100'000'000);

    ASSERT_EQ(ops[37].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[37].data[0], 1);

    // CORES

    ASSERT_EQ(ops[38].address[0], 0x443c20000);
    ASSERT_EQ(ops[38].data[0],11);

}

TEST(deployer, simple_multi_core_deployment) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":1,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "input_data": [],
            "inputs": [
                {
                    "name": "input_1",
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
                    },
                    "reg_n": 3,
                    "channel": [
                        0
                    ]
                },
                {
                    "name": "input_2",
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
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true
                    },
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
        },
        {
            "id": "test_2",
            "order": 1,
            "input_data": [],
            "inputs": [
                {
                    "name": "input_1",
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
                    },
                    "reg_n": 3,
                    "channel": [
                        0
                    ]
                },
                {
                    "name": "input_2",
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
                    "metadata":{
                        "type": "float",
                        "width":32,
                        "signed":true
                    },
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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);

    
    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x20003,
            0x10004,
            0x30005,
            0xc,
            0xc,
            0x60841,
            0xc,
    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    ASSERT_EQ(ops[1].type, rom_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x5'1000'0000);
    ASSERT_EQ(ops[1].data, reference_program);

    // DMA 1
    ASSERT_EQ(ops[2].type, control_plane_write);
    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[2].data[0], 0x50005);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x38);

    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[4].data[0], 1);

    // DMA 2
    ASSERT_EQ(ops[5].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[5].data[0], 0x10005);

    ASSERT_EQ(ops[6].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[6].data[0], 0x38);

    ASSERT_EQ(ops[7].address[0], 0x4'43c3'1000);
    ASSERT_EQ(ops[7].data[0], 1);

    // INPUTS 1
    ASSERT_EQ(ops[8].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[8].data[0], 0);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[9].data[0], 3);

    ASSERT_EQ(ops[10].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[10].data[0], 0x41f9999a);

    ASSERT_EQ(ops[11].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[11].data[0], 1);

    ASSERT_EQ(ops[12].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[12].data[0], 4);

    ASSERT_EQ(ops[13].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[13].data[0], 0x40800000);


    // INPUTS 2
    ASSERT_EQ(ops[14].address[0], 0x4'43c3'200c);
    ASSERT_EQ(ops[14].data[0], 0);

    ASSERT_EQ(ops[15].address[0], 0x4'43c3'2008);
    ASSERT_EQ(ops[15].data[0], 3);

    ASSERT_EQ(ops[16].address[0], 0x4'43c3'2000);
    ASSERT_EQ(ops[16].data[0], 0x41f9999a);

    ASSERT_EQ(ops[17].address[0], 0x4'43c3'200c);
    ASSERT_EQ(ops[17].data[0], 1);

    ASSERT_EQ(ops[18].address[0], 0x4'43c3'2008);
    ASSERT_EQ(ops[18].data[0], 4);

    ASSERT_EQ(ops[19].address[0], 0x4'43c3'2000);
    ASSERT_EQ(ops[19].data[0], 0x40800000);


    // SEQUENCER

    ASSERT_EQ(ops[20].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[20].data[0], 0);

    ASSERT_EQ(ops[21].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[21].data[0], 2);

    ASSERT_EQ(ops[22].address[0], 0x4'43c1'1008);
    ASSERT_EQ(ops[22].data[0], 0);

    ASSERT_EQ(ops[23].address[0], 0x4'43c1'000C);
    ASSERT_EQ(ops[23].data[0], 0x4e);

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

TEST(deployer, scalar_interconnect_test) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":1,
    "cores": [
        {
            "id": "test",
            "input_data": [],
            "inputs": [
                {
                    "name": "input_1",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":false
                    },
                    "reg_n": 1,
                    "source": {
                        "type": "constant",
                        "value": [
                            31.2
                        ]
                    },
                    "channel": 0
                },
                {
                    "name": "input_2",
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
                    },
                    "reg_n": 2,
                    "channel": 0
                }
            ],
            "outputs": [
                {
                    "reg_n": [
                        4
                    ],
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true
                    },
                    "type": "float",
                    "name": "out"
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
            "inputs": [],
            "input_data": [],
            "outputs": [
                {
                    "type": "float",
                    "reg_n": [
                        5
                    ],
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true
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
            "source": "test",
            "destination": "test_move",
            "channels": [
                {
                    "name": "interconnect_name",
                    "length": 1,
                    "type": "scalar_transfer",
                    "source_output": "out2",
                    "source": {
                        "channel": 0,
                        "register": 6
                    },
                    "destination_input": "input",
                    "destination": {
                        "channel": 0,
                        "register": 1
                    }
                }
            ]
        }
    ],
    "emulation_time": 2,
    "deployment_mode": false
}
)");


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);

    
    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x30005,
            0xc,
            0x20001,
            0x10002,
            0x30004,
            0x10006,
            0xc,
            0xc,
            0x60841,
            0x865,
            0xc

    };


    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x60003,
            0xc,
            0x10001,
            0x10005,
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
    ASSERT_EQ(ops[2].data[0], 0x10006);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x38);


    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[4].data[0], 0x40004);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[6].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[7].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[7].data[0], 0x50005);

    ASSERT_EQ(ops[8].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], 0x4'43c3'1000);
    ASSERT_EQ(ops[9].data[0], 1);

    // INPUTS

    ASSERT_EQ(ops[10].address[0], 0x443c2200c);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], 0x443c22008);
    ASSERT_EQ(ops[11].data[0], 1);

    ASSERT_EQ(ops[12].address[0], 0x443c22000);
    ASSERT_EQ(ops[12].data[0], 0x41f9999a);

    ASSERT_EQ(ops[13].address[0], 0x443c2200c);
    ASSERT_EQ(ops[13].data[0], 1);

    ASSERT_EQ(ops[14].address[0], 0x443c22008);
    ASSERT_EQ(ops[14].data[0], 2);

    ASSERT_EQ(ops[15].address[0], 0x443c22000);
    ASSERT_EQ(ops[15].data[0], 0x4202cccd);
    // SEQUENCER

    ASSERT_EQ(ops[16].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[16].data[0], 0);

    ASSERT_EQ(ops[17].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[17].data[0], 2);

    ASSERT_EQ(ops[18].address[0], 0x4'43c1'1008);
    ASSERT_EQ(ops[18].data[0], 0);

    ASSERT_EQ(ops[19].address[0], 0x4'43c1'000C);
    ASSERT_EQ(ops[19].data[0], 0x59);

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

TEST(deployer, scatter_interconnect_test) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":1,
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
                "input_data":[],
                "inputs":[],
                "outputs":[],
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
                "input_data":[],
                "inputs":[],
                "outputs":[
                    {
                        "name":"out",
                        "type": "float",
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true
                        },
                        "reg_n":[5]
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
                "source": "test_producer",
                "destination": "test_consumer",
                "channels": [
                    {
                        "name": "test_channel",
                        "type": "scatter_transfer",
                        "source": {
                            "channel": 0,
                            "register": 5
                        },
                        "source_output": "out",
                        "destination": {
                            "channel": 0,
                            "register": 1
                        },
                        "destination_input": "input",
                        "length": 2
                    }
                ]
            }
    ],
        "emulation_time": 1,
    "deployment_mode": false
    })");


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);

    
    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x50003,
            0xc,
            0x10005,
            0x20006,
            0xc,
            0xc,
            0x26,
            0x4179999a,
            0x46,
            0x4189999a,
            0xc
    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x40003,
            0xc,
            0x10001,
            0x30005,
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
    ASSERT_EQ(ops[2].data[0], 0x10005);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x38);


    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[4].data[0], 0x10010006);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[5].data[0], 0x38);


    ASSERT_EQ(ops[6].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[6].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[7].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[7].data[0], 0x50005);

    ASSERT_EQ(ops[8].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], 0x4'43c3'1008);
    ASSERT_EQ(ops[9].data[0], 0x3ed1005);

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

TEST(deployer, gather_interconnect_test) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":1,
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
            "input_data":[],
            "inputs":[
                {
                    "name": "input_1",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":true
                    },
                    "reg_n": 3,
                    "channel":[0,1],
                    "source":{"type": "constant","value": [31.2, 32.7]}
                },
                {
                    "name": "input_2",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true,
                        "common_io":true
                    },
                    "reg_n": 4,
                    "channel":[0,1],
                    "source":{"type": "constant","value": [31.2, 32.7]}
                }
            ],
            "outputs":[
                {
                    "name":"out",
                    "type": "float",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true
                    },
                    "reg_n":[5]
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
            "input_data":[],
            "inputs":[],
            "outputs":[
                {
                    "name":"out",
                    "type": "float",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true
                    },
                    "reg_n":[5]
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
            "source": "test_producer",
            "destination": "test_reducer",
            "channels": [
                {
                    "name": "test_channel",
                    "type": "gather_transfer",
                    "source": {
                        "channel": 0,
                        "register": 5
                    },
                    "source_output": "out",
                    "destination": {
                        "channel": 0,
                        "register": 1
                    },
                    "destination_input": "input_data",
                    "length": 2
                }
            ]
        }
],
    "emulation_time": 1,
    "deployment_mode": false
})");


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);

    
    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x20003,
            0x10004,
            0x30005,
            0xc,
            0xc,
            0x60841,
            0xc
    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x20004,
            0xc,
            0x20001,
            0x10002,
            0x30005,
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
    ASSERT_EQ(ops[2].data[0], 0x10005);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x38);


    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[4].data[0], 0x21005);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[6].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[7].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[7].data[0], 0x50005);

    ASSERT_EQ(ops[8].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], 0x4'43c3'1000);
    ASSERT_EQ(ops[9].data[0], 1);

    // INPUTS

    ASSERT_EQ(ops[10].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[10].data[0], 0);

    ASSERT_EQ(ops[11].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[11].data[0], 0x3);

    ASSERT_EQ(ops[12].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[12].data[0], 0x41f9999a);

    ASSERT_EQ(ops[13].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[13].data[0], 0x10000);

    ASSERT_EQ(ops[14].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[14].data[0], 0x10003);

    ASSERT_EQ(ops[15].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[15].data[0], 0x4202CCCD);


    ASSERT_EQ(ops[16].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[16].data[0], 1);

    ASSERT_EQ(ops[17].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[17].data[0], 0x4);

    ASSERT_EQ(ops[18].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[18].data[0], 0x41f9999a);


    ASSERT_EQ(ops[19].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[19].data[0], 0x10001);

    ASSERT_EQ(ops[20].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[20].data[0], 0x10004);

    ASSERT_EQ(ops[21].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[21].data[0], 0x4202CCCD);


    // SEQUENCER

    ASSERT_EQ(ops[22].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[22].data[0], 0);

    ASSERT_EQ(ops[23].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[23].data[0], 2);

    ASSERT_EQ(ops[24].address[0], 0x4'43c1'1008);
    ASSERT_EQ(ops[24].data[0], 0);

    ASSERT_EQ(ops[25].address[0], 0x4'43c1'000C);
    ASSERT_EQ(ops[25].data[0], 78);

    ASSERT_EQ(ops[26].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[26].data[0], 100'000'000);

    ASSERT_EQ(ops[27].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[27].data[0], 3);

    // CORES

    ASSERT_EQ(ops[28].address[0], 0x443c20000);
    ASSERT_EQ(ops[28].data[0],11);

    ASSERT_EQ(ops[29].address[0], 0x443c30000);
    ASSERT_EQ(ops[29].data[0],11);

}

TEST(deployer, vector_interconnect_test) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
        "version":1,
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
                "input_data":[],
                "inputs":[
                    {
                        "name": "input_1",
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io":true
                        },
                        "reg_n": 3,
                        "channel":[0,1],
                        "source":{"type": "constant","value": [31.2, 32.7]}
                    },
                    {
                        "name": "input_2",
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true,
                            "common_io":true
                        },
                        "reg_n": 4,
                        "channel":[0,1],
                        "source":{"type": "constant","value": [31.2, 32.7]}
                    }
                ],
                "outputs":[],
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
                "input_data":[],
                "inputs":[],
                "outputs":[
                    {
                        "name":"out",
                        "type": "float",
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": true
                        },
                        "reg_n":[7]
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
                "source": "test_producer",
                "destination": "test_consumer",
                "channels": [
                    {
                        "name": "test_channel",
                        "type": "vector_transfer",
                        "source": {
                            "channel": 0,
                            "register": 5
                        },
                        "source_output": "out",
                        "destination": {
                            "channel": 0,
                            "register": 1
                        },
                        "destination_input": "input",
                        "length": 2
                    }
                ]
            }
        ],
        "emulation_time": 1,
    "deployment_mode": false
    })");



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);

    
    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x20003,
            0x10004,
            0x30005,
            0xc,
            0xc,
            0x60841,
            0xc

    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x40003,
            0xc,
            0x10001,
            0x30007,
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
    ASSERT_EQ(ops[2].data[0], 0x10005);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x38);


    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[4].data[0], 0x10011005);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[6].data[0], 2);

    // DMA 2
    ASSERT_EQ(ops[7].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[7].data[0], 0x70007);

    ASSERT_EQ(ops[8].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[8].data[0], 0x38);

    ASSERT_EQ(ops[9].address[0], 0x4'43c3'1008);
    ASSERT_EQ(ops[9].data[0], 0x3ef1007);

    ASSERT_EQ(ops[10].address[0], 0x4'43c3'1048);
    ASSERT_EQ(ops[10].data[0], 0x38);

    ASSERT_EQ(ops[11].address[0], 0x4'43c3'1000);
    ASSERT_EQ(ops[11].data[0], 2);

    // INPUTS

    ASSERT_EQ(ops[12].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[12].data[0], 0);

    ASSERT_EQ(ops[13].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[13].data[0], 0x3);

    ASSERT_EQ(ops[14].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[14].data[0], 0x41f9999a);


    ASSERT_EQ(ops[15].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[15].data[0], 0x10000);

    ASSERT_EQ(ops[16].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[16].data[0], 0x10003);

    ASSERT_EQ(ops[17].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[17].data[0], 0x4202CCCD);


    ASSERT_EQ(ops[18].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[18].data[0], 0x1);

    ASSERT_EQ(ops[19].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[19].data[0], 0x4);

    ASSERT_EQ(ops[20].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[20].data[0], 0x41f9999a);


    ASSERT_EQ(ops[21].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[21].data[0], 0x10001);

    ASSERT_EQ(ops[22].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[22].data[0], 0x10004);

    ASSERT_EQ(ops[23].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[23].data[0], 0x4202CCCD);

    // SEQUENCER

    ASSERT_EQ(ops[24].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[24].data[0], 0);

    ASSERT_EQ(ops[25].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[25].data[0], 2);

    ASSERT_EQ(ops[26].address[0], 0x4'43c1'1008);
    ASSERT_EQ(ops[26].data[0], 0);

    ASSERT_EQ(ops[27].address[0], 0x4'43c1'000C);
    ASSERT_EQ(ops[27].data[0], 78);

    ASSERT_EQ(ops[28].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[28].data[0], 100'000'000);

    ASSERT_EQ(ops[29].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[29].data[0], 3);

    // CORES

    ASSERT_EQ(ops[30].address[0], 0x443c20000);
    ASSERT_EQ(ops[30].data[0],11);

    ASSERT_EQ(ops[31].address[0], 0x443c30000);
    ASSERT_EQ(ops[31].data[0],11);

}

TEST(deployer, 2d_vector_interconnect_test) {


    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
        "version":1,
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
                "input_data":[],
                "inputs":[],
                "outputs":[],
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
                "input_data":[],
                "inputs":[],
                "outputs":[
                    {
                        "name":"consumer_out",
                        "type": "float",
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": false
                        },
                        "reg_n":[7,8]
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
                "source": "test_producer",
                "destination": "test_consumer",
                "channels": [
                    {
                        "name": "test_channel",
                        "type": "2d_vector_transfer",
                        "source": {
                            "channel": 0,
                            "register": [5,6]
                        },
                        "source_output": "out",
                        "destination": {
                            "channel": 0,
                            "register": [1,2]
                        },
                        "destination_input": "input",
                        "length": 2,
                        "stride": 2
                    }
                ]
            }
        ],
        "emulation_time": 2,
    "deployment_mode": false
    })");


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);

    
    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x50003,
            0xc,
            0x10005,
            0x20006,
            0xc,
            0xc,
            0x26,
            0x4179999a,
            0x46,
            0x4189999a,
            0xc
    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    reference_program = {
            0x50005,
            0xc,
            0x20001,
            0x10002,
            0x40007,
            0x20008,
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
    ASSERT_EQ(ops[2].data[0], 0x10005);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[3].data[0], 0x38);

    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[4].data[0], 0x10011005);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[5].data[0], 0x38);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'100c);
    ASSERT_EQ(ops[6].data[0], 0x20006);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'104c);
    ASSERT_EQ(ops[7].data[0], 0x38);

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'1010);
    ASSERT_EQ(ops[8].data[0], 0x10021006);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'1050);
    ASSERT_EQ(ops[9].data[0], 0x38);

    ASSERT_EQ(ops[10].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[10].data[0], 4);

    // DMA 2
    ASSERT_EQ(ops[11].address[0], 0x4'43c3'1004);
    ASSERT_EQ(ops[11].data[0], 0x70007);

    ASSERT_EQ(ops[12].address[0], 0x4'43c3'1044);
    ASSERT_EQ(ops[12].data[0], 0x38);

    ASSERT_EQ(ops[13].address[0], 0x4'43c3'1008);
    ASSERT_EQ(ops[13].data[0], 0x3ef1007);

    ASSERT_EQ(ops[14].address[0], 0x4'43c3'1048);
    ASSERT_EQ(ops[14].data[0], 0x38);

    ASSERT_EQ(ops[15].address[0], 0x4'43c3'100c);
    ASSERT_EQ(ops[15].data[0], 0x80008);

    ASSERT_EQ(ops[16].address[0], 0x4'43c3'104c);
    ASSERT_EQ(ops[16].data[0], 0x38);

    ASSERT_EQ(ops[17].address[0], 0x4'43c3'1010);
    ASSERT_EQ(ops[17].data[0], 0x3f01008);

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

TEST(deployer, simple_single_core_output_select) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":1,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "input_data": [],
            "inputs": [
                {
                    "name": "input_1",
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
                    },
                    "reg_n": 3,
                    "channel": [
                        0
                    ]
                },
                {
                    "name": "input_2",
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
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true
                    },
                    "reg_n": [
                        5
                    ]
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
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);
    output_specs_t out;
    out.address = 5,
    out.channel = 1,
    out.core_name = "test";
    out.source_output = "out";
    d.select_output(0, out);


    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x20003,
            0x10004,
            0x30005,
            0xc,
            0xc,
            0x60841,
            0xc,
    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x50005);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1008);
    ASSERT_EQ(ops[3].data[0], 0x3ed1005);

    ASSERT_EQ(ops[4].address[0], 0x4'43c2'1048);
    ASSERT_EQ(ops[4].data[0], 0x38);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[5].data[0], 2);

    // INPUTS
    ASSERT_EQ(ops[6].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[6].data[0], 0);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[7].data[0], 3);

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[8].data[0], 0x41f9999a);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[9].data[0], 1);

    ASSERT_EQ(ops[10].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[10].data[0], 4);

    ASSERT_EQ(ops[11].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[11].data[0], 0x40800000);

    // SEQUENCER

    ASSERT_EQ(ops[12].address[0], 0x4'43c1'1004);
    ASSERT_EQ(ops[12].data[0], 0);

    ASSERT_EQ(ops[13].address[0], 0x4'43c1'0008);
    ASSERT_EQ(ops[13].data[0], 2);

    ASSERT_EQ(ops[14].address[0], 0x4'43c1'0004);
    ASSERT_EQ(ops[14].data[0], 100'000'000);

    ASSERT_EQ(ops[15].address[0], 0x4'43c1'1000);
    ASSERT_EQ(ops[15].data[0], 1);

    // CORES

    ASSERT_EQ(ops[16].address[0], 0x443c20000);
    ASSERT_EQ(ops[16].data[0],11);

    // select_output
    ASSERT_EQ(ops[17].address[0], 0x443c50004);
    ASSERT_EQ(ops[17].data[0],0x3ed);

}

TEST(deployer, simple_single_core_input_set) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":1,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "input_data": [],
            "inputs": [
                {
                    "name": "input_1",
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
                    },
                    "reg_n": 3,
                    "channel": [
                        0
                    ]
                },
                {
                    "name": "input_2",
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
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true
                    },
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


    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);
    d.set_input(4,90,"test");

    
    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x20003,
            0x10004,
            0x30005,
            0xc,
            0xc,
            0x60841,
            0xc,
    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x50005);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[4].data[0], 0);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[5].data[0], 3);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[6].data[0], 0x41f9999a);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[7].data[0], 1);

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[8].data[0], 4);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'2000);
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

    ASSERT_EQ(ops[15].address[0], 0x443c2200c);
    ASSERT_EQ(ops[15].data[0],1);

    ASSERT_EQ(ops[16].address[0], 0x443c22008);
    ASSERT_EQ(ops[16].data[0],4);

    ASSERT_EQ(ops[17].address[0], 0x443c22000);
    ASSERT_EQ(ops[17].data[0],90);

}

TEST(deployer, simple_single_core_start_stop) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":1,
    "cores": [
        {
            "id": "test",
            "order": 0,
            "input_data": [],
            "inputs": [
                {
                    "name": "input_1",
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
                    },
                    "reg_n": 3,
                    "channel": [
                        0
                    ]
                },
                {
                    "name": "input_2",
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
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": true
                    },
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



    auto ba = std::make_shared<bus_accessor>(true);
    hil_deployer d;
    d.set_accessor(ba);
    auto addr_map = get_addr_map();
    d.set_layout_map(addr_map);
    d.deploy(spec_json);
    d.start();
    d.stop();

    
    auto ops = ba->get_operations();

    std::vector<uint64_t> reference_program = {
            0x20004,
            0xc,
            0x20003,
            0x10004,
            0x30005,
            0xc,
            0xc,
            0x60841,
            0xc,
    };

    ASSERT_EQ(ops[0].type, rom_plane_write);
    ASSERT_EQ(ops[0].address[0], 0x5'0000'0000);
    ASSERT_EQ(ops[0].data, reference_program);

    // DMA
    ASSERT_EQ(ops[1].type, control_plane_write);
    ASSERT_EQ(ops[1].address[0], 0x4'43c2'1004);
    ASSERT_EQ(ops[1].data[0], 0x50005);

    ASSERT_EQ(ops[2].address[0], 0x4'43c2'1044);
    ASSERT_EQ(ops[2].data[0], 0x38);

    ASSERT_EQ(ops[3].address[0], 0x4'43c2'1000);
    ASSERT_EQ(ops[3].data[0], 1);

    // INPUTS
    ASSERT_EQ(ops[4].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[4].data[0], 0);

    ASSERT_EQ(ops[5].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[5].data[0], 3);

    ASSERT_EQ(ops[6].address[0], 0x4'43c2'2000);
    ASSERT_EQ(ops[6].data[0], 0x41f9999a);

    ASSERT_EQ(ops[7].address[0], 0x4'43c2'200c);
    ASSERT_EQ(ops[7].data[0], 1);

    ASSERT_EQ(ops[8].address[0], 0x4'43c2'2008);
    ASSERT_EQ(ops[8].data[0], 4);

    ASSERT_EQ(ops[9].address[0], 0x4'43c2'2000);
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

    ASSERT_EQ(ops[15].address[0], 0x4'43c0'0000);
    ASSERT_EQ(ops[15].data[0],1);

    ASSERT_EQ(ops[16].address[0], 0x4'43c0'0000);
    ASSERT_EQ(ops[16].data[0],0);

}