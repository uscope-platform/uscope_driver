

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


cores_endpoints::cores_endpoints(std::shared_ptr<fpga_bridge> &h) {
    hw = h;
}

nlohmann::json cores_endpoints::process_command(std::string command_string, nlohmann::json &arguments) {
    if(command_string == "apply_program") {
        return process_apply_program(arguments);
    } else if(command_string=="deploy_hil"){
        return process_deploy_hil(arguments);
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
    uint64_t address = arguments["address"];
    std::vector<uint32_t> program = arguments["program"];
    resp["response_code"] = hw->apply_program(address, program);
    return resp;
}

nlohmann::json cores_endpoints::process_deploy_hil(nlohmann::json &arguments) {
    nlohmann::json resp;
    hil.deploy(arguments);
    resp["response_code"] = responses::ok;
    return resp;
}


