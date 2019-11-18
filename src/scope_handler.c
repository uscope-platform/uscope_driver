#include "scope_handler.h"

void handle_scope_data(int fd, short what, void *arg){
        wait_for_Interrupt();
        handler_read_data(scope_data_buffer, internal_buffer_size);
        scope_data_ready = true;

}


void init_scope_handler(char * driver_file, int32_t buffer_size){

    internal_buffer_size = buffer_size;
    scope_data_buffer = malloc(internal_buffer_size* sizeof(int32_t));

    //mmap buffer
    fd_data = open(driver_file, O_RDWR| O_SYNC);

    buffer = (int32_t* ) mmap(NULL, buffer_size*sizeof(uint32_t), PROT_READ, MAP_SHARED, fd_data, 0);
    if(buffer < 0) {
        fprintf(stderr, "Cannot mmap uio device: %s\n",
                strerror(errno));
      }

    uint32_t write_val = 1;
    write(fd_data, &write_val, sizeof(write_val));
}

void cleanup_scope_handler(void){
    munmap((void*)buffer, internal_buffer_size* sizeof(uint32_t));
    close(fd_data);
    printf("Cleaned up scope handler\n");
}

int wait_for_Interrupt(void){
    uint32_t read_val;
    uint32_t write_val = 1;
    read(fd_data, &read_val, sizeof(read_val));
    write(fd_data, &write_val, sizeof(write_val));
    return 0;
}

void handler_read_data(int32_t *data, int size){
    memcpy(data, (void *)buffer,size*sizeof(int32_t));
}
