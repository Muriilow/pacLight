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

// Cabeçalhos de Protocolo
#include <linux/if_packet.h>// struct sockaddr_ll (Camada 2)
#include <net/ethernet.h>   // struct ethhdr (Cabeçalho Ethernet)

// Cabeçalhos locais
#include "Socket.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Uso: %s [servidor|player] [interface]\n", argv[0]);
        return 1; 
    }

    char *mode = argv[1];
    char *interface = (argc > 2) ? argv[2] : "lo";

    uint32_t ifindex = if_nametoindex(interface);
    int32_t file_desc = create_raw_socket(ifindex);
    

    int ack_status = 1;
    uint8_t ack_data[32];
    memset(ack_data, 0, 32);
    strcpy((char*)ack_data, "ACK");
    struct message *ack_message = create_message(10,0,ack_data);//tamanho minimo é 10

    size_t *size_ack = malloc(sizeof(size_t));
    uint8_t *ack = serialize_message(ack_message, size_ack);

    if(strcmp(mode, "player") == 0)
    {
        printf("Iniciando o jogador!\n");
        
        uint8_t my_data[32];
        memset(my_data, 0, 32);
        strcpy((char*)my_data, "Testando rawSocket!!");
        struct message* msg = create_message(20, 3, my_data);

        size_t *final_size = malloc(sizeof(size_t));
        uint8_t *buffer = serialize_message(msg, final_size);
        
        while(ack_status != 0){
        send_message(file_desc, ifindex, buffer, final_size);
        ack_status = wait_ack(file_desc); //está demorando muito na primeira execução do loop e depois faz varias de uma vez e trava novamente
        }

        free(msg);
        free(final_size);
        free(buffer);
    }

    if(strcmp(mode, "server") == 0)
    {
        printf("Iniciando o servidor!\n");
        while(1){
            if(listener_mode(file_desc) != 0 )
                send_message(file_desc,ifindex, ack, size_ack); // não está funcionando, erro sendto:invalid argument
        }
    }

    free(ack);
    free(ack_message);
    free(size_ack);
}
