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

#ifndef USCOPE_DRIVER_RPU_ENDPOINTS_HPP
#define USCOPE_DRIVER_RPU_ENDPOINTS_HPP

#include <optional>

#include <nlohmann/json.hpp>

#include "server_frontend/infrastructure/response.hpp"
#include "hw_interface/rpu/rpu_manager.hpp"

/// Commands for the RPU (R5) cores, all of them take a "core" argument (0 or 1)
class rpu_endpoints {
public:
    nlohmann::json process_command(const std::string& command_string, nlohmann::json &arguments);
private:
    nlohmann::json process_load_program(nlohmann::json &arguments);
    nlohmann::json process_stop(nlohmann::json &arguments);
    nlohmann::json process_status(nlohmann::json &arguments);

    static std::optional<uint8_t> get_core(const nlohmann::json &arguments);
    static nlohmann::json invalid_arguments(const std::string &message);

    rpu_manager rpu;
};

#endif //USCOPE_DRIVER_RPU_ENDPOINTS_HPP
