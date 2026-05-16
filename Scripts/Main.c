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

typedef enum { MOVE_UP, MOVE_DOWN, MOVE_LEFT, MOVE_RIGHT , ASK_JPG, MOVE_UNKNOWN} Moves;

Moves stringToEnum(char *str) {
    if (strcasecmp(str, "w") == 0) return MOVE_UP;
    if (strcasecmp(str, "s") == 0) return MOVE_DOWN;
    if (strcasecmp(str, "a") == 0) return MOVE_LEFT;
    if (strcasecmp(str, "d") == 0) return MOVE_RIGHT;
    if (strcasecmp(str, "k") == 0) return ASK_JPG;
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
    struct message received_msg;
        received_msg.data = NULL;

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
        int* radius = malloc(sizeof(int));
        char* map_view;
        
        while(1)
        {
            //Espera o Mapa (Sequence atual)
            result = -1;

            while (result != TYPE_VISUAL){
                raw_type = listener_mode(file_desc, &received_msg);
                result = handle_listen_result(file_desc, ifindex, raw_type, &received_msg, global_sequence.value);
                fprintf(stderr, "resultado: %d\n",result);
                if(result == TYPE_JPG || result == TYPE_TXT || result == TYPE_MP4){
                    wait_big(file_desc, ifindex);
                }
                fprintf(stderr,"esperando visual type\n");
            }
            fprintf(stderr, "visual recieved\n");
            memcpy(&radius, received_msg.data, sizeof(int));
            map_view = wait_map(file_desc,ifindex);
            printf("Mapa recebido!\n");
            fprintf(stderr, "INFO: %c\n",map_view[1]);
            print_game_screen(map_view, 1);

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
                    //Envia Comando (TYPE_UP) e espera a nova visualização
                        while (result != TYPE_ACK) 
                        {
                            send_up(file_desc, ifindex);
                            printf("waiting M_ack\n");
                            raw_type = listener_mode(file_desc, &msg);
                            result = handle_listen_result(file_desc, ifindex, raw_type, &msg, global_sequence.value);
                        }
                        break;
                    case MOVE_DOWN:
                        while (result != TYPE_ACK) 
                        {
                            send_down(file_desc, ifindex);
                            printf("waiting M_ack\n");
                            raw_type = listener_mode(file_desc, &msg);
                            result = handle_listen_result(file_desc, ifindex, raw_type, &msg, global_sequence.value);
                        }
                        break;
                    case MOVE_LEFT:
                        while (result != TYPE_ACK)
                        {
                            send_left(file_desc, ifindex);
                            printf("waiting M_ack\n");
                            raw_type = listener_mode(file_desc, &msg);
                            result = handle_listen_result(file_desc, ifindex, raw_type, &msg, global_sequence.value);
                        }
                        break;
                    case MOVE_RIGHT:
                        while (result != TYPE_ACK) 
                        {
                            send_right(file_desc, ifindex);
                            printf("waiting M_ack\n");
                            raw_type = listener_mode(file_desc, &msg);
                            result = handle_listen_result(file_desc, ifindex, raw_type, &msg, global_sequence.value);
                        }
                        break;
                    default:
                        printf("Comando invalido!\nOpcoes:\n 'w' - 'a' - 's' - 'd' - 'k'\n");
                        printf("Comando: ");
                        continue;
                }                
                fprintf(stderr, "while scan\n");
                break;
            }
        }
    }

    if(strcmp(mode, "server") == 0)
    {
        struct message received_msg;
        received_msg.data = NULL;
        int result = -1;
        int raw_type;
        int moved = 1;

        printf("Carregando o mapa!\n");
        GameState game = {0};
        init_game(&game);
        if (load_map_from_csv(&game, "Assets/mapa_padrao.csv") != 0){}
        server_print_map(&game);
        printf("Iniciando o servidor!\n");

        int loop_count = 0;
        while(1)
        {   
            printf("loop: %d\n", loop_count);
            loop_count++;
            fprintf(stderr, "moved: %d\n", moved);
            if (moved){
                //Envia o mapa e espera o ACK correspondente
                    send_map(file_desc, ifindex, &game);
                }
                fprintf(stderr,"ACK RECEBIDO\n");
                moved = 0;
            //Esperando comando do player (ja com a nova sequencia)
            result = -1;
            while(result < 10 || result > 13){
                printf("waiting input\n");
                raw_type = listener_mode(file_desc, &received_msg);
                result = handle_listen_result(file_desc, ifindex, raw_type, &received_msg, global_sequence.value);
                
                if(received_msg.data){
                    continue;
                }
                free(received_msg.data);
                received_msg.data = NULL;
            }
                moved = 1;

            send_ack(file_desc,ifindex,global_sequence.value);
            //Aqui trataremos a logica do jogo (Sequencia ja avancou no final do handle_listen_result)
            printf("Comando %d recebido!\n", result);
            int status;
            char name[2];
            switch (result)
            {
                case TYPE_UP:
                    status = handle_move(&game, 0);
                    snprintf(name, sizeof(name), "%d", status);
                    if(status == 1 || status == 2){
                        send_big(file_desc, ifindex,  name, TYPE_TXT);
                    } else if(status == 3 || status == 4){
                        send_big(file_desc, ifindex,  name, TYPE_JPG);
                    } else if(status == 5 || status == 6){
                        send_big(file_desc, ifindex,  name, TYPE_MP4);
                    }
                    update_map(&game);
                    server_print_map(&game);
                    break;

                case TYPE_DOWN:
                    status = handle_move(&game, 1);
                    snprintf(name, sizeof(name), "%d", status);
                    if(status == 1 || status == 2){
                        send_big(file_desc, ifindex,  name, TYPE_TXT);
                    } else if(status == 3 || status == 4){
                        send_big(file_desc, ifindex,  name, TYPE_JPG);
                    } else if(status == 5 || status == 6){
                        send_big(file_desc, ifindex,  name, TYPE_MP4);
                    }
                    update_map(&game);
                    server_print_map(&game);
                    break;

                case TYPE_LEFT:
                    status = handle_move(&game, 2);
                    snprintf(name, sizeof(name), "%d", status);
                    if(status == 1 || status == 2){
                        send_big(file_desc, ifindex,  name, TYPE_TXT);
                    } else if(status == 3 || status == 4){
                        send_big(file_desc, ifindex,  name, TYPE_JPG);
                    } else if(status == 5 || status == 6){
                        send_big(file_desc, ifindex,  name, TYPE_MP4);
                    }
                    update_map(&game);
                    server_print_map(&game);
                    break;

                case TYPE_RIGHT:
                    status = handle_move(&game, 3);
                    snprintf(name, sizeof(name), "%d", status);
                    if(status == 1 || status == 2){
                        send_big(file_desc, ifindex, name, TYPE_TXT);
                    } else if(status == 3 || status == 4){
                        send_big(file_desc, ifindex,  name, TYPE_JPG);
                    } else if(status == 5 || status == 6){
                        send_big(file_desc, ifindex,  name, TYPE_MP4);
                    }
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
