#include "Game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void init_game(GameState *game) {
    game->pacman_x = 0;
    game->pacman_y = 0;
    game->visibility_radius = INITIAL_VISIBILITY;
    game->move_count = 0;
    game->pills_collected = 0;
    memset(game->grid, '0', sizeof(game->grid));
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
    printf("\033[H\033[J"); // Limpa a tela e move o cursor para o topo
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
                    case '0': printf(". "); break;                  // Ponto (2 chars)
                    case '1': case '2': case '3': 
                    case '4': case '5': case '6': printf("\033[1;32m. \033[0m"); break; // Verde (2 chars)
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
