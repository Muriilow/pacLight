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
        int result = -1;
        int raw_type;
        
        while(1)
        {
            //Espera o Mapa (Sequence atual)
            result = -1;
            while (result != TYPE_VISUAL) 
            {
                raw_type = listener_mode(file_desc, &msg);
                result = handle_listen_result(file_desc, ifindex, raw_type, &msg, global_sequence.value);
            }

            printf("Mapa recebido!\n");
            print_game_screen(msg.data, 1);

            if (msg.data)
            {
                free(msg.data); 
                msg.data = NULL;
            }

            //Envia Comando (Ex: TYPE_UP) e espera o ACK correspondente
            result = -1;
            //while (result != TYPE_ACK) {
                // Por enquanto enviaremos apenas o tipo (sem dados extras no comando)
            //}
        }
    }

    if(strcmp(mode, "server") == 0)
    {
        struct message received_msg;
        received_msg.data = NULL;
        int result = -1;
        int raw_type;

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
            //Envia o mapa e espera o ACK correspondente
            send_map(file_desc, ifindex, global_sequence.value, &game);
            result = -1;
            while(result != TYPE_ACK)
            {
                raw_type = listener_mode(file_desc, &received_msg);
                result = handle_listen_result(file_desc, ifindex, raw_type, &received_msg, global_sequence.value);
                
                if(received_msg.data)
                    continue;

                free(received_msg.data);
                received_msg.data = NULL;
            }

            //Esperando comando do player (ja com a nova sequencia)
            result = -1;
            while (result < 10 || result > 13) 
            {
                raw_type = listener_mode(file_desc, &received_msg);
                result = handle_listen_result(file_desc, ifindex, raw_type, &received_msg, global_sequence.value);
                
                if(received_msg.data)
                    continue;

                free(received_msg.data);
                received_msg.data = NULL;
            }

            //Aqui trataremos a logica do jogo (Sequencia ja avancou no final do handle_listen_result)
            printf("Comando %d recebido!\n", result);
        }
    }

    return 0;
}
