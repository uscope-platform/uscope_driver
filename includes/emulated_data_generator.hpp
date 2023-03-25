// Copyright 2021 Filippo Savi
// Author: Filippo Savi <filssavi@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef USCOPE_DRIVER_EMULATED_DATA_GENERATOR_HPP
#define USCOPE_DRIVER_EMULATED_DATA_GENERATOR_HPP

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

class emulated_data_generator {
public:
    explicit emulated_data_generator(uint32_t size);
    void set_data_file(std::string file);
    std::array<std::vector<float>,6> get_data(std::vector<float> scaling_factors);
private:
    std::array<std::vector<std::array<float,1024>>, 6> data;
    int chunk_counter;
    bool external_emulator_data;
    uint32_t buffer_size;
};


#endif //USCOPE_DRIVER_EMULATED_DATA_GENERATOR_HPP
