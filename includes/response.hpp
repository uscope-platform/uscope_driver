//
// Created by fils on 21/08/2020.
//

#ifndef USCOPE_DRIVER_RESPONSE_HPP
#define USCOPE_DRIVER_RESPONSE_HPP

#include <cstdint>
#include <vector>
#include "commands.hpp"

class response {
public:
    uint16_t opcode = C_NULL_COMMAND; /// opcode of the command
    uint16_t return_code = RESP_OK; /// command completion code
    std::vector<uint32_t> body = {}; /// optional body of the response

};

#endif //USCOPE_DRIVER_RESPONSE_HPP
