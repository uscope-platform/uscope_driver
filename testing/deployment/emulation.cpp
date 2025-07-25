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
#include "deployment/hil_emulator.hpp"

#include "../deployment/fpga_bridge_mock.hpp"



TEST(emulator, simple_emulation) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
        "version":2,
        "cores": [
            {
                "id": "test_producer",
                "order": 1,
                "inputs": [],
                "outputs": [
                    {
                        "name": "out",
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": false,
                            "common_io":false
                        },
                        "is_vector":true,
                        "vector_size":2
                    }
                ],
                "memory_init": [
                    {
                        "name": "mem",
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": false
                        },
                        "is_output": true,
                        "is_input": false,
                        "is_vector":false,
                        "vector_size":1,
                        "value": [
                            0
                        ]
                    }
                ],
                "deployment": {
                    "control_address": 0,
                    "has_reciprocal": false,
                    "rom_address": 0
                },
                "channels": 1,
                "program": {
                    "content": "int main(){\n  float mem;\n\n  mem = mem  + 1.0;\n  mem = fti(mem);\n  mem = mem & 0x3ff;\n  mem = itf(mem);\n  \n  float out[2];\n  out[0] = mem + 100.0;\n  out[1] = mem + 1000.0;\n}",
                    "headers": []
                },
                "options": {
                    "comparators": "full",
                    "efi_implementation": "none"
                },
                "sampling_frequency": 2
            }
        ],
        "interconnect": [
        ],
        "emulation_time": 1,
        "deployment_mode": false
    })");

    hil_emulator emu;
    nlohmann::json results = emu.emulate(spec_json);


    nlohmann::json  expected_result;
    expected_result["code"] = 1;
    expected_result["duplicates"] = "";
    expected_result["results_valid"] = true;
    nlohmann::json prod_res;
    prod_res["timebase"] = {0.0, 0.5};
    prod_res["test_producer"]["error_code"] = "";
    prod_res["test_producer"]["outputs"] =  nlohmann::json();
    prod_res["test_producer"]["outputs"]["mem"] =  nlohmann::json();
    prod_res["test_producer"]["outputs"]["mem"]["0"] = {{1.0,2.0}};
    prod_res["test_producer"]["outputs"]["out"] =  nlohmann::json();
    prod_res["test_producer"]["outputs"]["out"]["0"] = {{101.0,102.0}, {1001.0, 1002.0}};
    expected_result["results"] = prod_res.dump();

    EXPECT_EQ(results, expected_result);
}


TEST(emulator, disassembly) {

    nlohmann::json spec_json = nlohmann::json::parse(
            R"({
    "version":2,
    "cores": [
        {
            "order": 1,
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
                    "metadata":{
                        "type": "float",
                        "width": 24,
                        "signed":false,
                        "common_io": false
                    },
                    "is_vector":false,
                    "vector_size":1,
                    "source":{"type": "constant","value": [31.2, 32.7]}
                },
                {
                    "name": "input_2",
                    "metadata":{
                        "type": "float",
                        "width": 24,
                        "signed":false,
                        "common_io": false
                    },
                    "is_vector":false,
                    "vector_size":1,
                    "source":{"type": "constant","value": [31.2, 32.7]}
                }
            ],
            "outputs":[
                {
                    "name":"out",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false,
                        "common_io": false
                    },
                    "is_vector":false,
                    "vector_size":1
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
            "order": 2,
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
                    "metadata":{
                        "type": "float",
                        "width": 24,
                        "signed":false,
                        "common_io": false
                    },
                    "source":{"type": "constant","value": [31.2, 32.7]},
                    "is_vector":true,
                    "vector_size":2
                }
            ],
            "outputs":[
                {
                    "name":"out",
                    "metadata": {
                        "type": "float",
                        "width": 32,
                        "signed": false,
                        "common_io": false
                    },
                    "is_vector":false,
                    "vector_size":1
                }
            ],
            "memory_init":[],
            "program": {
                "content": "int main(){\n    float input_data[2];\n    float out = input_data[0] * input_data[1];\n}\n",
                "headers": []
            },
            "deployment": {
                "has_reciprocal": false,
                "control_address": 18316525568,
                "rom_address": 17179869184
            }
        }
    ],
    "interconnect": [],
    "emulation_time": 1,
    "deployment_mode": false
})");

    hil_emulator emu;
    auto res = emu.disassemble(spec_json);

    std::unordered_map<std::string, fcore::disassembled_program> check_map;

    check_map["test_producer"] = {{{2,3},{3,1},{4,2}},"add r2, r1, r3\nstop\n"};
    check_map["test_reducer"] = {{{1,3}, {3,2}, {4,1}},"mul r2, r1, r3\nstop\n"};


    EXPECT_EQ(check_map["test_producer"].translation_table, res["test_producer"].translation_table);
    EXPECT_EQ(check_map["test_producer"].program, res["test_producer"].program);
    EXPECT_EQ(check_map["test_reducer"].translation_table, res["test_reducer"].translation_table);
    EXPECT_EQ(check_map["test_reducer"].program, res["test_reducer"].program);

}

