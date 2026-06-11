/*
 * THE CASTLE — Llaves de colores (recolección + inventario por color).
 * Fuente: tabla 0xE3D6 del ROOM LOADER (val >= 0x2A ⇒ llave, color=val-0x2A,
 * dispatcher real sub_5BB0). El loader las dibuja con su gráfico real; al
 * recogerlas se blanquean sus celdas. Los bloques (tabla COLL) NO van acá.
 */
#include "keys_port.h"
#include "room_loader.h"

PortKey g_pkey[KEY_MAX];
int     g_pkey_n;
int     g_key_inv[KEY_COLORS];
uint8_t g_key_color[KEY_COLORS];

/* Color MSX real de cada color lógico de llave (val-0x2A) — tabla ROM 0x6DC9
 * (sub_4EA2): {0x41,0x81,0xD1,0x21,0x71,0xA1} → ink 4,8,13,2,7,10.
 * 0=azul 1=rojo 2=magenta 3=verde 4=cyan 5=amarillo. */
const uint8_t KEY_COLMSX[KEY_COLORS] = { 4, 8, 13, 2, 7, 10 };

/* (la persistencia es la REAL: recoger pone slot[0]=0 en 0xE3D6; al salir
 * de la sala sub_6134 lo committea al bitfield 0xE1A7 y el loader no la
 * vuelve a dibujar) */

/* blanquea el gráfico 2x2 de una llave en la VRAM */
static void key_blank(const PortKey *p)
{
    for (int dr = 0; dr < p->sh; dr++)
        for (int dc = 0; dc < p->sw; dc++)
            rl_cell_blank(p->srow + dr, p->scol + dc);
}

void keys_room_init(unsigned char room)
{
    int ry = room >> 4, rx = room & 0x0F;
    g_pkey_n = 0;
    if (rx > 9 || ry > 9) return;
    for (int c = 0; c < KEY_COLORS; c++) g_key_color[c] = KEY_COLMSX[c];
    /* tabla 0xE3D6 del loader: 16 slots × [activo, val, col, fila] */
    for (int s = 0; s < 16 && g_pkey_n < KEY_MAX; s++) {
        uint16_t e = (uint16_t)(0xE3D6u + s * 4);
        uint8_t act = rl_ram_rb(e), val = rl_ram_rb((uint16_t)(e + 1));
        if (!act || val < 0x2Au) continue;
        int scol = rl_ram_rb((uint16_t)(e + 2)) + 1;
        int srow = rl_ram_rb((uint16_t)(e + 3)) + 4;
        if (scol > 30 || srow > 22) continue;
        {
            PortKey *p = &g_pkey[g_pkey_n++];
            p->active = 1;   /* el loader solo lista slots presentes (bit=1) */
            p->color  = (uint8_t)((val - 0x2Au < KEY_COLORS) ? val - 0x2Au : 0);
            p->slot   = (uint8_t)s;
            p->scol = scol; p->srow = srow;
            p->sw = 2; p->sh = 2;
        }
    }
}

/* AABB del jugador vs cada llave; al tocar, suma al inventario y la desactiva. */
void keys_update(int px, int py, int pw, int ph)
{
    for (int i = 0; i < g_pkey_n; i++) {
        PortKey *p = &g_pkey[i];
        if (!p->active) continue;
        int kx = p->scol * 8, ky = p->srow * 8;
        int kx2 = kx + p->sw * 8, ky2 = ky + p->sh * 8;
        if (px < kx2 && px + pw > kx && py < ky2 && py + ph > ky) {
            /* pickup REAL: inventario en RAM 0xE337+color (lo consume
             * sub_758C al abrir puertas), HUD por sub_5E01, slot[0]=0
             * (la persistencia la committea sub_6134 al salir de la sala) */
            uint16_t inv = (uint16_t)(0xE337u + p->color);
            rl_ram_wb(inv, (uint8_t)(rl_ram_rb(inv) + 1u));
            rl_ram_wb((uint16_t)(0xE3D6u + p->slot * 4u), 0u);
            rl_keys_hud_redraw();
            g_key_inv[p->color]++;
            p->active = 0;
            key_blank(p);                          /* desaparece de la pantalla */
        }
    }
}

/* ¿(sr,sc) cae en una llave (las llaves nunca son sólidas)? */
int key_cell(int sr, int sc)
{
    for (int i = 0; i < g_pkey_n; i++) {
        PortKey *p = &g_pkey[i];
        if (sr >= p->srow && sr < p->srow + p->sh && sc >= p->scol && sc < p->scol + p->sw) return 1;
    }
    return 0;
}
