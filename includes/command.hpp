// Copyright 2021 University of Nottingham Ningbo China
// Author: Filippo Savi <filssavi@gmail.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef USCOPE_DRIVER_COMMAND_HPP
#define USCOPE_DRIVER_COMMAND_HPP

#include <cstdint>
#include <string>

class command {
public:
    uint32_t opcode = C_NULL_COMMAND; /// opcode that indicates what type of command has been sent
    std::string operand_1; /// first command specific data field
    std::string operand_2; /// second command specific data field
};


#endif //USCOPE_DRIVER_COMMAND_HPP
