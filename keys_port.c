/*
 * THE CASTLE — Llaves de colores (recolección + inventario por color).
 * Las llaves son tiles con forma de llave repartidos por el escenario
 * (keys_data.c, detectados de la VRAM real, excluyendo los íconos de requisito
 * que están sobre las puertas). Se recogen al tocarlas; se acumulan por color y
 * se ven en el marcador del HUD. Los bloques empujables (tabla COLL) son otra
 * cosa y NO se recogen acá.
 */
#include "keys_port.h"
#include "keys_data.h"

PortKey g_pkey[KEY_MAX];
int     g_pkey_n;
int     g_key_inv[KEY_COLORS];
uint8_t g_key_color[KEY_COLORS];

/* Color MSX real de cada color lógico de llave (val-0x2A), extraído de la VRAM:
 * 0=azul 1=(sin uso) 2=magenta 3=verde 4=cyan 5=amarillo. */
const uint8_t KEY_COLMSX[KEY_COLORS] = { 4, 6, 13, 2, 7, 10 };

/* Bitmap 16x16 de la llave (forma real extraída del name table del ROM):
 * anillo en la cabeza + caña + dientes. Bit 15 = columna 0. */
const uint16_t KEY_BMP[16] = {
    0x0000,0x0000,0x03C0,0x0660,0x07E0,0x07E0,0x07E0,0x03C0,
    0x0180,0x01C0,0x0180,0x01E0,0x01E0,0x0180,0x0000,0x0000 };

/* estado PERSISTENTE de llaves ya recogidas (no reaparecen al volver) */
static uint8_t s_ktaken[100][KEY_MAX];
static int     s_kidx;

void keys_room_init(unsigned char room)
{
    int ry = room >> 4, rx = room & 0x0F;
    g_pkey_n = 0;
    if (rx > 9 || ry > 9) return;
    int idx = ry * 10 + rx;
    s_kidx = idx;
    for (int c = 0; c < KEY_COLORS; c++) g_key_color[c] = KEY_COLMSX[c];
    int n = KEY_COUNT[idx];
    for (int i = 0; i < n && g_pkey_n < KEY_MAX; i++) {
        const KeySpawn *k = &KEY_DATA[idx][i];
        PortKey *p = &g_pkey[g_pkey_n++];
        p->active = s_ktaken[idx][i] ? 0 : 1;   /* ya recogida -> no aparece */
        p->color  = k->color < KEY_COLORS ? k->color : 0;
        p->scol = k->scol; p->srow = k->srow;
        p->sw = k->sw ? k->sw : 2; p->sh = k->sh ? k->sh : 2;
        g_key_color[p->color] = KEY_COLMSX[p->color];
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
            g_key_inv[p->color]++;
            p->active = 0;   /* recogida: se blanquea en debug_draw_geom */
            if (i < KEY_MAX) s_ktaken[s_kidx][i] = 1;   /* persiste: no reaparece */
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
