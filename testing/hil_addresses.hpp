//   Copyright 2025 Filippo Savi <filssavi@gmail.com>
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

#ifndef USCOPE_DRIVER_HIL_ADDRESSES_HPP
#define USCOPE_DRIVER_HIL_ADDRESSES_HPP


#include <cstdint>

constexpr uint64_t control_plane = 0x443c00000;

constexpr struct {
    uint64_t cores_rom_plane = 0x500000000;
    struct {
        uint64_t scope = control_plane + 0x0;
        uint64_t controller = control_plane + 0x10000;
        uint64_t gpio = control_plane + 0x20000;
        uint64_t noise_generator = control_plane + 0x30000;
        uint64_t waveform_generator = control_plane + 0x40000;
        uint64_t cores_control = control_plane + 0x50000;
    }bases;

    struct {
        uint64_t cores = 0x10000;
        uint64_t timebase = 0x0;
        uint64_t sequencer = 0x1000;
        uint64_t inputs = 0x2000;
        uint64_t dma = 0x1000;
        uint64_t scope_mux = 0x0;
        uint64_t rom = 0x10000000;
    }offsets;

} addr_map;

constexpr struct {
    uint64_t control = 0x0;
    uint64_t ch_1 = 0x4;
    uint64_t ch_2 = 0x8;
    uint64_t ch_3 = 0xc;
    uint64_t ch_4 = 0x10;
    uint64_t ch_5 = 0x14;
    uint64_t ch_6 = 0x18;
}scope_mux_regs;


constexpr struct {
    uint64_t enable = 0;
    uint64_t divisors_1 = 0x4;
    uint64_t divisors_2 = 0x8;
    uint64_t divisors_3 = 0xC;
    uint64_t divisors_4 = 0x10;
}sequencer_regs;

constexpr struct {
    uint64_t enable = 0;
    uint64_t period = 0x4;
    uint64_t threshold_1 = 0x8;
    uint64_t threshold_2 = 0xC;
    uint64_t threshold_3 = 0x10;
    uint64_t threshold_4 = 0x14;
}enable_gen_regs;

constexpr struct {
    uint64_t out = 0;
    uint64_t in = 0x4;
}gpio_regs;

constexpr struct {
    uint64_t data_lsb = 0;
    uint64_t data_hsb = 0x4;
    uint64_t data_dest = 0x8;
    uint64_t selector = 0xC;
    uint64_t active_channels = 0x10;
    uint64_t metadata = 0x14;
}mc_const;

constexpr struct {
    uint64_t channels = 0;
    uint64_t fill_data = 0x4;
    uint64_t fill_channel = 0x8;
    uint64_t addr_base = 0xC;
    uint64_t user_base = 0x4c;
} dma_regs;

constexpr struct {
    uint64_t active_outputs = 0;
    uint64_t output_dest_1 = 0x4;
    uint64_t output_dest_2 = 0x8;
    uint64_t output_dest_3 = 0xC;
    uint64_t output_dest_4 = 0x10;
    uint64_t output_dest_5 = 0x14;
    uint64_t output_dest_6 = 0x18;
    uint64_t output_dest_7 = 0x1c;
    uint64_t output_dest_8 = 0x20;
    uint64_t output_dest_9 = 0x24;
    uint64_t output_dest_10 = 0x28;
    uint64_t output_dest_11 = 0x2c;
    uint64_t output_dest_12 = 0x30;
    uint64_t output_dest_13 = 0x34;
    uint64_t output_dest_14 = 0x38;
} noise_gen_regs;

constexpr struct {
    uint64_t active_channels = 0;
    uint64_t shape = 0x4;
    uint64_t channel_selector = 0x8;
    uint64_t parameter_0 = 0xc;
    uint64_t parameter_1 = 0x10;
    uint64_t parameter_2 = 0x14;
    uint64_t parameter_3 = 0x18;
    uint64_t parameter_4 = 0x1c;
    uint64_t parameter_5 = 0x20;
    uint64_t parameter_6 = 0x24;
    uint64_t parameter_7 = 0x28;
    uint64_t parameter_8 = 0x2c;
    uint64_t parameter_9 = 0x30;
    uint64_t parameter_10 = 0x34;
} wave_gen_regs;


static const nlohmann::json addr_map_v2 = nlohmann::json::parse( R"({
        "bases": {
            "controller": 18316591104,
            "cores_control": 18316853248,
            "cores_inputs": 8192,
            "cores_rom": 21474836480,
            "hil_control": 18316656640,
            "noise_generator": 18316722176,
            "waveform_generator": 18316787712,
            "scope_mux": 18316525568
        },
        "offsets": {
            "controller": 4096,
            "cores_control": 65536,
            "cores_inputs": 4096,
            "cores_rom": 268435456,
            "dma": 4096,
            "hil_tb": 0
        }
    })");
#endif //USCOPE_DRIVER_HIL_ADDRESSES_HPP