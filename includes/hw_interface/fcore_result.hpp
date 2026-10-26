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

#ifndef USCOPE_DRIVER_FCORE_RESULT_HPP
#define USCOPE_DRIVER_FCORE_RESULT_HPP

#include <stdexcept>
#include <utility>

#include "errors/result.hpp"

/// The fcore toolchain reports failures through result<T>, the driver reports them with exceptions: this is the
/// boundary between the two. A failure is rethrown as a std::runtime_error carrying the message of the toolchain error.
inline void unwrap(fcore::result<void> r) {
    if (!r) throw std::runtime_error(r.error().what());
}

template<typename T>
T unwrap(fcore::result<T> r) {
    if (!r) throw std::runtime_error(r.error().what());
    return std::move(*r);
}

#endif //USCOPE_DRIVER_FCORE_RESULT_HPP
