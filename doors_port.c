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

/* Las puertas de BORDE son pasajes entre salas: la misma puerta física existe
 * en las dos salas contiguas (col0/col28 lateral, row0/row17 vertical). Al abrir
 * una, hay que abrir su GEMELA en la sala vecina para que al pasar quede abierta
 * (si no, aparecés contra una puerta cerrada y hay que saltarla). El grid de
 * salas envuelve igual que la navegación del viewer. */
static void open_twin(int idx, const RoomDoor *p)
{
    int ry = idx / 10, rx = idx % 10;
    int nidx = -1, want_lat = 0;   /* lateral: matchear drow; vertical: dcol */
    int want_lo = 0, want_hi = 0;  /* rango de dcol (lat) o drow (vert) gemelo */
    if (p->dcol >= 28)     { nidx = ry * 10 + (rx == 9 ? 0 : rx + 1); want_lat = 1; want_lo = 0;  want_hi = 2;  }
    else if (p->dcol <= 2) { nidx = ry * 10 + (rx == 0 ? 9 : rx - 1); want_lat = 1; want_lo = 28; want_hi = 31; }
    else if (p->drow >= 20){ nidx = (ry == 9 ? 0 : ry + 1) * 10 + rx; want_lat = 0; want_lo = 0;  want_hi = 6;  }
    else if (p->drow <= 6) { nidx = (ry == 0 ? 9 : ry - 1) * 10 + rx; want_lat = 0; want_lo = 19; want_hi = 23; }
    if (nidx < 0) return;          /* puerta interior: no tiene gemela */
    int n = DOOR_COUNT[nidx];
    for (int j = 0; j < n && j < ROOMDOOR_MAX; j++) {
        const DoorDef *d = &DOOR_DATA[nidx][j];
        int pos   = want_lat ? d->dcol : d->drow;   /* lado del borde gemelo */
        int align = want_lat ? d->drow : d->dcol;   /* alineación transversal */
        int palign= want_lat ? p->drow : p->dcol;
        if (pos >= want_lo && pos <= want_hi && align >= palign - 1 && align <= palign + 1) {
            s_dopen[nidx][j] = 1;
            break;
        }
    }
}

/* Al tocar (o presionar contra) una puerta cerrada con las llaves requeridas,
 * la abre (y su gemela en la sala vecina) y descuenta las llaves. */
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
                open_twin(s_didx, p);     /* y abre la gemela de la sala vecina */
            }
        }
    }
}
