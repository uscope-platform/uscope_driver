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

#include "scope_handler.h"

extern bool debug_mode;

/// This is a libevent event handler, it is called every time the UIO driver file is readable, meaning new data is ready
/// and it copies the data from the DMA hardware buffer and appends them to the data list from data_linked_list.c
/// \param fd
/// \param what
/// \param arg
void handle_scope_data(int fd, short what, void *arg){
        wait_for_Interrupt();
        memcpy(scope_data_buffer, (void *)buffer,internal_buffer_size*sizeof(int32_t));
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

/// This function writes the captured data to a shared memory file, that will be then read from the uScope_server.
/// \param args
/// \return NULL
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

/// Initializes the scope_handler infrastructure, opening the UIO driver file and writing to it to clear any outstanding
/// interrupts
/// \param driver_file Path of the driver file
/// \param buffer_size Size of the capture buffer
void init_scope_handler(char * driver_file, int32_t buffer_size){
    writeback_done = true;
    scope_mode = MODE_RUN;
    internal_buffer_size = buffer_size;
    scope_data_buffer = malloc(internal_buffer_size* sizeof(int32_t));

    if(!debug_mode){
        //mmap buffer
        fd_data = open(driver_file, O_RDWR| O_SYNC);

        buffer = (int32_t* ) mmap(NULL, buffer_size*sizeof(uint32_t), PROT_READ, MAP_SHARED, fd_data, 0);
        if(buffer < 0) {
            fprintf(stderr, "Cannot mmap uio device: %s\n",
                    strerror(errno));
        }
    } else {
        fd_data = open("/dev/zero", O_RDWR| O_SYNC);
        buffer = (int32_t* ) mmap(NULL, buffer_size*sizeof(uint32_t), PROT_READ, MAP_SHARED, fd_data, 0);
    }


    uint32_t write_val = 1;
    write(fd_data, &write_val, sizeof(write_val));
}

/// clean up all the scope handler infrastructure once it is not needed anymore
void cleanup_scope_handler(void){
    munmap((void*)buffer, internal_buffer_size* sizeof(uint32_t));
    close(fd_data);
    printf("Cleaned up scope handler\n");
}

/// Wait for data to be ready to be read. This is signaled through an hardware interrupt. Since a UIO driver is
/// used the interrupt is waited upon with a read to the driver file
/// \return 0
int wait_for_Interrupt(void){
    uint32_t read_val;
    uint32_t write_val = 1;
    read(fd_data, &read_val, sizeof(read_val));
    write(fd_data, &write_val, sizeof(write_val));
    return 0;
}

/// Start data capture mode, saving a number of consecutive buffers of data for further analysis
/// \param n_buffers Number of buffers to capture.
void start_capture_mode(int n_buffers){
    if(!writeback_done){
        pthread_join(writeback_thread, NULL);
    }
    n_buffers_left = n_buffers;
    scope_mode = MODE_CAPTURE;
    init_data_list();
}

/// This function returns the number of data buffers left to capture
/// \return Number of buffers still to capture
int check_capture_progress(void){
    return n_buffers_left;
}