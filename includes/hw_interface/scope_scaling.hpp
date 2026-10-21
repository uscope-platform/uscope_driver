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

#ifndef USCOPE_DRIVER_SCOPE_SCALING_HPP
#define USCOPE_DRIVER_SCOPE_SCALING_HPP

#include <array>
#include <atomic>
#include <vector>

#include "hw_interface/bus/scope_accessor.hpp"

/// Scaling factor of each scope channel. It is written by the command handlers and read by the scope data reader,
/// which run on different threads.
class scope_scaling {
public:
    scope_scaling() {
        for (auto &f : factors) f = 1.0f;
    }

    /// Channels without a factor in the input keep their current one
    void set(const std::vector<float> &new_factors) {
        for (size_t i = 0; i < factors.size() && i < new_factors.size(); i++) factors[i] = new_factors[i];
    }

    std::array<float, scope_accessor::n_channels> get() const {
        std::array<float, scope_accessor::n_channels> ret{};
        for (size_t i = 0; i < ret.size(); i++) ret[i] = factors[i];
        return ret;
    }
private:
    std::array<std::atomic<float>, scope_accessor::n_channels> factors;
};

#endif //USCOPE_DRIVER_SCOPE_SCALING_HPP
