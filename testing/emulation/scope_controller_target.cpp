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
#include <boost/asio.hpp>

#include "emulation/emulation_context.hpp"
#include "server_frontend/data_plane/scope_controller.hpp"

static nlohmann::json run_command(scope_controller &controller, const std::string &cmd, const nlohmann::json &args) {
    boost::asio::io_context io;
    auto reply = boost::asio::co_spawn(io, controller.execute(cmd, args), boost::asio::use_future);
    io.run();
    return reply.get();
}

TEST(scope_controller, emulator_target_does_not_touch_the_board) {
    auto scaling = std::make_shared<scope_scaling>();
    emulation_context::instance().set_target(hil_target_kind::emulator);

    // there is no bus to write to: the commands of the board have to succeed without needing it
    scope_controller controller(nullptr, scaling);

    auto reply = run_command(controller, "set_scope_address", nlohmann::json::parse(R"({"address": 1024, "dma_buffer_offset": 0})"));
    EXPECT_TRUE(reply["ok"]);
    reply = run_command(controller, "disable_scope_dma", nlohmann::json::parse(R"({"status": true})"));
    EXPECT_TRUE(reply["ok"]);
    reply = run_command(controller, "set_acquisition", nlohmann::json::parse(
        R"({"trigger": "rising_edge", "level": 1.0, "source": 1, "mode": "continuous", "trigger_point": 0, "prescaler": 0, "level_type": "float"})"));
    EXPECT_TRUE(reply["ok"]);

    reply = run_command(controller, "get_acquisition_status", nlohmann::json());
    EXPECT_TRUE(reply["ok"]);
    EXPECT_EQ(reply["data"], "not present");
    emulation_context::instance().set_target(hil_target_kind::hardware);
}

TEST(scope_controller, emulator_target_scales_the_channels) {
    auto scaling = std::make_shared<scope_scaling>();
    emulation_context::instance().set_target(hil_target_kind::emulator);

    scope_controller controller(nullptr, scaling);

    auto reply = run_command(controller, "set_scaling_factors", nlohmann::json::parse("[2.5, 4.0]"));
    EXPECT_TRUE(reply["ok"]);
    EXPECT_EQ(scaling->get()[0], 2.5f);
    EXPECT_EQ(scaling->get()[1], 4.0f);
    emulation_context::instance().set_target(hil_target_kind::hardware);
}

TEST(scope_controller, emulator_target_still_rejects_unknown_commands) {
    auto scaling = std::make_shared<scope_scaling>();
    emulation_context::instance().set_target(hil_target_kind::emulator);

    scope_controller controller(nullptr, scaling);

    auto reply = run_command(controller, "not_a_command", nlohmann::json());
    EXPECT_FALSE(reply["ok"]);
    emulation_context::instance().set_target(hil_target_kind::hardware);
}
