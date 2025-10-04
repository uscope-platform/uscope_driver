

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

#ifndef USCOPE_DRIVER_HIL_EMULATOR_HPP
#define USCOPE_DRIVER_HIL_EMULATOR_HPP

#include <nlohmann/json.hpp>
#include <iostream>
#include <spdlog/spdlog.h>
#include <cstdint>
#include <utility>

#include "emulator/emulator_dispatcher.hpp"

struct emulation_results{
    std::string results;
    bool results_valid;
    std::string duplicates;
    int code;
};

typedef enum {
    command_add_breakpoint=1,
    command_remove_breakpoint=2,
    command_step_over=3,
    command_resume_emulation=4,
    command_initialize_emulation=5,
    command_start_emulation=6,
    command_get_breakpoints=7
}command_type;

struct interactive_command{
    command_type type;
    std::string id;
    uint32_t target_instruction;
    nlohmann::json spec;
};



void to_json(nlohmann::json& j, const emulation_results& p);

class hil_emulator {
public:
    std::string run_command(const interactive_command &c);
    emulation_results emulate(const nlohmann::json &specs);
    std::unordered_map<std::string, fcore::disassembled_program> disassemble(const nlohmann::json &specs);
private:
    fcore::emulator_dispatcher emu_manager;
    std::string initialize_emulation(const nlohmann::json &specs);
    std::string run_emulation();
    std::string add_breakpoint(std::string core_id, uint32_t line);
    std::string remove_breakpoint(std::string core_id, uint32_t line);
    std::string step_over();
    std::string get_breakpoints(const std::string &core_id);
    std::string continue_execution();


};


#endif //USCOPE_DRIVER_HIL_EMULATOR_HPP
