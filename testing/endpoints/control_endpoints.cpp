//   Copyright 2025 Filippo Savi <filssavi@gmail.com>
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
#include "server_frontend/endpoints/control_endpoints.hpp"
#include "../hil_addresses.hpp"


TEST(control_endpoints, write_single_register) {

    auto command = nlohmann::json::parse(R"(
    {
        "address": 18316525568,
        "value": 3122,
        "type": "direct",
        "proxy_address": 0,
        "proxy_type":""
    })");

    auto ba = std::make_shared<bus_accessor>();

    control_endpoints ep(true);
    ep.set_accessor(ba);
    auto resp = ep.process_command("register_write", command);

    EXPECT_EQ(resp["response_code"], responses::ok);
    auto ops = ba->get_operations();
    EXPECT_GE(ops.size(), 1);
    EXPECT_EQ(ops[0].address[0], 18316525568);
    EXPECT_EQ(ops[0].data[0], 3122);
}


TEST(control_endpoints, write_single_register_non_int_error) {

    auto command = nlohmann::json::parse(R"(
    {
        "address": 18316525568,
        "value": 3122.8,
        "type": "direct",
        "proxy_address": 0,
        "proxy_type":""
    })");

    auto ba = std::make_shared<bus_accessor>();

    control_endpoints ep(true);
    ep.set_accessor(ba);
    auto resp = ep.process_command("register_write", command);

    EXPECT_EQ(resp["response_code"], responses::invalid_arg);
    EXPECT_EQ(resp["data"], "DRIVER ERROR: addresses and values for register writes must be integers\n");
    auto dbg = resp.dump(4);
    auto ops = ba->get_operations();
    EXPECT_GE(ops.size(), 0);
}

TEST(control_endpoints, write_register_non_validating) {

    auto command = nlohmann::json::parse(R"(
    {
        "address": 18316525568,
        "type": "direct",
        "proxy_address": 0,
        "proxy_type":""
    })");

    auto ba = std::make_shared<bus_accessor>();

    control_endpoints ep(true);
    ep.set_accessor(ba);
    auto resp = ep.process_command("register_write", command);

    EXPECT_EQ(resp["response_code"], responses::invalid_arg);
    EXPECT_EQ(resp["data"], "DRIVER ERROR: Invalid arguments for the write register command\nError #1\n  context: <root>\n  desc:   Missing required property 'value'.");
    auto dbg = resp.dump(4);
    auto ops = ba->get_operations();
    EXPECT_GE(ops.size(), 0);
}



TEST(control_endpoints, read_register) {

    auto command = nlohmann::json::parse(R"(18316525568)");

    auto ba = std::make_shared<bus_accessor>();

    control_endpoints ep(true);
    ep.set_accessor(ba);
    auto resp = ep.process_command("register_read", command);

    EXPECT_EQ(resp["response_code"], responses::ok);
    auto ops = ba->get_operations();
    EXPECT_GE(ops.size(), 1);
    EXPECT_EQ(ops[0].address[0], 18316525568);
    EXPECT_EQ(ops[0].data[0], 0);
}




TEST(control_endpoints, read_non_register) {

    auto command = nlohmann::json::parse(R"({"error":true})");

    auto ba = std::make_shared<bus_accessor>();

    control_endpoints ep(true);
    ep.set_accessor(ba);
    auto resp = ep.process_command("register_read", command);

    EXPECT_EQ(resp["response_code"], responses::invalid_arg);
    auto ops = ba->get_operations();
    EXPECT_EQ(resp["data"], "DRIVER ERROR:The argument for the single read register call must be a numeric value\n");
    EXPECT_GE(ops.size(), 0);
}