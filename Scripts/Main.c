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
#include "Game.h"

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
        printf("Iniciando o jogador! Aguardando mapa inicial...\n");
        struct message initial_map_msg;
        
        // O jogador começa esperando o mapa do servidor
        int type = -1;
        while(1)
        {
            type = listener_mode(file_desc, &initial_map_msg);
            if(type != -1)
            {
                printf("recebido msg \n");
                break;
            }
        }
        printf("%d\n", type);
        if (type == TYPE_VISUAL) {
            printf("Mapa recebido! Iniciando interface...\n");
            if (initial_map_msg.data)
            {
                print_game_screen(initial_map_msg.data, 1);
                free(initial_map_msg.data);
            }
        }

        // Loop de jogo do Player (Movimentação)
        while(1) {
            // Futura implementação: ler teclado e enviar comando
            sleep(1); 
        }
    }

    if(strcmp(mode, "server") == 0)
    {
        uint8_t last_processed_seq = 255;
        struct message received_msg;
        
        printf("Carregando o mapa!\n");
        GameState game = {0};
        init_game(&game);
        if (load_map_from_csv(&game, "Assets/mapa_padrao.csv") != 0) {
            printf("Erro ao carregar mapa! Verifique Assets/mapa_padrao.csv\n");
            return 1;
        }
        
        printf("Iniciando o servidor! Enviando mapa inicial...\n");
        // O servidor toma a iniciativa e envia o mapa
        int response = -1;
        while(response != TYPE_ACK)
        {
            send_map(file_desc, ifindex, global_sequence.value, &game);
            response = wait_response(file_desc);
        }
        next_sequence();

        while(1)
        {
            int type = listener_mode(file_desc, &received_msg);
            if(type != -1) {
                if(received_msg.sequence == last_processed_seq)
                {
                    printf("Mensagem duplicada (Seq %d)! Reenviando ACK...\n", received_msg.sequence);
                    send_ack(file_desc, ifindex, received_msg.sequence); 
                    if (received_msg.data) free(received_msg.data);
                    continue;
                }

                printf("Comando recebido (Tipo %d)! Processando...\n", received_msg.type);
                send_ack(file_desc, ifindex, received_msg.sequence); 
                last_processed_seq = received_msg.sequence;

                // Após processar o movimento (futura implementação), envia o mapa atualizado
                send_map(file_desc, ifindex, global_sequence.value, &game);
                next_sequence();

                if (received_msg.data) free(received_msg.data);
            }
        }
    }

    return 0;
}
