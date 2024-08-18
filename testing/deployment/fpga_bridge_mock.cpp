

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

#include "fpga_bridge_mock.hpp"

responses::response_code fpga_bridge_mock::load_bitstream(const std::string &bitstream) {
    return responses::ok;
}

responses::response_code fpga_bridge_mock::single_write_register(const nlohmann::json &write_obj) {
    control_writes.push_back(std::make_pair(write_obj["address"], write_obj["value"]));
    return responses::ok;
}

responses::response_code fpga_bridge_mock::apply_program(uint64_t address, std::vector<uint32_t> program) {
    rom_writes.push_back(std::make_pair(address, program));
    return responses::ok;
}

responses::response_code fpga_bridge_mock::apply_filter(uint64_t address, std::vector<uint32_t> taps) {
    return responses::ok;
}

responses::response_code fpga_bridge_mock::set_clock_frequency(std::vector<uint32_t> freq) {
    return responses::ok;
}

responses::response_code fpga_bridge_mock::set_scope_data(uint64_t address) {
    return responses::ok;
}

std::string fpga_bridge_mock::get_module_version() {
    return std::string();
}

std::string fpga_bridge_mock::get_hardware_version() {
    return std::string();
}

void fpga_bridge_mock::write_direct(uint64_t addr, uint32_t val) {
}

void fpga_bridge_mock::write_proxied(uint64_t proxy_addr, uint32_t target_addr, uint32_t val) {

}

uint32_t fpga_bridge_mock::read_direct(uint64_t address) {
    return 0;
}

uint64_t fpga_bridge_mock::register_address_to_index(uint64_t address) const {
    return 0;
}

uint64_t fpga_bridge_mock::fcore_address_to_index(uint64_t address) const {
    return 0;
}

uint32_t fpga_bridge_mock::get_pl_clock(uint8_t clk_n) {
    return 0;
}

void fpga_bridge_mock::set_pl_clock(uint8_t clk_n, uint32_t freq) {

}

nlohmann::json fpga_bridge_mock::single_read_register(uint64_t address) {
    return nlohmann::json();
}
