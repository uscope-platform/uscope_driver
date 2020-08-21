//
// Created by fils on 21/08/2020.
//

#include "response.hpp"


response::response() {
    opcode = 0;
    return_code = RESP_OK;
    body = std::vector<uint32_t>();
}
