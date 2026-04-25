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
    address.sll_ifindex = (int32_t)ifindex;

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

    // Setando tempo de espera do socket (MANDATÓRIO PELO PROJETO)
    const int32_t timeoutMillis = 300;
    struct timeval tv = { 
        .tv_sec = timeoutMillis / 1000,
        .tv_usec = (timeoutMillis % 1000) * 1000
    };

    status = setsockopt(pac_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if(status < 0)
    {
        fprintf(stderr, "Erro ao setar tempo de espera do socket! {create_raw_socket}\n");
        exit(EXIT_FAILURE);
    }
    

    return pac_socket;
}

void send_message(int pac_socket, uint32_t ifindex, uint8_t *message, size_t *final_size)
{
    struct sockaddr_ll dest = {0};
    dest.sll_family = AF_PACKET;
    dest.sll_ifindex = (int32_t)ifindex;
    dest.sll_protocol = htons(ETH_P_ALL);
    dest.sll_halen = ETH_ALEN;

    // No loopback, o endereço MAC costuma ser tudo zero
    memset(dest.sll_addr, 0, 6);

    ssize_t send_bytes = sendto
    (
        pac_socket,
        message,
        *final_size,
        0,
        (struct sockaddr*)&dest,
        sizeof(struct sockaddr_ll)
    );

    if(send_bytes == -1) {
        perror("Erro ao enviar pacote");
    }
    else {
        printf("Mensagem enviada: %zd bytes na interface %d\n", send_bytes, ifindex);
    }
}

int listener_mode(int32_t fd, int8_t *current_seq)
{
    uint8_t type;
    uint8_t buffer[2048]; 
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);

    printf("Aguardando pacotes...\n");

    while (1) {
        ssize_t bytes_lidos = recvfrom(fd, buffer, sizeof(buffer), 0, 
                                       (struct sockaddr*)&src_addr, &addr_len);
        
        if (bytes_lidos < 0) {
            // Timeout do SO_RCVTIMEO
            return -1; 
        }

        // Verificar marcador de início
        if (buffer[0] == 126) {
            type = (buffer[2] >> 3) & 0x1F;

            // Ignoramos ACKs (tipo 0) no modo listener pois esperamos DADOS.
            if (type == TYPE_ACK || type == TYPE_NACK) continue;

            printf("\n--- Novo Pacote de Dados Recebido (%zd bytes) ---\n", bytes_lidos);
            
            uint8_t size = buffer[1] & 0x1F;
            uint8_t sequence = (uint8_t)((buffer[1] >> 5) | (buffer[2] << 3)) & 0x3F;
            *current_seq = sequence; 

            printf("Size: %d | Seq: %d | Type: %d\n", size, sequence, type);
             
            return type; 
        }
    }
}

int wait_response(int32_t fd)
{
    uint8_t buffer[2048]; 
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);

    printf("Aguardando Resposta (ACK/NACK)...\n");
    
    while(1) {
        ssize_t bytes_lidos = recvfrom(fd, buffer, sizeof(buffer), 0, 
                                       (struct sockaddr*)&src_addr, &addr_len);
        
        if (bytes_lidos < 0) {
            printf("Timeout: Nenhuma resposta recebida.\n");
            return -1;
        }

        if (buffer[0] == 126) {
            uint8_t type = (buffer[2] >> 3) & 0x1F;
            
            if (type == TYPE_ACK){
                printf("ACK recebido!\n");
                return TYPE_ACK;
            }
            if (type == TYPE_NACK){
                printf("NACK recebido!\n");
                return TYPE_NACK;
            }
            // Ignora outros pacotes (ecos de dados)
        }
    }
}
