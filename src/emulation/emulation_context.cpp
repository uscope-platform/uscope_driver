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

#include "emulation/emulation_context.hpp"

#include <set>

#include "hw_interface/bus/scope_accessor.hpp"
#include "server_frontend/infrastructure/response.hpp"

emulation_context &emulation_context::instance() {
    static emulation_context context;
    return context;
}

// the capture has the shape of the data of the board, so that the clients see no difference
emulation_context::emulation_context()
    : emulation(std::make_shared<fcore::live_emulation>(scope_accessor::n_channels, scope_accessor::buffer_size)) {}

void emulation_context::set_target(hil_target_kind new_target) {
    if(new_target != hil_target_kind::emulator && emulator_selected()) emulation->stop();
    target = new_target;
}

std::optional<nlohmann::json> emulation_context::process_command(const std::string &command, const nlohmann::json &arguments) {
    if(command == "set_hil_target") return process_set_target(arguments);
    if(command == "get_hil_target") return process_get_target();

    if(!emulator_selected()) return std::nullopt;
    if(command == "deploy_hil") return process_deploy(arguments);
    if(command == "hil_select_out") return process_select_out(arguments);
    if(command == "hil_set_in") return process_set_in(arguments);
    if(command == "hil_start") return process_start();
    if(command == "hil_stop") return process_stop();
    if(command == "get_sampling_frequency") return process_get_sampling_frequency();
    return std::nullopt;
}

nlohmann::json emulation_context::process_set_target(const nlohmann::json &arguments) {
    nlohmann::json resp;
    if(arguments.is_object() && arguments.contains("target") && arguments["target"].is_string()) {
        const std::string selected = arguments["target"];
        if(selected == "hardware" || selected == "emulator") {
            set_target(selected == "emulator" ? hil_target_kind::emulator : hil_target_kind::hardware);
            resp["response_code"] = responses::ok;
            return resp;
        }
    }
    resp["response_code"] = responses::as_integer(responses::invalid_arg);
    resp["data"] = "DRIVER ERROR: The target must be either hardware or emulator\n";
    return resp;
}

nlohmann::json emulation_context::process_get_target() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    resp["data"] = emulator_selected() ? "emulator" : "hardware";
    return resp;
}

nlohmann::json emulation_context::process_deploy(const nlohmann::json &arguments) {
    nlohmann::json resp;
    auto outcome = fcore::catching([&]() -> fcore::result<void> {
        return emulation->load(arguments);
    });

    resp["response_code"] = responses::as_integer(responses::ok);
    if(!outcome) {
        // reported the way the deployment on the hardware does
        nlohmann::json data;
        data["error_code"] = responses::as_integer(responses::deployment_error);
        data["error"] = std::string("HIL DEPLOYMENT ERROR:\n") + outcome.error().what();
        data["duplicates"] = std::vector<std::string>();
        resp["data"] = data;
    }
    return resp;
}

nlohmann::json emulation_context::process_select_out(const nlohmann::json &arguments) {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;

    // an empty request selects nothing, as it does on the hardware
    if(!arguments.contains("output")) return resp;

    auto selected = fcore::catching([&]() -> fcore::result<void> {
        fcore::emulator_v2::output_selection output;
        output.core = arguments["output"]["core"];
        output.name = arguments["output"]["name"];
        output.channel = arguments["output"]["channel"];
        return emulation->select_output(arguments["channel"], output);
    });
    if(!selected) return responses::error_response(selected.error());
    return resp;
}

nlohmann::json emulation_context::process_set_in(const nlohmann::json &arguments) {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;

    auto set = fcore::catching([&]() -> fcore::result<void> {
        std::string core = arguments["core"];
        std::string name = arguments["name"];
        uint32_t channel = arguments["channel"];
        double value = arguments["value"];
        return emulation->set_input(core, name, channel, value);
    });
    if(!set) return responses::error_response(set.error());
    return resp;
}

nlohmann::json emulation_context::process_start() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    if(auto started = emulation->start(); !started) return responses::error_response(started.error());
    return resp;
}

nlohmann::json emulation_context::process_stop() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    emulation->stop();
    return resp;
}

nlohmann::json emulation_context::process_get_sampling_frequency() {
    nlohmann::json resp;
    resp["response_code"] = responses::ok;
    resp["data"] = emulation->get_sampling_frequency();
    return resp;
}
