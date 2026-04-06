#include <stdio.h>          // printf
#include <stdlib.h>         // malloc/exit
#include <string.h>         // memset/memcpy
#include <unistd.h>         // close()

// O "Coração" dos Sockets
#include <sys/socket.h>     // socket(), bind(), sendto()
#include <arpa/inet.h>      // htons() e manipulação de endereços
#include <sys/ioctl.h>      // ioctl() para configurar a placa de rede
#include <net/if.h>         // struct ifreq (para achar a eth0)

// Cabeçalhos de Protocolo
#include <linux/if_packet.h>// struct sockaddr_ll (Camada 2)
#include <net/ethernet.h>   // struct ethhdr (Cabeçalho Ethernet)

struct global_sequence{
    unsigned int value : 6 ; 
} global_sequence = {0};

struct message {
    unsigned int start_marker : 8;
    unsigned int size : 5;
    unsigned int sequence : 6;
    unsigned int type : 5;
    long* data ;
    unsigned int CRC : 8;
};

//esse int na verdade é um file descriptor
int cria_raw_socket(char* nome_interface_rede) {
    int status;
    // Cria arquivo para o socket sem qualquer protocolo
    int soquete = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    assert(soquete); //Verifique se você é root
 
    int ifindex = if_nametoindex(nome_interface_rede);
 
    struct sockaddr_ll endereco = {0};
    endereco.sll_family = AF_PACKET;
    endereco.sll_protocol = htons(ETH_P_ALL);
    endereco.sll_ifindex = ifindex;
    // Inicializa socket
    status = bind(soquete, (struct sockaddr*) &endereco, sizeof(endereco));
    assert(status);
 
    struct packet_mreq mr = {0};
    mr.mr_ifindex = ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    // Não joga fora o que identifica como lixo: Modo promíscuo
    int status = setsockopt(soquete, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr));
    assert(status);

    return soquete;
}

struct message* cria_mensagem(unsigned int size, unsigned int type, int* data){
    struct message* new_message = malloc(sizeof(struct message));
    assert(new_message);

    new_message->start_marker = 126;
    new_message->size = size;
    
    new_message->sequence = global_sequence.value;
    global_sequence.value ++;

    new_message->type = type;
    new_message->type = data;
    new_message->CRC = 1;

    return new_message;
}