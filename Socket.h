#ifndef SOCKET_H
#define SOCKET_H

#include <stdio.h>          // printf
#include <stdint.h>

struct __attribute__((packed)) global_sequence{
    uint8_t value : 6 ; 
};

struct __attribute__((packed)) message {
    uint8_t start_marker;
    uint8_t size : 5;
    uint8_t sequence : 6;
    uint8_t type : 5;
    void *data;
    uint8_t CRC;
};

extern struct global_sequence global_sequence;
int create_raw_socket(uint32_t ifindex);
struct message* create_message(uint32_t size, uint32_t type, void *data);
uint8_t *serialize_message(struct message *msg, size_t *final_size);
void send_message(int pac_socket, uint32_t ifindex, uint8_t *message, size_t *final_size);

#endif
