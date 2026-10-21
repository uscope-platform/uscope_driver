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


#include "server_frontend/data_plane/scope_reader.hpp"
#include "server_frontend/data_plane/scope_frame.hpp"
#include "hw_interface/channel_metadata.hpp"
#include "hw_interface/sample_scaling.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <spdlog/spdlog.h>

scope_reader::scope_reader(std::shared_ptr<scope_accessor> sa, std::shared_ptr<scope_scaling> scaling, frame_sink sink)
    : scope_if(std::move(sa)), scaling(std::move(scaling)), sink(std::move(sink)),
      thread([this](std::stop_token stop){ read_loop(stop); }) {

}

scope_reader::~scope_reader() = default;

void scope_reader::read_loop(std::stop_token stop) {
    uint64_t sequence = 0;

    while (!stop.stop_requested()) {
        if (!scope_if->wait_for_data(50)) continue;

        std::array<uint64_t, scope_accessor::n_channels * scope_accessor::buffer_size> raw_data{};
        try {
            raw_data = scope_if->get_scope_data();
        } catch (const std::runtime_error &) {
            continue;
        }

        // the samples of each channel are grouped together, so that the clients can use them without further work
        constexpr size_t n_channels = scope_accessor::n_channels;
        constexpr size_t buffer_size = scope_accessor::buffer_size;
        const auto scaling_factors = scaling->get();

        std::array<std::array<float, buffer_size>, n_channels> channels{};
        std::array<size_t, n_channels> counts{};
        for (uint64_t word : raw_data) {
            size_t channel = (word >> 32) & 0xffff;
            if (channel >= n_channels || counts[channel] >= buffer_size) continue;

            channel_metadata metadata(static_cast<uint16_t>(word >> 48));
            channels[channel][counts[channel]++] = scale_sample(
                static_cast<uint32_t>(word), metadata.get_size(), scaling_factors[channel],
                metadata.is_signed(), metadata.is_float());
        }
        // channels are expected to have all the same number of samples, anything missing is sent as zero
        const uint32_t n_samples = static_cast<uint32_t>(*std::max_element(counts.begin(), counts.end()));

        scope_frame_header header{
            sequence++,
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()),
            scope_accessor::n_channels,
            n_samples
        };

        auto data = std::make_shared<std::vector<uint8_t>>(sizeof(header) + n_channels * n_samples * sizeof(float));
        std::memcpy(data->data(), &header, sizeof(header));
        for (size_t c = 0; c < n_channels; c++) {
            std::memcpy(data->data() + sizeof(header) + c * n_samples * sizeof(float),
                        channels[c].data(), n_samples * sizeof(float));
        }

        sink(std::move(data));
    }
}
