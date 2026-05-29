#include "Game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void init_ghost(Ghost* ghost, int x, int y, int direction){
    ghost->direction = direction;
    ghost->x = x;
    ghost->y = y;
}
void init_game(GameState *game) {
    game->pacman_x = 0;
    game->pacman_y = 0;
    game->visibility_radius = INITIAL_VISIBILITY;
    game->move_count = 0;
    game->pills_collected = 0;
    memset(game->grid, '0', sizeof(game->grid));
    init_ghost(&game->red, 1, 1, rand()%4);
    init_ghost(&game->green, 1, 38, rand()%4);
    game->last_green_turn = 1;
    init_ghost(&game->blue, 38, 1, rand()%4);
    init_ghost(&game->yellow, 38, 38, rand()%4);
    }
int load_map_from_csv(GameState *game, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        return -1;
    }

    char line[1024];
    int row = 0;
    while (fgets(line, sizeof(line), file) && row < MAP_SIZE) {
        char *token = strtok(line, ";");
        int col = 0;
        while (token && col < MAP_SIZE) {
            game->grid[row][col] = token[0];
            if (token[0] == 'P') {
                game->pacman_x = col;
                game->pacman_y = row;
            }
            token = strtok(NULL, ";");
            col++;
        }
        row++;
    }
    game->grid[game->red.x][game->red.y] = 'R';
    game->grid[game->green.x][game->green.y] = 'G';
    game->grid[game->blue.x][game->blue.y] = 'B';
    game->grid[game->yellow.x][game->yellow.y] = 'Y';
    fclose(file);
    return 0;
}

void update_visibility(GameState *game) {
    game->move_count++;
    if (game->move_count % VISIBILITY_INCREMENT_INTERVAL == 0) {
        game->visibility_radius++;
    }
}

void get_visible_map(GameState *game, char *buffer, int *size) {
    int count = 0;
    int r = game->visibility_radius;
    
    for (int i = game->pacman_y - r; i <= game->pacman_y + r; i++) {
        for (int j = game->pacman_x - r; j <= game->pacman_x + r; j++) {
            if (i >= 0 && i < MAP_SIZE && j >= 0 && j < MAP_SIZE) {
                buffer[count++] = game->grid[i][j];
            } else {
                buffer[count++] = 'X'; 
            }
        }
    }
    *size = count;
}

void print_game_screen(const char *visible_grid, int radius) {
    //printf("\033[H\033[J"); // Limpa a tela e move o cursor para o topo
    printf("PacLight - Pílulas: ?/6 | Visão: %d\n\n", radius);
    int k = 0;
    // Itera no bounding box, mas desenha apenas o que está no raio de Manhattan
    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            if (abs(i) + abs(j) <= radius) {
                char cell = visible_grid[k++];
                switch (cell) {
                    case 'P': printf("\033[1;33mP \033[0m"); break; // Amarelo (2 chars)
                    case 'X': printf("\033[1;34m# \033[0m"); break; // Azul (2 chars)
                    case 'R': printf("\033[1;91mR \033[0m"); break; // Vermelho brilhante(2 chars)
                    case 'G': printf("\033[1;92mG \033[0m"); break; // Verde brilhante (2 chars)
                    case 'B': printf("\033[1;94mB \033[0m"); break; // Azul brilhante(2 chars)
                    case 'Y': printf("\033[1;93mY \033[0m"); break; // Amarelo brilhante (2 chars)
                    case '0': printf(". "); break;                  // Ponto (2 chars)
                    case '1': case '2': case '3': 
                    case '4': case '5': case '6': printf("\033[1;32m%c \033[0m", cell); break; 
                    default: printf("%c ", cell); break;
                }
            } else {
                printf("  "); // Dois espaços vazios fora do diamante
            }
        }
        printf("\n");
    }
    printf("\nUse W/A/S/D para mover.\n");
}

int handle_move(GameState *game, uint16_t direction)
{
    int next_x = game->pacman_x;
    int next_y = game->pacman_y;

    switch (direction)
    {
        case 0:
            next_x--;
            break;
        case 1:
            next_x++;
            break;
        case 2:
            next_y--;
            break;
        case 3:
            next_y++;
            break;
        default:
            game->visibility_radius = INITIAL_VISIBILITY+game->move_count/VISIBILITY_INCREMENT_INTERVAL;
            return 0;
    }

    char colision = game->grid[next_x][next_y];
    switch(colision)
    {
        case ('X'):
            game->move_count --;
            break;
        case('1'):
        case('2'):
        case('3'):
        case('4'):
        case('5'):
        case('6'):
            game->grid[game->pacman_x][game->pacman_y] = '.';
            game->grid[next_x][next_y] = 'P';
            game->pacman_x = next_x;
            game->pacman_y = next_y;
            return (colision - '0');
        case('R'):
        case('G'):
        case('B'):
        case('Y'):
            return 8;
        default:
            if (direction == 3) {
                printf("y+1 = %c\n", game->grid[next_x][next_y]);
            }
            game->grid[game->pacman_x][game->pacman_y] = '.';
            game->grid[next_x][next_y] = 'P';
            game->pacman_x = next_x;
            game->pacman_y = next_y;
            break;
    }

    game->visibility_radius = INITIAL_VISIBILITY+game->move_count/VISIBILITY_INCREMENT_INTERVAL;
    return 0;
}

void update_map(GameState *game)
{  
    moveghosts(game);
    game->move_count ++;
    return;
}
void moveghosts(GameState* game){
    //0 = up, 1 = right, 2 down, 3 left
    switch (game->red.direction){
        case 0:
            fprintf(stderr,"RED UP \n");
            if (game->grid[game->red.x-1][game->red.y] == '0'){
                game->grid[game->red.x-1][game->red.y] = 'R';
                game->grid[game->red.x][game->red.y] = '0';
                game->red.x--;
            } else {
                game->red.direction = 3;
            }
        break;
        case 1:
            fprintf(stderr,"RED RIG\n");
            if (game->grid[game->red.x][game->red.y+1] == '0'){
                game->grid[game->red.x][game->red.y+1] = 'R';
                game->grid[game->red.x][game->red.y] = '0';
                game->red.y++;
            } else {
                    game->red.direction--;
            }
        break;
        case 2:
            fprintf(stderr,"RED DOW\n");
            if (game->grid[game->red.x+1][game->red.y] == '0'){
                game->grid[game->red.x+1][game->red.y] = 'R';
                game->grid[game->red.x][game->red.y] = '0';
                game->red.x++;
            } else {
                    game->red.direction--;
            }
        break;
        case 3:
            fprintf(stderr,"RED LEF\n");
            if (game->grid[game->red.x][game->red.y-1] == '0'){
                game->grid[game->red.x][game->red.y-1] = 'R';
                game->grid[game->red.x][game->red.y] = '0';
                game->red.y--;
            } else {
                    game->red.direction--;
            }
        break;
    }
    switch (game->blue.direction){
        case 0:
            fprintf(stderr,"BLUE UP \n");
            if (game->grid[game->blue.x-1][game->blue.y] == '0'){
                game->grid[game->blue.x-1][game->blue.y] = 'B';
                game->grid[game->blue.x][game->blue.y] = '0';
                game->blue.x--;
            } else {
                game->blue.direction++;
            }
        break;
        case 1:
            fprintf(stderr,"BLUE RIG\n");
            if (game->grid[game->blue.x][game->blue.y+1] == '0'){
                game->grid[game->blue.x][game->blue.y+1] = 'B';
                game->grid[game->blue.x][game->blue.y] = '0';
                game->blue.y++;
            } else {
                    game->blue.direction++;
            }
        break;
        case 2:
            fprintf(stderr,"BLUE DOW\n");
            if (game->grid[game->blue.x+1][game->blue.y] == '0'){
                game->grid[game->blue.x+1][game->blue.y] = 'B';
                game->grid[game->blue.x][game->blue.y] = '0';
                game->blue.x++;
            } else {
                    game->blue.direction++;
            }
        break;
        case 3:
            fprintf(stderr,"BLUE LEF\n");
            if (game->grid[game->blue.x][game->blue.y-1] == '0'){
                game->grid[game->blue.x][game->blue.y-1] = 'B';
                game->grid[game->blue.x][game->blue.y] = '0';
                game->blue.y--;
            } else {
                    game->blue.direction = 0;
            }
        break;
    }
    switch (rand()%4){
        case 0:
            fprintf(stderr,"YELLOW UP \n");
            if (game->grid[game->yellow.x-1][game->yellow.y] == '0'){
                game->grid[game->yellow.x-1][game->yellow.y] = 'Y';
                game->grid[game->yellow.x][game->yellow.y] = '0';
                game->yellow.x--;
            }
        break;
        case 1:
            fprintf(stderr,"YELLOW RIG\n");
            if (game->grid[game->yellow.x][game->yellow.y+1] == '0'){
                game->grid[game->yellow.x][game->yellow.y+1] = 'Y';
                game->grid[game->yellow.x][game->yellow.y] = '0';
                game->yellow.y++;
            }
        break;
        case 2:
            fprintf(stderr,"YELLOW DOW\n");
            if (game->grid[game->yellow.x+1][game->yellow.y] == '0'){
                game->grid[game->yellow.x+1][game->yellow.y] = 'Y';
                game->grid[game->yellow.x][game->yellow.y] = '0';
                game->yellow.x++;
            }
        break;
        case 3:
            fprintf(stderr,"YELLOW LEF\n");
            if (game->grid[game->yellow.x][game->yellow.y-1] == '0'){
                game->grid[game->yellow.x][game->yellow.y-1] = 'Y';
                game->grid[game->yellow.x][game->yellow.y] = '0';
                game->yellow.y--;
            }
        break;
    }
    switch (game->green.direction){
        case 0:
            fprintf(stderr,"GREEN UP \n");
            if (game->grid[game->green.x-1][game->green.y] == '0'){
                game->grid[game->green.x-1][game->green.y] = 'G';
                game->grid[game->green.x][game->green.y] = '0';
                game->green.x--;
            } else {
                game->last_green_turn *= -1;
                game->green.direction += game->last_green_turn;
            }
        break;
        case 1:
            fprintf(stderr,"GREEN RIG\n");
            if (game->grid[game->green.x][game->green.y+1] == '0'){
                game->grid[game->green.x][game->green.y+1] = 'G';
                game->grid[game->green.x][game->green.y] = '0';
                game->green.y++;
            } else {
                game->last_green_turn *= -1;
                game->green.direction += game->last_green_turn;
            }
        break;
        case 2:
            fprintf(stderr,"GREEN DOW\n");
            if (game->grid[game->green.x+1][game->green.y] == '0'){
                game->grid[game->green.x+1][game->green.y] = 'G';
                game->grid[game->green.x][game->green.y] = '0';
                game->green.x++;
            } else {
                game->last_green_turn *= -1;
                game->green.direction += game->last_green_turn;
            }
        break;
        case 3:
            fprintf(stderr,"GREEN LEF\n");
            if (game->grid[game->green.x][game->green.y-1] == '0'){
                game->grid[game->green.x][game->green.y-1] = 'G';
                game->grid[game->green.x][game->green.y] = '0';
                game->green.y--;
            } else {
                game->last_green_turn *= -1;
                game->green.direction += game->last_green_turn;
            }
        break;
    }
}
void server_print_map(GameState *game){
    //printf("\033[H\033[J"); // Limpa a tela e move o cursor para o topo
    // Itera no bounding box, mas desenha apenas o que está no raio de Manhattan
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            char cell = game->grid[i][j];
            switch (cell) {
                case 'P': printf("\033[1;33mP \033[0m"); break; // Amarelo (2 chars)
                    case 'X': printf("\033[1;34m# \033[0m"); break; // Azul (2 chars)
                    case 'R': printf("\033[1;91mR \033[0m"); break; // Vermelho brilhante(2 chars)
                    case 'G': printf("\033[1;92mG \033[0m"); break; // Verde brilhante (2 chars)
                    case 'B': printf("\033[1;94mB \033[0m"); break; // Azul brilhante(2 chars)
                    case 'Y': printf("\033[1;93mY \033[0m"); break; // Amarelo brilhante (2 chars)
                    case '0': printf(". "); break;                  // Ponto (2 chars)                  // Ponto (2 chars)
                case '1': case '2': case '3': 
                case '4': case '5': case '6': printf("\033[1;32m%c \033[0m", cell); break; // Verde (2 chars)
                default: printf("%c ", cell); break;
            }
        }
            printf("\n");
    }
    printf("Move counter: %d\n",game->move_count);
}
