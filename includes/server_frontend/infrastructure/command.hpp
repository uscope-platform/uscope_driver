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

#ifndef USCOPE_DRIVER_COMMAND_HPP
#define USCOPE_DRIVER_COMMAND_HPP

#include <cstdint>
#include <string>
#include <unordered_map>

#include "schema_validator.h"

namespace commands {

    static std::set<std::string> infrastructure_commands = {"null"};

    static std::set<std::string> control_commands = {"load_bitstream", "register_write", "register_read",
                                              "apply_filter"};

    static std::set<std::string> scope_commands = {"read_data", "set_scaling_factors", "set_channel_status",
                                            "set_scaling_factors", "set_channel_status", "disable_scope_dma"
                                             "get_acquisition_status", "set_acquisition", "set_scope_address"};

    static std::set<std::string> core_commands = {"apply_program", "deploy_hil", "emulate_hil", "compile_program",
                                                  "hil_select_out", "hil_set_in", "hil_start", "hil_stop",
                                                    "set_layout_map", "set_hil_address_map", "get_hil_address_map"};


    static std::set<std::string> platform_commands = {"set_pl_clock", "get_clock", "get_version", "set_debug_level",
                                                      "get_debug_level"};


    static nlohmann::json command = R"(
    {
        "$schema": "https://json-schema.org/draft/2019-09/schema",
        "title": "Command Schema",
        "properties": {
            "cmd": {
                "type": "string",
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


    static nlohmann::json  compile_program_schema = R"(

{
    "$schema": "https://json-schema.org/draft/2019-09/schema",
    "title": "Compile program schema",
    "properties": {
        "content": {
            "type": "string",
            "title": "content of the program to compile"
        },
        "headers": {
            "type": "array",
            "title": "Headers to compile with the program",
            "items": {
                "type": "object",
                "properties": {
                    "content": {
                        "title": "Content of the header",
                        "type": "string"
                    },
                    "name": {
                        "title": "Name of the header",
                        "type": "string"
                    }
                },
                "required": [
                    "content",
                    "name"
                ]
            }
        },
        "io": {
            "type": "array",
            "items": [
                {
                    "type": "object",
                    "properties":{
                        "address":{
                            "title": "IO address to use",
                            "type":"integer"
                        },
                        "associated_io":{
                            "title": "name of the associated variable",
                            "type":"string"
                        },
                        "type":{
                            "title": "Type of the io",
                            "type":"string"
                        }

                    }
                }
            ]
        }
    },
    "required": [
        "content",
        "headers",
        "io"
    ],
    "type": "object"
}
    )"_json;


    static nlohmann::json  apply_filter_schema = R"(
    {
        "$schema": "https://json-schema.org/draft/2019-09/schema",
        "title": "Apply filter schema",
        "properties": {
            "address": {
                "type": "integer",
                "title": "Address of the peripheral to load"
            },
            "taps": {
                "type": "array",
                "title": "filter taps",
                "items": {
                    "type": "integer"
                }
            }
        },
        "required": [
            "address",
            "taps"
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
        schema_validator sv(schema);
        return sv.validate(cmd, error);
    };
}


#endif //USCOPE_DRIVER_COMMAND_HPP
