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

#include "deployment/hil_emulator.hpp"

emulation_results hil_emulator::emulate(nlohmann::json &specs) {
    emulation_results ret_val;
    try{

        ret_val.results ="HIL BUS CONFLICT DETECTED\n";
        ret_val.results_valid = false;
        nlohmann::json test;
        test = std::vector<nlohmann::json>();
        nlohmann::json a;
        a["source"] = "test";
        a["name"] = "signal";
        a["address"] = 2;
        test.push_back(a);

        a["source"] = "test2";
        a["name"] = "signal1";
        a["address"] = 223;
        test.push_back(a);

        ret_val.duplicates = test.dump();
        ret_val.code = 9;
        return ret_val;
        spdlog::info("EMULATE HIL");
        fcore::emulator_manager emu_manager(specs, false);
        emu_manager.process();
        spdlog::info("COMPILATION DONE");
        emu_manager.emulate();
        auto results = emu_manager.get_results();
        spdlog::info("EMULATION RESULTS AVAILABLE");
        ret_val.results = results.dump();
        ret_val.results_valid = true;
        ret_val.code = 1;
    } catch (std::domain_error &e) {
        ret_val.results ="HIL BUS CONFLICT DETECTED\n";
        ret_val.results_valid = false;
        ret_val.duplicates = e.what();
        ret_val.code = 9;
    } catch (std::runtime_error &e) {
        ret_val.code = 7;
        ret_val.results = std::string("EMULATION ERROR:\n") + e.what();
        ret_val.results_valid = false;
    }
    return ret_val;
}

void to_json(nlohmann::json& j, const emulation_results& p) {
j = nlohmann::json{ {"results", p.results}, {"results_valid", p.results_valid}, {"duplicates", p.duplicates}, {"code", p.code} };
}


