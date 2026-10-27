

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

#include "server_frontend/endpoints/cores_endpoints.hpp"




nlohmann::json cores_endpoints::process_command(const std::string& command_string, const nlohmann::json &arguments) {
    if(command_string == "apply_program") {
        return process_apply_program(arguments);
    } else if(command_string=="deploy_hil") {
        return process_deploy_hil(arguments);
    } else if(command_string=="emulate_hil"){
        return process_emulate_hil(arguments);
    } else if(command_string=="hil_select_out"){
        return process_hil_select_out(arguments);
    } else if(command_string=="hil_set_in"){
        return process_hil_set_in(arguments);
    } else if(command_string=="hil_start"){
        return process_hil_start();
    } else if(command_string=="hil_stop") {
        return process_hil_stop();
    } else if(command_string=="hil_debug"){
        return process_hil_debug(arguments);
    } else if(command_string=="hil_disassemble"){
        return process_hil_disassemble(arguments);
    } else if(command_string=="hil_hardware_sim"){
        return process_hil_hardware_sim(arguments);
    } else if(command_string == "compile_program") {
        return process_compile_program(arguments);
    }else if(command_string == "set_hil_address_map"){
        return process_set_hil_address_map(arguments);
    }else if(command_string == "get_hil_address_map") {
        return process_get_hil_address_map(arguments);
    } else if(command_string == "get_sampling_frequency") {
        return process_get_sampling_frequency();
    } else if(command_string == "set_debugger_option") {
        return process_set_debugger_option(arguments);
    } else if(command_string == "get_debugger_option") {
        return process_get_debugger_option(arguments);
    } else {
        nlohmann::json resp;
        resp["response_code"] = responses::as_integer(responses::internal_error);
        resp["data"] = "DRIVER ERROR: Internal driver error\n";
        return resp;
    }
}

///
/// \param operand_1 address of the fCore Instance to program
/// \param operand_2 comma delimited list of instructions in the program
/// \return Success
nlohmann::json cores_endpoints::process_apply_program(const nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string error_message;
    if(!commands::validate_schema(arguments, commands::load_program, error_message)){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: Invalid arguments for the apply program command\n"+ error_message;
        return resp;
    }

    if(!check_float_intness(arguments["address"])){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: core address must be an integer like number\n"+ error_message;
        return resp;
    }

    uint64_t address = arguments["address"];
    std::vector<uint32_t> program = arguments["program"];
    auto applied = hw.apply_program(address, program);
    if(!applied) return responses::error_response(applied.error());
    resp["response_code"] = applied.value();
    return resp;
}

nlohmann::json cores_endpoints::process_deploy_hil(const nlohmann::json &arguments) {
    nlohmann::json resp;
    auto outcome = fcore::catching([&]() -> fcore::result<responses::response_code> {
        bool custom_deploy = arguments["deployment_mode"];
        return custom_deploy ? custom.deploy(arguments) : hil.deploy(arguments);
    });

    if(outcome){
        resp["response_code"] = outcome.value();
    } else {
        nlohmann::json data;
        resp["response_code"] = responses::as_integer(responses::ok);

        data["error_code"] = responses::as_integer(responses::deployment_error);
        data["error"] = std::string("HIL DEPLOYMENT ERROR:\n") + outcome.error().what();
        data["duplicates"] = std::vector<std::string>();
        resp["data"] = data;
    }
    return resp;
}

nlohmann::json cores_endpoints::process_emulate_hil(const nlohmann::json &arguments) {

    nlohmann::json resp;

    resp["response_code"] = responses::ok;
    resp["data"] = emulator.emulate(arguments);

    return resp;
}

nlohmann::json cores_endpoints::process_hil_select_out(const nlohmann::json &arguments) {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    output_specs_t out;
    if(arguments.contains("output")){
        out.core_name = arguments["output"]["core"];
        out.source_output = arguments["output"]["name"];
        out.channel = arguments["output"]["channel"];
        if(auto selected = hil.select_output(arguments["channel"], out); !selected){
            // either the output does not exist or the scope multiplexer cannot be written
            if(selected.error().code != fcore::error_code::bus_allocation) return responses::error_response(selected.error());
            resp["response_code"] = responses::as_integer(responses::invalid_arg);
            resp["data"] = "DRIVER ERROR: " + selected.error().what() + "\n";
        }
    } else{
        spdlog::trace("Received empty argument request for output_select call");
    }
    return resp;
}

nlohmann::json cores_endpoints::process_hil_set_in(const nlohmann::json &arguments) {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    auto dbg = arguments.dump();

    std::string name  = arguments["name"];
    double value = arguments["value"];
    std::string core =  arguments["core"];
    uint16_t channel = arguments["channel"];
    if(auto set = hil.set_input(core, name, channel, value); !set) return responses::error_response(set.error());
    return resp;
}

nlohmann::json cores_endpoints::process_hil_start() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    if(auto started = hil.start(); !started) return responses::error_response(started.error());
    return resp;
}

nlohmann::json cores_endpoints::process_get_sampling_frequency() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    resp["data"] = hil.get_sampling_frequency();
    return resp;
}

nlohmann::json cores_endpoints::process_hil_stop() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    if(auto stopped = hil.stop(); !stopped) return responses::error_response(stopped.error());
    return resp;
}

nlohmann::json cores_endpoints::process_compile_program(const nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string error_message;
    std::string dbg = arguments.dump();
    if(!commands::validate_schema(arguments, commands::compile_program_schema, error_message)){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: Invalid arguments for the compile program command\n"+ error_message;
        return resp;
    }
    nlohmann::json compilation_result;
    std::string content = arguments["content"];
    auto headers = arguments["headers"];
    auto io = arguments["io"];
    auto hex = toolchain.compile(content,headers, io);
    if(hex){
        compilation_result["status"] = "ok";
        compilation_result["hex"] = hex.value();
    } else {
        compilation_result["status"] = "error";
        compilation_result["error"] = hex.error().what();
    }
    resp["data"] = compilation_result;
    resp["response_code"] = responses::as_integer(responses::ok);
    return resp;
}

nlohmann::json cores_endpoints::process_hil_disassemble(const nlohmann::json &arguments) {
    std::vector<std::string> results;
    nlohmann::json resp;

    auto disassembly = emulator.disassemble(arguments);
    if(disassembly){
        resp["data"] = disassembly.value();
        resp["response_code"] = responses::as_integer(responses::ok);
    } else {
        resp["data"] = "DRIVER ERROR: " + disassembly.error().what() + "\n";
        resp["response_code"] = responses::as_integer(responses::emulation_error);
    }
    return resp;
}

nlohmann::json cores_endpoints::process_hil_hardware_sim(const nlohmann::json &arguments) {
    std::vector<std::string> results;
    nlohmann::json resp;


    auto sim_data = hil.get_hardware_sim_data(arguments);
    if(sim_data){
        resp["data"] = sim_data.value();
        resp["response_code"] = responses::as_integer(responses::ok);
    } else {
        resp["data"] = "DRIVER ERROR: " + sim_data.error().what() + "\n";
        resp["response_code"] = responses::as_integer(responses::deployment_error);
    }
    return resp;
}



nlohmann::json cores_endpoints::process_set_hil_address_map(const nlohmann::json &arguments) {
    nlohmann::json resp;
    hil.set_layout_map(arguments);
    custom.set_layout_map(arguments);
    resp["response_code"] = responses::as_integer(responses::ok);
    return resp;
}

/// \param arguments the name of the option and its value: {"name": "multichannel_debug", "value": true}
nlohmann::json cores_endpoints::process_set_debugger_option(const nlohmann::json &arguments) {
    nlohmann::json resp;
    if(!arguments.is_object() || !arguments.contains("name") || !arguments["name"].is_string() || !arguments.contains("value")){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the set debugger option command must contain the name of the option and its value\n";
        return resp;
    }

    std::string name = arguments["name"];
    if(name == "multichannel_debug"){
        if(!arguments["value"].is_boolean()){
            resp["response_code"] = responses::as_integer(responses::invalid_arg);
            resp["data"] = "DRIVER ERROR: The multichannel_debug option must be boolean\n";
            return resp;
        }
        emulator.set_multichannel_debug(arguments["value"]);
        resp["response_code"] = responses::as_integer(responses::ok);
        return resp;
    }

    resp["response_code"] = responses::as_integer(responses::invalid_arg);
    resp["data"] = "DRIVER ERROR: Unknown debugger option " + name + "\n";
    return resp;
}

/// \param arguments the name of the option: {"name": "multichannel_debug"}
/// \return the value of the option
nlohmann::json cores_endpoints::process_get_debugger_option(const nlohmann::json &arguments) {
    nlohmann::json resp;
    if(!arguments.is_object() || !arguments.contains("name") || !arguments["name"].is_string()){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the get debugger option command must contain the name of the option\n";
        return resp;
    }

    std::string name = arguments["name"];
    if(name == "multichannel_debug"){
        resp["data"] = emulator.get_multichannel_debug();
        resp["response_code"] = responses::as_integer(responses::ok);
        return resp;
    }

    resp["response_code"] = responses::as_integer(responses::invalid_arg);
    resp["data"] = "DRIVER ERROR: Unknown debugger option " + name + "\n";
    return resp;
}

nlohmann::json cores_endpoints::process_get_hil_address_map(const nlohmann::json &arguments) {
    nlohmann::json resp;
    resp["data"] = hil.get_layout_map();
    resp["response_code"] = responses::as_integer(responses::ok);
    return resp;
}

void cores_endpoints::set_accessor(const std::shared_ptr<bus_accessor> &ba) {
    hil.set_accessor(ba);
    custom.set_accessor(ba);
    hw.set_accessor(ba);
}

nlohmann::json cores_endpoints::process_hil_debug(const nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string dbg = arguments.dump();
    if(!arguments.contains("command") || !arguments.contains("arguments")){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: Invalid arguments for the debug hil command\n";
        return resp;
    }
    std::string command = arguments["command"];
    interactive_command c;
    if(command== "test"){
        resp["data"] = "success";
        resp["response_code"] = responses::as_integer(responses::ok);
        return resp;
    }
    if(command=="add_breakpoint"){
        if(!arguments["arguments"].contains("id") || !arguments["arguments"]["id"].is_string()){
            resp["response_code"] = responses::as_integer(responses::invalid_arg);
            resp["data"] = "DRIVER ERROR: The add_breakpoint command needs the core id as a string\n";
            return resp;
        }
        c.id = arguments["arguments"]["id"];
        c.type = command_add_breakpoint;
        c.target_instruction = arguments["arguments"]["line"];
    } else if(command=="remove_breakpoint") {
        c.type = command_remove_breakpoint;
        c.id = arguments["arguments"]["id"];
        c.target_instruction = arguments["arguments"]["line"];
    } else if(command=="step") {
        c.type = command_step_over;
    } else if(command=="resume") {
        c.type = command_resume_emulation;
    } else if(command=="initialize") {
        c.type = command_initialize_emulation;
        c.spec = arguments["arguments"];
    } else if(command=="run") {
        c.type = command_start_emulation;
    } else if(command=="get_breakpoints"){
        c.type = command_get_breakpoints;
        c.id = arguments["arguments"]["id"];
    }

    auto outcome = emulator.run_command(c);
    if(outcome){
        resp["data"] = outcome.value();
        resp["response_code"] = responses::as_integer(responses::ok);
    } else {
        resp["data"] = "DRIVER ERROR: " + outcome.error().what() + "\n";
        resp["response_code"] = responses::as_integer(responses::emulation_error);
    }
    return resp;
}
