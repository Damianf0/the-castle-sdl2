/*
 * THE CASTLE — Enemigos faithful por PATH-REPLAY.
 *
 * En vez de reimplementar la lógica de colisión del ROM (que usa un tilemap
 * lógico en RAM y reglas por tipo difíciles de replicar 1:1), se CAPTURÓ la
 * trayectoria exacta de cada enemigo corriendo la IA real del ROM en openMSX:
 * un stepper aislado llama sub_438D (el dispatcher de enemigos) frame a frame y
 * vuelca la tabla BAT (0xE416). Ver trace_step.tcl + gen de enemies_paths.c.
 *
 * El resultado es 100% fiel por construcción: cada enemigo reproduce su ciclo
 * real (patrulla vertical, wall-follower, o estacionario), a 1 tile / 2 frames,
 * que es la velocidad real verificada. Coords = (row,col) de la tabla BAT.
 */
#include <stdlib.h>
#include "enemies_port.h"
#include "enemies_paths.h"
#include "map_real.h"

PortEnemy g_pen[EN_MAX];
int       g_pen_n;
int       g_room_air;        /* tile de fondo del cuarto (para blanquear horneados) */

static const PathEnemy *s_pe[EN_MAX];   /* def de path por enemigo */
static int s_idx[EN_MAX];               /* índice actual en el ciclo */
static int s_tick;
static int s_div = 0;                   /* frames por movimiento de enemigo */

/* offset de columna del gráfico del enemigo vs su byte2 (X): el horneado en la
 * VRAM está 2 tiles a la derecha del valor lógico (verificado en sala 0x70). */
#define EN_COL_OFS 2

void enemies_room_init(unsigned char room)
{
    int ry = room >> 4, rx = room & 0x0F;
    g_pen_n = 0; s_tick = 0;
    if (rx > 9 || ry > 9) return;
    int idx = ry * 10 + rx;
    int n = PATH_ROOM_CNT[idx];
    int off = PATH_ROOM_OFF[idx];

    /* tile de fondo del cuarto = el más frecuente (para blanquear horneados) */
    {
        static int freq[RT_COUNT];
        for (int t = 0; t < RT_COUNT; t++) freq[t] = 0;
        for (int r = 0; r < RT_ROWS; r++)
            for (int c = 0; c < RT_COLS; c++) freq[ROOM_NT[idx][r][c]]++;
        int best = -1; g_room_air = 0;
        for (int t = 0; t < RT_COUNT; t++) if (freq[t] > best) { best = freq[t]; g_room_air = t; }
    }
    for (int i = 0; i < n && g_pen_n < EN_MAX; i++) {
        const PathEnemy *pe = &PATH_ENEMIES[off + i];
        PortEnemy *p = &g_pen[g_pen_n];
        p->active = 1;
        p->type   = pe->type;
        p->sr = pe->sr; p->sc = pe->sc;
        p->row = PATH_POS[pe->poff][0];
        p->col = PATH_POS[pe->poff][1];
        p->flags = 0; p->dir = 0; p->anim = 0;
        /* Gráfico REAL del enemigo, horneado en map_real en screen tile
         * (sc+4 fila, sr+2 columna), tamaño 2x2 (verificado vs VRAM real, sala
         * 0x70). El +2 de columna evita agarrar objetos del fondo vecinos. */
        int base_r = p->sc + 4, base_c = p->sr + EN_COL_OFS;
        p->gw = 2; p->gox = EN_COL_OFS;
        for (int dr = 0; dr < 2; dr++)
            for (int dc = 0; dc < 2; dc++) {
                int rr = base_r + dr, cc = base_c + dc;
                p->gfx[dr * 4 + dc] = (rr >= 0 && rr < RT_ROWS && cc >= 0 && cc < RT_COLS)
                                    ? ROOM_NT[idx][rr][cc] : (uint16_t)g_room_air;
            }
        /* facing nativo = dirección del primer movimiento horizontal del ciclo */
        p->face0 = 0;
        for (int k = 1; k < pe->plen; k++) {
            int dx = PATH_POS[pe->poff + k][0] - PATH_POS[pe->poff][0];
            if (dx) { p->face0 = (dx > 0) ? 1 : -1; break; }
        }
        p->face = p->face0;
        s_pe[g_pen_n] = pe;
        s_idx[g_pen_n] = 0;
        g_pen_n++;
    }
}

void enemies_step(void)
{
    /* Cadencia proporcional a la velocidad del jugador (CASTLE_SPEED): el ROM
     * mueve al jugador 4px/f y al enemigo 1 tile/2f (cruzan un tile en 2f, mismo
     * ritmo). Si el jugador va a S px/f, un tile son 8/S frames; igualamos el
     * enemigo a eso para que la proporción sea fiel. */
    if (s_div == 0) {
        const char *s = getenv("CASTLE_SPEED");
        int sp = s ? atoi(s) : 3;
        if (sp < 1) sp = 1;
        s_div = 8 / sp; if (s_div < 1) s_div = 1;
    }
    static int acc = 0;
    if (++acc < s_div) return;
    acc = 0;
    for (int i = 0; i < g_pen_n; i++) {
        PortEnemy *p = &g_pen[i];
        if (!p->active) continue;
        const PathEnemy *pe = s_pe[i];
        int old_x = p->row;
        s_idx[i] = (s_idx[i] + 1) % pe->plen;
        int k = pe->poff + s_idx[i];
        p->row = PATH_POS[k][0];
        p->col = PATH_POS[k][1];
        if (p->row != old_x) p->face = (p->row > old_x) ? 1 : -1;   /* dirección actual */
        p->anim ^= 1;
    }
}

/* ¿(sr,sc) cae en el gráfico horneado de spawn de algún enemigo? (se blanquea
 * en pantalla, así que para la colisión debe ser AIRE). */
int enemy_spawn_cell(int sr, int sc)
{
    for (int i = 0; i < g_pen_n; i++) {
        PortEnemy *p = &g_pen[i];
        int r0 = p->sc + 4, c0 = p->sr + p->gox;
        if (sr >= r0 && sr < r0 + 2 && sc >= c0 && sc < c0 + p->gw) return 1;
    }
    return 0;
}
