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

#ifndef USCOPE_DRIVER_SCOPE_FRAME_HPP
#define USCOPE_DRIVER_SCOPE_FRAME_HPP

#include <cstdint>
#include <memory>
#include <vector>

/// Header of every binary websocket frame pushed by the scope server, little endian.
/// It is followed by n_channels*n_samples 64 bit words, exactly as read from the kernel module.
/// Since slow clients only receive the newest data, gaps in the sequence number are the skipped frames.
struct scope_frame_header {
    uint64_t sequence;
    uint64_t timestamp_ns;  ///< system clock, nanoseconds since the epoch
    uint32_t n_channels;
    uint32_t n_samples;     ///< samples per channel
};
static_assert(sizeof(scope_frame_header) == 24);

/// A complete frame (header followed by the data), shared between the reader and all the clients
using scope_frame = std::shared_ptr<const std::vector<uint8_t>>;

#endif //USCOPE_DRIVER_SCOPE_FRAME_HPP
