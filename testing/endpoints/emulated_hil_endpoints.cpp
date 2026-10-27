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

#include "emulation/emulation_context.hpp"
#include "hw_interface/bus/scope_accessor.hpp"
#include "server_frontend/endpoints/cores_endpoints.hpp"
#include "../emulation/emulation_specs.hpp"

/// The emulator is shared by the whole driver: every test starts with nothing loaded, stopped, and the hardware as target
static std::shared_ptr<fcore::live_emulation> fresh_emulator() {
    auto &context = emulation_context::instance();
    context.get_emulation()->stop();
    // a specification that is not valid leaves nothing loaded
    (void) context.get_emulation()->load(nlohmann::json());
    context.set_target(hil_target_kind::hardware);
    return context.get_emulation();
}

/// The channel of the first block of the emulation that has a value for it, one value per sample
static std::vector<float> take_samples(const std::shared_ptr<fcore::live_emulation> &emulation, uint32_t channel) {
    std::vector<float> ret;
    if(!emulation->wait_for_block(std::chrono::milliseconds(2000))) return ret;
    auto block = emulation->take_block();
    if(!block.has_value()) return ret;
    for(uint32_t i = 0; i < block->n_samples; i++) {
        ret.push_back(fcore::emulator_v2::emulator_backend::uint32_to_float(block->samples[channel * block->n_samples + i]));
    }
    return ret;
}

TEST(cores_endpoints, get_hil_target) {
    auto emulation = fresh_emulator();

    cores_endpoints ep;

    auto resp = ep.process_command("get_hil_target", nlohmann::json());
    EXPECT_EQ(resp["response_code"], responses::ok);
    EXPECT_EQ(resp["data"], "hardware");

    ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "emulator"})"));
    EXPECT_EQ(ep.process_command("get_hil_target", nlohmann::json())["data"], "emulator");
    ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "hardware"})"));
}

TEST(cores_endpoints, set_hil_target) {
    auto emulation = fresh_emulator();

    cores_endpoints ep;

    auto resp = ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "emulator"})"));
    EXPECT_EQ(resp["response_code"], responses::ok);
    EXPECT_EQ(emulation_context::instance().get_target(), hil_target_kind::emulator);
    EXPECT_EQ(ep.process_command("get_hil_target", nlohmann::json())["data"], "emulator");

    resp = ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "hardware"})"));
    EXPECT_EQ(resp["response_code"], responses::ok);
    EXPECT_EQ(emulation_context::instance().get_target(), hil_target_kind::hardware);
}

TEST(cores_endpoints, set_hil_target_invalid) {
    auto emulation = fresh_emulator();

    cores_endpoints ep;

    auto resp = ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "fpga"})"));
    EXPECT_EQ(resp["response_code"], responses::invalid_arg);
    resp = ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": 3})"));
    EXPECT_EQ(resp["response_code"], responses::invalid_arg);
    resp = ep.process_command("set_hil_target", nlohmann::json::parse(R"({})"));
    EXPECT_EQ(resp["response_code"], responses::invalid_arg);

    // a request that is not valid does not change the target
    EXPECT_EQ(emulation_context::instance().get_target(), hil_target_kind::hardware);
}

TEST(cores_endpoints, emulator_deploy_start_stop) {
    auto emulation = fresh_emulator();

    cores_endpoints ep;
    ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "emulator"})"));

    auto resp = ep.process_command("deploy_hil", nlohmann::json::parse(counter_spec));
    EXPECT_EQ(resp["response_code"], responses::ok);
    EXPECT_FALSE(resp.contains("data"));
    EXPECT_FALSE(emulation->running());

    resp = ep.process_command("hil_select_out", nlohmann::json::parse(R"({"channel": 0, "output": {"core": "test", "name": "out", "channel": 0}})"));
    EXPECT_EQ(resp["response_code"], responses::ok);

    resp = ep.process_command("hil_start", nlohmann::json());
    EXPECT_EQ(resp["response_code"], responses::ok);
    EXPECT_TRUE(emulation->running());

    // the counter counts one more in every round
    auto samples = take_samples(emulation, 0);
    ASSERT_EQ(samples.size(), scope_accessor::buffer_size);
    for(int i = 1; i < samples.size(); i++) {
        EXPECT_EQ(samples[i], samples[i - 1] + 1);
    }

    resp = ep.process_command("hil_stop", nlohmann::json());
    EXPECT_EQ(resp["response_code"], responses::ok);
    EXPECT_FALSE(emulation->running());
}

TEST(cores_endpoints, emulator_deploy_error) {
    auto emulation = fresh_emulator();

    cores_endpoints ep;
    ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "emulator"})"));

    nlohmann::json command = nlohmann::json::parse(counter_spec);
    command["cores"][0]["order"] = "fs";
    auto resp = ep.process_command("deploy_hil", command);

    // reported the way the deployment on the hardware does
    EXPECT_EQ(resp["response_code"], responses::ok);
    std::string error = resp["data"]["error"];
    EXPECT_TRUE(error.starts_with("HIL DEPLOYMENT ERROR:\n"));
    EXPECT_EQ(resp["data"]["error_code"], responses::deployment_error);

    // there is nothing to run
    EXPECT_NE(ep.process_command("hil_start", nlohmann::json())["response_code"], responses::ok);
}

TEST(cores_endpoints, emulator_set_input) {
    auto emulation = fresh_emulator();

    cores_endpoints ep;
    ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "emulator"})"));
    ep.process_command("deploy_hil", nlohmann::json::parse(constant_spec));
    ep.process_command("hil_select_out", nlohmann::json::parse(R"({"channel": 0, "output": {"core": "test", "name": "out", "channel": 0}})"));
    ep.process_command("hil_start", nlohmann::json());

    auto resp = ep.process_command("hil_set_in", nlohmann::json::parse(R"({"core": "test", "name": "in_c", "channel": 0, "value": 5})"));
    EXPECT_EQ(resp["response_code"], responses::ok);

    // the output is twice the input: it ends up at 10, and the blocks before that are at 2
    bool changed = false;
    for(int i = 0; i < 1000 && !changed; i++) {
        auto samples = take_samples(emulation, 0);
        if(samples.empty()) break;
        changed = samples.back() == 10.0f;
    }
    EXPECT_TRUE(changed);
    ep.process_command("hil_stop", nlohmann::json());
}

TEST(cores_endpoints, emulator_set_input_error) {
    auto emulation = fresh_emulator();

    cores_endpoints ep;
    ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "emulator"})"));

    // nothing is deployed yet
    auto resp = ep.process_command("hil_set_in", nlohmann::json::parse(R"({"core": "test", "name": "in_c", "channel": 0, "value": 5})"));
    EXPECT_EQ(resp["response_code"], responses::invalid_arg);

    ep.process_command("deploy_hil", nlohmann::json::parse(constant_spec));
    resp = ep.process_command("hil_set_in", nlohmann::json::parse(R"({"core": "test", "name": "missing", "channel": 0, "value": 5})"));
    EXPECT_EQ(resp["response_code"], responses::invalid_arg);
    resp = ep.process_command("hil_set_in", nlohmann::json::parse(R"({"core": "test", "name": "in_c", "channel": 1, "value": 5})"));
    EXPECT_EQ(resp["response_code"], responses::invalid_arg);
}

TEST(cores_endpoints, emulator_select_output_error) {
    auto emulation = fresh_emulator();

    cores_endpoints ep;
    ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "emulator"})"));
    ep.process_command("deploy_hil", nlohmann::json::parse(counter_spec));

    auto resp = ep.process_command("hil_select_out", nlohmann::json::parse(R"({"channel": 0, "output": {"core": "test", "name": "missing", "channel": 0}})"));
    EXPECT_EQ(resp["response_code"], responses::invalid_arg);
    // the capture has six channels
    resp = ep.process_command("hil_select_out", nlohmann::json::parse(R"({"channel": 6, "output": {"core": "test", "name": "out", "channel": 0}})"));
    EXPECT_EQ(resp["response_code"], responses::invalid_arg);

    // a request that selects nothing is not an error, as on the hardware
    resp = ep.process_command("hil_select_out", nlohmann::json::parse(R"({"channel": 0})"));
    EXPECT_EQ(resp["response_code"], responses::ok);
}

TEST(cores_endpoints, emulator_sampling_frequency) {
    auto emulation = fresh_emulator();

    cores_endpoints ep;
    ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "emulator"})"));
    ep.process_command("deploy_hil", nlohmann::json::parse(counter_spec));

    auto resp = ep.process_command("get_sampling_frequency", nlohmann::json());
    EXPECT_EQ(resp["response_code"], responses::ok);
    EXPECT_EQ(resp["data"], 1);
}

TEST(cores_endpoints, selecting_the_hardware_stops_the_emulator) {
    auto emulation = fresh_emulator();

    cores_endpoints ep;
    ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "emulator"})"));
    ep.process_command("deploy_hil", nlohmann::json::parse(counter_spec));
    ep.process_command("hil_start", nlohmann::json());
    ASSERT_TRUE(emulation->running());

    ep.process_command("set_hil_target", nlohmann::json::parse(R"({"target": "hardware"})"));
    EXPECT_FALSE(emulation->running());
    EXPECT_EQ(emulation_context::instance().get_target(), hil_target_kind::hardware);
}
