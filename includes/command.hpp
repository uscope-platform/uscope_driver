//
// Created by fils on 21/08/2020.
//

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
