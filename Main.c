#include <stdio.h>          // printf
#include <stdlib.h>         // malloc/exit
#include <string.h>         // memset/memcpy
#include <unistd.h>         // close()

#include <sys/socket.h>     // socket(), bind(), sendto()
#include <arpa/inet.h>      // htons() e manipulação de endereços
#include <sys/ioctl.h>      // ioctl() para configurar a placa de rede
#include <net/if.h>         // struct ifreq (para achar a eth0)
#include <stdint.h>         // uint

#include <linux/if_packet.h>// struct sockaddr_ll (Camada 2)
#include <net/ethernet.h>   // struct ethhdr (Cabeçalho Ethernet)

#include "Socket.h"
#include "Message.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Uso: %s [server|player] [interface]\n", argv[0]);
        return 1; 
    }

    char *mode = argv[1];
    char *interface = (argc > 2) ? argv[2] : "lo";

    uint32_t ifindex = if_nametoindex(interface);
    int32_t file_desc = create_raw_socket(ifindex);
    
    if(strcmp(mode, "player") == 0)
    {
        printf("Iniciando o jogador!\n");
        
        uint8_t my_data[32];
        memset(my_data, 0, 32);
        strcpy((char*)my_data, "Testando rawSocket!!");
        struct message* msg = create_message(20, TYPE_DATA, my_data);

        size_t final_size;
        uint8_t *buffer = serialize_message(msg, &final_size);
        
        int response = -1;
        while(response != TYPE_ACK)
        {
            send_message(file_desc, ifindex, buffer, &final_size);
            response = wait_response(file_desc);
            
            if (response == TYPE_NACK) {
                printf("Servidor reportou erro (NACK). Reenviando...\n");
            } else if (response == -1) {
                printf("Timeout. Reenviando...\n");
            }
        }

        printf("Mensagem entregue com sucesso!\n");

        free(msg);
        free(buffer);
    }

    if(strcmp(mode, "server") == 0)
    {
        int8_t current_seq = 67;
        int8_t last_seq = 0;
        printf("Iniciando o servidor!\n");

        while(1)
        {
            int type = listener_mode(file_desc, &current_seq);
            if(type !=  -1) {

                if(current_seq == last_seq)
                {
                    printf("Mensagem duplicada! Ignorando e enviando ACK...\n");
                    send_ack(file_desc, ifindex); 
                    continue;
                }

                printf("Dados recebidos! Enviando ACK...\n");
                send_ack(file_desc, ifindex); 
                last_seq = current_seq;
            }
        }
    }

    return 0;
}
