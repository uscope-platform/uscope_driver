

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


bool hil_bus_map::is_bus_address_free(uint16_t addr) {
    if (std::ranges::any_of(bus_map,  [&addr](const bus_map_entry& e) { return e.destination_bus_address == addr;})){
        return false;
    } else{
        return true;
    }
}

bool hil_bus_map::is_io_address_free(uint16_t addr, const std::string &p_n) {
    if (std::ranges::any_of(bus_map,  [&addr, &p_n](const bus_map_entry& e) { return e.source_io_address == addr && e.core_name == p_n; })){
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

void hil_bus_map::add_interconnect_channel(const fcore::emulator::dma_channel &c, const std::string& source_core, const std::string& target_core) {
    for(int j = 0; j<c.source.address.size(); j++){
        if(is_bus_address_free(c.source.address[j])){
            bus_map_entry e;
            e.core_name = source_core;
            e.destination_bus_address =  c.destination.address[j];
            e.source_io_address = c.source.address[j];
            e.source_channel = 0;
            e.destination_channel = 0;
            e.type = "o";
            bus_map.push_back(e);
        } else {
            spdlog::warn("WARNING: Unsolvable input bus address conflict detected");
        }
    }
}

void hil_bus_map::add_standalone_output(const fcore::io_map_entry &out, const std::string &core_name) {
    if(is_io_address_free(out.io_addr, core_name)){
        bus_map_entry e;
        e.core_name = core_name;
        e.destination_bus_address = get_free_address(out.io_addr);
        e.source_io_address = out.io_addr;
        e.source_channel = 0;
        e.destination_channel = 0;
        e.type = out.type;
        bus_map.push_back(e);
    }
}
