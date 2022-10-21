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

#ifndef USCOPE_DRIVER_COMMAND_HPP
#define USCOPE_DRIVER_COMMAND_HPP

#include <cstdint>
#include <string>
#include <unordered_map>

/// Command with no effects
/// EXPECTED RESPONSE TYPE: #RESP_NOT_NEEDED
#define C_NULL_COMMAND 0

/// Load a bitstream by name
/// FORMAT: C_LOAD_BITSTREAM name
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_LOAD_BITSTREAM 1

/// Write to a single register
/// FORMAT: C_SINGLE_REGISTER_WRITE address value
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_SINGLE_REGISTER_WRITE 2

/// Write to multiple registers in bulk
/// FORMAT: C_BULK_REGISTER_WRITE address_1, address_2,... value_1, value_2,...
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_BULK_REGISTER_WRITE 3

/// Read from a single register
/// FORMAT: C_SINGLE_REGISTER_READ address
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_SINGLE_REGISTER_READ 4

/// Start capture mode
/// FORMAT: C_START_CAPTURE n_buffers
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_START_CAPTURE 6


/// Read captured data
/// FORMAT: C_READ_DATA
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_OUTBAND
#define C_READ_DATA 8

/// Check how many buffers are there left to capture
/// FORMAT: C_CHECK_CAPTURE_PROGRESS
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_CHECK_CAPTURE_PROGRESS 9


/// Set the size of each channel in bits
/// FORMAT: C_SET_CHANNEL_WIDTHS ch_1_width,ch2_width,...
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_SET_CHANNEL_WIDTHS 10


/// Load a program into the specified fCore instance memory
/// FORMAT: C_APPLY_PROGRAM fCore_address program_instruction_1, program_instruction_2, ...
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_APPLY_PROGRAM 11


/// Set the size of each channel in bits
/// FORMAT: C_SET_SCALING_FACTORS ch_1_sf,ch_1_sf,...
/// EXPECTED RESPONSE TYPE: #RESP_TYPE_INBAND
#define C_SET_SCALING_FACTORS 12


static std::unordered_map<uint32_t , std::string> command_map = {
        {0, "C_NULL_COMMAND"},
        {1, "C_LOAD_BITSTREAM"},
        {2, "C_SINGLE_REGISTER_WRITE"},
        {3, "C_BULK_REGISTER_WRITE"},
        {4, "C_SINGLE_REGISTER_READ"},
        {6, "C_START_CAPTURE"},
        {8, "C_READ_DATA"},
        {9, "C_CHECK_CAPTURE_PROGRESS"},
        {10, "C_SET_CHANNEL_WIDTHS"},
        {11, "C_APPLY_PROGRAM"},
        {12, "C_SET_SCALING_FACTORS"}

};


#include <nlohmann/json-schema.hpp>

namespace json_specs {

    static nlohmann::json command = R"(
    {
        "$schema": "https://json-schema.org/draft/2019-09/schema",
        "title": "Command Schema",
        "properties": {
            "cmd": {
                "type": "integer",
                "default": 0,
                "title": "The cmd Schema",
                "examples": [
                    1
                ]
            },
            "args": {
                "oneOf": [
                    {
                        "type": "object",
                        "title": "Relevant arguments for the specified command"
                    },
                    {
                        "type": "string",
                        "title": "Relevant arguments for the specified command"
                    },
                    {
                        "type": "number",
                        "title": "Relevant arguments for the specified command"
                    },
                    {
                        "type": "array",
                        "title": "Relevant arguments for the specified command"
                    }
                  ]
            }
        },
        "required": [
            "cmd",
            "args"
        ],
        "type": "object"
    }
    )"_json;

    static nlohmann::json  load_program = R"(
    {
        "$schema": "https://json-schema.org/draft/2019-09/schema",
        "title": "Load program schema",
        "properties": {
            "address": {
                "type": "integer",
                "title": "Address of the core to load"
            },
            "program": {
                "type": "array",
                "title": "Program to load",
                "items": {
                    "type": "integer"
                }
            }
        },
        "required": [
            "address",
            "program"
        ],
        "type": "object"
    }
    )"_json;

    static nlohmann::json  write_register = R"(
        {
            "$schema": "https://json-schema.org/draft/2019-09/schema",
            "title": "Write register schema",
            "properties": {
                "type": {
                    "type": "string",
                    "enum": [
                        "proxied",
                        "direct"
                    ],
                    "title": "Type of register write"
                },
                "proxy_type": {
                    "type": "string",
                    "title": "Type of proxied write"
                },
                "proxy_address": {
                    "type": "integer",
                    "title": "Address of the write proxy"
                },
                "address": {
                    "type": "integer",
                    "title": "Address of the register to write to"
                },
                "value": {
                    "type": "integer",
                    "title": "Value to write to the desired register"
                }
            },
            "required": [
                "type",
                "address",
                "value"
            ],
            "type": "object",
            "if": {
                "properties": {
                    "type": {
                        "enum": [
                            "proxied"
                        ]
                    }
                }
            },
            "then": {
                "required": [
                    "proxy_address",
                    "proxy_type"
                ]
            }
        }
    )"_json;

    static bool validate_schema(nlohmann::json &cmd, nlohmann::json &schema, std::string &error){
        nlohmann::json_schema::json_validator validator;
        try {
            validator.set_root_schema(schema);
            validator.validate(cmd);
        } catch (const std::exception &e) {
            error = e.what();
            return false;
        }
        return true;
    };
}


#endif //USCOPE_DRIVER_COMMAND_HPP
