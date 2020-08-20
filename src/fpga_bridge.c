// Copyright (C) 2020 Filippo Savi - All Rights Reserved

// This file is part of uscope_driver.

// uscope_driver is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 3 of the
// License.

// uscope_driver is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with uscope_driver.  If not, see <https://www.gnu.org/licenses/>.

#include "fpga_bridge.h"

extern bool debug_mode;

///  Helper function converting byte aligned addresses to array indices
/// \param address to convert
/// \return converted address
uint32_t address_to_index(uint32_t address){
    return(address - BASE_ADDR)/4;
}


/// This function initializes the fpga_bridge, mapping the AXI channels to memory through /dev/mem
void init_fpga_bridge(){

    volatile uint32_t* return_value;
    if(!debug_mode){
        if((regs_fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) FATAL;
        registers = (uint32_t*) mmap(0, 4096, PROT_READ |PROT_WRITE, MAP_SHARED, regs_fd, BASE_ADDR);
        if(registers < 0) {
            fprintf(stderr, "Cannot mmap uio device: %s\n",
                    strerror(errno));
        }
    } else{
        if((regs_fd = open("/dev/zero", O_RDWR | O_SYNC)) == -1) FATAL;
        registers = (uint32_t*) mmap(0, 4096, PROT_READ |PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1,0);
        if(registers < 0) {
            fprintf(stderr, "Cannot mmap uio device: %s\n",
                    strerror(errno));
        }
    }


}

/// This function loads a bitstrem by name through the linux kernel fpga_manager interface
/// \param bitstream Name of the bitstream to load
/// \return #RESP_OK if the file is found #RESP_ERR_BITSTREAM_NOT_FOUND otherwise
int load_bitstream(char *bitstream){
    if(debug_mode){
        printf("LOAD BITSTREAM: %s\n", bitstream);
        return RESP_OK;
    } else{
        system("echo 0 > /sys/class/fpga_manager/fpga0/flags");
        char filename[100];
        sprintf(filename, "/lib/firmware/%s", bitstream);
        struct stat buffer;

        if(stat (filename, &buffer) == 0){
            char command[100];
            sprintf(command, "echo %s > /sys/class/fpga_manager/fpga0/firmware", bitstream);
            system(command);
            return RESP_OK;
        } else {
            printf("ERROR");
            return RESP_ERR_BITSTREAM_NOT_FOUND;
        }
    }


}
/// Write to a single register
/// \param address Address to write to
/// \param value Value to write
/// \return #RESP_OK
int single_write_register(uint32_t address, uint32_t value){
    if(debug_mode) printf("WRITE SINGLE REGISTER: addr %x   value %u \n", address, value);

    int addr = address_to_index(address);

    registers[addr] = value;
    return RESP_OK;
}

/// Read a single register
/// \param address Address of the register to read
/// \param value Pointer where the read value will be put
/// \return #RESP_OK
int single_read_register(uint32_t address, volatile uint32_t *value){
    if(debug_mode) printf("READ SINGLE REGISTER: addr %x \n", address);

    uint32_t int_value = registers[address_to_index(address)];

    value = &int_value;

    return RESP_OK;
}

/// Write to multiple register in a single "Transaction"
/// \param address Pointer to the array of addresses to write to
/// \param value Pointer to the array of values to write
/// \param n_registers Number of registers to write
/// \return #RESP_OK
int bulk_write_register(uint32_t *address, volatile uint32_t *value, volatile uint32_t n_registers){
    if(debug_mode) printf("WRITE BULK REGISTERS\n");

    for(uint32_t i = 0; i< n_registers; i++){
        uint32_t current_addr = address_to_index(address[i]);
        registers[current_addr] = value[i];
    }
    return RESP_OK;
}

/// Read multiple addresses in a single "Transaction"
/// \param address Pointer to the array of addresses to write to
/// \param value Pointer to the array of values to write
/// \param n_registers Number of registers to write
/// \return #RESP_OK
int bulk_read_register(uint32_t *address, volatile uint32_t *value, volatile uint32_t n_registers){
    if(debug_mode) printf("READ BULK REGISTERS\n");

    for(uint32_t i = 0; i< n_registers; i++){
        uint32_t current_addr = address_to_index(address[i]);
        uint32_t int_value = registers[current_addr];
        value[i] = int_value;
    }
    return RESP_OK;
}

///  Start scope data capture
/// \param n_buffers Number of buffers to capture
/// \return #RESP_OK
int start_capture(uint32_t n_buffers){
    if(debug_mode) printf("START CAPTURE: n_buffers %u\n", n_buffers);

    start_capture_mode(n_buffers);
    return RESP_OK;
}

/// Write to a single address through a proxy
/// \param proxy_address Address of the proxy
/// \param reg_address Address of the register to write to
/// \param value Value to write
/// \return #RESP_OK
int single_proxied_write_register(uint32_t proxy_address,uint32_t reg_address, uint32_t value){
    if(debug_mode) printf("WRITE SINGLE PROXIED REGISTER: proxy address %x   register address %x  value %u \n", proxy_address,reg_address, value);


    registers[address_to_index(proxy_address)] = value;
    registers[address_to_index(proxy_address)+1] = reg_address;
    return RESP_OK;
}

/// Read scope data if ready
/// \param read_data pointer to the array the data will be put in
/// \return #RESP_OK if data is ready #RESP_DATA_NOT_READY otherwise
int read_data(int32_t * read_data){

    if(debug_mode) printf("READ DATA \n");


    int response;
    if(scope_data_ready) {
        memcpy(read_data, scope_data_buffer, 1024* sizeof(int32_t));
        response = RESP_OK;
    } else{
        response = RESP_DATA_NOT_READY;
    }

    scope_data_ready = false;
    return response;
}