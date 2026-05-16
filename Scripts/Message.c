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
char* name_handle(char* name, int type){
    size_t size = strlen(name);
    char* final = malloc(size+10);

    switch (type){
        case TYPE_JPG:
            sprintf(final,"Send/%s.jpg", name);
            break;
        case TYPE_TXT:
            sprintf(final,"Send/%s.txt", name);
            break;
        case TYPE_MP4:
            sprintf(final,"Send/%s.mp4", name);
            break;
        default:
            free(final);
            return NULL;
    }
    
    fprintf(stderr, "%s", final);
    return final;
}

void next_sequence() {
    global_sequence.value = (uint8_t)((global_sequence.value + 1) & 0x3F);
    printf("NEXT SEQUENCE: %d\n", global_sequence.value);
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
        printf("ACK ");
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
        printf("NACK ");
        send_message(fd, ifindex, buffer, &final_size);
        free(buffer);
    }
    free(msg);
}

void send_map(int fd, uint32_t ifindex, GameState *game)
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
                int current_y = x_axis + i;
                int current_x = y_axis + j;

                if (current_x >= 0 && current_x < MAP_SIZE && current_y >= 0 && current_y < MAP_SIZE)
                    visible_grid[k++] = game->grid[current_y][current_x];
                else 
                    visible_grid[k++] = 'X'; 
            }
        }
    }

    uint32_t total_size = (uint32_t)k;
    
    int result = -1;
    int raw_type;
    struct message ack_addr;
    uint32_t i;
    
    struct message *msg = create_message(sizeof(game->visibility_radius), TYPE_VISUAL, global_sequence.value, &game->visibility_radius);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);

    fprintf(stderr,"seq: %d expcSeq: %d\n", global_sequence.value, global_sequence.value);
    while(result != TYPE_ACK)
    {   
        fprintf(stderr,"enviando mapa %d\n",msg->type);
        send_message(fd, ifindex, buffer, &final_size);
        raw_type = listener_mode(fd, &ack_addr);
        result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);

        if(ack_addr.data)
            continue;

        free(ack_addr.data);
        ack_addr.data = NULL;
    }
    if (buffer)
        free(buffer);
    free(msg);
    fprintf(stderr,"entrando for totaldata: %d\n",total_size);
    for(i = 0; i < total_size - total_size%MAX_DATA; i+=MAX_DATA){
        char visible_grid_seg[MAX_DATA];
        memcpy(visible_grid_seg, &visible_grid[i], MAX_DATA);
        msg = create_message(MAX_DATA, TYPE_DATA, global_sequence.value, visible_grid_seg);
        buffer = serialize_message(msg, &final_size);

        while(result != TYPE_ACK){
            printf("MAP ");
            send_message(fd, ifindex, buffer, &final_size);
            raw_type = listener_mode(fd, &ack_addr);
            result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);
        }

        if (buffer) {
            free(buffer);
        }
        free(msg);
        free(visible_grid);
    }
    if (total_size%MAX_DATA > 0){
        char last_grid_seg[total_size%MAX_DATA];
        memcpy(last_grid_seg, &visible_grid[i], total_size%MAX_DATA);
        msg = create_message(total_size%MAX_DATA, TYPE_DATA, global_sequence.value, last_grid_seg);
        buffer = serialize_message(msg, &final_size);
        result = -1;
        while(result != TYPE_ACK){
            fprintf(stderr, "SIZE: %d ",total_size%MAX_DATA);
            fprintf(stderr, "INFO: %.*s\n",total_size%MAX_DATA,last_grid_seg);
            printf("SENT LAST DATA\n");
            send_message(fd, ifindex, buffer, &final_size);
            raw_type = listener_mode(fd, &ack_addr);
            result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);
        }

        if (buffer) {
            free(buffer);
        }
    
        free(msg);
        free(visible_grid);
    }
    msg = create_message(total_size%MAX_DATA, TYPE_END, global_sequence.value, NULL);
    buffer = serialize_message(msg, &final_size);

    result = -1;
    while(result != TYPE_ACK)
    {   
        printf("SEND END ");
        send_message(fd, ifindex, buffer, &final_size);
        raw_type = listener_mode(fd, &ack_addr);
        result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);

        if(result == TYPE_ACK){
            fprintf(stderr, "ACK RECEBIDO\n");
        }
        if(ack_addr.data)
            continue;

        free(ack_addr.data);
        ack_addr.data = NULL;
    }
}

void send_up(int fd, uint32_t ifindex)
{
    struct message *msg = create_message(0, TYPE_UP, global_sequence.value, NULL);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);
    
    if (buffer) {
        printf("UP  ");
        send_message(fd, ifindex, buffer, &final_size);
        free(buffer);
    }
    
    free(msg);
}
void send_down(int fd, uint32_t ifindex)
{
    struct message *msg = create_message(0, TYPE_DOWN, global_sequence.value, NULL);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);
    
    if (buffer) {
        printf("DOW ");
        send_message(fd, ifindex, buffer, &final_size);
        free(buffer);
    }
    
    free(msg);
}
void send_left(int fd, uint32_t ifindex)
{
    struct message *msg = create_message(0, TYPE_LEFT, global_sequence.value, NULL);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);
    
    if (buffer) {
        printf("LEF ");
        send_message(fd, ifindex, buffer, &final_size);
        free(buffer);
    }
    
    free(msg);
}
void send_right(int fd, uint32_t ifindex)
{
    struct message *msg = create_message(0, TYPE_RIGHT, global_sequence.value, NULL);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);
    
    if (buffer) {
        printf("RIG ");
        send_message(fd, ifindex, buffer, &final_size);
        free(buffer);
    }
    
    free(msg);
}
void send_big(int fd, uint32_t ifindex, char* name, uint32_t type){

    uint32_t n_size = (uint32_t) strlen(name)+1;
    if (n_size > 31){
        fprintf(stderr,"Nome muito grande\n");
        return;
    }
    int result = -1;
    int raw_type;
    struct message ack_addr;

    struct message *msg = create_message(n_size, type, global_sequence.value, name);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);

    fprintf(stderr,"seq: %d expcSeq: %d\n", global_sequence.value, global_sequence.value);
    while(result != TYPE_ACK)
    {   
        send_message(fd, ifindex, buffer, &final_size);
        raw_type = listener_mode(fd, &ack_addr);
        result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);

        if(ack_addr.data)
            continue;

        free(ack_addr.data);
        ack_addr.data = NULL;
    }
    if (buffer)
        free(buffer);
    free(msg);


    FILE *fptr = fopen(name_handle(name, (int)type), "rb");
    if (fptr == NULL) return;

    fseek(fptr, 0, SEEK_END);
    long size = ftell(fptr);
    if(size < 0){
        printf("ftell error\n");
        return;
    }

    fseek(fptr, 0, SEEK_SET);
    uint32_t seg_size;
    for(long i = 0; i < size - size%MAX_DATA; i += MAX_DATA){
        fprintf(stderr,"Tamanho total: %ld tamanho atual: %ld\n",size, i);
        char file_data[MAX_DATA];
        seg_size = (uint32_t) fread(file_data, 1, MAX_DATA, fptr);
        if (seg_size != MAX_DATA){
            fseek(fptr, -seg_size, SEEK_CUR);
        }
        else
        {

            msg = create_message(MAX_DATA, TYPE_DATA, global_sequence.value, file_data);
            buffer = serialize_message(msg, &final_size);
            result = -1;
            while(result != TYPE_ACK)
            {
                printf("DATA %d ", global_sequence.value);
                send_message(fd, ifindex, buffer, &final_size);
                raw_type = listener_mode(fd, &ack_addr);
                result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);

                if(ack_addr.data)
                    continue;

                free(ack_addr.data);
                ack_addr.data = NULL;
            }
            if(buffer)
                free(buffer);
            free(msg);
        }
    } 
    char file_data[size%MAX_DATA];
    fread(file_data, 1, (size_t)size%MAX_DATA, fptr);
    msg = create_message((uint32_t)(size%MAX_DATA), TYPE_DATA, global_sequence.value, file_data);
    buffer = serialize_message(msg, &final_size);
    result = -1;
    while(result != TYPE_ACK)
    {   
        printf("DATA %d ", global_sequence.value);
        send_message(fd, ifindex, buffer, &final_size);
        raw_type = listener_mode(fd, &ack_addr);
        result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);

        if(ack_addr.data)
            continue;

        free(ack_addr.data);
        ack_addr.data = NULL;
    }
    if(buffer)
        free(buffer);
    free(msg);
    fprintf(stderr,"ENDING\n");
    msg = create_message(0, TYPE_END, global_sequence.value, NULL);
    buffer = serialize_message(msg, &final_size);

    result = -1;
    while(result != TYPE_ACK)
    {   
        printf("END ");
        send_message(fd, ifindex, buffer, &final_size);
        raw_type = listener_mode(fd, &ack_addr);
        result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);

        if(result == TYPE_ACK){
            fprintf(stderr, "ACK RECEBIDO\n");
        }
        if(ack_addr.data)
            continue;

        free(ack_addr.data);
        ack_addr.data = NULL;
    }
    
    if (buffer)
        free(buffer);
    free(msg);
        fclose(fptr);
}

int handle_listen_result(int fd, uint32_t ifindex, int listen_return, struct message *received_msg, uint8_t expected_seq) 
{
    if (listen_return == LISTEN_TIMEOUT) 
        return listen_return;

    if (listen_return == LISTEN_CRC_ERROR) 
    {
        fprintf(stderr, "ERRO DE CRC\n");
        send_nack(fd, ifindex, expected_seq);
        return listen_return;
    }

    if (listen_return == TYPE_ACK || listen_return == TYPE_NACK)
    {
        if (received_msg->sequence == expected_seq)
        {
            if (listen_return == TYPE_ACK)
                next_sequence();
            return listen_return;
        }
        return LISTEN_SEQ_ERROR;
    }
    if (listen_return >= 10 && listen_return <= 13){
        return listen_return;
    }
    // Para pacotes de dados/comandos
    if (received_msg->sequence != expected_seq)
    {
        // Se a sequência for menor, o outro lado pode não ter recebido nosso ACK anterior
        if (received_msg->sequence < expected_seq){
            fprintf(stderr, "ERRO DE SEQUENCIA - Recebido:%d Esperado:%d\n",received_msg->sequence, expected_seq);
            send_ack(fd, ifindex, received_msg->sequence);
        }else{
            fprintf(stderr, "ERRO DE SEQUENCIA - Recebido:%d Esperado:%d\n",received_msg->sequence, expected_seq);
            send_nack(fd, ifindex, expected_seq);
        } 
        return LISTEN_SEQ_ERROR;
    }

    send_ack(fd, ifindex, received_msg->sequence);
    next_sequence();
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

void wait_big(int fd, uint32_t ifindex){
    struct message received_msg;
    int result = -1;
    int raw_type;
    while(result != TYPE_TXT && result != TYPE_JPG && result != TYPE_MP4 ){
        raw_type = listener_mode(fd, &received_msg);
        result = handle_listen_result(fd, ifindex, raw_type, &received_msg, global_sequence.value);
    }
    char name[42] = "Receive/"; //(tamanho de "Receive/")8  + 32 + 1 para termindaor nulo
    
    char* data_ptr = (char*)received_msg.data;
    
    if (result == TYPE_TXT)
        snprintf(name, sizeof(name), "Receive/%s.txt", data_ptr);
    else if (result == TYPE_JPG)
        snprintf(name, sizeof(name), "Receive/%s.jpg", data_ptr);
    else if (result == TYPE_MP4)
        snprintf(name, sizeof(name), "Receive/%s.mp4", data_ptr);

    FILE* new_file = fopen(name, "wb");
    result = -1;
    //não ta finalizando(devo ter esquecido alguma lógica na finalização)
    while(result != TYPE_END){
        raw_type = listener_mode(fd, &received_msg);
        result = handle_listen_result(fd, ifindex, raw_type, &received_msg, global_sequence.value);
        fprintf(stderr,"result in wait: %d\n", result);
        if(result == TYPE_DATA)
            fwrite(received_msg.data, 1, received_msg.size, new_file);
    }
    fclose(new_file);
}
char* wait_map(int fd, uint32_t ifindex){
    struct message received_msg;
    int result = -1;
    int raw_type;

    uint32_t size = 0;
    char* map_view = malloc(sizeof(char));
    result = -1;
    //não ta finalizando(devo ter esquecido alguma lógica na finalização)
    fprintf(stderr, "inside wainting map \n");
    while(result != TYPE_END){
        raw_type = listener_mode(fd, &received_msg);
        result = handle_listen_result(fd, ifindex, raw_type, &received_msg, global_sequence.value);
        fprintf(stderr,"result in wait: %d\n", result);
        if(result == TYPE_DATA){
            fprintf(stderr, "Recieved DATA\n");
            size += received_msg.size;
            fprintf(stderr, "REALLOC map size:%d\n", size);
            map_view = realloc(map_view, size*sizeof(char));
            memcpy(map_view, received_msg.data, received_msg.size);
        }
    }
    fprintf(stderr,"recieved END\n");
    return map_view;
}