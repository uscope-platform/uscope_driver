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


#ifndef USCOPE_DRIVER_SCOPE_SESSION_HPP
#define USCOPE_DRIVER_SCOPE_SESSION_HPP

#include <deque>
#include <memory>
#include <string>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <nlohmann/json.hpp>

#include "server_frontend/data_plane/scope_controller.hpp"
#include "server_frontend/data_plane/scope_frame.hpp"
#include "server_frontend/data_plane/scope_tickets.hpp"

/// A connected websocket client. Only the newest frame is kept for it: while a write is in progress, newer frames
/// replace the pending one, so a slow client skips frames instead of accumulating old data.
class scope_session : public std::enable_shared_from_this<scope_session> {
public:
    scope_session(boost::asio::ip::tcp::socket socket, std::shared_ptr<scope_tickets> tickets,
                  std::shared_ptr<scope_controller> controller);

    /// Replaces the frame waiting to be sent. Must be called from the event loop thread
    void push(const scope_frame &f);

    /// Queues a text message for the client. Must be called from the event loop thread
    void send_reply(std::string reply);

    /// Handshake, then serves the client until it disconnects
    static boost::asio::awaitable<void> run(std::shared_ptr<scope_session> self);

private:
    static boost::asio::awaitable<void> handle_command(std::shared_ptr<scope_session> self, std::string message);
    static boost::asio::awaitable<void> write_loop(std::shared_ptr<scope_session> self);
    static void report(const boost::system::system_error &e);
    void close();

    boost::beast::websocket::stream<boost::beast::tcp_stream> ws;
    boost::asio::steady_timer wake;
    scope_frame pending;
    std::shared_ptr<scope_tickets> tickets;
    std::shared_ptr<scope_controller> controller;
    std::deque<std::string> replies;
    static constexpr size_t max_command_size = 64 * 1024;
    std::string role;  ///< role of the user the connection ticket was issued for
    bool closed = false;
};

#endif //USCOPE_DRIVER_SCOPE_SESSION_HPP
