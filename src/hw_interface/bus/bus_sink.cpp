

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

#include "hw_interface/bus/bus_sink.hpp"

void bus_sink::write_register(const std::vector<uint64_t>& addresses, uint64_t data) {
    operations.push_back({addresses, {data}, "w"});
}

uint32_t bus_sink::read_register(const std::vector<uint64_t>& address) {
    operations.push_back({address, {0}, "r"});
    return rand()%100;
}

void bus_sink::load_program(uint64_t address, const std::vector<uint32_t> program) {
    std::vector<uint64_t> a = {address};
    std::vector<uint64_t> pn;
    for (auto &p:program) {
        pn.push_back(p);
    }
    operations.push_back({a, pn, "p"});
}
