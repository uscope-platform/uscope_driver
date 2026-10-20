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
#include <array>
#include <cstring>
#include <cstdlib>

#include "server_frontend/infrastructure/server_connector.hpp"


static void setup_socket_pair(boost::asio::io_context &io, boost::asio::ip::tcp::socket &server_side, boost::asio::ip::tcp::socket &client_side) {
    boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
    acceptor.async_accept(server_side, [](const boost::system::error_code &) {});
    client_side.connect(acceptor.local_endpoint());
    io.run();
}


TEST(server_connector, oversized_message_rejected) {
    setenv("ARCH", "zynqmp", 1);
    boost::asio::io_context io;
    boost::asio::ip::tcp::socket server_side(io);
    boost::asio::ip::tcp::socket client_side(io);
    setup_socket_pair(io, server_side, client_side);

    server_connector sc;

    uint32_t oversized = max_msg_size + 1;
    std::array<uint8_t, 4> raw_len{};
    std::memcpy(raw_len.data(), &oversized, sizeof(oversized));
    client_side.send(boost::asio::buffer(raw_len));

    EXPECT_THROW(sc.receive_command(server_side), std::runtime_error);

    client_side.close();
    server_side.close();
}

TEST(server_connector, clean_disconnect_during_length_read) {
    setenv("ARCH", "zynqmp", 1);
    boost::asio::io_context io;
    boost::asio::ip::tcp::socket server_side(io);
    boost::asio::ip::tcp::socket client_side(io);
    setup_socket_pair(io, server_side, client_side);

    server_connector sc;

    client_side.close();
    auto received = sc.receive_command(server_side);
    EXPECT_FALSE(received.has_value());

    server_side.close();
}

TEST(server_connector, valid_message_received) {
    setenv("ARCH", "zynqmp", 1);
    boost::asio::io_context io;
    boost::asio::ip::tcp::socket server_side(io);
    boost::asio::ip::tcp::socket client_side(io);
    setup_socket_pair(io, server_side, client_side);

    server_connector sc;

    nlohmann::json command = {{"cmd", "null"}, {"args", {}}};
    auto payload = nlohmann::json::to_msgpack(command);
    uint32_t size = payload.size();
    std::array<uint8_t, 4> raw_len{};
    std::memcpy(raw_len.data(), &size, sizeof(size));
    client_side.send(boost::asio::buffer(raw_len));
    client_side.send(boost::asio::buffer(payload));

    auto received = sc.receive_command(server_side);
    ASSERT_TRUE(received.has_value());
    EXPECT_EQ((*received)["cmd"], "null");

    client_side.close();
    server_side.close();
}