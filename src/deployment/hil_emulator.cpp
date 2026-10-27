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

emulation_results hil_emulator::emulate(const nlohmann::json &specs) {
    emulation_results ret_val;
    auto fail_with = [&ret_val](int code, const fcore::error &err) {
        ret_val.code = code;
        ret_val.results = std::string("EMULATION ERROR:\n") + err.what();
        ret_val.results_valid = false;
        return ret_val;
    };

    spdlog::info("EMULATE HIL");
    if(auto r = emu_manager.set_specs(specs); !r) return fail_with(7, r.error());
    if(auto r = emu_manager.set_multichannel_debug(multichannel_debug); !r) return fail_with(7, r.error());
    // the errors of the compilation are reported with their own code
    if(auto r = emu_manager.process(); !r) return fail_with(8, r.error());
    spdlog::info("COMPILATION DONE");

    if(auto r = emu_manager.emulate(); !r) return fail_with(7, r.error());
    auto results = emu_manager.get_results();
    if(!results) return fail_with(7, results.error());

    spdlog::info("EMULATION RESULTS AVAILABLE");
    ret_val.results = results.value().dump();
    ret_val.results_valid = true;
    ret_val.code = 1;
    return ret_val;
}

fcore::result<std::map<std::string, fcore::disassembled_program>> hil_emulator::disassemble(const nlohmann::json &specs) {
    if(auto r = emu_manager.set_specs(specs); !r) return fcore::fail(std::move(r).error());
    return emu_manager.disassemble();
}


fcore::result<std::string> hil_emulator::run_command(const interactive_command &c) {
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
            return fcore::fail(fcore::error_code::invalid_argument, "Unknown emulation command");
    }
}

fcore::result<std::string> hil_emulator::add_breakpoint(std::string core_id, uint32_t line) {
    if(auto r = emu_manager.add_breakpoint(core_id, line); !r) return fcore::fail(std::move(r).error());
    return "ok";
}

fcore::result<std::string> hil_emulator::remove_breakpoint(std::string core_id, uint32_t line) {
    if(auto r = emu_manager.remove_breakpoint(core_id, line); !r) return fcore::fail(std::move(r).error());
    return "ok";
}

fcore::result<std::string> hil_emulator::step_over() {
    auto checkpoint = emu_manager.step_over();
    if(!checkpoint) return fcore::fail(std::move(checkpoint).error());
    nlohmann::json ret = checkpoint.value();
    return ret.dump();
}

fcore::result<std::string> hil_emulator::continue_execution() {
    auto res = emu_manager.continue_emulation();
    if(!res) return fcore::fail(std::move(res).error());
    if(res.value().has_value()){
        nlohmann::json val = res.value().value();
        return val.dump();
    } else{
        return "{}";
    }
}

fcore::result<std::string> hil_emulator::initialize_emulation(const nlohmann::json &specs) {
    if(auto r = emu_manager.set_specs(specs); !r) return fcore::fail(std::move(r).error());
    if(auto r = emu_manager.process(); !r) return fcore::fail(std::move(r).error());
    return "{}";
}

fcore::result<std::string> hil_emulator::run_emulation() {
    if(auto r = emu_manager.set_multichannel_debug(false); !r) return fcore::fail(std::move(r).error());
    auto res = emu_manager.emulate();
    if(!res) return fcore::fail(std::move(res).error());
    if(res.value().has_value()){
        nlohmann::json val = res.value().value();
        return val.dump();
    } else {
        auto results = emu_manager.get_results();
        if(!results) return fcore::fail(std::move(results).error());
        nlohmann::json ret;
        ret["status"] = "complete";
        ret["emulation_result"] = results.value();
        return ret.dump();
    }
}

fcore::result<std::string> hil_emulator::get_breakpoints(const std::string &core_id) {
    auto breakpoints = emu_manager.get_breakpoints(core_id);
    if(!breakpoints) return fcore::fail(std::move(breakpoints).error());
    nlohmann::json ret = breakpoints.value();
    return ret.dump();
}


void to_json(nlohmann::json& j, const emulation_results& p) {
    j = nlohmann::json{ {"results", p.results}, {"results_valid", p.results_valid}, {"duplicates", p.duplicates}, {"code", p.code} };
}


