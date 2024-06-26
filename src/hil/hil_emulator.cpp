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

#include "hil/hil_emulator.hpp"

std::string hil_emulator::emulate(nlohmann::json &specs) {

    spdlog::info("EMULATE HIL");

    std::string results;

    fcore::emulator_manager emu_manager(specs, false, SCHEMAS_FOLDER);
    emu_manager.process();
    spdlog::info("COMPILATION DONE");
    emu_manager.emulate();
    results = emu_manager.get_results();
    spdlog::info("EMULATION RESULTS AVAILABLE");
    return results;
}


