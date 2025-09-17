

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

#include "deployment/hil_bus_map.hpp"

void hil_bus_map::push_back(const fcore::deployer_interconnect_slot &e) {
    bus_map.push_back(e);
}

std::vector<fcore::deployer_interconnect_slot>::iterator hil_bus_map::begin() {
    return bus_map.begin();
}

std::vector<fcore::deployer_interconnect_slot>::const_iterator hil_bus_map::begin() const {
    return bus_map.begin();
}

std::vector<fcore::deployer_interconnect_slot>::iterator hil_bus_map::end() {
    return bus_map.end();
}

std::vector<fcore::deployer_interconnect_slot>::const_iterator hil_bus_map::end() const {
    return bus_map.end();
}


bool hil_bus_map::is_bus_address_free(uint16_t addr) {
    if (std::ranges::any_of(bus_map,  [&addr](const fcore::deployer_interconnect_slot& e) { return e.destination_bus_address == addr;})){
        return false;
    } else{
        return true;
    }
}

bool hil_bus_map::is_io_address_free(uint16_t addr, const std::string &p_n) {
    if (std::ranges::any_of(bus_map,  [&addr, &p_n](const fcore::deployer_interconnect_slot& e) { return e.source_io_address == addr && e.source_id == p_n; })){
        return false;
    } else{
        return true;
    }

}

uint16_t hil_bus_map::get_free_address(uint16_t original_addr) {
    if(is_bus_address_free(original_addr)){
        return original_addr;
    }

    for(uint32_t i = bus_map.size(); i<(1<<16); i++){
        if(is_bus_address_free(i)){
            return i;
        }
    }
    throw std::runtime_error("Unable to find free bus address");
}


void hil_bus_map::check_conflicts() {
    std::set<std::pair<uint16_t, uint16_t>> processed_entries;
    for(auto &e:bus_map){
        std::pair<uint16_t, uint16_t> p = std::make_pair(e.destination_bus_address, e.destination_channel);
        if(processed_entries.contains(p)){
            throw std::domain_error(
                    "Bus conflict detected at address: " + std::to_string(e.destination_bus_address) +
                         " and channel: " + std::to_string(e.destination_channel));
        } else{
            processed_entries.insert(p);
        }
    }
}

bus_address hil_bus_map::translate_output(const output_specs_t &out) {
    for(auto &e:bus_map){
        if(e.source_id == out.core_name){
            if(e.source_name == out.source_output && e.source_channel == out.channel){
                return {e.destination_bus_address, e.destination_channel};
            }
        }
    }
    throw std::runtime_error(
            "Unable to find requested output: " +
            out.core_name + "." + out.source_output +
            "[" + std::to_string(out.channel) + "]");
}
