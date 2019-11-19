#include "scope_handler.h"

void handle_scope_data(int fd, short what, void *arg){
        wait_for_Interrupt();
        handler_read_data(scope_data_buffer, internal_buffer_size);
        if(scope_mode==MODE_CAPTURE){
            if(n_buffers_left ==1){
                scope_mode = MODE_RUN;

                int  iret1;
                writeback_done = false;
                iret1 = pthread_create( &writeback_thread, NULL, data_writeback, NULL);
            }
            data_push_back(scope_data_buffer);
            n_buffers_left--;
        }
        scope_data_ready = true;
}

void *data_writeback(void* args){
    int size = get_data_list_length()*1024;

    int raw_fd = shm_open("/uscope_capture_writeback", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    if (raw_fd < 0)
    {
        fprintf(stderr, "Error opening file: %s\n", strerror( errno ));
    }
    ftruncate(raw_fd,size*sizeof(int32_t));
    FILE * fd = fdopen(raw_fd, "w");
    while(get_data_list_length() > 0){
        data_ll_t *data = data_pop_back();
        for(int i = 1023; i>=0; i--){
            fprintf(fd, "%i\n", data->data[i]);
        }
        free(data);
    }


    printf("data write called\n");
    writeback_done = true;
    return NULL;
}

void init_scope_handler(char * driver_file, int32_t buffer_size){
    writeback_done = true;
    scope_mode = MODE_RUN;
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

void start_capture_mode(int n_buffers){
    if(!writeback_done){
        pthread_join(writeback_thread, NULL);
    }
    n_buffers_left = n_buffers;
    scope_mode = MODE_CAPTURE;
    init_data_list();
}

int check_capture_progress(void){
    return n_buffers_left;
}