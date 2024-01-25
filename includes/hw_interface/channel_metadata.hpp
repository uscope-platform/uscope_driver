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
    explicit channel_metadata(uint16_t r) {raw_metadata = r;};
    uint16_t get_size() const {
        return (raw_metadata & 0xf) + 8;
    };
    bool is_signed() { return (raw_metadata & 0x10)>>4;};
    bool is_float() {return (raw_metadata & 0x20)>>5;}
private:
    uint16_t raw_metadata;

};


#endif //USCOPE_DRIVER_CHANNEL_METADATA_HPP
