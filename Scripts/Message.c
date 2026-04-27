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

    //Applying CRC when serializing the message
    msg->CRC = crc8_bitwise((const uint8_t *)buffer, actual_payload_size - 1);
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

void send_map(int fd, uint32_t ifindex, uint8_t seq, GameState *game)
{
    int side = game->visibility_radius;
    int num_cells = 1 + 2 * side * (side + 1); 
    
    char *visible_grid = malloc((size_t)num_cells);
    if (!visible_grid) return;

    int x_axis = game->pacman_x;
    int y_axis = game->pacman_y;
    int k = 0;

    for (int i = -side; i <= side; i++) {
        for (int j = -side; j <= side; j++) {
            if (abs(i) + abs(j) <= side) {
                // i é o deslocamento em Y (linhas), j em X (colunas)
                int current_y = y_axis + i;
                int current_x = x_axis + j;

                if (current_x >= 0 && current_x < MAP_SIZE && current_y >= 0 && current_y < MAP_SIZE)
                    visible_grid[k++] = game->grid[current_y][current_x];
                else 
                    visible_grid[k++] = 'X'; 
            }
        }
    }

    uint32_t total_size = (uint32_t)k;
    if (total_size > 31) total_size = 31; // Fragmentação será implementada depois

    struct message *msg = create_message(total_size, TYPE_VISUAL, seq, visible_grid);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);
    
    if (buffer) {
        send_message(fd, ifindex, buffer, &final_size);
        free(buffer);
    }
    
    free(msg);
    free(visible_grid);
}

int handle_listen_result(int fd, uint32_t ifindex, int listen_return, struct message *received_msg, uint8_t expected_seq) {
    if (listen_return == LISTEN_TIMEOUT) {
        return listen_return;
    }

    if (listen_return == LISTEN_CRC_ERROR) {
        send_nack(fd, ifindex, expected_seq);
        return listen_return;
    }

    // Se chegou aqui, listen_return é o TYPE da mensagem recebida e o CRC está ok
    if (received_msg->sequence != expected_seq) {
        send_nack(fd, ifindex, expected_seq);
        return LISTEN_SEQ_ERROR;
    }

    if(listen_return != TYPE_ACK && listen_return != TYPE_NACK)
        send_ack(fd, ifindex, received_msg->sequence);

    return listen_return;
}

uint8_t crc8_bitwise(const uint8_t *data, size_t size) {
    uint8_t crc = 0x00;
    uint8_t polyn = 0x07;

    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ polyn);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}
