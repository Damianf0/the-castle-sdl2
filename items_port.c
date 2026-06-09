/*
 * THE CASTLE — Ítems recogibles (tabla 0xE3D6, dispatcher real sub_5BB0).
 * Comida/tesoros (0x27-0x29, dan puntos), power-ups (0x23-0x26), mapa (0x22).
 * Se recogen al tocarlos y desaparecen. NUNCA son sólidos (en el ROM la
 * colisión es contra el jugador, no contra el escenario). Persisten recogidos.
 */
#include "items_port.h"
#include "items_data.h"

PortItem g_pitem[ITEM_MAX];
int      g_pitem_n;

/* estado PERSISTENTE de ítems ya recogidos (no reaparecen al volver) */
static uint8_t s_itaken[100][ITEM_MAX];
static int     s_iidx;

void items_room_init(unsigned char room)
{
    int ry = room >> 4, rx = room & 0x0F;
    g_pitem_n = 0;
    if (rx > 9 || ry > 9) return;
    int idx = ry * 10 + rx;
    s_iidx = idx;
    int n = ITEM_COUNT[idx];
    for (int i = 0; i < n && g_pitem_n < ITEM_MAX; i++) {
        const ItemSpawn *s = &ITEM_DATA[idx][i];
        PortItem *p = &g_pitem[g_pitem_n++];
        p->active = s_itaken[idx][i] ? 0 : 1;
        p->val    = s->val;
        p->scol   = s->scol;
        p->srow   = s->srow;
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
            p->active = 0;                  /* recogido: se blanquea en el render */
            s_itaken[s_iidx][i] = 1;        /* persiste: no reaparece */
            /* TODO fiel: 0x27-0x29 suman puntos (sub_5D87), 0x22 revela el
             * mapa, 0x23-0x26 efectos — pendiente de portar */
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
