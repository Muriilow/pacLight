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
        
        printf("Iniciando o servidor!\n");

        while(1)
        {
            // O servidor envia o mapa inicial ou atualizado
            send_map(file_desc, ifindex, global_sequence.value, &game);

            // Espera ACK do mapa OU um novo comando do jogador
            int type = listener_mode(file_desc, &received_msg);
            
            if (type == -1) {
                // Timeout no mapa: o loop volta e reenvia o mapa automaticamente
                continue;
            }

            if (type == TYPE_ACK) {
                // Mapa foi entregue! Agora esperamos o próximo comando do player.
                // Avançamos a sequência para o próximo pacote que formos enviar.
                next_sequence();
                continue;
            }

            // Se recebemos algo que não é ACK, tratamos como novo comando (se for seq nova)
            if (type >= 10 && type <= 13) { // Movimentos
                if(received_msg.sequence == last_processed_seq) {
                    send_ack(file_desc, ifindex, received_msg.sequence); 
                } else {
                    send_ack(file_desc, ifindex, received_msg.sequence); 
                    last_processed_seq = received_msg.sequence;
                    // TODO: aplicar_movimento(type, &game);
                }
            }

            if (received_msg.data) free(received_msg.data);
        }
    }

    return 0;
}
