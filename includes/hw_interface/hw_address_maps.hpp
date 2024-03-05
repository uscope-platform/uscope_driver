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


#ifndef USCOPE_DRIVER_HW_ADDRESS_MAPS_HPP
#define USCOPE_DRIVER_HW_ADDRESS_MAPS_HPP

#include <cstdint>

class scope_mux_am {
public:
    uint64_t ctrl = 0x0;
    uint64_t ch_1 = 0x4;
    uint64_t ch_2 = 0x8;
    uint64_t ch_3 = 0xC;
    uint64_t ch_4 = 0x10;
    uint64_t ch_5 = 0x14;
    uint64_t ch_6 = 0x18;
};

class scope_tb_am {
public:
    uint64_t ctrl = 0x0;
    uint64_t period = 0x4;
    uint64_t threshold = 0x8;
};

class scope_internal_am {
public:
    uint64_t trg_mode = 0x0;
    uint64_t trg_lvl = 0x4;
    uint64_t buf_low = 0x8;
    uint64_t buf_high = 0xc;
    uint64_t trg_src = 0x10;
    uint64_t trg_point = 0x14;
    uint64_t acq_mode = 0x18;
    uint64_t trg_rearm = 0x1C;
};


class scope_address_map {
public:
    uint64_t mux_base = 0x0;
    uint64_t tb_base = 0x100;
    uint64_t internal_base = 0x200;
    scope_mux_am mux;
    scope_tb_am tb;
    scope_internal_am scope_int;
};

#endif //USCOPE_DRIVER_HW_ADDRESS_MAPS_HPP
