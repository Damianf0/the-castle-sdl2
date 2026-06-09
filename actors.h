#pragma once
#ifndef CASTLE_ACTORS_H
#define CASTLE_ACTORS_H
#include <stdint.h>

#define MAX_ENEMIES 4

/* estado leído por el renderer (hal_sdl2.c) */
extern int g_player_px, g_player_py, g_player_face, g_player_anim;
extern int g_player_moving;   /* 1 si el jugador se movió horizontal este frame */
extern int g_player_air;      /* 1 si el jugador está en el aire (saltando/cayendo) */
extern int g_player_lives;    /* vidas restantes */
extern int g_player_invuln;   /* >0 = invulnerable/parpadeando tras un golpe */
extern int g_actors_on;   /* definido en hal_sdl2.c, lo activa el viewer */
extern int g_enemy_n;
extern int g_enemy_px[MAX_ENEMIES], g_enemy_py[MAX_ENEMIES], g_enemy_dir[MAX_ENEMIES];

/* Inicializa jugador + enemigos para una sala. entry_edge: 0/3/7 = por dónde entró. */
void actors_init_room(unsigned char room, int entry_edge);

/* solidez del tile base de pantalla (sr,sc) — usado por blocks_port */
int actors_tile_solid(int srow, int scol);

/* Avanza un frame con el input (flags left/right/up).
 * Retorna borde de salida: 0=ninguno 1=arriba 3=der 5=abajo 7=izq. */
int actors_update(int left, int right, int up);

#endif
