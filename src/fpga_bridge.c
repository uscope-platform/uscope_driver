
#include "fpga_bridge.h"


uint32_t address_to_index(uint32_t address){
    return(address - BASE_ADDR)/4;
}



void init_fpga_bridge(void){

    volatile uint32_t* return_value;

    if((regs_fd = open("/dev/mem", O_RDWR | O_SYNC)) == -1) FATAL;
    registers = (uint32_t*) mmap(0, 4096, PROT_READ |PROT_WRITE, MAP_SHARED, regs_fd, BASE_ADDR);
    if(registers < 0) {
        fprintf(stderr, "Cannot mmap uio device: %s\n",
                strerror(errno));
    }

}


int load_bitstream(char *bitstream){
    #ifdef DEBUG
        printf("LOAD BITSTREAM: %s\n", bitstream);
    #endif

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

int single_write_register(uint32_t address, uint32_t value){
    #ifdef DEBUG
        printf("WRITE SINGLE REGISTER: addr %x   value %u \n", address, value);
    #endif

    registers[address_to_index(address)] = value;
    return RESP_OK;
}
int single_read_register(uint32_t address, volatile uint32_t *value){
    #ifdef DEBUG
        printf("READ SINGLE REGISTER: addr %x \n", address);
    #endif

    uint32_t int_value = registers[address_to_index(address)];

    value = &int_value;

    return RESP_OK;
}

int bulk_write_register(uint32_t *address, volatile uint32_t *value, volatile uint32_t n_registers){
    #ifdef DEBUG
        printf("WRITE BULK REGISTERS\n");
    #endif

    for(uint32_t i = 0; i< n_registers; i++){
        uint32_t current_addr = address_to_index(address[i]);
        registers[current_addr] = value[i];
    }
    return RESP_OK;
}
int bulk_read_register(uint32_t *address, volatile uint32_t *value, volatile uint32_t n_registers){
    #ifdef DEBUG
        printf("READ BULK REGISTERS\n");
    #endif
    for(uint32_t i = 0; i< n_registers; i++){
        uint32_t current_addr = address_to_index(address[i]);
        uint32_t int_value = registers[current_addr];
        value[i] = int_value;
    }
    return RESP_OK;
}

int start_capture(uint32_t n_buffers){
    #ifdef DEBUG
        printf("START CAPTURE: n_buffers %u\n", n_buffers);
    #endif

    return RESP_OK;
}

int single_proxied_write_register(uint32_t proxy_address,uint32_t reg_address, uint32_t value){
    #ifdef DEBUG
        printf("WRITE SINGLE PROXIED REGISTER: proxy address %x   register address %x  value %u \n", proxy_address,reg_address, value);
    #endif

    registers[address_to_index(proxy_address)] = value;
    registers[address_to_index(proxy_address)+1] = reg_address;
    return RESP_OK;
}

int read_data(int32_t * read_data){
    #ifdef DEBUG
        printf("READ DATA \n");
    #endif


    int response;
    if(scope_data_ready) {
        printf("DATA READY \n");
        response = RESP_OK;
    } else{
        printf("DATA NOT READY \n");
        response = RESP_DATA_NOT_READY;
    }
    memcpy(read_data, scope_data_buffer, 1024* sizeof(int32_t));
    scope_data_ready = false;
    return response;
}