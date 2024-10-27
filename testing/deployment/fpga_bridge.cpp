//   Copyright 2024 Filippo Savi <filssavi@gmail.com>
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


#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include "hw_interface/fpga_bridge.hpp"


TEST(fpga_bridge, single_write_register_direct) {
    nlohmann::json spec_json = nlohmann::json::parse(R"(
    {
        "address":1136918532,
        "proxy_address":0,
        "proxy_type": "",
        "type": "direct",
        "value": 423
    }
    )");

    if_dict.set_arch("testing");
    runtime_config.emulate_hw = true;

    fpga_bridge bridge;

    auto res = bridge.single_write_register(spec_json);
    auto ops = bridge.get_bus_operations()[0];
    std::vector<uint64_t> ref_addr = {1136918532};

    EXPECT_EQ(ops.address, ref_addr);
    EXPECT_EQ(ops.type, "w");
    EXPECT_EQ(ops.data[0], 423);

    EXPECT_EQ(res,responses::ok);
}


TEST(fpga_bridge, single_write_register_axis_proxied) {
    nlohmann::json spec_json = nlohmann::json::parse(R"(
    {
        "address":1136918532,
        "proxy_address":76342,
        "proxy_type": "axis_constant",
        "type": "proxied",
        "value": 222
    }
    )");

    if_dict.set_arch("testing");
    runtime_config.emulate_hw = true;

    fpga_bridge bridge;

    auto res = bridge.single_write_register(spec_json);
    auto ops = bridge.get_bus_operations()[0];
    std::vector<uint64_t> ref_addr = {1136918532, 76342};

    EXPECT_EQ(ops.address, ref_addr);
    EXPECT_EQ(ops.type, "w");
    EXPECT_EQ(ops.data[0], 222);

    EXPECT_EQ(res,responses::ok);
}


TEST(fpga_bridge, single_write_type_error) {
    nlohmann::json spec_json = nlohmann::json::parse(R"(
    {
        "address":1136918532,
        "proxy_address":0,
        "proxy_type": "",
        "type": "directasd",
        "value": 423
    }
    )");

    if_dict.set_arch("testing");
    runtime_config.emulate_hw = true;

    fpga_bridge bridge;

    try{
        auto res = bridge.single_write_register(spec_json);
        EXPECT_TRUE(false); // The call should throw and exception
    } catch (std::runtime_error &e){
        std::string err = e.what();
        EXPECT_EQ(err,"ERROR: Unrecognised register write type");
    }

}



TEST(fpga_bridge, single_write_proxy_error) {
    nlohmann::json spec_json = nlohmann::json::parse(R"(
    {
        "address":1136918532,
        "proxy_address":0,
        "proxy_type": "fasf",
        "type": "proxied",
        "value": 423
    }
    )");

    if_dict.set_arch("testing");
    runtime_config.emulate_hw = true;

    fpga_bridge bridge;

    try{
        auto res = bridge.single_write_register(spec_json);
        EXPECT_TRUE(false); // The call should throw and exception
    } catch (std::runtime_error &e){
        std::string err = e.what();
        EXPECT_EQ(err,"ERROR: Unrecognised write proxy type");
    }

}

TEST(fpga_bridge, single_write_register_read) {

    if_dict.set_arch("testing");
    runtime_config.emulate_hw = true;

    fpga_bridge bridge;

    auto res = bridge.single_read_register(5432);
    auto ops = bridge.get_bus_operations()[0];

    EXPECT_LT(res["data"], 100);
    EXPECT_GT(res["data"], 0);
    EXPECT_EQ(res["response_code"], responses::ok);

    EXPECT_EQ(ops.address[0], 5432);
    EXPECT_EQ(ops.type, "r");
}


TEST(fpga_bridge, load_program) {

    if_dict.set_arch("testing");
    runtime_config.emulate_hw = true;

    fpga_bridge bridge;

    std::vector<uint32_t> prog = {34313124,4231,11,23141,12};

    auto res = bridge.apply_program(64235, prog);
    auto ops = bridge.get_bus_operations()[0];

    std::vector<uint64_t> prog_ref = {34313124,4231,11,23141,12};
    std::vector<uint64_t> prog_res = ops.data;

    EXPECT_EQ(res, responses::ok);

    EXPECT_EQ(ops.address[0], 64235);
    EXPECT_EQ(ops.type, "p");
    EXPECT_EQ(prog_res, prog_ref);
}

TEST(fpga_bridge, set_clock) {

    if_dict.set_arch("testing");
    runtime_config.emulate_hw = true;

    fpga_bridge bridge;

    std::ofstream o(if_dict.get_clock_if(2));
    auto resp = bridge.set_pl_clock(2, 1200000);
    EXPECT_EQ(resp, responses::ok);
    std::ifstream result_file(if_dict.get_clock_if(2));
    std::string res;
    result_file>>res;
    EXPECT_EQ(res, "1200000");
    std::filesystem::remove(if_dict.get_clock_if(2));
}

TEST(fpga_bridge, set_clock_file_errorr) {

    if_dict.set_arch("testing");
    runtime_config.emulate_hw = true;

    fpga_bridge bridge;

    std::ofstream o(if_dict.get_clock_if(1));
    auto resp = bridge.set_pl_clock(2, 1200000);
    EXPECT_EQ(resp, responses::driver_file_not_found);
}


TEST(fpga_bridge, set_scope_data) {

    if_dict.set_arch("testing");
    runtime_config.emulate_hw = true;

    fpga_bridge bridge;

    std::ofstream o(if_dict.get_buffer_address_if());
    o<< "124786"<<std::endl;
    o.close();


    runtime_config.emulate_hw = false;

    auto resp = bridge.set_scope_data(1200);
    auto ops = bridge.get_bus_operations()[0];

    std::vector<uint64_t> ref_addr = {1200};

    EXPECT_EQ(ops.address, ref_addr);
    EXPECT_EQ(ops.type, "w");
    EXPECT_EQ(ops.data[0], 124786);
    EXPECT_EQ(resp, responses::ok);

    std::filesystem::remove(if_dict.get_buffer_address_if());
}

