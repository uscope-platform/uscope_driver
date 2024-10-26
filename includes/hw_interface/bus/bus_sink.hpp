

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

#ifndef USCOPE_DRIVER_BUS_SINK_HPP
#define USCOPE_DRIVER_BUS_SINK_HPP

#include <cstdint>
#include <vector>
#include <string>

struct bus_op{
     std::vector<uint64_t> address;
    std::vector<uint64_t> data;
     std::string type;
};

class bus_sink {
public:
    void load_program(uint64_t address, const std::vector<uint32_t> program);
    void write_register(const std::vector<uint64_t>& addresses, uint64_t data);
    uint32_t read_register(const std::vector<uint64_t>& address);

    std::vector<bus_op> get_operations() {return operations;};
private:
    std::vector<bus_op> operations;
};


#endif //USCOPE_DRIVER_BUS_SINK_HPP
