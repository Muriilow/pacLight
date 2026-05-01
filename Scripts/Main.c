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

typedef enum { MOVE_UP, MOVE_DOWN, MOVE_LEFT, MOVE_RIGHT , MOVE_UNKNOWN} Moves;

Moves stringToEnum(char *str) {
    if (strcmp(str, "w") == 0) return MOVE_UP;
    if (strcmp(str, "s") == 0) return MOVE_DOWN;
    if (strcmp(str, "a") == 0) return MOVE_LEFT;
    if (strcmp(str, "d") == 0) return MOVE_RIGHT;
    return MOVE_UNKNOWN;
}

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
        char command[20];

        while (result != TYPE_VISUAL) 
        {
            printf("waiting map\n");
            raw_type = listener_mode(file_desc, &msg);
            result = handle_listen_result(file_desc, ifindex, raw_type, &msg, global_sequence.value);
        }
        printf("Mapa recebido!\n");
        print_game_screen(msg.data, 1);
        
        while(1)
        {
            //Espera o Mapa (Sequence atual)
            result = -1;

            printf("Mapa recebido!\n");
            print_game_screen(msg.data, 1);

            if (msg.data)
            {
                free(msg.data); 
                msg.data = NULL;
            }

            //Captura comando do usuário
            printf("Comando: ");
            while (scanf("%19s", command) == 1)
            {   
                result = -1;
                switch(stringToEnum(command))
                {
                    case MOVE_UP:
                        while (result != TYPE_VISUAL) 
                        {
                            send_up(file_desc, ifindex, global_sequence.value);
                            printf("waiting map\n");
                            raw_type = listener_mode(file_desc, &msg);
                            result = handle_listen_result(file_desc, ifindex, raw_type, &msg, global_sequence.value);
                        }
                        break;
                    case MOVE_DOWN:
                        //Envia Comando (TYPE_DOWN) e espera o ACK correspondente
                        while (result != TYPE_VISUAL) 
                        {
                            send_down(file_desc, ifindex, global_sequence.value);
                            printf("waiting map\n");
                            raw_type = listener_mode(file_desc, &msg);
                            result = handle_listen_result(file_desc, ifindex, raw_type, &msg, global_sequence.value);
                        }
                        break;
                    case MOVE_LEFT:
                        while (result != TYPE_VISUAL)
                        {
                            send_left(file_desc, ifindex, global_sequence.value);
                            printf("waiting map\n");
                            raw_type = listener_mode(file_desc, &msg);
                            result = handle_listen_result(file_desc, ifindex, raw_type, &msg, global_sequence.value);
                        }
                        break;
                    case MOVE_RIGHT:
                        while (result != TYPE_VISUAL) 
                        {
                            send_right(file_desc, ifindex, global_sequence.value);
                            printf("waiting map\n");
                            raw_type = listener_mode(file_desc, &msg);
                            result = handle_listen_result(file_desc, ifindex, raw_type, &msg, global_sequence.value);
                        }
                        break;
                    default:
                        printf("Comando invalido!\nOpcoes:\n 'w' - 'a' - 's' - 'd'\n");
                        printf("Comando: ");
                        continue;
                }
                break;
                printf("while scan\n");
            }
            printf("while fora\n");
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
        }
        server_print_map(&game);
        printf("Iniciando o servidor!\n");

        while(1)
        {
            //Envia o mapa e espera o ACK correspondente
            result = -1;
            while(result != TYPE_ACK)
            {

                send_map(file_desc, ifindex, global_sequence.value, &game);
                printf("waiting ack\n");
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
                printf("waiting input\n");
                raw_type = listener_mode(file_desc, &received_msg);
                result = handle_listen_result(file_desc, ifindex, raw_type, &received_msg, global_sequence.value);
                if(received_msg.data){
                    continue;
                }
                free(received_msg.data);
                received_msg.data = NULL;
            }

            //Aqui trataremos a logica do jogo (Sequencia ja avancou no final do handle_listen_result)
            printf("Comando %d recebido!\n", result);
            switch (result)
            {
                case TYPE_UP:
                    handle_move(&game, 0);
                    update_map(&game);
                    server_print_map(&game);
                    break;
                case TYPE_DOWN:
                    handle_move(&game, 1);
                    update_map(&game);
                    server_print_map(&game);
                    break;
                case TYPE_LEFT:
                    handle_move(&game, 2);
                    update_map(&game);
                    server_print_map(&game);
                    break;
                case TYPE_RIGHT:
                    printf("==direita==\n");
                    handle_move(&game, 3);
                    update_map(&game);
                    server_print_map(&game);
                    break;
                default:
                    break;
            }
            if(received_msg.data)
            {
                free(received_msg.data);
                received_msg.data = NULL;
            }            
            //break; //para nao dar loop infinito, depois retirar
        }
    }

    return 0;
}
