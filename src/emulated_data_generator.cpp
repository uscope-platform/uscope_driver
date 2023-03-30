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
    buffer_size = size/n_channels;
    chunk_counter = 0;
}

std::vector<std::vector<float>> emulated_data_generator::get_data(std::vector<float> scaling_factors) {
    std::vector<std::vector<float>> selected_data;
    if(external_emulator_data){

        for(int i = 0; i< n_channels; ++i){
            std::vector<float> vec(data[i][chunk_counter].begin(), data[i][chunk_counter].end());
            selected_data.push_back(vec);
        }
        if(chunk_counter == data[0].size()-1)
            chunk_counter = 0;
        else
            ++chunk_counter;
    } else {
        std::vector<float> tb;
        tb.push_back(0);
        for(int j= 0; j<buffer_size-1; j++){
            tb.push_back(tb.back()+(float)1/10e3);
        }

        for(int i = 0; i<n_channels; i++){
            std::vector<float> ch_data;
            for(int j= 0; j<buffer_size; j++){
                float phase = i*2*M_PI/n_channels;
                float sample = 4000.0*sin(2*M_PI*50*tb[j]+phase);
                sample = sample + std::rand()%1000;
                float scaled_sample = sample*scaling_factors[i];
                ch_data.push_back(scaled_sample);
            }
            selected_data.push_back(ch_data);
        }
    }
    return selected_data;
}

void emulated_data_generator::set_data_file(std::string file) {

    std::ifstream fs(file);
    nlohmann::json json_data = nlohmann::json::parse(fs);
    std::array<std::vector<float>, n_channels> raw_data = json_data["data"];

    for(int i = 0; i< raw_data.size(); ++i){
        if(raw_data[i].size()%buffer_size!= 0){
            std::cerr << "ERROR: Wrong data format in the specified emulation file, each channel array must be a multiple of the buffer size ("<< std::to_string(buffer_size)<< ")" <<std::endl;
            exit(-1);
        }
        for (std::size_t j(0); j <raw_data[i].size()/buffer_size; ++j) {
            std::vector<float> chunk(buffer_size,0);
            std::copy(raw_data[i].begin() + j * buffer_size, raw_data[i].begin() + (j + 1) * buffer_size, chunk.begin());
            data[i].push_back(chunk);
        }
    }
    external_emulator_data = true;
}