//   Copyright 2026 Filippo Savi <filssavi@gmail.com>
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

#ifndef USCOPE_DRIVER_TEST_EMULATION_SPECS_HPP
#define USCOPE_DRIVER_TEST_EMULATION_SPECS_HPP

#include <string_view>

/// One core that counts: its output is 1, 2, 3... one more in every round
constexpr std::string_view counter_spec = R"(
    {
      "version": 2,
      "cores": [
        {
          "id": "test",
          "inputs": [],
          "order": 1,
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
          "program":{
            "content": "void main(){float mem; mem += 1.0; float out = mem*1.0;}",
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
              "is_output": false,
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
        }
      ],
      "interconnect":[],
      "emulation_time": 1,
      "deployment_mode": false
    }
)";

/// One core with a constant input, its output is twice the input (the input starts at 1)
constexpr std::string_view constant_spec = R"(
    {
      "version": 2,
      "cores": [
        {
          "id": "test",
          "inputs": [
            {
              "name": "in_c",
              "is_vector": false,
              "metadata": {
                "type": "float",
                "width": 32,
                "signed": true,
                "common_io": false
              },
              "source": {"type": "constant", "value": [1]}
            }
          ],
          "order": 1,
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
          "program":{
            "content": "void main(){float in_c; float out = in_c*2.0;}",
            "headers": []
          },
          "memory_init": [],
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
      "interconnect":[],
      "emulation_time": 1,
      "deployment_mode": false
    }
)";

#endif //USCOPE_DRIVER_TEST_EMULATION_SPECS_HPP
