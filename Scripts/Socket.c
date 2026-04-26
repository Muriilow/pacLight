#include <stdlib.h>         // malloc/exit
#include <string.h>         // memset/memcpy
#include <unistd.h>         // close()

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
    tv.tv_sec = 2; 
    tv.tv_usec = 0;
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
        printf("Mensagem enviada: %zd bytes na interface %d\n", send_bytes, ifindex);
    }
}

int listener_mode(int32_t fd, struct message *received_msg) {
    uint8_t buffer[2048]; 
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);

    printf("Aguardando pacotes...\n");

    while (1) {
        ssize_t bytes_lidos = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&src_addr, &addr_len);
        
        if (bytes_lidos < 0) 
            return -1; 

        if (buffer[0] == 126) {
            uint8_t size = buffer[1] & 0x1F;
            uint8_t seq = (uint8_t)(((buffer[1] >> 5) | (buffer[2] << 3)) & 0x3F);
            uint8_t type = (buffer[2] >> 3) & 0x1F;

            // Ignora ACKs/NACKs e o próprio mapa enviado (evita eco em loopback)
            if (type == TYPE_ACK || type == TYPE_NACK)
                continue;

            // Verifica CRC (do marcador até o byte antes do CRC)
            if (crc8_bitwise(buffer, (size_t)(3 + size)) != buffer[3 + size]) {
                printf("Erro na verificacao de integridade (CRC incorreto)!\n");
                continue;
            }

            if (received_msg != NULL) {
                received_msg->start_marker = 126;
                received_msg->size = (uint8_t)(size & 0x1F);
                received_msg->sequence = (uint8_t)(seq & 0x3F);
                received_msg->type = (uint8_t)(type & 0x1F);
                if (size > 0) {
                    received_msg->data = malloc(size);
                    memcpy(received_msg->data, &buffer[3], size);
                } else {
                    received_msg->data = NULL;
                }
            }
            // ... resto do log ...

            printf("\n--- Novo Pacote Recebido (%zd bytes) ---\n", bytes_lidos);
            printf("Size: %d | Seq: %d | Type: %d\n", size, seq, type);
             
            return (int)type; 
        }
    }
}

int wait_response(int32_t fd) {
    uint8_t buffer[2048]; 
    while(1) {
        ssize_t bytes_lidos = recvfrom(fd, buffer, sizeof(buffer), 0, NULL, NULL);
        if (bytes_lidos < 0) {
            printf("Timeout: Nenhuma resposta recebida.\n");
            return -1;
        }

        if (buffer[0] == 126) {
            uint8_t type = (buffer[2] >> 3) & 0x1F;
            if (type == TYPE_ACK) return TYPE_ACK;
            if (type == TYPE_NACK) return TYPE_NACK;
        }
    }
}
