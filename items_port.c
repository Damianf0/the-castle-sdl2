/*
 * THE CASTLE — Ítems recogibles (tabla 0xE3D6 del ROOM LOADER, dispatcher
 * real sub_5BB0). Comida/tesoros (0x27-0x29, dan puntos), power-ups
 * (0x23-0x26), mapa (0x22). Se recogen al tocarlos y se blanquean sus celdas.
 * NUNCA son sólidos. Persisten recogidos.
 */
#include "items_port.h"
#include "room_loader.h"

PortItem g_pitem[ITEM_MAX];
int      g_pitem_n;

/* (persistencia REAL: slot[0]=0 al recoger; sub_6134 lo committea al salir) */

static void item_blank(const PortItem *p)
{
    for (int dr = 0; dr < 2; dr++)
        for (int dc = 0; dc < 2; dc++)
            rl_cell_blank(p->srow + dr, p->scol + dc);
}

void items_room_init(unsigned char room)
{
    int ry = room >> 4, rx = room & 0x0F;
    g_pitem_n = 0;
    if (rx > 9 || ry > 9) return;
    for (int s = 0; s < 16 && g_pitem_n < ITEM_MAX; s++) {
        uint16_t e = (uint16_t)(0xE3D6u + s * 4);
        uint8_t act = rl_ram_rb(e), val = rl_ram_rb((uint16_t)(e + 1));
        if (!act || val < 0x22u || val > 0x29u) continue;
        {
            int scol = rl_ram_rb((uint16_t)(e + 2)) + 1;
            int srow = rl_ram_rb((uint16_t)(e + 3)) + 4;
            PortItem *p;
            if (scol > 30 || srow > 22) continue;
            p = &g_pitem[g_pitem_n++];
            p->active = 1;   /* el loader solo lista slots presentes */
            p->val    = val;
            p->slot   = (uint8_t)s;
            p->scol   = scol;
            p->srow   = srow;
        }
    }
}

/* AABB del jugador vs cada ítem; al tocar, lo recoge (desaparece). */
void items_update(int px, int py, int pw, int ph)
{
    for (int i = 0; i < g_pitem_n; i++) {
        PortItem *p = &g_pitem[i];
        if (!p->active) continue;
        int ix = p->scol * 8, iy = p->srow * 8;
        if (px < ix + 16 && px + pw > ix && py < iy + 16 && py + ph > iy) {
            p->active = 0;
            rl_ram_wb((uint16_t)(0xE3D6u + p->slot * 4u), 0u);  /* persiste */
            item_blank(p);                  /* desaparece de la pantalla */
            /* TODO fiel (Fase 5): efectos reales vía sub_5B96/0x5C3A+ —
             * 0x27-0x29 puntos (sub_5D87), 0x22 mapa, 0x23-0x26 power-ups */
        }
    }
}

/* ¿(sr,sc) cae en un ítem? (los ítems nunca son sólidos, recogidos o no) */
int item_cell(int sr, int sc)
{
    for (int i = 0; i < g_pitem_n; i++) {
        PortItem *p = &g_pitem[i];
        if (sr >= p->srow && sr < p->srow + 2 && sc >= p->scol && sc < p->scol + 2)
            return 1;
    }
    return 0;
}
