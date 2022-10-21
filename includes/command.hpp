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


#include <nlohmann/json-schema.hpp>

namespace commands {

    typedef enum {
        null = 0,
        load_bitstream = 1,
        register_write = 2,
        register_read = 4,
        start_capture = 6,
        read_data = 8,
        check_capture = 9,
        set_channel_widths=10,
        apply_program=11,
        set_scaling_factors = 12

    } command_code;

    template <typename command_code>
    auto as_integer(command_code const value)
    -> typename std::underlying_type<command_code>::type
    {
        return static_cast<typename std::underlying_type<command_code>::type>(value);
    }


    static std::unordered_map<command_code, std::string> commands_name_map = {
        {null, "C_NULL_COMMAND"},
        {load_bitstream, "C_LOAD_BITSTREAM"},
        {register_write, "C_SINGLE_REGISTER_WRITE"},
        {register_read, "C_SINGLE_REGISTER_READ"},
        {start_capture, "C_START_CAPTURE"},
        {read_data, "C_READ_DATA"},
        {check_capture, "C_CHECK_CAPTURE_PROGRESS"},
        {set_channel_widths, "C_SET_CHANNEL_WIDTHS"},
        {apply_program, "C_APPLY_PROGRAM"},
        {set_scaling_factors, "C_SET_SCALING_FACTORS"}
    };

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
