

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

#include "hw_interface/operations/timing_manager.hpp"

timing_manager::timing_manager(fpga_bridge &h) : hw(&h) {}

fcore::result<responses::response_code> timing_manager::set_base_clock(uint8_t clk_n, uint64_t clk_f) {
    return hw->set_pl_clock(clk_n, clk_f);
}

/// The clocks are read back from the kernel module, which reports the rate actually set (after any rounding)
fcore::result<uint64_t> timing_manager::get_base_clock(uint8_t clk_n) {
    auto f = hw->get_pl_clock(clk_n);
    if(!f) return fcore::fail(std::move(f).error());
    return f.value();
}

fcore::result<responses::response_code> timing_manager::set_rpu_clock(uint64_t clk_f) {
    return hw->set_rpu_clock(clk_f);
}

fcore::result<uint64_t> timing_manager::get_rpu_clock() {
    auto f = hw->get_rpu_clock();
    if(!f) return fcore::fail(std::move(f).error());
    return f.value();
}


void timing_manager::add_generated_clock(std::string clock_name, const clock_definition &c) {
    generated_clocks[clock_name] = c;
}

fcore::result<responses::response_code>  timing_manager::set_generated_clock(std::string clock_name, uint16_t m, uint16_t d, uint16_t p) {
    if (generated_clocks.contains(clock_name)) {
        auto clk_def = generated_clocks[clock_name];
        clk_def.divider = d;
        clk_def.phase = p;
        if(clk_def.multiplier_present){
            generated_clocks[clock_name].multiplier = m;
            setup_pll(clk_def.generator_base_address, clk_def.divider, clk_def.multiplier, clk_def.phase);
        } else {
            if(auto divider = setup_clock_divider(clk_def.generator_base_address, clk_def.divider, clk_def.phase); !divider) return fcore::fail(std::move(divider).error());
        }
    }
    return responses::ok;
}

fcore::result<uint64_t> timing_manager::get_generated_clock(const std::string& clock_name) {
    auto it = generated_clocks.find(clock_name);
    if(it == generated_clocks.end()) {
        return fcore::fail(fcore::error_code::invalid_argument, "Unknown generated clock '" + clock_name + "'");
    }
    const auto &clk_def = it->second;
    if(clk_def.divider == 0) {
        return fcore::fail(fcore::error_code::invalid_argument, "The generated clock '" + clock_name + "' has a zero divider");
    }
    auto base_freq = get_base_clock(clk_def.base_clock);
    if(!base_freq) return base_freq;

    return base_freq.value() * clk_def.multiplier/clk_def.divider;
}

void timing_manager::setup_pll(uint64_t base_address, uint16_t div, uint16_t mult, uint16_t phase) {
    // PLL BASED DYNAMIC CLOCK FREQUENCY NOT IMPLEMENTED IN HARDWARE YET
}

fcore::result<void> timing_manager::setup_clock_divider(uint64_t base_address, uint16_t div, uint16_t phase) {
    if(auto written = hw->write_direct(base_address +0x4, div); !written) return written;
    return hw->write_direct(base_address + 0x8, phase);
}


