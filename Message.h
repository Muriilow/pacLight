#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include <stddef.h>

#define TYPE_ACK  0
#define TYPE_NACK 1
#define TYPE_DATA 3

struct __attribute__((packed)) global_sequence {
    uint8_t value : 6;
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

struct message* create_message(uint32_t size, uint32_t type, void *data);
uint8_t *serialize_message(struct message *msg, size_t *final_size);
void send_ack(int fd, uint32_t ifindex);
void send_nack(int fd, uint32_t ifindex);

#endif
