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
        "cores": [
            {
                "id": "test_producer",
                "order": 1,
                "input_data": [],
                "inputs": [],
                "outputs": [
                    {
                        "name": "out",
                        "metadata": {
                            "type": "float",
                            "width": 32,
                            "signed": false
                        },
                        "reg_n": [
                            5,6
                        ]
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
                        "reg_n": [
                            32
                        ],
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
                    "build_settings": {
                        "io": {
                            "inputs": [],
                            "memories": [
                                "mem"
                            ],
                            "outputs": [
                                "out"
                            ]
                        }
                    },
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
    auto results = emu.emulate(spec_json);

    std::string expected_result = "{\"test_producer\":{\"error_code\":\"\",\"outputs\":{\"mem\":{\"0\":[[1.0,2.0]]},\"out\":{\"0\":[[101.0,102.0],[1001.0,1002.0]]}}},\"timebase\":[0.0]}";
    EXPECT_EQ(results, expected_result);
}