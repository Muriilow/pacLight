#ifndef SOCKET_H
#define SOCKET_H

#include <stdio.h>          // printf
#include <stdint.h>
#include "Message.h"

int create_raw_socket(uint32_t ifindex);
void send_message(int pac_socket, uint32_t ifindex, uint8_t *message, size_t *final_size);
int listener_mode(int32_t fd, struct message *received_msg);

#endif