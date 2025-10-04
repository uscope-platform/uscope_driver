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
#include "server_frontend/endpoints/cores_endpoints.hpp"
#include "../hil_addresses.hpp"


constexpr std::string_view default_hil_spec = R"(
    {
        "version":2,
        "cores": [
            {
                "id": "test",
                "order": 1,
                "inputs": [
                    {
                        "name": "input_1",
                        "is_vector": false,
                        "metadata":{
                            "type": "float",
                            "width":16,
                            "signed":true,
                            "common_io":false
                        },
                        "source": {
                            "type": "constant",
                            "value": [
                                31.2
                            ]
                        }
                    },
                    {
                        "name": "input_2",
                        "is_vector": false,
                        "metadata":{
                            "type": "float",
                            "width":16,
                            "signed":true,
                            "common_io":false
                        },
                        "source": {
                            "type": "constant",
                            "value": [
                                4
                            ]
                        }
                    }
                ],
                "outputs": [
                    {
                        "name": "out",
                        "is_vector": false,
                        "metadata":{
                            "type": "float",
                            "width":16,
                            "signed":true,
                            "common_io":false
                        }
                    }
                ],
                "memory_init": [],
                "channels": 2,
                "options": {
                    "comparators": "reducing",
                    "efi_implementation": "none"
                },
                "program": {
                    "content": "int main(){\n  float input_1;\n  float input_2;\n  float out = input_1 + input_2;\n}",
                    "headers": []
                },
                "sampling_frequency": 1,
                "deployment": {
                    "has_reciprocal": false,
                    "control_address": 18316525568,
                    "rom_address": 17179869184
                }
            }
        ],
        "interconnect": [],
        "emulation_time": 2,
        "deployment_mode": false
    }
    )";


TEST(cores_endpoints, hil_sim_data) {

    nlohmann::json command = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);
    auto resp = ep.process_command("hil_hardware_sim", command);
    auto dbg = resp.dump(4);

    auto control_ref = "18316857356:131073\n18316857420:56\n18316857360:268636161\n18316857424:56\n18316857344:2\n18316861452:0\n18316861448:3\n18316861440:1106876826\n18316861452:65536\n18316861448:65539\n18316861440:1106876826\n18316861452:1\n18316861448:2\n18316861440:1082130432\n18316861452:65537\n18316861448:65538\n18316861440:1082130432\n18316595204:0\n18316591112:2\n18316591108:100000000\n18316595200:1\n18316853248:11\n18316656640:1\n";
    auto outputs_ref = "2:test.out[0]\n65539:test.out[1]\n";
    auto rom_ref = "21474836480:131076\n21474836484:12\n21474836488:196609\n21474836492:65538\n21474836496:131075\n21474836500:12\n21474836504:12\n21474836508:395329\n21474836512:12\n";
    auto inputs_ref = "test[0].input_1,18316861440,3,0,0\ntest[0].input_2,18316861440,2,1,0\ntest[1].input_1,18316861440,65539,65536,0\ntest[1].input_2,18316861440,65538,65537,0\n";

    std::string rom_res = resp["data"]["code"];
    std::string control_res = resp["data"]["control"];
    std::string outputs_res = resp["data"]["outputs"];
    std::string inputs_res = resp["data"]["inputs"];

    EXPECT_EQ(rom_res, rom_ref);
    EXPECT_EQ(outputs_res, outputs_ref);
    EXPECT_EQ(inputs_res, inputs_ref);
    EXPECT_EQ(control_res, control_ref);
}

TEST(cores_endpoints, deployed_hil_sim_data) {

    nlohmann::json command = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);
    ep.process_command("deploy_hil", command);
    auto resp = ep.process_command("hil_hardware_sim", command);

    auto control_ref = "18316857356:131073\n18316857420:56\n18316857360:268636161\n18316857424:56\n18316857344:2\n18316861452:0\n18316861448:3\n18316861440:1106876826\n18316861452:65536\n18316861448:65539\n18316861440:1106876826\n18316861452:1\n18316861448:2\n18316861440:1082130432\n18316861452:65537\n18316861448:65538\n18316861440:1082130432\n18316595204:0\n18316591112:2\n18316591108:100000000\n18316595200:1\n18316853248:11\n";
    auto outputs_ref = "2:test.out[0]\n65539:test.out[1]\n";
    auto rom_ref = "21474836480:131076\n21474836484:12\n21474836488:196609\n21474836492:65538\n21474836496:131075\n21474836500:12\n21474836504:12\n21474836508:395329\n21474836512:12\n";
    auto inputs_ref = "test[0].input_1,18316861440,3,0,0\ntest[0].input_2,18316861440,2,1,0\ntest[1].input_1,18316861440,65539,65536,0\ntest[1].input_2,18316861440,65538,65537,0\n";

    std::string rom_res = resp["data"]["code"];
    std::string control_res = resp["data"]["control"];
    std::string outputs_res = resp["data"]["outputs"];
    std::string inputs_res = resp["data"]["inputs"];

    EXPECT_EQ(rom_res, rom_ref);
    EXPECT_EQ(outputs_res, outputs_ref);
    EXPECT_EQ(inputs_res, inputs_ref);
    EXPECT_EQ(control_res, control_ref);
}


TEST(cores_endpoints, cache_invalidation_hil_sim_data) {

    nlohmann::json command = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);
    ep.process_command("deploy_hil", command);
    command["cores"][0]["inputs"][0]["source"]["value"] = 344;
    auto dbg = command.dump(4);
    auto resp = ep.process_command("hil_hardware_sim", command);

    auto control_ref = "18316857356:131073\n18316857420:56\n18316857360:268636161\n18316857424:56\n18316857344:2\n18316861452:0\n18316861448:3\n18316861440:1135345664\n18316861452:65536\n18316861448:65539\n18316861440:1135345664\n18316861452:1\n18316861448:2\n18316861440:1082130432\n18316861452:65537\n18316861448:65538\n18316861440:1082130432\n18316595204:0\n18316591112:2\n18316591108:100000000\n18316595200:1\n18316853248:11\n18316656640:1\n";
    auto outputs_ref = "2:test.out[0]\n65539:test.out[1]\n";
    auto rom_ref = "21474836480:131076\n21474836484:12\n21474836488:196609\n21474836492:65538\n21474836496:131075\n21474836500:12\n21474836504:12\n21474836508:395329\n21474836512:12\n";
    auto inputs_ref = "test[0].input_1,18316861440,3,0,0\ntest[0].input_2,18316861440,2,1,0\ntest[1].input_1,18316861440,65539,65536,0\ntest[1].input_2,18316861440,65538,65537,0\n";

    std::string rom_res = resp["data"]["code"];
    std::string control_res = resp["data"]["control"];
    std::string outputs_res = resp["data"]["outputs"];
    std::string inputs_res = resp["data"]["inputs"];

    EXPECT_EQ(rom_res, rom_ref);
    EXPECT_EQ(outputs_res, outputs_ref);
    EXPECT_EQ(inputs_res, inputs_ref);
    EXPECT_EQ(control_res, control_ref);
}

TEST(cores_endpoints, deploy_hil) {
    nlohmann::json command = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);
    auto resp = ep.process_command("deploy_hil", command);

    EXPECT_EQ(resp["response_code"], responses::ok);
    auto ops = ba->get_operations();
    EXPECT_GE(ops.size(), 20);
};


TEST(cores_endpoints, emulate_hil) {

    nlohmann::json command = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);
    auto resp = ep.process_command("emulate_hil", command);
    EXPECT_EQ(resp["response_code"], responses::ok);
    std::string raw_result = resp["data"]["results"];
    auto emulation_result = nlohmann::json::parse(raw_result);
    auto dbg = emulation_result.dump(4);
    auto data = emulation_result["test"]["outputs"]["out"]["0"][0];
    std::vector<float> expected_data = {35.2, 35.2};
    EXPECT_EQ(data, expected_data);
    data = emulation_result["test"]["outputs"]["out"]["1"][0];
    EXPECT_EQ(data, expected_data);
}


TEST(cores_endpoints, start_hil) {
    nlohmann::json command = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);
    ep.process_command("deploy_hil", command);
    auto resp = ep.process_command("hil_start",{});

    EXPECT_EQ(resp["response_code"], responses::ok);
    auto dbg = resp.dump(4);
    auto ops = ba->get_operations();
    EXPECT_GE(ops.size(), 20);
    EXPECT_EQ(ops[23].address[0], addr_map_v2["bases"]["hil_control"]);
    EXPECT_EQ(ops[23].data[0], 1);
}



TEST(cores_endpoints, stop_hil) {
    nlohmann::json command = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);
    ep.process_command("deploy_hil", command);
    auto resp = ep.process_command("hil_stop",{});

    EXPECT_EQ(resp["response_code"], responses::ok);
    auto ops = ba->get_operations();
    EXPECT_GE(ops.size(), 20);
    EXPECT_EQ(ops[23].address[0], addr_map_v2["bases"]["hil_control"]);
    EXPECT_EQ(ops[23].data[0], 0);
}


TEST(cores_endpoints, set_input) {
    nlohmann::json command = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);
    ep.process_command("deploy_hil", command);
    command = nlohmann::json::parse(R"(
        {
            "name":"input_1",
            "channel":0,
            "core":"test",
            "value":132.3
        }
    )");
    auto resp = ep.process_command("hil_set_in", command);

    EXPECT_EQ(resp["response_code"], responses::ok);
    auto ops = ba->get_operations();
    EXPECT_GE(ops.size(), 20);
    EXPECT_EQ(ops[25].address[0], 0x443c52000);
    EXPECT_EQ(ops[25].data[0], 0x43044CCD);
}

TEST(cores_endpoints, select_output) {
    nlohmann::json command = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);
    ep.process_command("deploy_hil", command);
    command = nlohmann::json::parse(R"(
        {
            "channel":3,
            "output": {
                "channel":0,
                "core":"test",
                "name":"out"
            }
        }
    )");
    auto resp = ep.process_command("hil_select_out",command);

    EXPECT_EQ(resp["response_code"], responses::ok);
    auto ops = ba->get_operations();
    EXPECT_GE(ops.size(), 20);
    EXPECT_EQ(ops[23].address[0], 0x443c00010);
    EXPECT_EQ(ops[23].data[0], 2);
}


TEST(cores_endpoints, get_address_map) {
    nlohmann::json command = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);
    auto resp = ep.process_command("get_hil_address_map", {});

    auto dbg = resp.dump(4);

    EXPECT_EQ(resp["response_code"], responses::ok);
    EXPECT_EQ(resp["data"], addr_map_v2);
}

TEST(cores_endpoints, hil_debug) {

    nlohmann::json spec = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);

    nlohmann::json payload;
    payload["command"] = "initialize";
    payload["arguments"] = spec;
    auto resp = ep.process_command("hil_debug", payload);
    auto dbg = resp.dump(4);
    EXPECT_EQ(resp["response_code"], responses::ok);
}


TEST(cores_endpoints, hil_disassemble) {

    nlohmann::json command = nlohmann::json::parse(default_hil_spec);


    auto ba = std::make_shared<bus_accessor>();


    cores_endpoints ep(true);
    ep.set_accessor(ba);
    ep.process_command("set_hil_address_map", addr_map_v2);
    auto resp = ep.process_command("hil_disassemble", command);
    auto out = resp.dump(0);
    EXPECT_EQ(resp["response_code"], responses::ok);
    auto expected_out = "{\n\"data\": {\n\"test\": {\n\"common_io_translation_table\": [],\n\"program\": \"add r2, r1, r3\\nstop\\n\",\n\"translation_table\": [\n[\n1,\n{\n\"address\": 3,\n\"name\": \"out\"\n}\n],\n[\n2,\n{\n\"address\": 1,\n\"name\": \"input_2\"\n}\n],\n[\n3,\n{\n\"address\": 2,\n\"name\": \"input_1\"\n}\n]\n]\n}\n},\n\"response_code\": 1\n}";
    EXPECT_EQ(out, expected_out);
}