#include <stdio.h>          // printf
#include <stdlib.h>         // malloc/exit
#include <string.h>         // memset/memcpy
#include <unistd.h>         // close()

// O "Coração" dos Sockets
#include <sys/socket.h>     // socket(), bind(), sendto()
#include <arpa/inet.h>      // htons() e manipulação de endereços
#include <sys/ioctl.h>      // ioctl() para configurar a placa de rede
#include <net/if.h>         // struct ifreq (para achar a eth0)
#include <stdint.h>         // uint
                            //
// Cabeçalhos de Protocolo
#include <linux/if_packet.h>// struct sockaddr_ll (Camada 2)
#include <net/ethernet.h>   // struct ethhdr (Cabeçalho Ethernet)

// Cabecalhos locais
#include "Socket.h"

struct global_sequence global_sequence = {0};

//esse int na verdade é um file descriptor
int create_raw_socket(uint32_t ifindex) {
    int32_t status;
    // Cria arquivo para o socket sem qualquer protocolo
    int32_t pac_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(pac_socket < 0)
    {
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
    address.sll_ifindex = (uint32_t)ifindex;

    // Inicializa socket
    status = bind(pac_socket, (struct sockaddr*) &address, sizeof(address));
    if(status < 0)
    {
        fprintf(stderr, "Erro ao conectar endereço ao socket! {create_raw_socket}\n");
        exit(EXIT_FAILURE);
    }
 
    struct packet_mreq mr = {0};
    mr.mr_ifindex = (int)ifindex;
    mr.mr_type = PACKET_MR_PROMISC;

    // Não joga fora o que identifica como lixo: Modo promíscuo
    status = setsockopt(pac_socket, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr));
    if(status < 0)
    {
        fprintf(stderr, "Erro ao setar socket como promiscuo! {create_raw_socket}\n");
        exit(EXIT_FAILURE);
    }

    return pac_socket;
}

struct message* create_message(uint32_t size, uint32_t type, uint8_t data[32]){
    struct message* new_message = malloc(sizeof(struct message));
    if(new_message == 0)
    {
        fprintf(stderr, "Erro ao criar mensagem! {create_message}\n");
        exit(EXIT_FAILURE);
    }

    new_message->start_marker = 126;
    new_message->size = (uint8_t)(size & 0x1F);
    
    new_message->sequence = (uint8_t)(global_sequence.value & 0x3F);
    global_sequence.value++;

    new_message->type = (uint8_t)(type & 0x1F);
    memcpy(new_message->data, data, sizeof(new_message->data));
    new_message->CRC = 1;

    return new_message;
}

void send_message(int pac_socket, int ifindex, struct message* message)
{
    struct sockaddr_ll dest = {0};
    dest.sll_family = AF_PACKET;
    dest.sll_ifindex = ifindex;
    dest.sll_halen = ETH_ALEN;

    // No loopback, o endereço MAC costuma ser tudo zero
    memset(dest.sll_addr, 0, 6);

    ssize_t send_bytes = sendto
    (
        pac_socket,
        message,
        message->size,
        0,
        (struct sockaddr*)&dest,
        sizeof(struct sockaddr_ll)
    );

    if(send_bytes == -1) {
        fprintf(stderr, "Erro ao enviar pacote {send_message}\n");
    }
    else {
        printf("Mensagem enviada: %zd bytes enviados ns interface %d\n", send_bytes, ifindex);
    }
}
