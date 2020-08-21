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
    uint32_t opcode = C_NULL_COMMAND; /// opcode of the command
    uint32_t return_code = RESP_OK; /// command completion code
    std::vector<uint32_t> body = {}; /// optional body of the response

};

#endif //USCOPE_DRIVER_RESPONSE_HPP