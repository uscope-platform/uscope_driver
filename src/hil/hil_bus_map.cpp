

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

#include "hil/hil_bus_map.hpp"

void hil_bus_map::push_back(const bus_map_entry &e) {
    bus_map.push_back(e);
}

std::vector<bus_map_entry>::iterator hil_bus_map::begin() {
    return bus_map.begin();
}

std::vector<bus_map_entry>::const_iterator hil_bus_map::begin() const {
    return bus_map.begin();
}

std::vector<bus_map_entry>::iterator hil_bus_map::end() {
    return bus_map.end();
}

std::vector<bus_map_entry>::const_iterator hil_bus_map::end() const {
    return bus_map.end();
}

std::optional<bus_map_entry> hil_bus_map::at_io(uint16_t i, const std::string& p_n) {
    for(auto &e:bus_map){
        if(e.io_address == i && e.core_name == p_n){
            return e;
        }
    }
    return {};
}

std::optional<bus_map_entry> hil_bus_map::at_bus(uint16_t i, const std::string &p_n) {
    for(auto &e:bus_map){
        if(e.bus_address == i && e.core_name == p_n){
            return e;
        }
    }
    return {};
}

bool hil_bus_map::has_bus(uint16_t addr) {
    for(auto &e:bus_map){
        if(e.bus_address == addr){
            return true;
        }
    }
    return false;
}

bool hil_bus_map::has_io(uint16_t addr, const std::string &p_n) {
    if (std::ranges::any_of(bus_map,  [&addr, &p_n](const bus_map_entry& e) { return e.io_address == addr && e.core_name == p_n; })){
        return true;
    } else{
        return false;
    }

}
