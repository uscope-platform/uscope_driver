

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

class bus_map_entry{
public:
    uint16_t source_io_address;
    uint16_t destination_bus_address;
    uint16_t source_channel;
    uint16_t destination_channel;
    std::string core_name;
    std::string type;
};


class hil_bus_map {
public:

    void push_back(const bus_map_entry &e);;

    std::vector<bus_map_entry>::iterator begin();

    std::vector<bus_map_entry>::const_iterator begin() const;

    std::vector<bus_map_entry>::iterator end();

    std::vector<bus_map_entry>::const_iterator end() const;
    uint16_t get_free_address(uint16_t original_addr);
    bool is_io_address_free(uint16_t addr, const std::string& p_n);
    bool is_bus_address_free(uint16_t addr);
    void clear() {bus_map.clear();};

private:
    std::vector<bus_map_entry> bus_map;

};


#endif //USCOPE_DRIVER_HIL_BUS_MAP_HPP
