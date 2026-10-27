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

#include "emulation/emulated_scope_reader.hpp"

#include <chrono>
#include <cstring>
#include <vector>

#include "hw_interface/sample_scaling.hpp"

namespace {
    /// The value of a raw sample of the emulation, converted the way the samples of the board are
    float sample_value(uint32_t raw, const fcore::emulator_v2::live_channel_info &info, float scaling_factor) {
        if(!info.selected) return 0.0f;
        if(info.type == fcore::type_float) return scale_sample(raw, 32, scaling_factor, false, true);
        // scale_sample masks the sample to its width, which a 32 bit one does not have
        if(info.width >= 32) {
            return scaling_factor * (info.is_signed ? static_cast<float>(static_cast<int32_t>(raw)) : static_cast<float>(raw));
        }
        return scale_sample(raw, info.width, scaling_factor, info.is_signed, false);
    }
}

emulated_scope_reader::emulated_scope_reader(std::shared_ptr<fcore::live_emulation> emulation, std::shared_ptr<scope_scaling> scaling, frame_sink sink)
    : emulation(std::move(emulation)), scaling(std::move(scaling)), sink(std::move(sink)),
      thread([this](std::stop_token stop){ read_loop(stop); }) {

}

emulated_scope_reader::~emulated_scope_reader() = default;

void emulated_scope_reader::read_loop(std::stop_token stop) {
    uint64_t sequence = 0;

    while(!stop.stop_requested()) {
        if(!emulation->wait_for_block(std::chrono::milliseconds(50))) continue;
        auto block = emulation->take_block();
        if(!block.has_value()) continue;

        const uint32_t n_channels = block->n_channels;
        const uint32_t n_samples = block->n_samples;
        const auto scaling_factors = scaling->get();

        scope_frame_header header{
            sequence++,
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()),
            n_channels,
            n_samples
        };

        auto data = std::make_shared<std::vector<uint8_t>>(sizeof(header) + static_cast<size_t>(n_channels) * n_samples * sizeof(float));
        std::memcpy(data->data(), &header, sizeof(header));

        std::vector<float> channel_samples(n_samples);
        for(uint32_t c = 0; c < n_channels; c++) {
            // the channels that have no scaling factor are not scaled
            const float factor = c < scaling_factors.size() ? scaling_factors[c] : 1.0f;
            for(uint32_t i = 0; i < n_samples; i++) {
                channel_samples[i] = sample_value(block->samples[static_cast<size_t>(c) * n_samples + i], block->channels[c], factor);
            }
            std::memcpy(data->data() + sizeof(header) + static_cast<size_t>(c) * n_samples * sizeof(float),
                        channel_samples.data(), n_samples * sizeof(float));
        }

        sink(std::move(data));
    }
}
