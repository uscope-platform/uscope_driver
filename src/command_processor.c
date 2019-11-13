#include "command_processor.h"


uint32_t process_load_bitstream(char *operand){
    return load_bitstream(operand);
}

uint32_t process_single_write_register(char *operand_1, char *operand_2){
    uint32_t address = strtoul(operand_1,NULL, 0);
    uint32_t value = strtoul(operand_2,NULL, 0);
    return single_write_register(address,value);
}

uint32_t process_single_read_register(char *operand_1, response_t * response){
    uint32_t address = strtoul(operand_1,NULL, 0);
    response->body = malloc(sizeof(uint32_t));
    response->body_size = 1;
    return single_read_register(address, response->body);
}

uint32_t process_bulk_write_register(char *operand_1, char *operand_2){

    uint32_t write_addresses[100];
    uint32_t write_data[100];
    uint32_t array_pointer = 0;

    char *addr_sptr, *val_sptr;
    char* addr_tok = strtok_r(operand_1, ",", &addr_sptr);
    char* val_tok = strtok_r(operand_2, ",", &val_sptr);

    while (addr_tok != NULL){
        write_addresses[array_pointer] = strtoul(addr_tok, NULL, 0);
        write_data[array_pointer] = strtoul(val_tok, NULL, 0);
        array_pointer++;

        addr_tok = strtok_r(NULL, ",", &addr_sptr);
        val_tok = strtok_r(NULL, ",", &val_sptr);
    }

    return bulk_write_register(write_addresses, write_data, array_pointer+1);
}

uint32_t process_bulk_read_register(char *operand_1, response_t *response){

    uint32_t read_addresses[100];
    uint32_t array_pointer = 0;

    char *addr_sptr;
    char* addr_tok = strtok_r(operand_1, ",", &addr_sptr);

    while (addr_tok != NULL){
        read_addresses[array_pointer] = strtoul(addr_tok, NULL, 0);
        array_pointer++;
        addr_tok = strtok_r(NULL, ",", &addr_sptr);
    }
    response->body = malloc((array_pointer+1)*sizeof(uint32_t));
    response->body_size = array_pointer+1;
    return bulk_read_register(read_addresses, response->body, array_pointer+1);;
}

uint32_t process_start_capture(char *operand){
    uint32_t n_buffers = strtoul(operand,NULL, 0);
    return start_capture(n_buffers);
}

response_t *process_command(command_t *command){

    response_t *response = malloc(sizeof(response_t));
    response->opcode = command->opcode;
    response->body_size = 0;
    response->body = NULL;
    switch(command->opcode){
        case C_NULL_COMMAND:
            response->return_code = RESP_OK;
            break;
        case C_LOAD_BITSTREAM:
            response->return_code = process_load_bitstream(command->operand_1);
            break;
        case C_SINGLE_REGISTER_READ:
            response->return_code = process_single_read_register(command->operand_1, response);
            break;
        case C_SINGLE_REGISTER_WRITE:
            response->return_code = process_single_write_register(command->operand_1, command->operand_2);
            break;
        case C_BULK_REGISTER_READ:
            response->return_code = process_bulk_read_register(command->operand_1, response);
            break;
        case C_BULK_REGISTER_WRITE:
            response->return_code = process_bulk_write_register(command->operand_1, command->operand_2);
            break;
        case C_START_CAPTURE:
            response->return_code = process_start_capture(command->operand_1);
            break;
    }

    return response;
}

