/*
 * THE CASTLE — Puertas con requisito de llaves.
 * Requisitos detectados del mapa/VRAM real (doors_data.c): cada puerta tiene un
 * color + cantidad de llaves. Cerrada = sólida (bloquea); al tocarla con el
 * inventario suficiente se abre (y descuenta las llaves). Las puertas abiertas
 * se vuelven transitables y se blanquean en pantalla.
 */
#include "doors_port.h"
#include "doors_data.h"
#include "keys_port.h"

RoomDoor g_door[ROOMDOOR_MAX];
int      g_door_n;

/* estado PERSISTENTE de puertas abiertas (sobrevive al cambio de sala) */
static uint8_t s_dopen[100][ROOMDOOR_MAX];
static int     s_didx;

void doors_room_init(unsigned char room)
{
    int ry = room >> 4, rx = room & 0x0F;
    g_door_n = 0;
    if (rx > 9 || ry > 9) return;
    int idx = ry * 10 + rx, n = DOOR_COUNT[idx];
    s_didx = idx;
    for (int i = 0; i < n && g_door_n < ROOMDOOR_MAX; i++) {
        const DoorDef *d = &DOOR_DATA[idx][i];
        RoomDoor *p = &g_door[g_door_n++];
        p->dcol = d->dcol; p->drow = d->drow; p->dw = d->dw; p->dh = d->dh;
        p->color = d->color;
        p->count = 1;   /* requisito = 1 llave del color (la que corresponde) */
        p->open = s_dopen[idx][i];   /* restaurar si ya estaba abierta */
    }
}

/* ¿la celda de pantalla (srow,scol) pertenece a una puerta? */
int door_block(int srow, int scol)
{
    for (int i = 0; i < g_door_n; i++) {
        RoomDoor *p = &g_door[i];
        if (scol >= p->dcol && scol < p->dcol + p->dw &&
            srow >= p->drow && srow < p->drow + p->dh)
            return p->open ? -1 : 1;
    }
    return 0;
}

/* Al tocar (o presionar contra) una puerta cerrada con las llaves requeridas,
 * la abre y descuenta las llaves del inventario. */
void doors_update(int px, int py, int pw, int ph)
{
    for (int i = 0; i < g_door_n; i++) {
        RoomDoor *p = &g_door[i];
        if (p->open) continue;
        int dx  = p->dcol * 8 - 3,             dy  = p->drow * 8 - 3;
        int dx2 = (p->dcol + p->dw) * 8 + 3,   dy2 = (p->drow + p->dh) * 8 + 3;
        if (px < dx2 && px + pw > dx && py < dy2 && py + ph > dy) {
            int c = p->color;
            if (c >= 0 && c < KEY_COLORS && g_key_inv[c] >= p->count) {
                g_key_inv[c] -= p->count;
                p->open = 1;
                s_dopen[s_didx][i] = 1;   /* persiste: queda abierta */
            }
        }
    }
}
