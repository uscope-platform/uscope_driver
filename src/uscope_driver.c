//  Hello World server

#include "uscope_driver.h"



void free_command(command_t * command){;
    free(command);
}

command_t *parse_command(char *received_string){
    command_t *parsed_command;
    parsed_command = malloc(sizeof(parsed_command));
    char * saveptr;

    parsed_command->opcode = strtol(strtok_r(received_string, " ", &saveptr), NULL, 0);
    parsed_command->operand_1 = strtok_r(NULL, " ", &saveptr);
    parsed_command->operand_2 = strtok_r(NULL, " ", &saveptr);
    return parsed_command;
}


char *serialize_response(response_t *response){
    char *raw_response = malloc(10000* sizeof(char));
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

    return raw_response;
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

    char *raw_response = serialize_response(response);

    redisReply * response_status = redisCommand(reply_channel,"PUBLISH response %s", raw_response);
    free(raw_response);
    freeReplyObject(response_status);

}

void onData(redisAsyncContext *c, void *reply, void *privdata) {
    redisReply *r = reply;

    if (reply == NULL) return;

    if (r->type == REDIS_REPLY_ARRAY) {
        for (int j = 0; j < r->elements; j++) {
            printf("%u) %s\n", j, r->element[j]->str);
        }
    }
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

}

int setup_main_loop(void){
    signal(SIGPIPE, SIG_IGN);
    main_loop = event_base_new();

    redisAsyncContext *c = redisAsyncConnect("127.0.0.1", 6379);
    if (c->err) {
        printf("error: %s\n", c->errstr);
        return 1;
    }


    redisLibeventAttach(c, main_loop);
    redisAsyncCommand(c, onCommand, NULL, "SUBSCRIBE command");
    redisAsyncCommand(c, onData, NULL, "SUBSCRIBE data");

    return 0;
}

int main (int argc, char **argv) {

    init_fpga_bridge();
    setup_main_loop();
    setup_response_channels();

    event_base_dispatch(main_loop);

    redisFree(reply_channel);

    return 0;
}