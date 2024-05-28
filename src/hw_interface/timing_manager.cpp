

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

#include "../../includes/hw_interface/timing_manager.hpp"

timing_manager::timing_manager(std::shared_ptr<fpga_bridge> &h) {
    hw = h;
    ps_pl_clk_f[0] = hw->get_pl_clock(0);
    ps_pl_clk_f[1] = hw->get_pl_clock(1);
    ps_pl_clk_f[2] = hw->get_pl_clock(2);
    ps_pl_clk_f[3] = hw->get_pl_clock(3);
}

void timing_manager::set_base_clock(uint8_t clk_n, uint64_t clk_f) {
    hw->set_pl_clock(clk_n, clk_f);
    ps_pl_clk_f[clk_n] = clk_f;
}

uint64_t timing_manager::get_base_clock(uint8_t clk_n) {
    return ps_pl_clk_f[clk_n];
}

void timing_manager::add_generated_clock(std::string clock_name, const clock_definition &c) {
    generated_clocks[clock_name] = c;
}

void timing_manager::set_generated_clock(std::string clock_name, uint16_t m, uint16_t d, uint16_t p) {
    if (generated_clocks.contains(clock_name)) {
        auto clk_def = generated_clocks[clock_name];
        clk_def.divider = d;
        clk_def.phase = p;
        if(clk_def.multiplier_present){
            generated_clocks[clock_name].multiplier = m;
            setup_pll(clk_def.generator_base_address, clk_def.divider, clk_def.multiplier, clk_def.phase);
        } else {
            setup_clock_divider(clk_def.generator_base_address, clk_def.divider, clk_def.phase);
        }
    }

}

uint64_t timing_manager::get_generated_clock(const std::string& clock_name) {
    auto clk_def = generated_clocks[clock_name];
    auto base_freq = ps_pl_clk_f[clk_def.base_clock];

    return base_freq * clk_def.multiplier/clk_def.divider;
}

void timing_manager::setup_pll(uint64_t base_address, uint16_t div, uint16_t mult, uint16_t phase) {
    // PLL BASED DYNAMIC CLOCK FREQUENCY NOT IMPLEMENTED IN HARDWARE YET
}

void timing_manager::setup_clock_divider(uint64_t base_address, uint16_t div, uint16_t phase) {
    hw->write_direct(base_address +0x4, div);
    hw->write_direct(base_address + 0x8, phase);
}


