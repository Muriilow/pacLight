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
        
        // Player envia usando a global_sequence.value
        struct message* msg = create_message(20, TYPE_DATA, global_sequence.value, my_data);
        size_t final_size;
        uint8_t *buffer = serialize_message(msg, &final_size);
        
        int response = -1;
        while(response != TYPE_ACK)
        {
            send_message(file_desc, ifindex, buffer, &final_size);
            response = wait_response(file_desc);
        }

        printf("Mensagem entregue com sucesso! Avançando sequência...\n");
        next_sequence(); 

        free(msg);
        free(buffer);
    }

    if(strcmp(mode, "server") == 0)
    {
        uint8_t last_processed_seq = 255;
        uint8_t current_received_seq = 0;
        printf("Iniciando o servidor!\n");

        while(1)
        {
            int type = listener_mode(file_desc, &current_received_seq);
            if(type != -1) {
                if(current_received_seq == last_processed_seq)
                {
                    printf("Mensagem duplicada (Seq %d)! Reenviando ACK...\n", current_received_seq);
                    send_ack(file_desc, ifindex, current_received_seq); 
                    continue;
                }

                printf("Nova mensagem (Seq %d)! Processando e enviando ACK...\n", current_received_seq);
                send_ack(file_desc, ifindex, current_received_seq); 
                last_processed_seq = current_received_seq;
            }
        }
    }

    return 0;
}