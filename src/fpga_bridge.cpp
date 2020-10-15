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

#include "fpga_bridge.hpp"


fpga_bridge::fpga_bridge(const std::string& driver_file, unsigned int dma_buffer_size, bool debug, bool log) : scope_handler(driver_file,dma_buffer_size, debug, log) {

    if(log){
        std::cout << "fpga_bridge initialization started"<< std::endl;
    }

    debug_mode = debug;
    log_enabled = log;
    std::string file_path;
    if(!debug){
        if((devmem_fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1){
            std::cerr << "Error at line "<< __LINE__ <<", file "<< __FILE__ << " ("<<errno<<") [" << strerror(errno)<<"]" <<std::endl;
            exit(1);
        }
        registers = (uint32_t*) mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, devmem_fd, REGISTERS_BASE_ADDR);
        if(registers == MAP_FAILED) {
            std::cerr << "Cannot mmap /dev/mem: "<< strerror(errno) << std::endl;
            exit(1);
        }

        fCore = (uint32_t*) mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, devmem_fd, FCORE_BASE_ADDR);
        if(registers == MAP_FAILED) {
            std::cerr << "Cannot mmap /dev/mem: "<< strerror(errno) << std::endl;
            exit(1);
        }

    } else {
        registers = (uint32_t*) mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if(registers == MAP_FAILED) {
            std::cerr << "Cannot create /dev/mem emulator anonymous mapping: "<< strerror(errno) << std::endl;
            exit(1);
        }
    }

    if(log){
        std::cout << "fpga_bridge initialization started"<< std::endl;
    }
}

/// This method loads a bitstrem by name through the linux kernel fpga_manager interface
/// \param bitstream Name of the bitstream to load
/// \return #RESP_OK if the file is found #RESP_ERR_BITSTREAM_NOT_FOUND otherwise
int fpga_bridge::load_bitstream(const std::string& bitstream) {
    if(log_enabled) std::cout << "LOAD BITSTREAM: " << bitstream<<std::endl;

    if(!debug_mode){
        system("echo 0 > /sys/class/fpga_manager/fpga0/flags");

        std::string filename = "/lib/firmware/" + bitstream;
        //struct stat buffer;
        if(log_enabled) std::cout << filename << std::endl;
        
        if(std::filesystem::exists(filename)){
            std::string command = "echo " + bitstream + " > /sys/class/fpga_manager/fpga0/firmware";
            if(log_enabled) std::cout << command << std::endl;

            system(command.c_str());
            return RESP_OK;
        } else {
            std::cerr << "ERROR: Bitstream not found" << filename << std::endl;
            return RESP_ERR_BITSTREAM_NOT_FOUND;
        }
    } else{
        return RESP_OK;
    }
}

/// Write to a single register
/// \param address Address to write to
/// \param value Value to write
/// \return #RESP_OK
int fpga_bridge::single_write_register(uint32_t address, uint32_t value) {
    if(log_enabled) std::cout << "WRITE SINGLE REGISTER: addr "<< std::hex<< address <<"   value "<< std::dec<< value<<std::endl;
    registers[address_to_index(address)] = value;
    return RESP_OK;
}

/// Read a single register
/// \param address Address of the register to read
/// \param value Pointer where the read value will be put
/// \return #RESP_OK
int fpga_bridge::single_read_register(uint32_t address,  std::vector<uint32_t> &value) {
    uint32_t int_value;

    if(debug_mode) {
        if(log_enabled) std::cout << "READ SINGLE REGISTER: addr "<< std::hex << address <<std::endl;
        int_value = rand() % 32767;
    } else{
        int_value = registers[address_to_index(address)];
    }

    value.push_back(int_value);

    return RESP_OK;
}

/// Write to multiple register in a single "Transaction"
/// \param address Pointer to the array of addresses to write to
/// \param value Pointer to the array of values to write
/// \param n_registers Number of registers to write
/// \return #RESP_OK
int fpga_bridge::bulk_write_register(std::vector<uint32_t> address, std::vector<uint32_t> value) {
    if(log_enabled) std::cout << "WRITE BULK REGISTERS"<<std::endl;

    for(int i=0;i< address.size(); i++){
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
int fpga_bridge::bulk_read_register(std::vector<uint32_t> address, std::vector<uint32_t> value) {
    if(log_enabled) std::cout << "READ BULK REGISTERS"<<std::endl;

    for(unsigned int addr : address){
        uint32_t current_addr = address_to_index(addr);
        uint32_t int_value = registers[current_addr];
        value.push_back(int_value);
    }
    return RESP_OK;
}

///  Start scope data capture
/// \param n_buffers Number of buffers to capture
/// \return #RESP_OK
int fpga_bridge::start_capture(uint32_t n_buffers) {
    if(log_enabled) std::cout << "START CAPTURE: n_buffers "<< n_buffers<<std::endl;
    scope_handler.start_capture(n_buffers);
    return RESP_OK;
}

/// Write to a single address through a proxy
/// \param proxy_address Address of the proxy
/// \param reg_address Address of the register to write to
/// \param value Value to write
/// \return #RESP_OK
int fpga_bridge::single_proxied_write_register(uint32_t proxy_address, uint32_t reg_address, uint32_t value) {
    if(log_enabled)
        std::cout << "WRITE SINGLE PROXIED REGISTER: proxy address "<< std::hex << proxy_address<<"   register address "<< std::hex <<reg_address<<"  value "<<std::dec<<value<<std::endl;
    registers[address_to_index(proxy_address)] = value;
    registers[address_to_index(proxy_address)+1] = reg_address;
    return RESP_OK;
}

/// Read scope data if ready
/// \param read_data pointer to the array the data will be put in
/// \return #RESP_OK if data is ready #RESP_DATA_NOT_READY otherwise
int fpga_bridge::read_data(std::vector<uint32_t> &read_data) {
    if(debug_mode){
        if(log_enabled) std::cout << "READ DATA" << std::endl;
        scope_handler.read_data(read_data);
        return RESP_OK;
    }
    int response;
    if(scope_handler.is_data_ready()) {
        scope_handler.read_data(read_data);
        response = RESP_OK;
    } else{
        response = RESP_DATA_NOT_READY;
    }
    return response;
}



///  Helper function converting byte aligned addresses to array indices
/// \param address to convert
/// \return converted address
uint32_t fpga_bridge::address_to_index(uint32_t address) {
    return (address - REGISTERS_BASE_ADDR) / 4;
}

unsigned int fpga_bridge::check_capture_progress() {
    return scope_handler.check_capture_progress();
}

void fpga_bridge::stop_scope() {
    scope_handler.stop_thread();
}


int fpga_bridge::set_channel_status(std::vector<bool> status) {
    scope_handler.set_channel_status(status);
    return 0;
}

int fpga_bridge::apply_program(uint32_t address, std::vector<uint32_t> program) {
    std::cout<< "APPLY PROGRAM: address: " << address << " program: ";
    for(auto&item:program){
        std::cout<< item<< ", <";
    }
    std::cout<< std::endl;
    return 0;
}



