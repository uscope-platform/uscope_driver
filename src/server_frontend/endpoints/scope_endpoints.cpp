

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

#include "server_frontend/endpoints/scope_endpoints.hpp"

scope_endpoints::scope_endpoints(std::shared_ptr<scope_manager> &sc) {
 scope = sc;
}

nlohmann::json scope_endpoints::process_command(std::string command_string, nlohmann::json &arguments) {
    if(command_string == "start_capture"){
        return process_start_capture(arguments);
    } else if( command_string == "read_data"){
        return process_read_data();
    } else if( command_string == "check_capture"){
        return process_check_capture_progress();
    } else if( command_string == "set_channel_widths"){
        return process_set_widths(arguments);
    } else if( command_string == "set_scaling_factors"){
        return process_set_scaling_factors(arguments);
    } else if( command_string == "set_channel_status"){
        return process_set_channel_status(arguments);
    } else if( command_string == "set_channel_signs"){
        return process_set_channel_signs(arguments);
    } else if( command_string == "enable_manual_metadata") {
        return process_enable_mannual_metadata();
    }else if(command_string == "get_acquisition_status") {
        return process_get_acquisition_status();
    }else if(command_string == "set_acquisition") {
        return process_set_acquisition(arguments);
    }else if(command_string == "set_scope_address"){
        return process_set_scope_address(arguments);
    }else {
        nlohmann::json resp;
        resp["response_code"] = responses::as_integer(responses::internal_erorr);
        resp["data"] = "DRIVER ERROR: Internal driver error\n";
        return resp;
    }
}



///
/// \param Operand number of buffers to captire
/// \return Success
nlohmann::json scope_endpoints::process_start_capture(nlohmann::json &arguments) {
    nlohmann::json resp;
    std::string n_buffers_s = arguments[0];
    uint32_t n_buffers = std::stoul(n_buffers_s, nullptr, 0);
    resp["response_code"] = scope->start_capture(n_buffers);
    return resp;
}

///
/// \param response Pointer to the structure where the data will eventually be put
/// \return Either success of failure depending on if the data is actually ready
nlohmann::json scope_endpoints::process_read_data() {
    nlohmann::json resp;
    std::vector<nlohmann::json> resp_data;
    resp["response_code"] = scope->read_data(resp_data);
    resp["data"] = resp_data;
    return resp;
}

///
/// \param Response pointer to the response_t structure where the result of the check will be put
/// \return Success
nlohmann::json scope_endpoints::process_check_capture_progress() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    resp["data"] = scope->check_capture_progress();
    return resp;
}

///
/// \param operand_2 comma delimited list of channel widths
/// \return Success
nlohmann::json scope_endpoints::process_set_widths(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::array){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the set channel widths command must be an array\n";
        return resp;
    }
    if(arguments.empty()){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the set channel widths can not be an empty array\n";
        return resp;
    }
    if(!std::all_of(arguments.begin(), arguments.end(), [](const nlohmann::json& el){ return el.is_number(); })){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the set channel widths command must be numeric values\n";
        return resp;
    }
    std::vector<uint32_t> widths = arguments;
    resp["response_code"] = scope->set_channel_widths(widths);
    return resp;
}

nlohmann::json  scope_endpoints::process_set_scaling_factors(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::array){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the set scaling factor command must be an array\n";
        return resp;
    }
    if(arguments.empty()){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the set scaling factor command can not be an empty array\n";
        return resp;
    }
    if(!std::all_of(arguments.begin(), arguments.end(), [](const nlohmann::json& el){ return el.is_number(); })){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the set scaling factor command must be numeric values\n";
        return resp;
    }
    std::vector<float> sfs = arguments;
    resp["response_code"] = scope->set_scaling_factors(sfs);
    return resp;
}

nlohmann::json scope_endpoints::process_set_channel_status(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::object){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the set channel status command must be an array of objects\n";
        return resp;
    }
    std::unordered_map<int, bool> status;
    for(auto &item:arguments.items()){
        status[std::stoi(item.key())] = item.value();
    }
    resp["response_code"] = scope->set_channel_status(status);
    return resp;
}

nlohmann::json scope_endpoints::process_set_channel_signs(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.type() != nlohmann::detail::value_t::array){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The argument for the set channel signs command must be an array of objects\n";
        return resp;
    }
    std::unordered_map<int, bool> signs;
    for(auto &item:arguments.items()){
        signs[std::stoi(item.key())] = item.value();
    }
    resp["response_code"] = scope->set_channel_signed(signs);
    return resp;
}


nlohmann::json scope_endpoints::process_enable_mannual_metadata() {
    return scope->enable_manual_metadata();
}

nlohmann::json scope_endpoints::process_get_acquisition_status() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    resp["data"] = scope->get_acquisition_status();
    return resp;
}

nlohmann::json scope_endpoints::process_set_acquisition(nlohmann::json &arguments) {
    nlohmann::json resp;

    if(!arguments.contains("trigger") ||
        !arguments.contains("level")  ||
        !arguments.contains("source")  ||
        !arguments.contains("mode")  ||
        !arguments.contains("level_type") ||
        !arguments.contains("trigger_point") ||
        !arguments.contains("prescaler")

    ) {
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: Wrong arguments for set acquisition command\n";
    }
    acquisition_metadata data;
    data.trigger_level = arguments["level"];
    data.trigger_source = arguments["source"];
    data.mode = arguments["mode"];
    data.level_type = arguments["level_type"];
    data.trigger_mode = arguments["trigger"];
    data.trigger_point = arguments["trigger_point"];
    data.acquisition_divider = arguments["prescaler"];

    resp["response_code"] = scope->set_acquisition(data);
    return resp;
}

nlohmann::json scope_endpoints::process_set_scope_address(nlohmann::json &arguments) {
    nlohmann::json resp;
    bool invalid = false;
    if(!arguments.contains("address")){
        invalid = true;
    }
    if(arguments["address"].type() != nlohmann::detail::value_t::number_unsigned){
        invalid = true;
    }

    if(invalid){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The scope address must be an unsigned integer\n";
        return resp;
    }

    resp["response_code"] = responses::ok;
    scope->set_scope_address(arguments["address"]);
    return resp;
}
