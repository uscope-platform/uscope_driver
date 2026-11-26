//   Copyright 2026 Filippo Savi <filssavi@gmail.com>
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

#include "server_frontend/endpoints/rpu_endpoints.hpp"

#include <cppcodec/base64_rfc4648.hpp>

nlohmann::json rpu_endpoints::process_command(const std::string &command_string, nlohmann::json &arguments) {
    if(command_string == "rpu_load_program") {
        return process_load_program(arguments);
    } else if(command_string == "rpu_stop") {
        return process_stop(arguments);
    } else if(command_string == "rpu_status") {
        return process_status(arguments);
    }
    nlohmann::json resp;
    resp["response_code"] = responses::as_integer(responses::internal_error);
    resp["data"] = "DRIVER ERROR: Internal driver error\n";
    return resp;
}

std::optional<uint8_t> rpu_endpoints::get_core(const nlohmann::json &arguments) {
    if(!arguments.is_object() || !arguments.contains("core") || !arguments["core"].is_number_unsigned()) return std::nullopt;
    auto core = arguments["core"].get<uint64_t>();
    if(core > 1) return std::nullopt;
    return static_cast<uint8_t>(core);
}

nlohmann::json rpu_endpoints::invalid_arguments(const std::string &message) {
    nlohmann::json resp;
    resp["response_code"] = responses::as_integer(responses::invalid_arg);
    resp["data"] = "DRIVER ERROR: " + message + "\n";
    return resp;
}

static nlohmann::json outcome_response(const fcore::result<void> &outcome) {
    if(!outcome) return responses::error_response(outcome.error());
    nlohmann::json resp;
    resp["response_code"] = responses::as_integer(responses::ok);
    return resp;
}

nlohmann::json rpu_endpoints::process_load_program(nlohmann::json &arguments) {
    auto core = get_core(arguments);
    if(!core) return invalid_arguments("The arguments for the rpu load program command must contain the core (0 or 1)");
    if(!arguments.contains("program") || !arguments["program"].is_string()) {
        return invalid_arguments("The program for the rpu load program command must be a base 64 encoded string");
    }
    std::vector<uint8_t> program;
    try {
        program = cppcodec::base64_rfc4648::decode(arguments["program"].get<std::string>());
    } catch (const std::exception &e) {
        return invalid_arguments(std::string("The program for the rpu load program command is not valid base 64: ") + e.what());
    }
    return outcome_response(rpu.load_program(core.value(), program));
}

nlohmann::json rpu_endpoints::process_stop(nlohmann::json &arguments) {
    auto core = get_core(arguments);
    if(!core) return invalid_arguments("The arguments for the rpu stop command must contain the core (0 or 1)");
    return outcome_response(rpu.stop(core.value()));
}

nlohmann::json rpu_endpoints::process_status(nlohmann::json &arguments) {
    auto core = get_core(arguments);
    if(!core) return invalid_arguments("The arguments for the rpu status command must contain the core (0 or 1)");
    auto state = rpu.get_state(core.value());
    if(!state) return responses::error_response(state.error());
    nlohmann::json resp;
    resp["response_code"] = responses::as_integer(responses::ok);
    resp["data"] = state.value();
    return resp;
}
