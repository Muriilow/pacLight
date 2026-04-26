#include "Message.h"
#include "Socket.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct global_sequence global_sequence = {0};

struct message* create_message(uint32_t size, uint32_t type, uint8_t seq, void* data) {
    struct message* new_message = malloc(sizeof(struct message));
    if (new_message == NULL) {
        fprintf(stderr, "Erro ao criar mensagem! {create_message}\n");
        exit(EXIT_FAILURE);
    }

    new_message->start_marker = 126;
    new_message->size = (uint8_t)(size & 0x1F);
    new_message->sequence = (uint8_t)(seq & 0x3F);
    new_message->type = (uint8_t)(type & 0x1F);
    new_message->data = data;
    new_message->CRC = 1; 

    return new_message;
}

void next_sequence() {
    global_sequence.value = (uint8_t)((global_sequence.value + 1) & 0x3F);
}

uint8_t *serialize_message(struct message *msg, size_t *final_size) {
    size_t actual_payload_size = (size_t)(3 + msg->size + 1);
    *final_size = (actual_payload_size < 14) ? 14 : actual_payload_size;

    uint8_t *buffer = calloc(1, *final_size); 
    if (!buffer) return NULL;

    memcpy(buffer, msg, 3);
    if (msg->size > 0 && msg->data != NULL) {
        memcpy(buffer + 3, msg->data, msg->size);
    }
    buffer[actual_payload_size - 1] = msg->CRC;

    return buffer;
}

void send_ack(int fd, uint32_t ifindex, uint8_t seq) {
    struct message *msg = create_message(0, TYPE_ACK, seq, NULL);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);
    if (buffer) {
        send_message(fd, ifindex, buffer, &final_size);
        free(buffer);
    }
    free(msg);
}

void send_nack(int fd, uint32_t ifindex, uint8_t seq) {
    struct message *msg = create_message(0, TYPE_NACK, seq, NULL);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);
    if (buffer) {
        send_message(fd, ifindex, buffer, &final_size);
        free(buffer);
    }
    free(msg);
}
