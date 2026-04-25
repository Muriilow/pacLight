#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>
#include <stddef.h>

#define TYPE_ACK  0
#define TYPE_NACK 1
#define TYPE_VISUAL 2
#define TYPE_INITIALIZING 3
#define TYPE_DATA 4 
#define TYPE_TXT 5
#define TYPE_JPG 6 
#define TYPE_MP4 7
#define TYPE_RIGHT 10
#define TYPE_LEFT 11
#define TYPE_DOWN 12
#define TYPE_UP 13
#define TYPE_ERROR 15 
#define TYPE_END 16 

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

struct message* create_message(uint32_t size, uint32_t type, uint8_t seq, void *data);
uint8_t *serialize_message(struct message *msg, size_t *final_size);
void next_sequence();
void send_ack(int fd, uint32_t ifindex, uint8_t seq);
void send_nack(int fd, uint32_t ifindex, uint8_t seq);

#endif
