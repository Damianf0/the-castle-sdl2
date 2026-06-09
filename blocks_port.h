#ifndef BLOCKS_PORT_H
#define BLOCKS_PORT_H
#include <stdint.h>

/* Bloques empujables (tabla COLL: ollas/ladrillos). Son sólidos; el jugador los
 * empuja horizontalmente si hay lugar atrás; caen por gravedad si no tienen
 * apoyo. Gráfico real 2x2 extraído de la VRAM. */
#define BLOCK_MAX 8

typedef struct {
    uint8_t  active;
    int      scol, srow;     /* posición actual (tiles pantalla) */
    int      sc0, sr0;       /* posición de spawn (para blanquear el horneado) */
    uint16_t gfx[4];         /* 2x2 tiles del bloque (dedup idx de RT_TILES) */
} Block;

extern Block g_block[BLOCK_MAX];
extern int   g_block_n;

void blocks_room_init(unsigned char room);
int  block_solid(int srow, int scol);   /* 1 si una celda pertenece a un bloque */
/* Intenta empujar un bloque en 'dir' (-1 izq / +1 der) desde la fila del jugador.
 * Devuelve 1 si empujó (el jugador puede avanzar). */
int  blocks_push(int player_px, int player_py, int ph, int dir);
void blocks_step(void);   /* gravedad */
int  block_spawn_cell(int srow, int scol);   /* 1 si la celda es spawn horneado de bloque */

#endif
