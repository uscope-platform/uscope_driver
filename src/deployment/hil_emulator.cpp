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

#include "deployment/hil_emulator.hpp"

emulation_results hil_emulator::emulate(nlohmann::json &specs) {
    emulation_results ret_val;
    try{
        spdlog::info("EMULATE HIL");
        fcore::emulator_manager emu_manager;
        emu_manager.set_specs(specs);
        emu_manager.process();
        spdlog::info("COMPILATION DONE");
        emu_manager.emulate(false);
        auto results = emu_manager.get_results();
        spdlog::info("EMULATION RESULTS AVAILABLE");
        ret_val.results = results.dump();
        ret_val.results_valid = true;
        ret_val.code = 1;
    } catch (std::domain_error &e) {
        ret_val.results ="HIL BUS CONFLICT DETECTED\n";
        ret_val.results_valid = false;
        ret_val.duplicates = e.what();
        ret_val.code = 9;
    } catch (std::runtime_error &e) {
        ret_val.code = 7;
        ret_val.results = std::string("EMULATION ERROR:\n") + e.what();
        ret_val.results_valid = false;
    }
    return ret_val;
}

std::unordered_map<std::string, std::string> hil_emulator::disassemble(nlohmann::json &specs) {
    fcore::emulator_manager emu_manager;
    emu_manager.set_specs(specs);
    return emu_manager.disassemble();
}

void hil_emulator::start_interactive_session(nlohmann::json &specs) {

}

std::string hil_emulator::run_command(const interactive_command &c) {
    switch (c.type) {
        case command_add_breakpoint:
            return add_breakpoint(c.id, c.target_instruction);
        case command_remove_breakpoint:
            return remove_breakpoint(c.id, c.target_instruction);
        case command_step_over:
            return step_over();
        case command_resume_emulation:
            return continue_execution();
        default:
            throw std::runtime_error("Unknown emulation command");
    }
}

std::string hil_emulator::add_breakpoint(std::string core_id, uint32_t line) {
    return std::string();
}

std::string hil_emulator::remove_breakpoint(std::string core_id, uint32_t line) {
    return std::string();
}

std::string hil_emulator::step_over() {
    return std::string();
}

std::string hil_emulator::continue_execution() {
    return std::string();
}

void to_json(nlohmann::json& j, const emulation_results& p) {
j = nlohmann::json{ {"results", p.results}, {"results_valid", p.results_valid}, {"duplicates", p.duplicates}, {"code", p.code} };
}


