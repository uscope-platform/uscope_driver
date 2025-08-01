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



void sigsegv_handler(int dummy) {
    spdlog::error("Segmentation fault encounteded while communicating with FPGA");
    exit(-1);
}

void sigbus_handler(int dummy) {
    spdlog::error("Bus error encountered while communicating with FPGA");
    exit(-1);
}

bus_accessor::bus_accessor(bool sm) {

    sink_mode = sm;

    spdlog::info("fpga_bridge initialization started");

    signal(SIGSEGV,sigsegv_handler);
    signal(SIGBUS,sigbus_handler);

    if(!sink_mode){

        std::string arch = std::getenv("ARCH");
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
        registers = (uint32_t*) mmap(nullptr, n_pages_ctrl*4096, PROT_READ | PROT_WRITE, MAP_SHARED, registers_fd, control_addr);
        if(registers == MAP_FAILED) {
            spdlog::error("Cannot mmap control bus: {0}", strerror(errno));
            exit(1);
        }

        spdlog::info("Mapping {0} memory pages from the fcore programming bus, starting at address: ", n_pages_fcore, core_addr);
        fCore = (uint32_t*) mmap(nullptr, n_pages_fcore*4096, PROT_READ | PROT_WRITE, MAP_SHARED, fcore_fd, core_addr);
        if(fCore == MAP_FAILED) {
            spdlog::error("Cannot mmap fcore programming bus: {0}", strerror(errno));
            exit(1);
        }

    }

    spdlog::info("fpga_bridge initialization done");
}

void bus_accessor::write_register(const std::vector<uint64_t>& addresses, uint64_t data) {
    if(sink_mode){
        operations.push_back({addresses, {data}, control_plane_write});
    } else {
        m.lock();
        if(addresses.size() ==1){
            registers[register_address_to_index(addresses[0])] = data;
        } else {
            registers[register_address_to_index(addresses[1]+4)] = addresses[0];
            registers[register_address_to_index(addresses[1])] = data;
        }
        m.unlock();
    }
}

uint32_t bus_accessor::read_register(const std::vector<uint64_t>& address) {
    if(sink_mode){
        operations.push_back({address, {0}, control_plane_read});
        return rand()%100;
    } else {
        uint32_t ret_val;
        m.lock();
        if(address.size()==1){
            auto reg_n = register_address_to_index(address[0]);
            spdlog::trace("READ from Register #{0} at address {1}", reg_n, address[0]);
            ret_val = registers[reg_n];
        } else{
            ret_val = 0;
        }
        m.unlock();
        return ret_val;
    }
}

uint64_t bus_accessor::fcore_address_to_index(uint64_t address) const {
    if(core_addr>address){
        spdlog::critical("Tried to write the core address: 0x{0:x} which is below the minimum allowed: 0x{1:x}", address, control_addr);
        exit(-1);
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
    if(control_addr>address){
        spdlog::critical("Tried to write the control address: 0x{0:x} which is below the minimum allowed: 0x{1:x}", address, control_addr);
        exit(-1);
    }
    return (address - control_addr) / 4;
}

void bus_accessor::load_program(uint64_t address, const std::vector<uint32_t> program) {
    if(sink_mode){
        std::vector<uint64_t> a = {address};
        std::vector<uint64_t> pn;
        for (auto &p:program) {
            pn.push_back(p);
        }
        operations.push_back({a, pn, rom_plane_write});
    } else {
        m.lock();
        for(int i = 0; i< program.size(); i++){
            fCore[i+fcore_address_to_index(address)] = program[i];
            usleep(1);
        }

        m.unlock();
    }
}

