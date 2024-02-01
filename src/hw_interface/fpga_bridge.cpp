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

#include "hw_interface/fpga_bridge.hpp"


volatile int registers_fd, fcore_fd;

using namespace std::chrono_literals;

void sigsegv_handler(int dummy) {
    std::cerr << "ERROR:A Segmentation fault happened while writing to an mmapped region" <<std::endl;
    exit(-1);
}

void sigbus_handler(int dummy) {
    std::cerr << "ERROR:A bus error happened while writing to an mmapped region" <<std::endl;
    exit(-1);
}


fpga_bridge::fpga_bridge() {

    if(runtime_config.log){
        std::cout << "fpga_bridge initialization started"<< std::endl;
    }

    arch = std::getenv("ARCH");

    if(arch == "zynqmp"){
        control_addr = ZYNQMP_REGISTERS_BASE_ADDR;
        core_addr = ZYNQMP_FCORE_BASE_ADDR;
    } else {
        control_addr = ZYNQ_REGISTERS_BASE_ADDR;
        core_addr = ZYNQ_FCORE_BASE_ADDR;
    }


    signal(SIGSEGV,sigsegv_handler);
    signal(SIGBUS,sigbus_handler);


    int n_pages_ctrl, n_pages_fcore;

    char const* t = getenv("MMAP_N_PAGES_CTRL");
    if(t == nullptr){
        n_pages_ctrl = 344000;
    } else {
        n_pages_ctrl = std::stoi(std::string(t));
    }

    t = getenv("MMAP_N_PAGES_FCORE");
    if(t == nullptr){
        n_pages_fcore = 344000;
    } else {
        n_pages_fcore = std::stoi(std::string(t));
    }

    std::string file_path;
    if(!runtime_config.emulate_hw){
        registers_fd = open(if_dict.get_control_bus().c_str(), O_RDWR | O_SYNC);
        if(registers_fd == -1){
            std::cerr << "error while mapping the axi control bus (M_GP0)" <<std::endl;
            exit(1);
        }

        fcore_fd = open(if_dict.get_cores_bus().c_str(), O_RDWR | O_SYNC);
        if(fcore_fd == -1){
            std::cerr << "error while mapping the fcore rom bus (M_GP1)" <<std::endl;
            exit(1);
        }

        std::cout << "Mapping " <<std::to_string(n_pages_ctrl)<<" memory pages from the axi control bus, starting at address: "<< to_hex(control_addr) <<std::endl;
        registers = (uint32_t*) mmap(nullptr, n_pages_ctrl*4096, PROT_READ | PROT_WRITE, MAP_SHARED, registers_fd, control_addr);
        if(registers == MAP_FAILED) {
            std::cerr << "Cannot mmap AXI GP0 bus: "<< strerror(errno) << std::endl;
            exit(1);
        }

        std::cout << "Mapping " <<std::to_string(n_pages_ctrl)<<" memory pages from the axi control bus, starting at address: "<< to_hex(core_addr) <<std::endl;
        fCore = (uint32_t*) mmap(nullptr, n_pages_fcore*4096, PROT_READ | PROT_WRITE, MAP_SHARED, fcore_fd, core_addr);
        if(fCore == MAP_FAILED) {
            std::cerr << "Cannot mmap AXI GP1 bus: "<< strerror(errno) << std::endl;
            exit(1);
        }

    } else {
        registers = (uint32_t*) mmap(nullptr, n_pages_ctrl*4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
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
responses::response_code fpga_bridge::load_bitstream(const std::string& bitstream) {
    if(runtime_config.log) std::cout << "LOAD BITSTREAM: " << bitstream<<std::endl;

    if(!runtime_config.emulate_hw){
        std::ofstream ofs("/sys/class/fpga_manager/fpga0/flags");
        ofs << "0";
        ofs.flush();

        std::string filename = "/lib/firmware/" + bitstream;

        if(std::filesystem::exists(filename)){
            ofs = std::ofstream("/sys/class/fpga_manager/fpga0/firmware");
            ofs << bitstream;
            ofs.flush();

            std::string state;
            std::ifstream ifs("/sys/class/fpga_manager/fpga0/state");
            int timeout_counter = 500;
            do {
                std::this_thread::sleep_for(5ms);
                ifs >> state;
                timeout_counter--;
            } while (state != "operating" && timeout_counter>=0);

            if(timeout_counter <0){
                if(runtime_config.log) std::cout << "ERROR: Bitstream load failed to complete in time";
                return responses::bitstream_load_failed;
            } else{
                return responses::ok;
            }
        } else {
            std::cerr << "ERROR: Bitstream not found" << filename << std::endl;
            return responses::bitstream_not_found;
        }
    } else{
        return responses::ok;
    }
}

/// Write to a single register
/// \param address Address to write to
/// \param value Value to write
/// \return #RESP_OK
responses::response_code fpga_bridge::single_write_register(const nlohmann::json &write_obj) {

    if(write_obj["type"] == "direct"){
        if(runtime_config.log) std::cout << "WRITE SINGLE REGISTER (DIRECT) : addr "<< to_hex(write_obj["address"]) <<" value "<< write_obj["value"]<<std::endl;

        registers[register_address_to_index(write_obj["address"])] = write_obj["value"];
    } else if(write_obj["type"] == "proxied") {
         if(write_obj["proxy_type"] == "axis_constant"){
            if(runtime_config.log) std::cout << "WRITE SINGLE REGISTER (AXIS PROXIED) : proxy_addr "<<to_hex(write_obj["proxy_address"]) <<" addr "<< to_hex(write_obj["address"]) <<" value "<< write_obj["value"]<<std::endl;
            uint32_t proxy_addr = write_obj["proxy_address"];
            registers[register_address_to_index(proxy_addr+4)] = write_obj["address"];
            registers[register_address_to_index(proxy_addr)] = write_obj["value"];
        }
    } else {
        throw std::runtime_error("ERROR: Unrecognised register write type");
    }

    return responses::ok;
}




/// Read a single register
/// \param address Address of the register to read
/// \param value Pointer where the read value will be put
/// \return #RESP_OK
nlohmann::json fpga_bridge::single_read_register(uint64_t address) {
    nlohmann::json response_body;
    if(runtime_config.log) std::cout << "READ SINGLE REGISTER: addr "<< to_hex(address) <<std::endl;
    if(runtime_config.emulate_hw) {
        response_body["data"] = rand() % 32767;
    } else{
        response_body["data"] = registers[register_address_to_index(address)];
    }
    response_body["response_code"] = responses::as_integer(responses::ok);
    return response_body;
}

///  Helper function converting byte aligned addresses to array indices
/// \param address to convert
/// \return converted address
uint64_t fpga_bridge::fcore_address_to_index(uint64_t address) const {
    return (address - core_addr) / 4;
}

///  Helper function converting byte aligned addresses to array indices
/// \param address to convert
/// \return converted address
uint64_t fpga_bridge::register_address_to_index(uint64_t address) const {
    return (address - control_addr) / 4;
}

/// Load a program into the specified fCore instance through the AXI bus
/// \param address Address of the fCore instance
/// \param program Vector with the instructions of the program to load
/// \return #RESP_OK
responses::response_code fpga_bridge::apply_program(uint64_t address, std::vector<uint32_t> program) {
    std::cout<< "APPLY PROGRAM: address: " <<  to_hex(address) << " program_size: "<< program.size()<<std::endl;
    uint32_t offset =  fcore_address_to_index(address);

    if(!runtime_config.emulate_hw) {
        for(int i = 0; i< program.size(); i++)
            fCore[i+offset] = program[i];
    }
    return responses::ok;
}

responses::response_code fpga_bridge::set_clock_frequency(std::vector<uint32_t> freq) {
    if(runtime_config.log)
        std::cout << "SET_CLOCK FREQUENCY: clock #" + std::to_string(freq[0]) + " to " + std::to_string(freq[1]) + "Hz" <<std::endl;

    std::string command;

    if(arch=="zynq"){
        command = "echo " + std::to_string(freq[1]) + " > " + if_dict.get_clock_if(freq[0]);
    } else{
        return responses::ok;
    }
    if(!runtime_config.emulate_hw) {
        system(command.c_str());
    }
    return responses::ok;
}


responses::response_code fpga_bridge::apply_filter(uint64_t address, std::vector<uint32_t> taps) {
    std::cout<< "APPLY FILTER: address: " << to_hex(address) << " N. Filter Taps " << taps.size()<<std::endl;

    auto filter_address= register_address_to_index(address);

    if(!runtime_config.emulate_hw) {
        for(int i = 0; i< taps.size(); i++){
            registers[filter_address] = taps[i];
            registers[filter_address+1] = i;
        }
    }

    return responses::ok;
}

std::string fpga_bridge::get_module_version() {
    return "KERNEL MODULE VERSIONING NOT IMPLEMENTED YET";
}

std::string fpga_bridge::get_hardware_version() {
    return "HARDWARE VERSIONING NOT IMPLEMENTED YET";
}

responses::response_code fpga_bridge::set_scope_data(commands::scope_data data) {
    uint64_t buffer;

    if(!runtime_config.emulate_hw){
        std::ifstream fs(if_dict.get_buffer_address_if());
        fs >> buffer;
    }


    if(runtime_config.log){
        std::cout << "SET_SCOPE_BUFFER_ADDDRESS: writing buffer address "
                + std::to_string(buffer)
                + " to scope at address "
                + std::to_string(data.buffer_address)<< std::endl;
    }
    registers[register_address_to_index(data.buffer_address)] = buffer;
    registers[register_address_to_index(data.enable_address)] = 1;
    return responses::ok;
}
