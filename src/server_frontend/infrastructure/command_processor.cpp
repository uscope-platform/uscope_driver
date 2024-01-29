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
#include "server_frontend/infrastructure/command_processor.hpp"

command_processor::command_processor(std::shared_ptr<fpga_bridge> &h, std::shared_ptr<scope_thread> &sc) :
cores_ep(h), scope_ep(sc), control_ep(h) {
    hw = h;
}

/// This function, core of the driver operation, is called upon message reception and parsing, acts as a dispatcher,
/// sending the command to one of several other functions that will further parse the operands and further call
/// appropriate functions in the fpga_bridge.c or scope_handler.c files. Process_command also populates
/// the response structure that defines what will be sent back to the server.
/// \param Command Command received from the uscope_server
nlohmann::json command_processor::process_command(std::string command_string, nlohmann::json &arguments) {

    nlohmann::json response_obj;
    response_obj["cmd"] = command_string;

    if(runtime_config.log && runtime_config.log_level>1){
        std::cout << "Received command: " <<command_string << std::endl;
    }

    if(commands::control_commands.contains(command_string)){
        response_obj["body"] = control_ep.process_command(command_string, arguments);
    } else if(commands::scope_commands.contains(command_string)){
        response_obj["body"] = scope_ep.process_command(command_string, arguments);
    } else if(commands::core_commands.contains(command_string)){
        response_obj["body"] = cores_ep.process_command(command_string, arguments);
    } else if(commands::infrastructure_commands.contains(command_string)){
        if(command_string == "null"){
            response_obj["body"] = process_null();
        } else if( command_string == "get_version"){
            response_obj["body"] = process_get_version(arguments);
        }
    } else{
        response_obj["body"] = nlohmann::json();
        response_obj["body"]["response_code"] = responses::as_integer(responses::invalid_cmd_schema);
        response_obj["body"]["data"] = "DRIVER ERROR: Unknown command received\n";
    }

    return response_obj;
}


nlohmann::json command_processor::process_null() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    return resp;
}


nlohmann::json command_processor::process_get_version(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::string){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the get version command must be a string\n";
        return resp;
    }
    std::string component = arguments;
    if(component == "driver"){
        resp["data"] = uscope_driver_versions;
    } else if(component == "module"){
        resp["data"] = hw->get_module_version();
    } else if(component == "hardware"){
        resp["data"] = hw->get_hardware_version();
    } else {
        resp["data"] = "DRIVER ERROR: Unknown component\n";
    }
    resp["response_code"] = responses::as_integer(responses::ok);
    return resp;
}






