

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

#include "server_frontend/endpoints/platform_endpoints.hpp"

platform_endpoints::platform_endpoints(std::shared_ptr <fpga_bridge> &h) : tm(h) {
    hw = h;
}

nlohmann::json platform_endpoints::process_command(const std::string &command_string, nlohmann::json &arguments) {
    if(command_string == "set_pl_clock") {
        return process_set_clock(arguments);
    } else if(command_string=="get_clock") {
        return process_get_clock(arguments);
    } else if(command_string=="get_version"){
        return process_get_version(arguments);
    } else {
        nlohmann::json resp;
        resp["response_code"] = responses::as_integer(responses::internal_erorr);
        resp["data"] = "DRIVER ERROR: Internal driver error\n";
        return resp;
    }
}

nlohmann::json platform_endpoints::process_get_clock(nlohmann::json &arguments) {
    nlohmann::json resp;

    if(!arguments.contains("is_primary") || !arguments.contains("id")){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the get clock command must contain the id and is_primary keys\n";
        return resp;
    }

    if(arguments["is_primary"].type() != nlohmann::detail::value_t::boolean){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The is_primary argument of the get clock command must be boolean\n";
        return resp;
    }

    bool is_primary = arguments["is_primary"];

    if(is_primary){
        uint8_t clk_n = arguments["id"];
        resp["data"] = tm.get_base_clock(clk_n);
    } else {
        resp["data"] = tm.get_generated_clock(arguments["id"]);
    }
    resp["response_code"] = responses::as_integer(responses::ok);

    return resp;
}

nlohmann::json platform_endpoints::process_set_clock(nlohmann::json &arguments) {
    nlohmann::json resp;
    if(!(arguments.contains("is_primary") && arguments.contains("id") && arguments.contains("value"))){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The arguments for the set clock clock command must contain the id and is_primary and value keys\n";
    }
    if(arguments["is_primary"].type() != nlohmann::detail::value_t::boolean){
        resp["response_code"] = responses::as_integer(responses::invalid_arg);
        resp["data"] = "DRIVER ERROR: The is_primary argument of the get clock command must be boolean\n";
    }
    bool is_primary = arguments["is_primary"];

    if(is_primary){
        uint32_t freq = arguments["value"];
        tm.set_base_clock(arguments["id"], freq);
    } else {
        tm.set_generated_clock(arguments["id"], arguments["value"]["m"], arguments["value"]["d"], arguments["value"]["p"]);
    }
    resp["response_code"] = responses::as_integer(responses::ok);
    return resp;
}

nlohmann::json platform_endpoints::process_get_version(nlohmann::json &arguments) {
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