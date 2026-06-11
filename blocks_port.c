/*
 * THE CASTLE — Bloques empujables (tabla COLL 0xE386 del ROOM LOADER:
 * ollas/ladrillos, códigos 0x30-0x35; el 0x34 son trampas, no bloques).
 * Sólidos; el jugador los empuja en horizontal si hay lugar atrás; caen por
 * gravedad si quedan sin apoyo. Gráfico real 2x2 capturado de la VRAM al
 * cargar la sala; el horneado del spawn se blanquea (se dibujan dinámicos).
 */
#include "blocks_port.h"
#include "room_loader.h"
#include "actors.h"

Block g_block[BLOCK_MAX];
int   g_block_n;

#define BW 2
#define BH 2

void blocks_room_init(unsigned char room)
{
    int ry = room >> 4, rx = room & 0x0F;
    g_block_n = 0;
    if (rx > 9 || ry > 9) return;
    /* tabla COLL 0xE386: 8 slots × [activo, código, col, fila, 0] */
    for (int s = 0; s < 8 && g_block_n < BLOCK_MAX; s++) {
        uint16_t e = (uint16_t)(0xE386u + s * 5);
        uint8_t act = rl_ram_rb(e), code = rl_ram_rb((uint16_t)(e + 1));
        if (!act || code < 0x30u || code >= 0x36u || code == 0x34u) continue;
        {
            int scol = rl_ram_rb((uint16_t)(e + 2)) + 1;
            int srow = rl_ram_rb((uint16_t)(e + 3)) + 4;
            Block *b = &g_block[g_block_n++];
            b->active = 1;
            b->scol = b->sc0 = scol;
            b->srow = b->sr0 = srow;
            for (int dr = 0; dr < BH; dr++)
                for (int dc = 0; dc < BW; dc++) {
                    rl_cell_gfx(srow + dr, scol + dc, b->gfx[dr * BW + dc]);
                    rl_cell_blank(srow + dr, scol + dc);   /* se dibuja dinámico */
                }
        }
    }
}

int block_solid(int srow, int scol)
{
    for (int i = 0; i < g_block_n; i++) {
        Block *b = &g_block[i];
        if (!b->active) continue;
        if (scol >= b->scol && scol < b->scol + BW &&
            srow >= b->srow && srow < b->srow + BH)
            return 1;
    }
    return 0;
}

/* ¿celda libre para que un bloque ocupe? (ni pared ni otro bloque) */
static int cell_free_for_block(int srow, int scol, Block *self)
{
    if (scol < 0 || scol >= RT_COLS || srow < 0 || srow >= RT_ROWS) return 0;
    if (actors_tile_solid(srow, scol)) return 0;
    for (int i = 0; i < g_block_n; i++) {
        Block *b = &g_block[i];
        if (b == self || !b->active) continue;
        if (scol >= b->scol && scol < b->scol + BW &&
            srow >= b->srow && srow < b->srow + BH)
            return 0;
    }
    return 1;
}

int blocks_push(int player_px, int player_py, int ph, int dir)
{
    static int gate = 0;
    if (dir == 0) return 0;
    /* tile líder del jugador en la dirección de empuje (caja 16 de ancho) */
    int lead = (dir > 0) ? (player_px + 16) / 8 : (player_px - 1) / 8;
    int r0 = player_py / 8, r1 = (player_py + ph - 1) / 8;
    for (int i = 0; i < g_block_n; i++) {
        Block *b = &g_block[i];
        if (!b->active) continue;
        /* el bloque está justo adelante y solapa las filas del jugador */
        if (lead >= b->scol && lead < b->scol + BW &&
            !(r1 < b->srow || r0 >= b->srow + BH)) {
            /* columna nueva que debe quedar libre (las 2 filas del bloque) */
            int nc = (dir > 0) ? b->scol + BW : b->scol - 1;
            int ok = 1;
            for (int dr = 0; dr < BH; dr++)
                if (!cell_free_for_block(b->srow + dr, nc, b)) { ok = 0; break; }
            if (!ok) return 0;
            if (++gate < 6) return 0;     /* empuje pesado: 1 paso cada 6 frames */
            gate = 0;
            b->scol += dir;
            g_player_px += dir * 8;        /* el jugador avanza con el bloque */
            return 1;
        }
    }
    return 0;
}

void blocks_step(void)
{
    static int gctr = 0;
    if (++gctr < 4) return;   /* gravedad cada 4 frames */
    gctr = 0;
    for (int i = 0; i < g_block_n; i++) {
        Block *b = &g_block[i];
        if (!b->active) continue;
        int below = b->srow + BH;
        int sup = 0;
        for (int dc = 0; dc < BW; dc++)
            if (!cell_free_for_block(below, b->scol + dc, b)) { sup = 1; break; }
        if (!sup && below < RT_ROWS) b->srow++;   /* cae */
    }
}

/* ¿(sr,sc) cae en el spawn horneado de un bloque? (se blanquea -> AIRE en colisión;
 * la posición ACTUAL del bloque la maneja block_solid). */
int block_spawn_cell(int sr, int sc)
{
    for (int i = 0; i < g_block_n; i++) {
        Block *b = &g_block[i];
        if (sr >= b->sr0 && sr < b->sr0 + BH && sc >= b->sc0 && sc < b->sc0 + BW) return 1;
    }
    return 0;
}
