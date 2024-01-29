

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

#include "hil/hil_deployer.hpp"

void hil_deployer::deploy(nlohmann::json &spec) {

    std::string s_f = SCHEMAS_FOLDER;
    try{
        fcore_toolchain::schema_validator_base validator(s_f + "/emulator_spec_schema.json");
        validator.validate(spec);
    } catch(std::invalid_argument &ex){
        throw std::runtime_error(ex.what());
    }

    emulator_builder e_b(false);
    for(auto &item:spec["cores"]){
        std::string id = item["id"];
        try{

            if(item["program"].contains("filename")){
                emulators[id] = e_b.load_json_program(item, {}, {});
                cores_ordering = e_b.get_core_ordering();
            } else {

                std::vector<nlohmann::json> src = {};
                std::vector<nlohmann::json> dst = {};
                for(auto &ic:spec_file["interconnect"]){
                    std::string source = ic["source"];
                    if(id == source) src.push_back(ic);
                    std::string destination = ic["destination"];
                    if(id == destination) dst.push_back(ic);
                }
                emulators[id] = e_b.load_json_program(item, dst, src);
                cores_ordering = e_b.get_core_ordering();
            }
        } catch(std::runtime_error &e){
            errors[id] = e.what();
        }

        emulators[id].input = load_input(item);
        emulators[id].output_specs = load_output_specs(item);
        emulators[id].memory_init = load_memory_init(item["memory_init"]);

    }
    int i = 0;
}
