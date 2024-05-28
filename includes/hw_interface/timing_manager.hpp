

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

#ifndef USCOPE_DRIVER_TIMING_MANAGER_HPP
#define USCOPE_DRIVER_TIMING_MANAGER_HPP


#include <cstdint>
#include <memory>
#include "fpga_bridge.hpp"

struct clock_definition{
    uint16_t multiplier;
    uint16_t divider;
    uint8_t  base_clock;
    uint64_t  generator_base_address;
    bool multiplier_present;
    uint64_t  phase;
};


class timing_manager {
public:
    explicit timing_manager(std::shared_ptr<fpga_bridge> &hw);
    void set_base_clock(uint8_t clk_n, uint64_t clk_f);
    uint64_t get_base_clock(uint8_t clk_n);
    void set_generated_clock(std::string clock_name, uint16_t m, uint16_t d, uint16_t p);
    void add_generated_clock(std::string clock_name, const struct  clock_definition &c);
    uint64_t get_generated_clock(const std::string& clock_name);

private:
    void setup_pll(uint64_t base_address, uint16_t div, uint16_t mult, uint16_t phase);
    void setup_clock_divider(uint64_t base_address, uint16_t div, uint16_t phase);

    std::shared_ptr<fpga_bridge> hw;
    std::array<uint64_t, 4> ps_pl_clk_f;
    std::map<std::string, clock_definition> generated_clocks;
};


#endif //USCOPE_DRIVER_TIMING_MANAGER_HPP
