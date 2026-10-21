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

#include <memory>
#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include "server_frontend/data_plane/scope_frame.hpp"

/// A connected websocket client. Only the newest frame is kept for it: while a write is in progress, newer frames
/// replace the pending one, so a slow client skips frames instead of accumulating old data.
class scope_session : public std::enable_shared_from_this<scope_session> {
public:
    explicit scope_session(boost::asio::ip::tcp::socket socket);

    /// Replaces the frame waiting to be sent. Must be called from the event loop thread
    void push(const scope_frame &f);

    /// Handshake, then serves the client until it disconnects
    static boost::asio::awaitable<void> run(std::shared_ptr<scope_session> self);

private:
    static boost::asio::awaitable<void> write_loop(std::shared_ptr<scope_session> self);
    static void report(const boost::system::system_error &e);
    void close();

    boost::beast::websocket::stream<boost::beast::tcp_stream> ws;
    boost::asio::steady_timer wake;
    scope_frame pending;
    bool closed = false;
};

#endif //USCOPE_DRIVER_SCOPE_SESSION_HPP
