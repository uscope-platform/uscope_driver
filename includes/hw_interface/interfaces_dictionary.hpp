// Copyright 2024 Filippo Savi
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

#ifndef USCOPE_DRIVER_INTERFACES_DICTIONARY_HPP
#define USCOPE_DRIVER_INTERFACES_DICTIONARY_HPP

#include <utility>
#include <stdexcept>

class interfaces_dictionary {
public:
    void set_arch(const std::string &a) {arch = a;};
    std::string get_control_bus() const {
        if(arch == "trap") throw std::runtime_error("Attempted access to configuration before architecture selection");
        return control_bus_if.at(arch);
    };
    std::string get_cores_bus() const{
        if(arch == "trap") throw std::runtime_error("Attempted access to configuration before architecture selection");
        return cores_bus_if.at(arch);
    };
    std::string get_data_bus() const {
        if(arch == "trap") throw std::runtime_error("Attempted access to configuration before architecture selection");
        return data_bus_if.at(arch);
    };
    std::string get_clock_if(uint8_t clock_number) const {
        if(arch == "trap") throw std::runtime_error("Attempted access to configuration before architecture selection");
        if(clock_number >3) throw std::invalid_argument("The PS<->PL interface has only clocks 0 to 3, requested variation of clock #" + std::to_string(clock_number));
        return clock_if.at(arch) + std::to_string(clock_number);
    };
    std::string get_buffer_address_if() const {
        if(arch == "trap") throw std::runtime_error("Attempted access to configuration before architecture selection");
        return buffer_addres_if.at(arch);
    };
private:

    std::unordered_map<std::string, std::string> control_bus_if = {
            {"zynq", "/dev/uscope_BUS_0"},
            {"zynqmp", "/dev/uscope_BUS_0"},
            {"emulate", "/dev/zero"}
    };
    std::unordered_map<std::string, std::string> cores_bus_if = {
            {"zynq", "/dev/uscope_BUS_1"},
            {"zynqmp", "/dev/uscope_BUS_1"},
            {"emulate", "/dev/zero"}
    };
    std::unordered_map<std::string, std::string> data_bus_if = {
            {"zynq", "/dev/uscope_data"},
            {"zynqmp", "/dev/uscope_data"},
            {"emulate", "/dev/uscope_data"}
    };
    std::unordered_map<std::string, std::string> clock_if = {
            {"zynq", "/sys/devices/soc0/fffc0000.uScope/fclk_"},
            {"zynqmp", "/dev/zero"},
            {"emulate", "/dev/zero"}
    };
    std::unordered_map<std::string, std::string> buffer_addres_if = {
            {"zynq", "/sys/devices/soc0/fffc0000.uScope/dma_addr"},
            {"zynqmp", "/sys/devices/platform/fffc000000008000.uScope/dma_addr"},
            {"emulate", "/dev/urandom"}
    };
    std::string arch = "trap";

};

extern interfaces_dictionary if_dict;


#endif //USCOPE_DRIVER_INTERFACES_DICTIONARY_HPP
