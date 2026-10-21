//   Copyright 2026 Filippo Savi <filssavi@gmail.com>
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

#ifndef USCOPE_DRIVER_SAMPLE_SCALING_HPP
#define USCOPE_DRIVER_SAMPLE_SCALING_HPP

#include <cstdint>
#include <cstring>

/// Sign extends the lowest `bits` bits of value
inline int32_t sign_extend(uint32_t value, uint32_t bits) {
    const uint32_t mask = 1U << (bits - 1);
    value = value & ((1U << bits) - 1);
    return static_cast<int32_t>((value ^ mask) - mask);
}

/// Converts a raw sample of the scope data to its value: width and sign are applied, then the scaling factor.
/// Floating point samples are reinterpreted and not scaled.
inline float scale_sample(uint32_t raw_sample, unsigned int size, float scaling_factor, bool is_signed, bool is_float) {
    if (is_float) {
        float ret;
        std::memcpy(&ret, &raw_sample, sizeof(float));
        return ret;
    }
    int32_t sample = is_signed ? sign_extend(raw_sample, size)
                               : static_cast<int32_t>(raw_sample & ((1U << size) - 1));
    return scaling_factor * static_cast<float>(sample);
}

#endif //USCOPE_DRIVER_SAMPLE_SCALING_HPP
