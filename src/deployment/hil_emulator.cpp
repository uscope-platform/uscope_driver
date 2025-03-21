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
        emu_manager.set_specs(specs);
        emu_manager.process();
        spdlog::info("COMPILATION DONE");

        emu_manager.emulate();
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

std::unordered_map<std::string, fcore::disassembled_program> hil_emulator::disassemble(nlohmann::json &specs) {
    emu_manager.set_specs(specs);
    return emu_manager.disassemble();
}


std::string hil_emulator::run_command(const interactive_command &c) {
    switch (c.type) {
        case command_initialize_emulation:
            return initialize_emulation(c.spec);
        case command_start_emulation:
            return run_emulation();
        case command_add_breakpoint:
            return add_breakpoint(c.id, c.target_instruction);
        case command_remove_breakpoint:
            return remove_breakpoint(c.id, c.target_instruction);
        case command_step_over:
            return step_over();
        case command_resume_emulation:
            return continue_execution();
        case command_get_breakpoints:
            return get_breakpoints(c.id);
        default:
            throw std::runtime_error("Unknown emulation command");
    }
}

std::string hil_emulator::add_breakpoint(std::string core_id, uint32_t line) {
    emu_manager.add_breakpoint(core_id, line);
    return "ok";
}

std::string hil_emulator::remove_breakpoint(std::string core_id, uint32_t line) {
    emu_manager.remove_breakpoint(core_id, line);
    return "ok";
}

std::string hil_emulator::step_over() {
    nlohmann::json ret;
    ret =  emu_manager.step_over();
    return ret.dump();
}

std::string hil_emulator::continue_execution() {
    auto res = emu_manager.continue_emulation();
    if(res.has_value()){
        nlohmann::json val = res.value();
        return val.dump();
    } else{
        return "{}";
    }
}

std::string hil_emulator::initialize_emulation(const nlohmann::json &specs) {
    emu_manager.set_specs(specs);
    emu_manager.process();
    return "{}";
}

std::string hil_emulator::run_emulation() {
    emu_manager.set_multichannel_debug(options->get_bool_option("multichannel_debug"));
    auto res = emu_manager.emulate();
    if(res.has_value()){
        nlohmann::json val = res.value();
        return val.dump();
    } else {
        nlohmann::json ret;
        ret["status"] = "complete";
        ret["emulation_result"] = emu_manager.get_results();
        return ret.dump();
    };
}

std::string hil_emulator::get_breakpoints(const std::string &core_id) {
    nlohmann::json ret = emu_manager.get_breakpoints(core_id);
    return ret.dump();
}


void to_json(nlohmann::json& j, const emulation_results& p) {
    j = nlohmann::json{ {"results", p.results}, {"results_valid", p.results_valid}, {"duplicates", p.duplicates}, {"code", p.code} };
}


