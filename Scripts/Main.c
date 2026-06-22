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

static void free_message_data(struct message *msg)
{
    if (msg != NULL && msg->data != NULL)
    {
        free(msg->data);
        msg->data = NULL;
    }
}

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
        printf("Uso: %s [server|player] [interface] [mapa.csv] [log_tty]\n", argv[0]);
        printf("Exemplo com log do servidor: %s server lo Assets/mapa.csv /dev/pts/4\n", argv[0]);
        return 1; 
    }

    char *mode = argv[1];
    char *interface = (argc > 2) ? argv[2] : "lo";
    const char *map_filename = (argc > 3) ? argv[3] : "Assets/mapa_padrao.csv";
    const char *server_log_tty = (argc > 4) ? argv[4] : NULL;
    struct message received_msg = {0};

    uint32_t ifindex = if_nametoindex(interface);
    int32_t file_desc = create_raw_socket(ifindex);
    
    if(strcmp(mode, "player") == 0)
    {
        printf("Iniciando o jogador! Aguardando mapa inicial...\n");
        struct message msg = {0};
        int result = -4;
        int raw_type;
        char command[20];
        int radius = 0;
        char* map_view = NULL;
        
        while(1)
        {
            //Espera o Mapa (Sequence atual)
            result = -4;

            while (result != TYPE_VISUAL){
                //fprintf(stderr,"waiting visual");
                raw_type = listener_mode(file_desc, &received_msg);
                result = handle_listen_result(file_desc, ifindex, raw_type, &received_msg, global_sequence.value);
                //fprintf(stderr, "resultado: %d\n",result);
                if(result == TYPE_JPG || result == TYPE_TXT || result == TYPE_MP4){
                    wait_file(file_desc, ifindex, result, (char*)received_msg.data);
                    free_message_data(&received_msg);
                } else if(result == TYPE_FINISH){
                    free_message_data(&received_msg);
                    free_message_data(&msg);
                    close(file_desc);
                    return 0;
                } else if(result != TYPE_VISUAL){
                    free_message_data(&received_msg);
                }
                //fprintf(stderr,"esperando visual type\n");
            }
            //fprintf(stderr, "visual recieved  ");
            //fprintf(stderr, "raio: %d\n" ,(int)received_msg.data);
            if (received_msg.data != NULL && received_msg.size >= sizeof(radius)) {
                memcpy(&radius, received_msg.data, sizeof(radius));
            }
            free_message_data(&received_msg);
            //fprintf(stderr,"raio: %d", radius);
            //fprintf(stderr,"esperando mapa\n");
            map_view = wait_map(file_desc,ifindex);
            //printf("Mapa recebido!\n");
            if (map_view != NULL) {
                print_game_screen(map_view, radius);
                free(map_view);
                map_view = NULL;
            }

            free_message_data(&msg);

            //Captura comando do usuário
            printf("Comando: ");
            while (scanf("%19s", command) == 1)
            {
                Moves move = stringToEnum(command);
                if (move == MOVE_UNKNOWN || move == ASK_JPG)
                {
                    printf("Comando invalido!\nOpcoes:\n 'w' - 'a' - 's' - 'd'\n");
                    printf("Comando: ");
                    continue;
                }

                result = -4;
                while (result != TYPE_ACK)
                {
                    switch(move)
                    {
                        case MOVE_UP:
                            send_up(file_desc, ifindex);
                            break;
                        case MOVE_DOWN:
                            send_down(file_desc, ifindex);
                            break;
                        case MOVE_LEFT:
                            send_left(file_desc, ifindex);
                            break;
                        case MOVE_RIGHT:
                            send_right(file_desc, ifindex);
                            break;
                        default:
                            break;
                    }
                    //printf("waiting M_ack\n");
                    raw_type = listener_mode(file_desc, &msg);
                    result = handle_listen_result(file_desc, ifindex, raw_type, &msg, global_sequence.value);
                    free_message_data(&msg);
                }
                break; // impede de ficar em loop esperando input
            }
        }
    }

    if(strcmp(mode, "server") == 0)
    {
        struct message received_msg = {0};
        int result = -4;
        int raw_type;
        int moved = 1;

        if (open_message_log_tty(server_log_tty) != 0)
        {
            close(file_desc);
            return 1;
        }

        //printf("Carregando o mapa!\n");
        GameState game = {0};
        init_game(&game);
        if (load_map_from_csv(&game, map_filename) != 0)
        {
            fprintf(stderr, "Erro ao carregar o mapa em %s\n", map_filename);
            close_message_log_tty();
            close(file_desc);
            return 1;
        }
        server_print_map(&game);
        printf("Iniciando o servidor!\n");

        int loop_count = 0;
        while(1)
        {   
            //printf("loop: %d\n", loop_count);
            loop_count++;
            //fprintf(stderr, "moved: %d\n", moved);
            if (moved){
                //fprintf(stderr,"SERVER GLOBAL SEQ:%d\n", global_sequence.value);
                //Envia o mapa e espera o ACK correspondente
                    send_map(file_desc, ifindex, &game);
                }
                moved = 0;
            //Esperando comando do player (ja com a nova sequencia)
            result = -4;
            //fprintf(stderr,"GLOBAL SEQ BEFORE MOVE: %d\n",global_sequence.value);
            while(result < 10 || result > 13){
                printf("waiting input\n");
                raw_type = listener_mode(file_desc, &received_msg);
                result = handle_listen_result(file_desc, ifindex, raw_type, &received_msg, global_sequence.value);
                free_message_data(&received_msg);
            }
                moved = 1;


            //fprintf(stderr,"GLOBAL SEQ AFTER MOVE: %d\n",global_sequence.value);
            //Aqui trataremos a logica do jogo (Sequencia ja avancou no final do handle_listen_result)
            //printf("Comando %d recebido!\n", result);
            int direction = -1;
            switch (result)
            {
                case TYPE_UP:
                    direction = 0;
                    break;
                case TYPE_DOWN:
                    direction = 1;
                    break;
                case TYPE_LEFT:
                    direction = 2;
                    break;
                case TYPE_RIGHT:
                    direction = 3;
                    break;
                default:
                    break;
            }

            if (direction != -1)
            {
                int status = handle_move(&game, (uint16_t)direction);
                char name[2];
                snprintf(name, sizeof(name), "%d", status);

                if(status == 1 || status == 2){
                    send_file(file_desc, ifindex, name, TYPE_TXT);
                } else if(status == 3 || status == 4){
                    send_file(file_desc, ifindex, name, TYPE_JPG);
                } else if(status == 5 || status == 6){
                    send_file(file_desc, ifindex, name, TYPE_MP4);
                } else if(status == 8){
                    send_file(file_desc, ifindex, "dead", TYPE_JPG);
                }
                if(game.pills_collected == 6){
                    send_file(file_desc, ifindex, "end", TYPE_JPG);
                    end_game(file_desc, ifindex);
                    close_message_log_tty();
                    close(file_desc);
                    return 0;
                }
                update_map(&game);
                server_print_map(&game);
            }
            free_message_data(&received_msg);
        }
    }
    return 0;
}
