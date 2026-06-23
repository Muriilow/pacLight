#include "Message.h"
#include "Socket.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <sys/inotify.h>

//ir para a linah 132
struct global_sequence global_sequence = {0};

static uint8_t normalize_sequence(uint8_t seq)
{
    return (uint8_t)(seq & 0x3F);
}

static uint8_t previous_sequence(uint8_t seq)
{
    return normalize_sequence((uint8_t)(seq - 1));
}

static void free_message_data(struct message *msg)
{
    if (msg != NULL && msg->data != NULL) {
        free(msg->data);
        msg->data = NULL;
    }
}

uint32_t prepare_data(const char* data, uint32_t size, char* updated_data){
    if(data == NULL || updated_data == NULL)
        return 0;

    uint32_t j = 0;
    for(uint32_t i = 0; i < size; i++){
        unsigned char byte = (unsigned char)data[i];
        updated_data[j++] = data[i];
        if(byte == 0x81 || byte == 0x88 || byte == 0xff){
            updated_data[j++] = (char)0xff;
        }
    }
    return j;
}

struct message* create_message(uint32_t size, uint32_t type, uint8_t seq, void* data) {
    struct message* new_message = malloc(sizeof(struct message));
    char* new_data = NULL;

    if (new_message == NULL) {
        fprintf(stderr, "Erro ao criar mensagem! {create_message}\n");
        exit(EXIT_FAILURE);
    }

    if (size > MAX_DATA) {
        fprintf(stderr, "Payload muito grande! {create_message}\n");
        free(new_message);
        return NULL;
    }

    if (size > 0 && data != NULL) {
        new_data = malloc(sizeof(char) * size);
        if (new_data == NULL) {
            free(new_message);
            return NULL;
        }
        memcpy(new_data, data, size);
    }

    new_message->start_marker = 126;
    new_message->size = (uint8_t)(size & 0x1F);
    new_message->sequence = (uint8_t)(seq & 0x3F);
    new_message->type = (uint8_t)(type & 0x1F);
    new_message->data = new_data;
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
    
    //fprintf(stderr, "%s", final);
    return final;
}

void next_sequence() {
    global_sequence.value = (uint8_t)((global_sequence.value + 1) & 0x3F);
}

uint8_t *serialize_message(struct message *msg, size_t *final_size) {
    size_t normal_frame_size = (size_t)(3 + msg->size + 1);
    uint8_t *normal_frame = calloc(1, normal_frame_size);
    if (!normal_frame) return NULL;

    memcpy(normal_frame, msg, 3);
    if (msg->size > 0 && msg->data != NULL) {
        memcpy(normal_frame + 3, msg->data, msg->size);
    }

    //Applying CRC before escaping payload bytes
    msg->CRC = crc8_bitwise((const uint8_t *)normal_frame, normal_frame_size - 1);
    normal_frame[normal_frame_size - 1] = msg->CRC;

    size_t max_payload_size = (size_t)msg->size * 2;
    size_t max_frame_size = 3 + max_payload_size + 1;
    uint8_t *buffer = calloc(1, max_frame_size);
    if (!buffer) {
        free(normal_frame);
        return NULL;
    }

    memcpy(buffer, normal_frame, 3);

    uint32_t escaped_size = 0;
    if (msg->size > 0 && msg->data != NULL) {
        escaped_size = prepare_data((const char*)msg->data, msg->size, (char*)buffer + 3);
    }

    size_t actual_frame_size = 3 + escaped_size + 1;
    buffer[actual_frame_size - 1] = msg->CRC;
    free(normal_frame);

    *final_size = (actual_frame_size < 14) ? 14 : actual_frame_size;
    if (*final_size > actual_frame_size) {
        uint8_t *padded_buffer = realloc(buffer, *final_size);
        if (padded_buffer == NULL) {
            free(buffer);
            return NULL;
        }
        memset(padded_buffer + actual_frame_size, 0, *final_size - actual_frame_size);
        buffer = padded_buffer;
    }

    return buffer;
}


void send_ack(int fd, uint32_t ifindex, uint8_t seq) {
    struct message *msg = create_message(0, TYPE_ACK, seq, NULL);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);
    if (buffer) {
        //printf("ACK ");
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
        //printf("NACK ");
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
    
    int result = -4;
    int raw_type;
    struct message ack_addr = {0};
    uint32_t i;
    int vision = game->visibility_radius;
    struct message *msg = create_message(sizeof(vision), TYPE_VISUAL, global_sequence.value, &vision);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);

    while(result != TYPE_ACK)
    {   
        //printf("VIS ");
        send_message(fd, ifindex, buffer, &final_size);
        raw_type = listener_mode(fd, &ack_addr);
        result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);

        if(ack_addr.data) {
            free(ack_addr.data);
            ack_addr.data = NULL;
        }
    }
    if (buffer)
        free(buffer);
    if(msg->data)
        free(msg->data);
    free(msg);
    //fprintf(stderr,"total size: %d\n", total_size);
    for(i = 0; i < total_size - total_size%MAX_DATA; i+=MAX_DATA){
        //fprintf(stderr,"i: %d \n", i);
        char visible_grid_seg[MAX_DATA];
        memcpy(visible_grid_seg, &visible_grid[i], MAX_DATA);
        msg = create_message(MAX_DATA, TYPE_DATA, global_sequence.value, visible_grid_seg);
        buffer = serialize_message(msg, &final_size);

        result = -4;
        while(result != TYPE_ACK){
            //printf("MAP ");
            send_message(fd, ifindex, buffer, &final_size);
            raw_type = listener_mode(fd, &ack_addr);
            result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);
            free_message_data(&ack_addr);
        }

        if (buffer)
            free(buffer);
        if(msg->data)
            free(msg->data);
        free(msg);
    }
    if (total_size%MAX_DATA > 0){
        char last_grid_seg[total_size%MAX_DATA];
        memcpy(last_grid_seg, &visible_grid[i], total_size%MAX_DATA);
        msg = create_message(total_size%MAX_DATA, TYPE_DATA, global_sequence.value, last_grid_seg);
        buffer = serialize_message(msg, &final_size);
        result = -4;
        while(result != TYPE_ACK){
            //printf("LAST MAP ");
            send_message(fd, ifindex, buffer, &final_size);
            raw_type = listener_mode(fd, &ack_addr);
            result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);
            free_message_data(&ack_addr);
        }

        if (buffer)
            free(buffer);
        if(msg->data)
            free(msg->data);
        free(msg);
    }

    free(visible_grid);
    msg = create_message(0, TYPE_END, global_sequence.value, NULL);
    buffer = serialize_message(msg, &final_size);

    result = -4;
    while(result != TYPE_ACK)
    {   
        //printf("END ");
        send_message(fd, ifindex, buffer, &final_size);
        raw_type = listener_mode(fd, &ack_addr);
        result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);

        if(ack_addr.data) {
            free(ack_addr.data);
            ack_addr.data = NULL;
        }
    }
    if (buffer)
        free(buffer);
    free(msg);
    //fprintf(stderr, "ACK RECEBIDO\n");
}

void send_up(int fd, uint32_t ifindex)
{
    struct message *msg = create_message(0, TYPE_UP, global_sequence.value, NULL);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);
    
    if (buffer) {
        //printf("UP  ");
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
        //printf("DOW ");
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
        //printf("LEF ");
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
        //printf("RIG ");
        send_message(fd, ifindex, buffer, &final_size);
        free(buffer);
    }
    
    free(msg);
}
void send_file(int fd, uint32_t ifindex, char* name, uint32_t type){

    uint32_t n_size = (uint32_t) strlen(name)+1;
    if (n_size > 31){
        fprintf(stderr,"Nome muito grande\n");
        return;
    }
    int result = -4;
    int raw_type;
    struct message ack_addr = {0};

    struct message *msg = create_message(n_size, type, global_sequence.value, name);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);

    while(result != TYPE_ACK)
    {   
        send_message(fd, ifindex, buffer, &final_size);
        raw_type = listener_mode(fd, &ack_addr);
        result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);

        if(ack_addr.data) {
            free(ack_addr.data);
            ack_addr.data = NULL;
        }
    }
    if (buffer)
        free(buffer);
    if(msg->data)
        free(msg->data);
    free(msg);
    
    char* file_name = name_handle(name, (int)type);
    FILE *fptr = fopen(file_name, "rb");
    free(file_name);
    if (fptr == NULL) return;

    fseek(fptr, 0, SEEK_END);
    long size = ftell(fptr);
    if(size < 0){
        printf("ftell error\n");
        fclose(fptr);
        return;
    }

    fseek(fptr, 0, SEEK_SET);
    uint32_t seg_size;
    for(long i = 0; i < size - size%MAX_DATA; i += MAX_DATA){
        //fprintf(stderr,"envio: %8.5f\n",(float)i/(float)size*100);
        char file_data[MAX_DATA];
        seg_size = (uint32_t) fread(file_data, 1, MAX_DATA, fptr);
        if (seg_size != MAX_DATA){
            fseek(fptr, -seg_size, SEEK_CUR);
        }
        else
        {

            msg = create_message(MAX_DATA, TYPE_DATA, global_sequence.value, file_data);
            buffer = serialize_message(msg, &final_size);
            result = -4;
            while(result != TYPE_ACK)
            {
                //printf("DATA %d ", global_sequence.value);
                send_message(fd, ifindex, buffer, &final_size);
                raw_type = listener_mode(fd, &ack_addr);
                result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);
                free_message_data(&ack_addr);
            }
            if(buffer)
                free(buffer);
            if(msg->data)
                free(msg->data);
            free(msg);
        }
    } 
    char file_data[size%MAX_DATA];
    fread(file_data, 1, (size_t)size%MAX_DATA, fptr);
    msg = create_message((uint32_t)(size%MAX_DATA), TYPE_DATA, global_sequence.value, file_data);
    buffer = serialize_message(msg, &final_size);
    result = -4;
    while(result != TYPE_ACK)
    {   
        //printf("DATA %d ", global_sequence.value);
        send_message(fd, ifindex, buffer, &final_size);
        raw_type = listener_mode(fd, &ack_addr);
        result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);

        if(ack_addr.data) {
            free(ack_addr.data);
            ack_addr.data = NULL;
        }
    }
    if(buffer)
        free(buffer);
    if(msg->data)
        free(msg->data);
    free(msg);
    //fprintf(stderr,"ENDING\n");
    msg = create_message(0, TYPE_END, global_sequence.value, NULL);
    buffer = serialize_message(msg, &final_size);

    result = -4;
    while(result != TYPE_ACK)
    {   
        //printf("END ");
        send_message(fd, ifindex, buffer, &final_size);
        raw_type = listener_mode(fd, &ack_addr);
        result = handle_listen_result(fd, ifindex, raw_type, &ack_addr, global_sequence.value);

        if(result == TYPE_ACK){
            //fprintf(stderr, "ACK RECEBIDO\n");
        }
        if(ack_addr.data) {
            free(ack_addr.data);
            ack_addr.data = NULL;
        }
    }
    
    if (buffer)
        free(buffer);
    if(msg->data)
            free(msg->data);
    free(msg);
    fclose(fptr);
}

int handle_listen_result(int fd, uint32_t ifindex, int listen_return, struct message *received_msg, uint8_t expected_seq) 
{
    uint8_t expected = normalize_sequence(expected_seq);

    if (listen_return == LISTEN_TIMEOUT) 
        return listen_return;

    if (listen_return == LISTEN_CRC_ERROR) 
    {
        //fprintf(stderr, "ERRO DE CRC\n");
        send_nack(fd, ifindex, expected);
        return listen_return;
    }

    if (listen_return == TYPE_ACK || listen_return == TYPE_NACK)
    {
        if (normalize_sequence(received_msg->sequence) == expected)
        {
            if (listen_return == TYPE_ACK)
                next_sequence();
            return listen_return;
        }
        return LISTEN_SEQ_ERROR;
    }

    // Para pacotes de dados/comandos
    uint8_t received = normalize_sequence(received_msg->sequence);
    if (received != expected)
    {
        // Stop-and-wait: o unico pacote antigo aceitavel e o anterior modulo 64.
        if (received == previous_sequence(expected)){
            //fprintf(stderr, "ERRO DE SEQUENCIA - Recebido:%d Esperado:%d\n",received_msg->sequence, expected_seq);
            send_ack(fd, ifindex, received);
        }else{
            //fprintf(stderr, "ERRO DE SEQUENCIA - Recebido:%d Esperado:%d\n",received_msg->sequence, expected_seq);
            send_nack(fd, ifindex, expected);
        } 
        return LISTEN_SEQ_ERROR;
    }

    send_ack(fd, ifindex, received);
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

void wait_file(int fd, uint32_t ifindex, int type, char* fileName){
    //fprintf(stderr,"Waiting file\n");
    struct message received_msg = {0};
    int result = -4;
    int raw_type;
    char name[42] = {0};
    
    if (type == TYPE_TXT)
        snprintf(name, sizeof(name), "%s.txt", fileName);
    else if (type == TYPE_JPG)
        snprintf(name, sizeof(name), "%s.jpg", fileName);
    else if (type == TYPE_MP4)
        snprintf(name, sizeof(name), "%s.mp4", fileName);

    //fprintf(stderr,"%s\n",name);
    FILE* new_file = fopen(name, "wb");
    if (new_file == NULL) {
        perror("Erro ao criar arquivo recebido");
        return;
    }
    //não ta finalizando(devo ter esquecido alguma lógica na finalização)
    while(result != TYPE_END){
        raw_type = listener_mode(fd, &received_msg);
        result = handle_listen_result(fd, ifindex, raw_type, &received_msg, global_sequence.value);
        //fprintf(stderr,"result in wait: %d\n", result);
        if(result == TYPE_DATA && received_msg.data != NULL)
            fwrite(received_msg.data, 1, received_msg.size, new_file);

        if(received_msg.data) {
            free(received_msg.data);
            received_msg.data = NULL;
        }
    }
    fclose(new_file);

    pid_t pid = fork();
    if (pid == 0) {
        char *sudo_gid = getenv("SUDO_GID");
        char *sudo_uid = getenv("SUDO_UID");
        char *sudo_user = getenv("SUDO_USER");

        if (sudo_gid != NULL && sudo_uid != NULL) {
            uid_t uid = (uid_t)strtoul(sudo_uid, NULL, 10);
            gid_t gid = (gid_t)strtoul(sudo_gid, NULL, 10);
            struct passwd *pw = getpwuid(uid);
            char runtime_dir[64];

            if (pw != NULL && pw->pw_dir != NULL) {
                setenv("HOME", pw->pw_dir, 1);
            }
            if (sudo_user != NULL) {
                setenv("USER", sudo_user, 1);
                setenv("LOGNAME", sudo_user, 1);
            }
            snprintf(runtime_dir, sizeof(runtime_dir), "/run/user/%lu", (unsigned long)uid);
            setenv("XDG_RUNTIME_DIR", runtime_dir, 1);
            unsetenv("XDG_CONFIG_HOME");
            unsetenv("XDG_CACHE_HOME");
            unsetenv("XDG_DATA_HOME");

            if (setgid(gid) != 0 || setuid(uid) != 0) {
                _exit(EXIT_FAILURE);
            }
        }

        freopen("/dev/null", "r", stdin);
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        setsid();

        execlp("xdg-open", "xdg-open", name, (char *)NULL);
        //perror("Error deleting file");
        //fprintf(stdout, "\n\nSOCORRO\n\n");
        //fprintf(stderr, "\n\nSOCORRO\n\n");
        _exit(EXIT_FAILURE);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
        char command[256];
        // Usa o comando fuser para ver se o arquivo está sendo usado em qualquer aplicativo 
        snprintf(command, sizeof(command), "fuser -s %s", name);

        // Sleep para dar tempo ao xdg-open
        sleep(3); 

        // enquanto o arquivo estiver aberto por qualquer processo ele não será excluido, testa a cada segundo
        while (system(command) == 0) {
            sleep(1);   
        }
        remove(name);

    } else {
        perror("Erro ao abrir arquivo recebido");
    }
}
char* wait_map(int fd, uint32_t ifindex){
    struct message received_msg = {0};
    int result = -4;
    int raw_type;

    uint32_t size = 0;
    char* map_view = malloc(sizeof(char));
    result = -4;
    //não ta finalizando(devo ter esquecido alguma lógica na finalização)
    //fprintf(stderr, "inside wainting map \n");
    while(result != TYPE_END){
        raw_type = listener_mode(fd, &received_msg);
        result = handle_listen_result(fd, ifindex, raw_type, &received_msg, global_sequence.value);
        //fprintf(stderr,"result in wait: %d\n", result);
        if(result == TYPE_DATA){
            //fprintf(stderr, "Received DATA\n");
            size += received_msg.size;
            //fprintf(stderr, "REALLOC map size:%d\n", size);
            map_view = realloc(map_view, size*sizeof(char));
            memcpy(map_view+size-received_msg.size, received_msg.data, received_msg.size);
        }
        if(received_msg.data){
            free(received_msg.data);
            received_msg.data = NULL;
        }
    }
    //fprintf(stderr,"recieved END\n");
    return map_view;
}
void end_game(int fd, uint32_t ifindex){
    struct message *msg = create_message(0, TYPE_FINISH, global_sequence.value, NULL);
    size_t final_size;
    uint8_t *buffer = serialize_message(msg, &final_size);
    
    if (buffer) {
        //printf("FINISH ");
        send_message(fd, ifindex, buffer, &final_size);
        free(buffer);
    }
    
    free(msg);
}
