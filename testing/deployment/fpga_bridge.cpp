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

    nlohmann::json spec_json = nlohmann::json::parse("{\"address\":1136918532,\"proxy_address\":0,\"proxy_type\":\"\",\"type\":\"direct\",\"value\":231}");

    if_dict.set_arch("zynq");
    runtime_config.emulate_hw = true;

    fpga_bridge bridge;

    auto res = bridge.single_write_register(spec_json);
    auto ops = bridge.get_bus_operations()[0];
    std::vector<uint64_t> ref_addr = {1136918532};

    EXPECT_EQ(ops.address, ref_addr);
    EXPECT_EQ(ops.type, "w");
    EXPECT_EQ(ops.data, 231);

    EXPECT_EQ(res,responses::ok);
}



