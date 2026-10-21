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

#include <chrono>
#include <cstring>
#include <spdlog/spdlog.h>

scope_reader::scope_reader(std::shared_ptr<scope_accessor> sa, frame_sink sink)
    : scope_if(std::move(sa)), sink(std::move(sink)),
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

        scope_frame_header header{
            sequence++,
            static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()),
            scope_accessor::n_channels,
            scope_accessor::buffer_size
        };

        auto data = std::make_shared<std::vector<uint8_t>>(sizeof(header) + sizeof(raw_data));
        std::memcpy(data->data(), &header, sizeof(header));
        std::memcpy(data->data() + sizeof(header), raw_data.data(), sizeof(raw_data));

        sink(std::move(data));
    }
}
