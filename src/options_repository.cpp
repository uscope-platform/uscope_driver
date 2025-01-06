

//   Copyright 2025 Filippo Savi <filssavi@gmail.com>
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

#include "options_repository.hpp"

void options_repository::set_option(const std::string &name, nlohmann::json &value) {
    options[name] = value;
}

nlohmann::json options_repository::get_option(const std::string &name) {
    return options[name];
}

options_repository::options_repository() {
    options["multichannel_debug"] = false;
}
