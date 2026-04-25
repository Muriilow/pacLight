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

    // Setando tempo de espera do socket 
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    status = setsockopt(pac_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if(status < 0)
    {
        fprintf(stderr, "Erro ao setar tempo de espera do socket! {create_raw_socket}\n");
        exit(EXIT_FAILURE);
    }
    

    return pac_socket;
}

struct message* create_message(uint32_t size, uint32_t type, void* data){
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
    new_message->data = data;
    new_message->CRC = 1;
    
    return new_message;
}

uint8_t *serialize_message(struct message *msg, size_t *final_size)
{
    *final_size = (uint64_t)3 + msg->size + 1;

    uint8_t *buffer = malloc(*final_size);
    memcpy(buffer, msg, 3);
    memcpy(buffer + 3, msg->data, msg->size);
    buffer[*final_size - 1] = msg->CRC; 
    return buffer; 
}

void send_message(int pac_socket, uint32_t ifindex, uint8_t *message, size_t *final_size)
{
    struct sockaddr_ll dest = {0};
    dest.sll_family = AF_PACKET;
    dest.sll_ifindex = (int32_t)ifindex;
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
        //fprintf(stderr, "Erro ao enviar pacote {send_message}\n");
    }
    else {
        printf("Mensagem enviada: %zd bytes enviados ns interface %d\n", send_bytes, ifindex);
    }
}
int listener_mode(int32_t fd)
{
    int waiting = 1;
    uint8_t type;
    uint8_t buffer[2048]; //Big buffer to assure segurance 
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);

    printf("Aguardando pacotes...\n");

    while (waiting) {
        ssize_t bytes_lidos = recvfrom(fd, buffer, sizeof(buffer), 0, 
                                       (struct sockaddr*)&src_addr, &addr_len);
        
        if (bytes_lidos < 0) {
            perror("Erro no recvfrom");
            break;
        }

        // 1. Verificar se o pacote é o nosso (Start Marker)
        if (buffer[0] == 126) {
            printf("\n--- Novo Pacote Recebido (%zd bytes) ---\n", bytes_lidos);
            
            // 2. Descompactar campos de bits do Header (Bytes 1 e 2)
            uint8_t size = buffer[1] & 0x1F;
            uint8_t sequence = (uint8_t)((buffer[1] >> 5) | (buffer[2] << 3)) & 0x3F;
            type = (buffer[2] >> 3) & 0x1F; // movi a declaração do type pra cima para usar como valor de retorno

            printf("Size: %d | Seq: %d | Type: %d\n", size, sequence, type);

            // 3. Extrair os Dados (começam no byte 3)
            printf("Dados: ");
            for(int i = 0; i < size; i++) {
                printf("%c", buffer[3 + i]);
            }
            
            // 4. Extrair CRC (está logo após os dados)
            uint8_t crc_recebido = buffer[3 + size];
            printf("\nCRC: %d\n", crc_recebido);
             
            waiting = 0;
        }
    }
    return type;
}
//o timeout não está funcionando, trava por bastante tempo no primeiro loop, 
//executa o loop várias vezes, depois trava de novo e fica repetindo
int wait_ack(int32_t fd)
{
    uint8_t buffer[2048]; //Big buffer to assure segurance 
    struct sockaddr_ll src_addr;
    socklen_t addr_len = sizeof(src_addr);

    printf("Aguardando ACK...\n");
    
        ssize_t bytes_lidos = recvfrom(fd, buffer, sizeof(buffer), 0, 
                                       (struct sockaddr*)&src_addr, &addr_len);
        
        if (bytes_lidos < 0) {
            perror("Não recebeu mensagem de volta. {wait_ack}");
            return -1;
        }

        // 1. Verificar se o pacote é o nosso (Start Marker)
        if (buffer[0] == 126) {

            uint8_t type = (buffer[2] >> 3) & 0x1F;
            
            // Esperar até uma mensagem do tipo ACK
            if (type == 0){
                printf("ack recebido\n");
                return 0;
            }
        }

    return -1;
}
