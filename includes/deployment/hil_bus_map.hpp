

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

#ifndef USCOPE_DRIVER_HIL_BUS_MAP_HPP
#define USCOPE_DRIVER_HIL_BUS_MAP_HPP


#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <optional>
#include <algorithm>


#include "emulator/emulator_dispatcher.hpp"



class bus_map_entry{
public:
    uint16_t source_io_address;
    uint16_t destination_bus_address;
    uint16_t source_channel;
    uint16_t destination_channel;
    std::string core_name;
    std::string type;
    fcore::emulator::iom_metadata metadata;
};


struct output_specs_t{
    std::string core_name;
    std::string source_output;
    uint32_t address;
    uint32_t channel;
};

class hil_bus_map {
public:

    void push_back(const bus_map_entry &e);;

    std::vector<bus_map_entry>::iterator begin();

    std::vector<bus_map_entry>::const_iterator begin() const;

    std::vector<bus_map_entry>::iterator end();

    void add_interconnect_channel(const fcore::emulator::dma_channel &c, const std::string& source_core, const std::string& target_core);
    void add_standalone_output(const fcore::emulator::emulator_core &out);

    void process_scalar_channel(const fcore::emulator::dma_channel &c, const std::string& source_core);
    void process_scatter_channel(const fcore::emulator::dma_channel &c, const std::string& source_core);
    void process_gather_channel(const fcore::emulator::dma_channel &c, const std::string& source_core);
    void process_vector_channel(const fcore::emulator::dma_channel &c, const std::string& source_core);
    void process_2d_vector_channel(const fcore::emulator::dma_channel &c, const std::string& source_core);

    std::pair<uint16_t, uint16_t> translate_output(const output_specs_t &out);

    std::vector<bus_map_entry>::const_iterator end() const;
    uint16_t get_free_address(uint16_t original_addr);
    bool is_io_address_free(uint16_t addr, const std::string& p_n);
    bool is_bus_address_free(uint16_t addr);
    void clear() {bus_map.clear();};

    void check_conflicts();

private:
    std::vector<bus_map_entry> bus_map;
    std::map<std::string, std::set<std::string>> interconnect_exposed_outputs;

};


#endif //USCOPE_DRIVER_HIL_BUS_MAP_HPP
