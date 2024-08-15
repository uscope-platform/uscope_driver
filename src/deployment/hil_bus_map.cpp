

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

    switch(c.type) {
        case fcore::emulator::dma_link_scalar:
            process_scalar_channel(c,source_core);
            break;
        case fcore::emulator::dma_link_scatter:
            process_scatter_channel(c,source_core);
            break;
        case fcore::emulator::dma_link_gather:
            process_gather_channel(c,source_core);
            break;
        case fcore::emulator::dma_link_vector:
            process_vector_channel(c,source_core);
            break;
        case fcore::emulator::dma_link_2d_vector:
            process_2d_vector_channel(c,source_core);
            break;
    }
    interconnect_exposed_outputs[source_core].insert(c.source.io_name);
}

void hil_bus_map::add_standalone_output(const fcore::emulator::emulator_core &core) {
    for(auto &out:core.outputs){
        if(out.address.size()>1){ // TODO: unify the two branches to handle the channelized vector output
            for(auto addr:out.address){
                if(!interconnect_exposed_outputs[core.id].contains(out.name)){
                    bus_map_entry e;
                    e.core_name = core.id;
                    e.destination_bus_address = get_free_address(addr);
                    e.source_io_address = addr;
                    e.source_channel = 0;
                    e.destination_channel = 0;
                    e.type = 'o';
                    bus_map.push_back(e);
                }
            }
        } else {
            for(int i = 0; i<core.channels; i++){
                if(!interconnect_exposed_outputs[core.id].contains(out.name)){
                    bus_map_entry e;
                    e.core_name = core.id;
                    e.destination_bus_address = get_free_address(out.address[0]);
                    e.source_io_address = out.address[0];
                    e.source_channel = i;
                    e.destination_channel = 0;
                    e.type = 'o';
                    bus_map.push_back(e);
                }
            }
        }

    }


}

void hil_bus_map::process_scalar_channel(const fcore::emulator::dma_channel &c, const std::string &source_core) {
    bus_map_entry e;
    e.core_name = source_core;
    e.destination_bus_address =  c.destination.address[0];
    e.destination_channel = c.destination.channel[0];
    e.source_io_address = c.source.address[0];
    e.source_channel = c.source.channel[0];
    e.type = "o";
    bus_map.push_back(e);
}

void hil_bus_map::process_scatter_channel(const fcore::emulator::dma_channel &c, const std::string &source_core) {
    for(uint32_t i = 0; i<c.length; i++){
        bus_map_entry e;
        e.core_name = source_core;
        e.source_io_address = c.source.address[0] + i;
        e.source_channel = c.destination.channel[0];
        e.destination_bus_address =  c.destination.address[0];
        e.destination_channel = c.destination.channel[0] + i;
        e.type = "o";
        bus_map.push_back(e);
    }
}

void hil_bus_map::process_gather_channel(const fcore::emulator::dma_channel &c, const std::string &source_core) {
    for(uint32_t i = 0; i<c.length; i++){
        bus_map_entry e;
        e.core_name = source_core;
        e.source_io_address = c.source.address[0];
        e.source_channel =  c.destination.channel[0] + i;
        e.destination_bus_address =  c.destination.address[0] + i;
        e.destination_channel = c.destination.channel[0];
        e.type = "o";
        bus_map.push_back(e);
    }
}

void hil_bus_map::process_vector_channel(const fcore::emulator::dma_channel &c, const std::string &source_core) {
    for(uint32_t i = 0; i<c.length; i++){
        bus_map_entry e;
        e.core_name = source_core;
        e.source_io_address = c.source.address[0];
        e.source_channel = c.source.channel[0] + i;
        e.destination_bus_address =  c.destination.address[0];
        e.destination_channel = c.destination.channel[0] + i;
        e.type = "o";
        bus_map.push_back(e);
    }
}

void hil_bus_map::process_2d_vector_channel(const fcore::emulator::dma_channel &c, const std::string &source_core) {
    for(uint32_t j = 0; j<c.stride; j++){
        for(uint32_t i = 0; i<c.length; i++){
            bus_map_entry e;
            e.core_name = source_core;
            e.source_io_address = c.source.address[0] + j;
            e.source_channel = c.source.channel[0] + i;
            e.destination_bus_address =  c.destination.address[0] + j;
            e.destination_channel = c.destination.channel[0] + i;
            e.type = "o";
            bus_map.push_back(e);
        }
    }

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

std::pair<uint16_t, uint16_t> hil_bus_map::translate_output(const output_specs_t &out) {
    for(auto &e:bus_map){
        if(e.core_name == out.core_name){
            if(e.source_io_address == out.address && e.source_channel == out.channel){
                return std::make_pair(e.destination_bus_address, e.destination_channel);
            }
        }
    }
    throw std::runtime_error(
            "Unable to find requested output: " +
            out.core_name + "." + out.source_output +
            "(" + std::to_string(out.address) + "," + std::to_string(out.channel) + ")");
}
