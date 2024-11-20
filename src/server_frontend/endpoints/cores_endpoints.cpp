

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



nlohmann::json cores_endpoints::process_command(const std::string& command_string, nlohmann::json &arguments) {
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
    } else if(command_string == "compile_program") {
        return process_compile_program(arguments);
    }else if(command_string  == "set_layout_map") {
        return process_set_layout_map(arguments);
    }else if(command_string == "set_hil_address_map"){
        return process_set_hil_address_map(arguments);
    }else if(command_string == "get_hil_address_map"){
        return process_get_hil_address_map(arguments);
    } else {
        nlohmann::json resp;
        resp["response_code"] = responses::as_integer(responses::internal_erorr);
        resp["data"] = "DRIVER ERROR: Internal driver error\n";
        return resp;
    }
}

///
/// \param operand_1 address of the fCore Instance to program
/// \param operand_2 comma delimited list of instructions in the program
/// \return Success
nlohmann::json cores_endpoints::process_apply_program(nlohmann::json &arguments) {
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
    resp["response_code"] = hw.apply_program(address, program);
    return resp;
}

nlohmann::json cores_endpoints::process_deploy_hil(nlohmann::json &arguments) {
    nlohmann::json resp;
    try{
        auto specs = fcore::emulator::emulator_specs(arguments);
        fcore::emulator_manager em(arguments, runtime_config.debug_hil);
        auto programs = em.get_programs();
        if(specs.custom_deploy_mode){
            resp["response_code"] = custom.deploy(specs, programs);
        } else {
            resp["response_code"] = hil.deploy(specs, programs);
        }
    } catch (std::domain_error &e) {

        nlohmann::json data;
        data["response_code"] = responses::as_integer(responses::ok);

        resp["error_code"] = responses::as_integer(responses::hil_bus_conflict_warning);
        data["error"] = std::string("HIL BUS CONFLICT DETECTED\n");
        data["duplicates"] = e.what();
        resp["data"] = data;

    } catch (std::runtime_error &e){

        nlohmann::json data;
        resp["response_code"] = responses::as_integer(responses::ok);

        data["error_code"] = responses::as_integer(responses::deployment_error);
        data["error"] = std::string("HIL DEPLOYMENT ERROR:\n") + e.what();
        data["duplicates"] = std::vector<std::string>();
        resp["data"] = data;
    }
    return resp;
}

nlohmann::json cores_endpoints::process_emulate_hil(nlohmann::json &arguments) {

    nlohmann::json resp;

    resp["response_code"] = responses::ok;
    resp["data"] = emulator.emulate(arguments);

    return resp;
}

nlohmann::json cores_endpoints::process_hil_select_out(nlohmann::json &arguments) {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;

    output_specs_t out;
    if(arguments.contains("output")){
        out.core_name = arguments["output"]["source"];
        out.source_output = arguments["output"]["output"];
        out.address = arguments["output"]["address"];
        out.channel = arguments["output"]["channel"];
        hil.select_output(arguments["channel"], out);
    } else{
        spdlog::trace("Received empty argument request for output_select call");
    }
    return resp;
}

nlohmann::json cores_endpoints::process_hil_set_in(nlohmann::json &arguments) {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    uint64_t address = arguments["address"][0];
    uint32_t value = arguments["value"];
    std::string core =  arguments["core"];
    hil.set_input(address, value, core);
    return resp;
}

nlohmann::json cores_endpoints::process_hil_start() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    hil.start();
    return resp;
}

nlohmann::json cores_endpoints::process_hil_stop() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    hil.stop();
    return resp;
}

nlohmann::json cores_endpoints::process_compile_program(nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string error_message;
    std::string dbg = arguments.dump();
    if(!commands::validate_schema(arguments, commands::compile_program_schema, error_message)){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: Invalid arguments for the compile program command\n"+ error_message;
        return resp;
    }
    nlohmann::json compilation_result;
    try{
        std::string content = arguments["content"];
        auto headers = arguments["headers"];
        auto io = arguments["io"];
        compilation_result["status"] = "ok";
        compilation_result["hex"] = toolchain.compile(content,headers, io);
    } catch (std::runtime_error &e){
        compilation_result["status"] = "error";
        compilation_result["error"] = e.what();
    }
    resp["data"] = compilation_result;
    resp["response_code"] = responses::as_integer(responses::ok);
    return resp;
}

nlohmann::json cores_endpoints::process_hil_disassemble(nlohmann::json &arguments) {
    std::vector<std::string> results;
    nlohmann::json resp;

    resp["data"] = emulator.disassemble(arguments);
    resp["response_code"] = responses::as_integer(responses::ok);
    return resp;
}


nlohmann::json cores_endpoints::process_set_layout_map(nlohmann::json &arguments) {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    hil.set_layout_map(arguments);
    return resp;
}

nlohmann::json cores_endpoints::process_set_hil_address_map(nlohmann::json &arguments) {
    nlohmann::json resp;
    hil.set_layout_map(arguments);
    custom.set_layout_map(arguments);
    resp["response_code"] = responses::as_integer(responses::ok);
    return resp;
}

nlohmann::json cores_endpoints::process_get_hil_address_map(nlohmann::json &arguments) {
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

nlohmann::json cores_endpoints::process_hil_debug(nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string dbg = arguments.dump();
    if(!arguments.contains("command") || !arguments.contains("arguments")){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: Invalid arguments for the debug hil command\n";
        return resp;
    }
    std::string command = arguments["command"];

    if(command== "test")
        resp["data"] = "success";
    else if(command=="add_breakpoint")
        resp["data"] = emulator.run_command({command_add_breakpoint, arguments["arguments"]["id"], arguments["arguments"]["line"]});
    else if(command=="remove_breakpoint")
        resp["data"] = emulator.run_command({command_remove_breakpoint, arguments["arguments"]["id"], arguments["arguments"]["line"]});
    else if(command=="step")
        resp["data"] = emulator.run_command({command_step_over, 0});
    else if(command=="resume")
        resp["data"] = emulator.run_command({command_resume_emulation, 0});
    else if(command=="run")
        emulator.start_interactive_session(arguments["arguments"]);
    resp["response_code"] = responses::as_integer(responses::ok);
    return resp;
}
