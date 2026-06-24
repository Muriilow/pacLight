#include <stdlib.h>         // malloc/exit
#include <string.h>         // memset/memcpy
#include <unistd.h>         // close()
#include <fcntl.h>          // open()

// O "Coração" dos Sockets
#include <sys/socket.h>     // socket(), bind(), sendto()
#include <arpa/inet.h>      // htons() e manipulação de endereços
#include <sys/ioctl.h>      // ioctl() para configurar a placa de rede
#include <net/if.h>         // struct ifreq (para achar a eth0)
#include <stdint.h>         // uint
#include <sys/time.h>       // struct timeval

// Cabeçalhos de Protocolo
#include <linux/if_packet.h>// struct sockaddr_ll (Camada 2)
#include <net/ethernet.h>   // struct ethhdr (Cabeçalho Ethernet)

// Cabeçalhos locais
#include "Socket.h"

static int message_log_fd = -1;

static const char *message_type_name(uint8_t type)
{
    switch (type) {
        case TYPE_ACK: return "ACK";
        case TYPE_NACK: return "NACK";
        case TYPE_VISUAL: return "VISUAL";
        case TYPE_INITIALIZING: return "INITIALIZING";
        case TYPE_DATA: return "DATA";
        case TYPE_TXT: return "TXT";
        case TYPE_JPG: return "JPG";
        case TYPE_MP4: return "MP4";
        case TYPE_END: return "END";
        case TYPE_RIGHT: return "RIGHT";
        case TYPE_LEFT: return "LEFT";
        case TYPE_DOWN: return "DOWN";
        case TYPE_UP: return "UP";
        case TYPE_ERROR: return "ERROR";
        case TYPE_EXIT: return "EXIT";
        default: return "UNKNOWN";
    }
}

static void log_frame(const char *direction, uint32_t ifindex, const uint8_t *frame,
                      size_t frame_size, ssize_t io_bytes)
{
    if (message_log_fd < 0 || frame == NULL || frame_size < 3 || frame[0] != 126) {
        return;
    }

    uint8_t size = frame[1] & 0x1F;
    uint8_t seq = (uint8_t)(((frame[1] >> 5) | (frame[2] << 3)) & 0x3F);
    uint8_t type = (frame[2] >> 3) & 0x1F;

    dprintf(message_log_fd,
            "[%s] if=%u bytes=%zd frame=%zu seq=%u type=%u(%s) payload=%u raw=",
            direction, ifindex, io_bytes, frame_size, seq, type, message_type_name(type), size);

    for (size_t i = 0; i < frame_size; i++) {
        dprintf(message_log_fd, "%02x", frame[i]);
        if (i + 1 < frame_size) {
            dprintf(message_log_fd, " ");
        }
    }
    dprintf(message_log_fd, "\n");
}

int open_message_log_tty(const char *tty_path)
{
    if (tty_path == NULL || tty_path[0] == '\0') {
        return 0;
    }

    int fd = open(tty_path, O_WRONLY | O_NOCTTY);
    if (fd < 0) {
        perror("Erro ao abrir terminal de log");
        return -1;
    }

    if (message_log_fd >= 0) {
        close(message_log_fd);
    }

    message_log_fd = fd;
    dprintf(message_log_fd, "\n=== pacLight server message log ===\n");
    return 0;
}

void close_message_log_tty(void)
{
    if (message_log_fd >= 0) {
        dprintf(message_log_fd, "=== fim do log ===\n");
        close(message_log_fd);
        message_log_fd = -1;
    }
}

int create_raw_socket(uint32_t ifindex) {
    int32_t status;
    int32_t pac_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(pac_socket < 0) {
        fprintf(stderr, "Erro ao criar socket! {create_raw_socket}\n");
        exit(EXIT_FAILURE);
    }

    if (ifindex == 0) {
        fprintf(stderr, "Interface %d não encontrada!\n", ifindex);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_ll address = {0};
    address.sll_family = AF_PACKET;
    address.sll_protocol = htons(ETH_P_ALL);
    address.sll_ifindex = (int32_t)ifindex;

    status = bind(pac_socket, (struct sockaddr*) &address, sizeof(address));
    if(status < 0) {
        fprintf(stderr, "Erro ao conectar endereço ao socket! {create_raw_socket}\n");
        exit(EXIT_FAILURE);
    }
 
    struct packet_mreq mr = {0};
    mr.mr_ifindex = (int)ifindex;
    mr.mr_type = PACKET_MR_PROMISC;

    status = setsockopt(pac_socket, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr));
    if(status < 0) {
        fprintf(stderr, "Erro ao setar socket como promiscuo! {create_raw_socket}\n");
        exit(EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = 1; 
    tv.tv_usec = 300;
    status = setsockopt(pac_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if(status < 0) {
        fprintf(stderr, "Erro ao setar tempo de espera do socket! {create_raw_socket}\n");
        exit(EXIT_FAILURE);
    }

    return pac_socket;
}

void send_message(int pac_socket, uint32_t ifindex, uint8_t *message, size_t *final_size) {
    struct sockaddr_ll dest = {0};
    dest.sll_family = AF_PACKET;
    dest.sll_ifindex = (int32_t)ifindex;
    dest.sll_protocol = htons(ETH_P_ALL);
    dest.sll_halen = ETH_ALEN;
    memset(dest.sll_addr, 0, 6);

    ssize_t send_bytes = sendto(pac_socket, message, *final_size, 0, (struct sockaddr*)&dest, sizeof(struct sockaddr_ll));

    if(send_bytes == -1) {
        perror("Erro ao enviar pacote");
    } else {
        //fprintf(stderr,"Mensagem %d enviada: %zd bytes na interface %d\n",global_sequence.value, send_bytes, ifindex);
        log_frame("TX", ifindex, message, *final_size, send_bytes);
    }
}

int listener_mode(int32_t fd, struct message *received_msg) {
    uint8_t buffer[2048]; 
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);

    //printf("listener mode entered\n");
    while (1) {
        ssize_t bytes_lidos = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &addr_len);
        
        if (bytes_lidos < 0)
            return LISTEN_TIMEOUT; 

        if (buffer[0] == 126) {
            log_frame("RX", (uint32_t)src_addr.sll_ifindex, buffer, (size_t)bytes_lidos, bytes_lidos);

            uint8_t size = buffer[1] & 0x1F;
            uint8_t seq = (uint8_t)(((buffer[1] >> 5) | (buffer[2] << 3)) & 0x3F);
            uint8_t type = (buffer[2] >> 3) & 0x1F;

            //fprintf(stderr,"type: %d \n", type);
            // Popula a estrutura mesmo se o CRC puder falhar, para debug ou uso parcial
            if (received_msg != NULL) {
                received_msg->start_marker = 126;
                received_msg->size = (uint8_t)(size & 0x1F);
                received_msg->sequence = (uint8_t)(seq & 0x3F);
                received_msg->type = (uint8_t)(type & 0x1F);
            }

            size_t escaped_size = 0;
            uint8_t decoded_size = 0;
            while (decoded_size < size && 3 + escaped_size < (size_t)bytes_lidos) {
                unsigned char byte = (unsigned char)buffer[3 + escaped_size];
                escaped_size++;
                decoded_size++;
                if ((byte == 0x81 || byte == 0x88 || byte == 0xff) &&
                    3 + escaped_size < (size_t)bytes_lidos &&
                    (unsigned char)buffer[3 + escaped_size] == 0xff) {
                    escaped_size++;
                }
            }

            size_t crc_index = 3 + escaped_size;
            if (decoded_size != size || crc_index >= (size_t)bytes_lidos) {
                //fprintf(stderr, "pacote incompleto debug: bytes_lidos=%zd size=%u escaped_size=%zu decoded_size=%u crc_index=%zu\n",
                //        bytes_lidos, size, escaped_size, decoded_size, crc_index);
                //printf("Erro: pacote incompleto.\n");
                return LISTEN_CRC_ERROR;
            }

            uint8_t normal_frame[3 + MAX_DATA + 1] = {0};
            memcpy(normal_frame, buffer, 3);

            uint8_t unescaped_size = 0;
            for (size_t i = 0; unescaped_size < size; i++) {
                unsigned char byte = (unsigned char)buffer[3 + i];
                normal_frame[3 + unescaped_size++] = buffer[3 + i];
                if ((byte == 0x81 || byte == 0x88 || byte == 0xff) &&
                    i + 1 < escaped_size &&
                    (unsigned char)buffer[3 + i + 1] == 0xff) {
                    i++;
                }
            }
            normal_frame[3 + size] = buffer[crc_index];

            // Verifica CRC sobre a mensagem reconstruída, sem bytes de escape.
            if (crc8_bitwise(normal_frame, (size_t)(3 + size)) != normal_frame[3 + size]) {
                //printf("Erro: CRC inválido.\n");
                return LISTEN_CRC_ERROR;
            }

            if (received_msg != NULL && size > 0) {
                received_msg->data = malloc(size);
                if (received_msg->data) {
                    memcpy(received_msg->data, normal_frame + 3, size);
                }
            } else if (received_msg != NULL) {
                received_msg->data = NULL;
            }
            return (int)type; 
        }
    }
}
