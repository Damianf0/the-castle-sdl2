/*
 * THE CASTLE — Puertas con requisito de llaves.
 * Fuente: tabla 0xE346 del ROOM LOADER (sub_5FDF): val=(variante<<4)|(color+1).
 * Cerrada = sólida (bloquea); al tocarla con el inventario suficiente se abre,
 * descuenta la llave, se blanquea en pantalla y se limpia su colisión.
 * Persistencia por POSICIÓN (sala, dcol, drow) — cubre también a la puerta
 * GEMELA de la sala vecina (las puertas de borde existen en ambas salas).
 * TODO Fase 5: portar sub_758C (apertura real) y la persistencia por bitfield
 * 0xE00D del ROM (que codifica las gemelas naturalmente).
 */
#include "doors_port.h"
#include "room_loader.h"
#include "keys_port.h"
#include "actors.h"

RoomDoor g_door[ROOMDOOR_MAX];
int      g_door_n;

/* puertas abiertas, por posición: [sala][i] = (dcol<<8)|drow; 0 = libre */
#define OPEN_MAX 8
static uint16_t s_open[100][OPEN_MAX];
static int      s_didx;

static int pos_is_open(int idx, int dcol, int drow)
{
    for (int i = 0; i < OPEN_MAX; i++) {
        uint16_t v = s_open[idx][i];
        if (!v) continue;
        int oc = v >> 8, orw = v & 0xFF;
        if (oc >= dcol - 1 && oc <= dcol + 1 && orw >= drow - 1 && orw <= drow + 1)
            return 1;
    }
    return 0;
}
static void pos_set_open(int idx, int dcol, int drow)
{
    if (pos_is_open(idx, dcol, drow)) return;
    for (int i = 0; i < OPEN_MAX; i++)
        if (!s_open[idx][i]) { s_open[idx][i] = (uint16_t)((dcol << 8) | drow); return; }
}

/* blanquea el gráfico 2x3 de la puerta en la VRAM */
static void door_blank(const RoomDoor *p)
{
    for (int dr = 0; dr < p->dh; dr++)
        for (int dc = 0; dc < p->dw; dc++)
            rl_cell_blank(p->drow + dr, p->dcol + dc);
}

void doors_room_init(unsigned char room)
{
    int ry = room >> 4, rx = room & 0x0F;
    g_door_n = 0;
    if (rx > 9 || ry > 9) return;
    s_didx = ry * 10 + rx;
    /* tabla 0xE346 del loader: 16 slots × [activo, val, col, fila] */
    for (int s = 0; s < 16 && g_door_n < ROOMDOOR_MAX; s++) {
        uint16_t e = (uint16_t)(0xE346u + s * 4);
        uint8_t act = rl_ram_rb(e), val = rl_ram_rb((uint16_t)(e + 1));
        int color, dcol, drow;
        if (!act || !val) continue;
        color = (val & 0x0F) - 1;
        if (color < 0 || color > 5) continue;
        dcol = rl_ram_rb((uint16_t)(e + 2)) + 1;
        drow = rl_ram_rb((uint16_t)(e + 3)) + 4;
        if (dcol > 30 || drow > 21) continue;
        {
            RoomDoor *p = &g_door[g_door_n++];
            p->dcol = (uint8_t)dcol; p->drow = (uint8_t)drow;
            p->dw = 2; p->dh = 3;
            p->color = (uint8_t)color;
            p->count = 1;   /* sub_758C descuenta exactamente 1 llave */
            p->open = (uint8_t)pos_is_open(s_didx, dcol, drow);
            if (p->open) {  /* abierta: borrar gráfico + limpiar colisión */
                door_blank(p);
                actors_cm_clear(p->dcol, p->drow, p->dw, p->dh);
            }
        }
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

/* Las puertas de BORDE son la misma puerta física en las dos salas contiguas
 * (col0/col28 lateral, row0/row17 vertical). Al abrir una se registra también
 * la posición GEMELA en la sala vecina. El grid envuelve como el viewer. */
static void open_twin(int idx, const RoomDoor *p)
{
    int ry = idx / 10, rx = idx % 10;
    if (p->dcol >= 28)
        pos_set_open(ry * 10 + (rx == 9 ? 0 : rx + 1), 1, p->drow);
    else if (p->dcol <= 2)
        pos_set_open(ry * 10 + (rx == 0 ? 9 : rx - 1), 29, p->drow);
    else if (p->drow >= 20)
        pos_set_open((ry == 9 ? 0 : ry + 1) * 10 + rx, p->dcol, 4);
    else if (p->drow <= 6)
        pos_set_open((ry == 0 ? 9 : ry - 1) * 10 + rx, p->dcol, 21);
}

/* Al tocar (o presionar contra) una puerta cerrada con las llaves requeridas,
 * la abre (y su gemela en la sala vecina) y descuenta las llaves. */
void doors_update(int px, int py, int pw, int ph)
{
    for (int i = 0; i < g_door_n; i++) {
        RoomDoor *p = &g_door[i];
        if (p->open) continue;
        {
            int dx  = p->dcol * 8 - 3,             dy  = p->drow * 8 - 3;
            int dx2 = (p->dcol + p->dw) * 8 + 3,   dy2 = (p->drow + p->dh) * 8 + 3;
            if (px < dx2 && px + pw > dx && py < dy2 && py + ph > dy) {
                int c = p->color;
                if (c >= 0 && c < KEY_COLORS && g_key_inv[c] >= p->count) {
                    g_key_inv[c] -= p->count;
                    p->open = 1;
                    pos_set_open(s_didx, p->dcol, p->drow);   /* persiste */
                    door_blank(p);
                    actors_cm_clear(p->dcol, p->drow, p->dw, p->dh);
                    open_twin(s_didx, p);
                }
            }
        }
    }
}
