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
int main() 
{
     uint32_t ifindex = if_nametoindex("lo");
     int32_t file_desc = create_raw_socket(ifindex);

     uint32_t my_data[32];
     memset(my_data, 0, 32);
     strcpy((char*)my_data, "Testando rawSocket!!");


}
