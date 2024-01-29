

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

#include "server_frontend/endpoints/control_endpoints.hpp"


control_endpoints::control_endpoints(std::shared_ptr<fpga_bridge> &h) {
    hw = h;
}

nlohmann::json control_endpoints::process_command(const std::string& command_string, nlohmann::json &arguments) {
    if(command_string == "load_bitstream"){
        return process_load_bitstream(arguments);
    } else if( command_string == "register_write"){
        return process_single_write_register(arguments);
    } else if( command_string == "set_frequency"){
        return process_set_frequency(arguments);
    } else if( command_string == "register_read"){
        return process_single_read_register(arguments);
    } else if( command_string == "set_scope_data"){
        return process_set_scope_data(arguments);
    } else if( command_string == "apply_filter"){
        return process_apply_filter(arguments);
    } else {
        nlohmann::json resp;
        resp["response_code"] = responses::as_integer(responses::internal_erorr);
        resp["data"] = "DRIVER ERROR: Internal driver error\n";
        return resp;
    }
}

///
/// \param operand_1 Register address
/// \param operand_2 Value to write
/// \return Success
nlohmann::json control_endpoints::process_single_write_register(nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string error_message;
    if(!commands::validate_schema(arguments, commands::write_register, error_message)){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: Invalid arguments for the write register command\n"+ error_message;
        return resp;
    }
    resp["response_code"] = hw->single_write_register(arguments);
    return resp;
}

///
/// \param operand_1 Register address
/// \param response Response object where the read result will be placed
/// \return Success
nlohmann::json control_endpoints::process_single_read_register(nlohmann::json &arguments) {
    if(!arguments.is_number()){
        nlohmann::json resp;
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR:The argument for the single read register call must be a numeric value\n";
        return resp;
    }
    uint64_t address = arguments;
    return hw->single_read_register(address);
}


nlohmann::json control_endpoints::process_set_frequency(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::array){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the set clock frequency command must be an array\n";
        return resp;
    }
    std::vector<uint32_t> freq = arguments;
    resp["response_code"] = hw->set_clock_frequency(freq);
    return resp;
}


///
/// \param Operand bitstream name
/// \return
nlohmann::json control_endpoints::process_load_bitstream(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() !=nlohmann::detail::value_t::string) {
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The load bitstream command must be a string";
        return resp;
    }
    std::string bitstram_name = arguments;
    resp["response_code"] = hw->load_bitstream(bitstram_name);
    return resp;
}

nlohmann::json control_endpoints::process_apply_filter(nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string error_message;
    if(!commands::validate_schema(arguments, commands::apply_filter_schema, error_message)){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: Invalid arguments for the apply filter command\n"+ error_message;
        return resp;
    }
    uint32_t address = arguments["address"];
    std::vector<uint32_t> taps = arguments["taps"];
    resp["response_code"] = hw->apply_filter(address, taps);
    return resp;
}

nlohmann::json control_endpoints::process_set_scope_data(nlohmann::json &arguments) {
    nlohmann::json resp;
    commands::scope_data sd;
    sd.enable_address = arguments["enable"];
    sd.buffer_address = arguments["buffer_address"];

    resp["response_code"] = hw->set_scope_data(sd);
    return resp;
}

