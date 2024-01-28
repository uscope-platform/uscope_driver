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

#ifndef USCOPE_DRIVER_CHANNEL_METADATA_HPP
#define USCOPE_DRIVER_CHANNEL_METADATA_HPP

#include <cstdint>

class channel_metadata {
public:
    channel_metadata() {
        raw_metadata = 0;
    };
    explicit channel_metadata(uint16_t r) {raw_metadata = r;};
    uint16_t get_size() const {
        return (raw_metadata & 0xf) + 8;
    };
    bool is_signed() const { return (raw_metadata & 0x10)>>4;};
    bool is_float() const {return (raw_metadata & 0x20)>>5;}

    void set_signed(const bool &s) {
        if(s){
            raw_metadata |= 0x10;
        } else {
            raw_metadata &= ~0x10;
        }
    };
    void set_size(const uint16_t &s) {
        raw_metadata &= 0xfff0;
        raw_metadata |= (s-8)&0x000F;
    };
    void set_float(const bool &f) {
        if(f){
            raw_metadata |= 0x20;
        } else {
            raw_metadata &= ~0x20;
        }
    };
    uint64_t get_high_side_field(uint16_t dest) const{
        uint32_t  metadata = (raw_metadata<<16);
        uint32_t  combined_hsf = metadata | dest;
        uint64_t res = ((uint64_t) combined_hsf)<<32;
        return res;
    };
private:
    uint16_t raw_metadata;

};


#endif //USCOPE_DRIVER_CHANNEL_METADATA_HPP6