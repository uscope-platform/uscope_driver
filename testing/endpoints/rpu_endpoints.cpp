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

// These tests drive the fake R5 cores of the mock module through the real remoteproc
// interface, they need /lib/firmware/uscope to exist and be writable by the test user.

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <cppcodec/base64_rfc4648.hpp>

#include "server_frontend/endpoints/rpu_endpoints.hpp"
#include "hw_interface/operations/interfaces_dictionary.hpp"

namespace {
    std::vector<uint8_t> fake_elf(size_t size) {
        std::vector<uint8_t> elf(size, 0xA5);
        elf[0] = 0x7f; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
        return elf;
    }

    nlohmann::json load_args(uint64_t core, const std::vector<uint8_t> &program) {
        nlohmann::json args;
        args["core"] = core;
        args["program"] = cppcodec::base64_rfc4648::encode(program);
        return args;
    }

    nlohmann::json core_args(uint64_t core) {
        nlohmann::json args;
        args["core"] = core;
        return args;
    }

    /// size of the last firmware the fake core <core> received (mock module only)
    size_t loaded_firmware_size(int core) {
        std::ifstream ifs("/sys/devices/platform/fffc000000008000.uScope/rpu_fw_size_" + std::to_string(core));
        size_t size = 0;
        ifs >> size;
        return size;
    }

    std::string status(rpu_endpoints &ep, uint64_t core) {
        auto args = core_args(core);
        auto resp = ep.process_command("rpu_status", args);
        EXPECT_EQ(resp["response_code"], responses::ok);
        return resp["data"];
    }

    void stop(rpu_endpoints &ep, uint64_t core) {
        auto args = core_args(core);
        EXPECT_EQ(ep.process_command("rpu_stop", args)["response_code"], responses::ok);
    }
}

TEST(rpu_endpoints, load_program) {
    rpu_endpoints ep;
    stop(ep, 0);

    auto args = load_args(0, fake_elf(4321));
    auto resp = ep.process_command("rpu_load_program", args);
    EXPECT_EQ(resp["response_code"], responses::ok) << resp["data"];

    EXPECT_EQ(status(ep, 0), "running");
    EXPECT_EQ(loaded_firmware_size(0), 4321);
    EXPECT_FALSE(std::filesystem::exists(if_dict.get_rpu_firmware_dir().value() + "/rpu_0.elf"));

    stop(ep, 0);
    EXPECT_EQ(status(ep, 0), "offline");
}

TEST(rpu_endpoints, reload_running_core) {
    rpu_endpoints ep;

    auto first = load_args(1, fake_elf(100));
    ASSERT_EQ(ep.process_command("rpu_load_program", first)["response_code"], responses::ok);
    ASSERT_EQ(status(ep, 1), "running");

    auto second = load_args(1, fake_elf(200));
    auto resp = ep.process_command("rpu_load_program", second);
    EXPECT_EQ(resp["response_code"], responses::ok) << resp["data"];
    EXPECT_EQ(status(ep, 1), "running");
    EXPECT_EQ(loaded_firmware_size(1), 200);

    stop(ep, 1);
}

TEST(rpu_endpoints, stop_offline_core) {
    rpu_endpoints ep;
    stop(ep, 0);
    stop(ep, 0);
    EXPECT_EQ(status(ep, 0), "offline");
}

TEST(rpu_endpoints, reload_stopped_core) {
    rpu_endpoints ep;
    auto first = load_args(0, fake_elf(64));
    ASSERT_EQ(ep.process_command("rpu_load_program", first)["response_code"], responses::ok);
    stop(ep, 0);

    // a stopped core is restarted by loading its program again
    auto second = load_args(0, fake_elf(128));
    auto resp = ep.process_command("rpu_load_program", second);
    EXPECT_EQ(resp["response_code"], responses::ok) << resp["data"];
    EXPECT_EQ(status(ep, 0), "running");
    EXPECT_EQ(loaded_firmware_size(0), 128);
    stop(ep, 0);
}

TEST(rpu_endpoints, not_an_elf) {
    rpu_endpoints ep;
    stop(ep, 0);
    auto args = load_args(0, std::vector<uint8_t>(64, 0x12));
    auto resp = ep.process_command("rpu_load_program", args);
    EXPECT_EQ(resp["response_code"], responses::invalid_arg);
    EXPECT_EQ(status(ep, 0), "offline");
}

TEST(rpu_endpoints, invalid_arguments) {
    rpu_endpoints ep;

    std::vector<nlohmann::json> bad_loads = {
        load_args(2, fake_elf(64)),
        nlohmann::json::parse(R"({"core": 0})"),
        nlohmann::json::parse(R"({"core": 0, "program": 12})"),
        nlohmann::json::parse(R"({"core": 0, "program": "not base 64!"})"),
        nlohmann::json::parse(R"({"core": -1, "program": "f0VMRg=="})"),
        nlohmann::json::parse(R"("f0VMRg==")")
    };
    for(auto &args: bad_loads) {
        EXPECT_EQ(ep.process_command("rpu_load_program", args)["response_code"], responses::invalid_arg) << args;
    }

    for(const std::string cmd: {"rpu_stop", "rpu_status"}) {
        auto args = core_args(5);
        EXPECT_EQ(ep.process_command(cmd, args)["response_code"], responses::invalid_arg) << cmd;
        auto empty = nlohmann::json::object();
        EXPECT_EQ(ep.process_command(cmd, empty)["response_code"], responses::invalid_arg) << cmd;
    }
}
