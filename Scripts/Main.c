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
        struct message msg;
        msg.data = NULL;
        
        while(1)
        {
            int raw_type = listener_mode(file_desc, &msg);
            int result = handle_listen_result(file_desc, ifindex, raw_type, &msg, msg.sequence);
            
            if(result < 0) {
                if (msg.data) {
                    free(msg.data); 
                    msg.data = NULL;
                }
                continue;
            }

            if (result == TYPE_VISUAL) {
                printf("Mapa recebido! Iniciando interface...\n");
                if (msg.data)
                {
                    print_game_screen(msg.data, 1);
                    free(msg.data);
                    msg.data = NULL;
                }
            }

            //Implementar logica de enviar comando e tals
        }
    }

    if(strcmp(mode, "server") == 0)
    {
        uint8_t last_processed_seq = 255;
        struct message received_msg;
        received_msg.data = NULL;
        
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

            // Espera ACK do mapa
            int raw_type = listener_mode(file_desc, &received_msg);
            int result = handle_listen_result(file_desc, ifindex, raw_type, &received_msg, global_sequence.value);
            
            if (received_msg.data) { 
                free(received_msg.data);
                received_msg.data = NULL; 
            }

            if (result < 0) 
                continue;

            if (result == TYPE_ACK)
                next_sequence();

            // Espera novo comando do jogador
            raw_type = listener_mode(file_desc, &received_msg);
            result = handle_listen_result(file_desc, ifindex, raw_type, &received_msg, global_sequence.value);

            if (result >= 10 && result <= 13) {
                if(received_msg.sequence != last_processed_seq) {
                    last_processed_seq = received_msg.sequence;
                    // TODO: aplicar_movimento(result, &game);
                    next_sequence(); 
                }
            }

            if (received_msg.data) {
                free(received_msg.data);
                received_msg.data = NULL;
            }
        }
    }

    return 0;
}
