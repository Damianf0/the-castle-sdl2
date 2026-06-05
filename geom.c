/*
 * THE CASTLE — Decoder de geometría de sala (port fiel del motor de la ROM).
 * =========================================================================
 * Reconstruido por ingeniería inversa del disasm (sub_6616 / sub_66A2 /
 * sub_6766 / sub_6973 / sub_69AA) y validado contra el mapa Castle-SG-All.png
 * y las posiciones de enemigos conocidas. Ver decode_geom.py (prototipo Python).
 *
 * Cada sala tiene 3 punteros en 0x7CF2 + (42*row + 4*col), offsets +0/+4/+2:
 *   ptr#1 (+0): sub_6616, cursor row=0   -> banda/borde superior
 *   ptr#2 (+4): sub_6616, cursor row=28  -> banda/borde inferior
 *   ptr#3 (+2): cuerpo de sala -> 3 pasadas que comparten cursor y stream:
 *               sub_66A2 (col-major fill) + sub_6766 (row-major) + sub_69AA (objetos)
 *
 * Resultado:
 *   g_map[row*20+col] = shape (0 = aire, !=0 = pared) — espacio 30 filas x 20 cols.
 *   g_geom_objects[]  = objetos/enemigos/coleccionables decodificados del pass final.
 */
#include <stdint.h>
#include <string.h>
#include "game.h"
#include "geom.h"

#define ROM_ORG   0x4000u
#define TABLE     0x7CF2u
#define MAP_COLS  20u
#define MAP_ROWS  30u

GeomObj g_geom_objects[64];
int     g_geom_object_count;
uint8_t g_geom_room;      /* última sala decodificada (para tinte azul/rojo) */

/* ---- estado del decoder (equivale a 0xEADB row / 0xEADC col / 0xEAD9 ptr) ---- */
typedef struct { int row, col; uint32_t ptr; } Dec;

static inline uint8_t grb(uint16_t addr) {
    uint32_t off = (uint32_t)addr - ROM_ORG;
    return (g_rom && off < g_rom_size) ? g_rom[off] : 0xFFu;
}
static inline uint16_t grw(uint16_t addr) {
    return (uint16_t)(grb(addr) | ((uint16_t)grb((uint16_t)(addr + 1u)) << 8));
}
static inline uint8_t dec_byte(Dec *d) {
    uint8_t b = grb((uint16_t)d->ptr); d->ptr++; return b;
}

static void mark(int r, int c, uint8_t s) {
    if (c >= 0 && c < (int)MAP_COLS && r >= 0 && r < (int)MAP_ROWS)
        g_map[r * (int)MAP_COLS + c] = s;
}

/* sub_6A3A — avance row-major (en filas borde 0/0x1C avanza por columna) */
static void adv_row(Dec *d) {
    if (d->row == 0 || d->row == 0x1C) { /* sub_6A63 */
        d->col++; if (d->col == 0x14) { d->col = 0; d->row += 2; }
        return;
    }
    d->row += 2;
    if (d->row == 0x1C) { d->row = 2; d->col++; }
}
/* sub_6A63 — avance col-major */
static void adv_col(Dec *d) {
    d->col++; if (d->col == 0x14) { d->col = 0; d->row += 2; }
}

/* sub_6998 — count extendido (suma bytes hasta != 0xFF), con guardas */
static int ext_count(Dec *d, int count) {
    int guard = 0;
    while (guard++ < 64) {
        uint8_t b = dec_byte(d);
        count += b;
        if (b != 0xFF) break;
    }
    return count > 600 ? 600 : count;
}

/* sub_66B0 — footprint de pintado row-major */
static void paint_rowmajor(Dec *d, uint8_t a) {
    int r = d->row, c = d->col;
    if (a == 0) {
        /* aire: solo avanza */
    } else if (a >= 1 && a <= 3) {
        mark(r, c, a); mark(r + 1, c, a);                 /* par vertical */
    } else if (a >= 0xA && a <= 0xB) {
        mark(r, c, a); mark(r + 1, c, a);
        mark(r, c + 1, a); mark(r + 1, c + 1, a);          /* bloque 2x2 */
    } else {
        mark(r, c, a);
    }
    adv_row(d);
}
/* sub_6774 — footprint de pintado col-major (run horizontal) */
static void paint_colmajor(Dec *d, uint8_t a) {
    if (a != 0) mark(d->row, d->col, a);
    adv_col(d);
}

/* sub_6616 — decoder principal (ptr#1/ptr#2): count=(b&7)+1, shape=b>>3 */
static void run_6616(Dec *d) {
    for (int it = 0; it < 4000; it++) {
        uint8_t byte = dec_byte(d);
        int count = (byte & 7) + 1;
        if (count == 8) count = ext_count(d, count);
        uint8_t shape = byte >> 3;
        if      (shape == 7 && d->row == 0x1C) shape = 8;
        else if (shape == 8 && d->row == 0x00) shape = 7;
        if (shape == 0)
            for (int k = 0; k < count; k++) paint_rowmajor(d, 0);
        else if (shape < 0x10)
            for (int k = 0; k < count; k++) paint_colmajor(d, shape);
        else
            for (int k = 0; k < count; k++) paint_rowmajor(d, shape & 0x0F);
        if (d->row == 0x02 || d->row == 0x1E) return;
    }
}

/* sub_6973 — decoder nibble (ptr#3): count=(b&0xF)+1, shape=b>>4 */
static void read_6973(Dec *d, uint8_t *shape, int *count) {
    uint8_t byte = dec_byte(d);
    int cnt = (byte & 0x0F) + 1;
    if (cnt == 0x10) cnt = ext_count(d, cnt);
    *count = cnt;
    *shape = byte >> 4;
}
/* sub_66A2 — pasada col-major-fill (avance row-major), termina en col==0x14 */
static void run_66A2(Dec *d) {
    for (int it = 0; it < 8000; it++) {
        uint8_t shape; int count;
        read_6973(d, &shape, &count);
        for (int k = 0; k < count; k++) paint_rowmajor(d, shape);
        if (d->col == 0x14) return;
    }
}
/* sub_6766 — pasada row-fill (avance col-major), termina en row==0x1C */
static void run_6766(Dec *d) {
    for (int it = 0; it < 8000; it++) {
        uint8_t shape; int count;
        read_6973(d, &shape, &count);
        for (int k = 0; k < count; k++) paint_colmajor(d, shape);
        if (d->row == 0x1C) return;
    }
}
/* sub_69AA — stream de objetos (2 bytes), terminador b0==0 */
static void run_objects(Dec *d) {
    for (int n = 0; n < 64; n++) {
        uint8_t b0 = grb((uint16_t)d->ptr);
        if (b0 == 0) break;
        uint8_t b1 = grb((uint16_t)(d->ptr + 1)); d->ptr += 2;
        GeomObj *o = &g_geom_objects[g_geom_object_count++];
        o->type  = (uint8_t)((b0 >> 4) + ((b1 & 0x40u) ? 0x20u : 0x30u));
        o->row   = (uint8_t)((b0 & 0x0Fu) * 2u);
        o->col   = (uint8_t)(b1 & 0x1Fu);
        o->flags = b1;
        if (g_geom_object_count >= (int)(sizeof(g_geom_objects)/sizeof(g_geom_objects[0])))
            break;
    }
}

void geom_decode_room(uint8_t room_x) {
    memset(g_map, 0, sizeof(uint8_t) * MAP_ROWS * MAP_COLS);
    g_geom_object_count = 0;
    g_geom_room = room_x;
    if (!g_rom) return;

    uint8_t row = (uint8_t)(room_x >> 4), col = (uint8_t)(room_x & 0x0Fu);
    uint16_t base = (uint16_t)(TABLE + 42u * row + 4u * col);
    uint16_t p1 = grw((uint16_t)(base + 0));
    uint16_t p2 = grw((uint16_t)(base + 4));
    uint16_t p3 = grw((uint16_t)(base + 2));

    Dec d;
    if (p1 >= ROM_ORG) { d.row = 0;    d.col = 0; d.ptr = p1; run_6616(&d); }
    if (p2 >= ROM_ORG) { d.row = 0x1C; d.col = 0; d.ptr = p2; run_6616(&d); }
    if (p3 >= ROM_ORG) {
        d.ptr = p3;
        d.row = 2; d.col = 0; run_66A2(&d);
        d.row = 2; d.col = 0; run_6766(&d);
        run_objects(&d);
    }
}
