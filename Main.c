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
    
    if(strcmp(mode, "player") == 0)
    {
        uint32_t ifindex = if_nametoindex(interface);
        int32_t file_desc = create_raw_socket(ifindex);
        
        uint8_t my_data[32];
        memset(my_data, 0, 32);
        strcpy((char*)my_data, "Testando rawSocket!!");
        struct message* msg = create_message(19, 1, my_data);

        size_t *final_size;
        uint8_t *buffer = serialize_message(msg, final_size);
        send_message(file_desc, ifindex, buffer, final_size);

        free(msg);
    }
    if(strcmp(mode, "server") == 0)
    {
        printf("server!");
    }
}
