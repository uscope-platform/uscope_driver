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

#include "emulated_data_generator.hpp"

emulated_data_generator::emulated_data_generator(uint32_t size) {
    external_emulator_data = false;
    buffer_size = size;
    chunk_counter = 0;
}

std::array<std::vector<float>,6> emulated_data_generator::get_data(std::vector<float> scaling_factors) {
    std::array<std::vector<float>,6> selected_data;
    if(external_emulator_data){

        for(int i = 0; i< 6; ++i){
            std::vector<float> vec(data[i][chunk_counter].begin(), data[i][chunk_counter].end());
            selected_data[i] = vec;
        }
        if(chunk_counter == data[0].size()-1)
            chunk_counter = 0;
        else
            ++chunk_counter;
        return selected_data;
    } else {
        std::vector<float> tb;
        tb.push_back(0);
        for(int j= 0; j<buffer_size/6-1; j++){
            tb.push_back(tb.back()+(float)1/10e3);
        }

        std::array<float,6> phases = {0, M_PI/3, 2*M_PI/3, M_PI, 4*M_PI/3, 5*M_PI/3};
        for(int i = 0; i<6; i++){
            for(int j= 0; j<buffer_size/6; j++){
                float sample = 4000.0*sin(2*M_PI*50*tb[j]+phases[i]);
                sample = sample + std::rand()%1000;
                float scaled_sample = sample*scaling_factors[i];
                selected_data[i].push_back(scaled_sample);
            }
        }
    }

}

void emulated_data_generator::set_data_file(std::string file) {

    std::ifstream fs(file);
    nlohmann::json json_data = nlohmann::json::parse(fs);
    std::array<std::vector<float>, 6> raw_data = json_data["data"];

    for(int i = 0; i< raw_data.size(); ++i){
        if(raw_data[i].size()%1024!= 0){
            std::cerr << "ERROR: Wrong data format in the specified emulation file, each channel array must be a multiple of 1024" <<std::endl;
            exit(-1);
        }
        for (std::size_t j(0); j <raw_data[i].size()/1024; ++j) {
            std::array<float,1024> chunk{};
            std::copy(raw_data[i].begin() + j * 1024, raw_data[i].begin() + (j + 1) * 1024, chunk.begin());
            data[i].push_back(chunk);
        }
    }
    external_emulator_data = true;
}