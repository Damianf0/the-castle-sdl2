#ifndef ENEMIES_PORT_H
#define ENEMIES_PORT_H
#include <stdint.h>

/* Enemigos faithful: comportamiento capturado del ROM real vía openMSX.
 *  - 0x39/0x3A/0x3B: patrullero VERTICAL (col fija, rebota arriba/abajo).
 *  - 0x37/0x38: wall-follower (recorre plataformas/paredes).
 *  - 0x36: estacionario (solo anima).
 * Mueven 1 tile cada 2 frames. flags: bit0=activo, bit1=dir vertical (1=abajo),
 * bit2=horizontal activo, bit3=dir horizontal. */

#define EN_MAX 8

typedef struct {
    uint8_t  active;
    uint8_t  type;
    int      row, col;      /* posición BAT: row=byte2=X(columna), col=byte3=Y(fila) */
    int      sr, sc;        /* spawn (path[0]): sr=byte2_0(X), sc=byte3_0(Y) */
    uint8_t  flags;
    int      dir;
    int      anim;
    uint8_t  gfx[8][16];    /* celdas del gráfico: 8 bytes patrón + 8 color (VRAM) */
    int      gw, gox;       /* ancho del gráfico (tiles) y offset de columna vs spawn */
    int      face, face0;   /* dirección actual y nativa (+1 der, -1 izq, 0 sin) -> espejo */
} PortEnemy;

extern PortEnemy g_pen[EN_MAX];
extern int       g_pen_n;
extern int       g_room_air;   /* tile de fondo del cuarto (blanqueo de horneados) */

void enemies_room_init(unsigned char room);
void enemies_step(void);
int  enemy_spawn_cell(int srow, int scol);   /* 1 si la celda es spawn horneado de enemigo */

#endif
