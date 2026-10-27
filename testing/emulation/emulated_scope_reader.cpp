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

#include <gtest/gtest.h>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <vector>

#include "emulation/emulated_scope_reader.hpp"
#include "../emulation/emulation_specs.hpp"

namespace {
    /// Collects the frames that the reader publishes
    struct frame_collector {
        std::mutex mutex;
        std::condition_variable received;
        std::vector<scope_frame> frames;

        void add(scope_frame f) {
            {
                std::lock_guard lock(mutex);
                frames.push_back(std::move(f));
            }
            received.notify_all();
        }

        /// \return the frames, once there are at least n of them (less if it takes too long)
        std::vector<scope_frame> wait_for(size_t n) {
            std::unique_lock lock(mutex);
            received.wait_for(lock, std::chrono::seconds(2), [&] { return frames.size() >= n; });
            return frames;
        }
    };

    scope_frame_header header_of(const scope_frame &f) {
        scope_frame_header header{};
        std::memcpy(&header, f->data(), sizeof(header));
        return header;
    }

    std::vector<float> channel_of(const scope_frame &f, uint32_t channel) {
        const auto header = header_of(f);
        std::vector<float> ret(header.n_samples);
        std::memcpy(ret.data(), f->data() + sizeof(header) + channel * header.n_samples * sizeof(float), header.n_samples * sizeof(float));
        return ret;
    }
}

TEST(emulated_scope_reader, publishes_the_blocks_as_frames) {
    auto emulation = std::make_shared<fcore::live_emulation>(6, 4);
    ASSERT_TRUE(emulation->load(nlohmann::json::parse(counter_spec)));
    ASSERT_TRUE(emulation->select_output(0, fcore::emulator_v2::output_selection{"test", "out", 0, 0}));

    frame_collector collector;
    emulated_scope_reader reader(emulation, std::make_shared<scope_scaling>(), [&](scope_frame f) { collector.add(std::move(f)); });
    ASSERT_TRUE(emulation->start());

    auto frames = collector.wait_for(3);
    emulation->stop();
    ASSERT_GE(frames.size(), 3);

    for(size_t i = 0; i < frames.size(); i++) {
        auto header = header_of(frames[i]);
        EXPECT_EQ(header.sequence, i);
        EXPECT_EQ(header.n_channels, 6);
        EXPECT_EQ(header.n_samples, 4);
        EXPECT_EQ(frames[i]->size(), sizeof(scope_frame_header) + 6 * 4 * sizeof(float));

        // the counter counts one more in every round, the channels without an output are zero
        auto counter = channel_of(frames[i], 0);
        for(int s = 1; s < counter.size(); s++) {
            EXPECT_EQ(counter[s], counter[s - 1] + 1);
        }
        for(uint32_t c = 1; c < 6; c++) {
            EXPECT_EQ(channel_of(frames[i], c), std::vector<float>(4, 0.0f));
        }
    }
}

TEST(emulated_scope_reader, signed_integer_samples_are_extended_and_scaled) {
    nlohmann::json spec = nlohmann::json::parse(constant_spec);
    // the output has the bits of the float 1.2 (0x3f99999a) as an integer: its lowest 16 bits as a signed number are -26214
    spec["cores"][0]["inputs"][0]["source"]["value"] = {1.2};
    spec["cores"][0]["program"]["content"] = "void main(){float in_c; float out = in_c*1.0;}";
    spec["cores"][0]["outputs"][0]["metadata"] = nlohmann::json::parse(R"({"type": "integer", "width": 16, "signed": true, "common_io": false})");

    auto emulation = std::make_shared<fcore::live_emulation>(6, 4);
    ASSERT_TRUE(emulation->load(spec));
    ASSERT_TRUE(emulation->select_output(0, fcore::emulator_v2::output_selection{"test", "out", 0, 0}));

    auto scaling = std::make_shared<scope_scaling>();
    scaling->set({0.5f});

    frame_collector collector;
    emulated_scope_reader reader(emulation, scaling, [&](scope_frame f) { collector.add(std::move(f)); });
    ASSERT_TRUE(emulation->start());

    auto frames = collector.wait_for(1);
    emulation->stop();
    ASSERT_GE(frames.size(), 1);
    EXPECT_EQ(channel_of(frames[0], 0), std::vector<float>(4, -13107.0f));
}

TEST(emulated_scope_reader, thirty_two_bit_integer_samples_are_scaled) {
    nlohmann::json spec = nlohmann::json::parse(constant_spec);
    // the output has the bits of the float 2.0 (0x40000000) as an unsigned integer
    spec["cores"][0]["outputs"][0]["metadata"] = nlohmann::json::parse(R"({"type": "integer", "width": 32, "signed": false, "common_io": false})");

    auto emulation = std::make_shared<fcore::live_emulation>(6, 4);
    ASSERT_TRUE(emulation->load(spec));
    ASSERT_TRUE(emulation->select_output(0, fcore::emulator_v2::output_selection{"test", "out", 0, 0}));

    auto scaling = std::make_shared<scope_scaling>();
    scaling->set({2.0f});

    frame_collector collector;
    emulated_scope_reader reader(emulation, scaling, [&](scope_frame f) { collector.add(std::move(f)); });
    ASSERT_TRUE(emulation->start());

    auto frames = collector.wait_for(1);
    emulation->stop();
    ASSERT_GE(frames.size(), 1);
    EXPECT_EQ(channel_of(frames[0], 0), std::vector<float>(4, 2147483648.0f));
}

TEST(emulated_scope_reader, nothing_is_published_while_stopped) {
    auto emulation = std::make_shared<fcore::live_emulation>(6, 4);
    ASSERT_TRUE(emulation->load(nlohmann::json::parse(counter_spec)));
    ASSERT_TRUE(emulation->select_output(0, fcore::emulator_v2::output_selection{"test", "out", 0, 0}));

    frame_collector collector;
    emulated_scope_reader reader(emulation, std::make_shared<scope_scaling>(), [&](scope_frame f) { collector.add(std::move(f)); });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    std::lock_guard lock(collector.mutex);
    EXPECT_TRUE(collector.frames.empty());
}
