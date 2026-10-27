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

#ifndef USCOPE_DRIVER_EMULATED_SCOPE_READER_HPP
#define USCOPE_DRIVER_EMULATED_SCOPE_READER_HPP

#include <functional>
#include <memory>
#include <stop_token>
#include <thread>

#include "emulator/live_emulation.hpp"
#include "hw_interface/scope_scaling.hpp"
#include "server_frontend/data_plane/scope_frame.hpp"

/// Publishes the blocks of the emulation as frames, in the same format scope_reader produces them from the data of
/// the board. The emulation hands over values and what they are, so they are converted straight to the frame.
/// It knows nothing about who consumes the frames.
class emulated_scope_reader {
public:
    /// Called from the reader thread for every frame
    using frame_sink = std::function<void(scope_frame)>;

    /// Starts the reader thread, which is stopped and joined by the destructor
    emulated_scope_reader(std::shared_ptr<fcore::live_emulation> emulation, std::shared_ptr<scope_scaling> scaling, frame_sink sink);
    ~emulated_scope_reader();

    emulated_scope_reader(const emulated_scope_reader &) = delete;
    emulated_scope_reader &operator=(const emulated_scope_reader &) = delete;

private:
    void read_loop(std::stop_token stop);

    std::shared_ptr<fcore::live_emulation> emulation;
    std::shared_ptr<scope_scaling> scaling;
    frame_sink sink;
    std::jthread thread;  // last, so that it is started after and joined before the other members
};

#endif //USCOPE_DRIVER_EMULATED_SCOPE_READER_HPP
