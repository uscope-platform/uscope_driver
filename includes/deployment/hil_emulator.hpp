

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

#include "emulator/emulator_manager.hpp"

struct emulation_results{
    std::string results;
    bool results_valid;
    std::string duplicates;
    int code;
};

typedef enum {
    add_breakpoint=1,
    remove_breakpoint=2,
    step_over=3,
    continue_emulation=4
}command_type;

struct interactive_command{
    command_type type;
    uint32_t target_instruction;
};



void to_json(nlohmann::json& j, const emulation_results& p);

class hil_emulator {
public:
    void start_interactive_session(nlohmann::json &specs);
    std::string run_command(const interactive_command &c);
    emulation_results emulate(nlohmann::json &specs);
    std::unordered_map<std::string, std::string> disassemble(nlohmann::json &specs);
private:
};


#endif //USCOPE_DRIVER_HIL_EMULATOR_HPP
