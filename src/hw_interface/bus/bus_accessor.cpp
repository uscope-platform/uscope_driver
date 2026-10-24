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


#include "hw_interface/bus/bus_accessor.hpp"

std::mutex m;

std::atomic<bool> bus_accessor::sink_mode{false};


void sigsegv_handler(int dummy) {
    spdlog::error("Segmentation fault encounteded while communicating with FPGA");
    exit(-1);
}

void sigbus_handler(int dummy) {
    spdlog::error("Bus error encountered while communicating with FPGA");
    exit(-1);
}

bus_accessor::bus_accessor() {

    spdlog::info("fpga_bridge initialization started");

    signal(SIGSEGV,sigsegv_handler);
    signal(SIGBUS,sigbus_handler);
    std::string arch;
    if (const char * env_val = std::getenv("ARCH")) {
        arch = env_val;
    } else {
        spdlog::error("The architecture must be selected with the ARCH environment variable");
        exit(-1);
    }
    if(arch == "zynqmp"){
        control_addr = ZYNQMP_REGISTERS_BASE_ADDR;
        core_addr = ZYNQMP_FCORE_BASE_ADDR;
    } else {
        control_addr = ZYNQ_REGISTERS_BASE_ADDR;
        core_addr = ZYNQ_FCORE_BASE_ADDR;
    }

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

    auto registers_fd = open(if_dict.get_control_bus().c_str(), O_RDWR | O_SYNC);
    if(registers_fd == -1){
        spdlog::error( "Error while mapping the axi control bus");
        exit(1);
    }

    auto fcore_fd = open(if_dict.get_cores_bus().c_str(), O_RDWR | O_SYNC);
    if(fcore_fd == -1){
        spdlog::error("Error while mapping the fcore rom bus");
        exit(1);
    }

    spdlog::info("Mapping {0} memory pages from the axi control bus, starting at address: 0x{1:x}", n_pages_ctrl, control_addr);
    control_words = static_cast<uint64_t>(n_pages_ctrl)*4096/4;
    registers = (uint32_t*) mmap(nullptr, n_pages_ctrl*4096, PROT_READ | PROT_WRITE, MAP_SHARED, registers_fd, control_addr);
    if(registers == MAP_FAILED) {
        spdlog::error("Cannot mmap control bus: {0}", strerror(errno));
        exit(1);
    }

    spdlog::info("Mapping {0} memory pages from the fcore programming bus, starting at address: ", n_pages_fcore, core_addr);
    fCore = (uint32_t*) mmap(nullptr, n_pages_fcore*4096, PROT_READ | PROT_WRITE, MAP_SHARED, fcore_fd, core_addr);
    fcore_words = static_cast<uint64_t>(n_pages_fcore)*4096/4;
    if(fCore == MAP_FAILED) {
        spdlog::error("Cannot mmap fcore programming bus: {0}", strerror(errno));
        exit(1);
    }


    spdlog::info("fpga_bridge initialization done");
}

void bus_accessor::write_register(const std::vector<uint64_t>& addresses, uint64_t data) {
    if(!sink_mode){
        // all the addresses are validated before touching anything, so a rejected write has no effect at all
        if(addresses.size() ==1){
            auto index = register_address_to_index(addresses[0]);
            std::lock_guard lock(m);
            registers[index] = data;
        } else {
            auto address_index = register_address_to_index(addresses[1]+4);
            auto data_index = register_address_to_index(addresses[1]);
            std::lock_guard lock(m);
            registers[address_index] = addresses[0];
            registers[data_index] = data;
        }
    }
    operations.push_back({addresses, {data}, control_plane_write});
}

uint32_t bus_accessor::read_register(const std::vector<uint64_t>& address) {
    if(!sink_mode){
        uint32_t ret_val = 0;
        if(address.size()==1){
            auto reg_n = register_address_to_index(address[0]);
            spdlog::trace("READ from Register #{0} at address {1}", reg_n, address[0]);
            std::lock_guard lock(m);
            ret_val = registers[reg_n];
        }
        operations.push_back({address, {0}, control_plane_read});
        return ret_val;
    } else {
        operations.push_back({address, {0}, control_plane_read});
        return rand()%100;
    }
}

uint64_t bus_accessor::fcore_address_to_index(uint64_t address) const {
    if(address < core_addr || (address - core_addr) / 4 >= fcore_words){
        throw bus_address_error("Core bus address 0x" + fmt::format("{:x}", address) + " is outside of the mapped window (from 0x"
                                + fmt::format("{:x}", core_addr) + " for " + std::to_string(fcore_words*4) + " bytes)");
    }
    return (address - core_addr) / 4;
}

std::pair<std::string, std::string> bus_accessor::get_hardware_simulation_data() {
    std::string rom_plane, control_plane;
    auto ops = get_operations();
    for(auto &o:ops) {
        if(o.type == rom_plane_write) {
            auto rom_address = o.address[0];
            for(int i = 0; i<o.data.size(); i++) {
                rom_plane += std::to_string(rom_address + 4*i) + ":" + std::to_string(o.data[i]) + '\n';
            }
        } else if(o.type == control_plane_write) {
            for(int i = 0; i<o.data.size(); i++) {
                control_plane += std::to_string(o.address[i]) + ":"  + std::to_string(o.data[i]) + "\n";
            }
        }
    }
    return {rom_plane, control_plane};
}

uint64_t bus_accessor::register_address_to_index(uint64_t address) const {
    if(address < control_addr || (address - control_addr) / 4 >= control_words){
        throw bus_address_error("Control bus address 0x" + fmt::format("{:x}", address) + " is outside of the mapped window (from 0x"
                                + fmt::format("{:x}", control_addr) + " for " + std::to_string(control_words*4) + " bytes)");
    }
    return (address - control_addr) / 4;
}

void bus_accessor::load_program(uint64_t address, const std::vector<uint32_t> program) {

    std::vector<uint64_t> pn;
    std::vector<uint64_t> a = {address};

    if(sink_mode){
        for (auto &p:program) {
            pn.push_back(p);
        }
    } else {
        // an address outside of the mapped window would be a segmentation fault, which kills the driver
        if(address < core_addr || (address-core_addr)/4 + program.size() > fcore_words){
            throw bus_address_error("Cannot load the program at address 0x" + fmt::format("{:x}", address) +
                                     ": the programming bus is mapped from 0x" + fmt::format("{:x}", core_addr) +
                                     " for " + std::to_string(fcore_words*4) + " bytes (check the ARCH of the driver and the code address of the core)");
        }
        std::lock_guard lock(m);
        a = {address};
        for(int i = 0; i< program.size(); i++){
            fCore[i+fcore_address_to_index(address)] = program[i];
            pn.push_back(program[i]);
            usleep(1);
        }
    }
        operations.push_back({a, pn, rom_plane_write});
}

