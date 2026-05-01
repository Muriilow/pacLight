#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define MAP_SIZE 40
#define INITIAL_VISIBILITY 1
#define VISIBILITY_INCREMENT_INTERVAL 5

typedef struct {
    char grid[MAP_SIZE][MAP_SIZE];
    int pacman_x, pacman_y;
    int visibility_radius;
    int move_count;
    int pills_collected;
} GameState;

void init_game(GameState *game);
int load_map_from_csv(GameState *game, const char *filename);
void update_visibility(GameState *game);
void get_visible_map(GameState *game, char *buffer, int *size);
void print_game_screen(const char *visible_grid, int radius);
int handle_move(GameState *game, uint16_t direction);
void update_map(GameState *game);
void server_print_map(GameState *game);
#endif
