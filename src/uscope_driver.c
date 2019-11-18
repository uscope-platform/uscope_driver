//  Hello World server

#include "uscope_driver.h"

void free_command(command_t * command){;
    free(command);
}

command_t *parse_command(char *received_string){
    command_t *parsed_command;
    parsed_command = malloc(sizeof(command_t));
    char * saveptr;

    parsed_command->opcode = strtol(strtok_r(received_string, " ", &saveptr), NULL, 0);
    parsed_command->operand_1 = strtok_r(NULL, " ", &saveptr);
    parsed_command->operand_2 = strtok_r(NULL, " ", &saveptr);
    return parsed_command;
}


void respond(response_t *response){
    char *raw_response = malloc(10000* sizeof(char));

    if(response->type == RESP_TYPE_INBAND){
        if(response->body_size!=0){
            sprintf(raw_response, "%u %u\n", response->opcode, response->return_code);
            for(uint32_t i = 0; i<response->body_size; i++){
                char result[100];
                sprintf(result," %u,", response->body[i]);
                strcat(raw_response, result);
            }
        } else {
            sprintf(raw_response, "%u %u", response->opcode, response->return_code);
        }

        redisReply *response_status = redisCommand(reply_channel,"PUBLISH response %s", raw_response);
        free(raw_response);
        freeReplyObject(response_status);

    } else if (response->type == RESP_TYPE_OUTBAND) {
        sprintf(raw_response, "%u %u", response->opcode, response->return_code);
        shared_memory[0] = 0xcafebebe;
        //memcpy((void*) shared_memory, response->body, response->body_size* sizeof(int32_t));

        redisReply *response_status = redisCommand(reply_channel,"PUBLISH response %s", raw_response);
        free(raw_response);
        freeReplyObject(response_status);


    } else {
        printf("ERRROR: response type unknown");
        exit(1);
    }

}

void onCommand(redisAsyncContext *c, void *reply, void *privdata) {
    redisReply *r = reply;
    redisReply **rra = r->element;


    if (reply == NULL) return;
    char * raw_command;
    if (r->type == REDIS_REPLY_ARRAY && strcmp(r->element[0]->str, "message")==0) {
        raw_command= r->element[2]->str;
    } else {
        return;
    }
    command_t * command = parse_command(raw_command);
    response_t * response = process_command(command);
    free_command(command);

    respond(response);
    free(response->body);
    free(response);
}


int setup_response_channels(void){

    struct timeval timeout = { 1, 500000 }; // 1.5 seconds

    reply_channel = redisConnectWithTimeout("127.0.0.1", 6379, timeout);

    if (reply_channel == NULL || reply_channel->err) {
        if (reply_channel) {
            printf("Connection error: %s\n", reply_channel->errstr);
            redisFree(reply_channel);
        } else {
            printf("Connection error: can't allocate redis context\n");
        }
        exit(1);
    }

    sh_mem_fd = shm_open("/uscope_mapped_mem", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    if (sh_mem_fd < 0)
    {
        fprintf(stderr, "Error opening file: %s\n", strerror( errno ));
    }
    ftruncate(sh_mem_fd,1024*sizeof(int32_t));
    shared_memory = (uint32_t *) mmap(NULL, 1024* sizeof(int32_t), PROT_READ | PROT_WRITE, MAP_SHARED, sh_mem_fd, 0);
    if (shared_memory < 0)
    {
        fprintf(stderr, "Error mmapping file: %s\n", strerror( errno ));
    }

    return 0;
}


void intHandler(int args) {
    event_base_loopbreak(main_loop);
    cleanup_scope_handler();
    exit(0);
}


int setup_main_loop(void){
    signal(SIGPIPE, SIG_IGN);
    main_loop = event_base_new();
    printf("here");
    signal(SIGINT, intHandler);
    printf("here");
    scope_data = event_new(main_loop, fd_data, EV_READ| EV_PERSIST, handle_scope_data,event_self_cbarg());


    /* Add it to the active events, without a timeout */
    event_add(scope_data, NULL);


    redisAsyncContext *c = redisAsyncConnect("127.0.0.1", 6379);
    if (c->err) {
        printf("error: %s\n", c->errstr);
        return 1;
    }

    redisLibeventAttach(c, main_loop);
    redisAsyncCommand(c, onCommand, NULL, "SUBSCRIBE command");

    return 0;
}

int main (int argc, char **argv) {
    init_fpga_bridge();
    init_scope_handler("/dev/uio0", 1024*4);

    setup_main_loop();

    setup_response_channels();

    redisReply *response_status = redisCommand(reply_channel,"SELECT 4");
    freeReplyObject(response_status);

    event_base_dispatch(main_loop);

    redisFree(reply_channel);

    return 0;
}