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

command_processor::command_processor(std::shared_ptr<fpga_bridge> &h, std::shared_ptr<scope_manager> &sc) :
cores_ep(h), scope_ep(sc), control_ep(h), platform_ep(h){
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

    spdlog::trace("Received command: {0}", command_string);

    if(commands::control_commands.contains(command_string)){
        response_obj["body"] = control_ep.process_command(command_string, arguments);
    } else if(commands::scope_commands.contains(command_string)){
        response_obj["body"] = scope_ep.process_command(command_string, arguments);
    } else if(commands::core_commands.contains(command_string)) {
        response_obj["body"] = cores_ep.process_command(command_string, arguments);
    } else if(commands::platform_commands.contains(command_string)){
        response_obj["body"] = platform_ep.process_command(command_string, arguments);
    } else if(commands::infrastructure_commands.contains(command_string)){
        if(command_string == "null"){
            response_obj["body"] = process_null();
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



