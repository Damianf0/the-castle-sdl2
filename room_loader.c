/*
 * THE CASTLE - ROOM LOADER fiel (sub_64DD @0x64DD y rutinas hijas).
 * ===================================================================
 * Port instruccion-a-instruccion del disasm. Decodifica una sala desde el
 * ROM y puebla:
 *   - espejo de RAM 0xE000-0xEAFF: colmap 0xE496 (20 filas x 30 cols,
 *     fila*30+col), tabla paralela 0xE6EE (celda->objeto), puertas 0xE346,
 *     coleccionables 0xE3D6, enemigos COLL 0xE386 / BAT 0xE416,
 *     estructurales 0xE43E, contadores 0xE48E-0xE495, tablas de asignacion
 *     de tiles por tercio 0xE946/0xE9A6/0xEA06 + contadores 0xEA66-68,
 *     bitfields de persistencia 0xE00D+ (puertas), 0xE0DF+ (COLL),
 *     0xE1A7+ (items), 0xE26F+ (BAT).
 *   - VRAM emulada: tiles asignados dinamicamente por tercio (el alocador
 *     sub_6B0B carga el grafico de un codigo la primera vez que un tercio
 *     lo usa) + name table.
 *
 * Coordenadas: EADB = columna de campo 0..29 (avanza de a 2), EADC = fila
 * 0..19. Pantalla: name[(fila+4)*32 + (col+1)]. Tercio por fila: <4 -> 0,
 * <12 -> 1, resto 2.
 *
 * Verificado contra tests/fixtures/ (dumps openMSX por force-call a
 * sub_64DD): colmap_XX.bin (E496+E6EE), e346/e3d6/e43e_XX.bin,
 * objs_XX.bin (E380-E49F), ont_XX.bin (name table).
 */
#include <stdint.h>
#include <string.h>
#include "hal.h"
#include "game.h"
#include "room_loader.h"

/* ===== espejo de RAM 0xE000-0xEAFF ===== */
static uint8_t ram[0xB00];
#define R(a)  ram[(uint16_t)((a) - 0xE000u)]

uint8_t rl_ram_rb(uint16_t a) { return R(a); }
void    rl_ram_wb(uint16_t a, uint8_t v) { R(a) = v; }

static uint16_t rw_ram(uint16_t a) { return (uint16_t)(R(a) | ((uint16_t)R(a + 1u) << 8)); }
static void     ww_ram(uint16_t a, uint16_t v) { R(a) = (uint8_t)v; R(a + 1u) = (uint8_t)(v >> 8); }

/* ===== ROM (direcciones Z80 0x4000-0xBFFF) ===== */
static uint8_t rb(uint16_t a)
{
    uint32_t o = (uint32_t)a - 0x4000u;
    return (g_rom && o < g_rom_size) ? g_rom[o] : 0xFFu;
}
static uint16_t rwrd(uint16_t a) { return (uint16_t)(rb(a) | ((uint16_t)rb((uint16_t)(a + 1u)) << 8)); }
/* lectura "de memoria Z80": RAM espejo o ROM (el stream y los buffers conviven) */
static uint8_t mem_rb(uint16_t a) { return (a >= 0xE000u) ? R(a) : rb(a); }

/* ===== VRAM SCREEN 2 (WRTVRM 0x004D / RDVRM 0x004A) ===== */
#define NAME_BASE 0x1800u   /* (0xF3C7) GRPNAM */
#define COL_BASE  0x2000u   /* (0xF3C9) GRPCOL */
#define PAT_BASE  0x0000u   /* (0xF3CB) GRPCGP */

/* sub_6ADF/6AEF: name addr de la celda de campo (col30, fila20) */
static uint16_t field_name_addr(uint8_t col, uint8_t row)
{
    return (uint16_t)(NAME_BASE + (uint16_t)(row + 4u) * 32u + (uint16_t)(col + 1u));
}

/* ===== forward ===== */
static void s_6B0B(uint8_t e, int third);
static void adv_col(uint8_t *h, uint8_t *l);
static void s_6055(uint8_t a, uint8_t h, uint8_t l, uint16_t de);

/* ==========================================================================
 * sub_5E80 - escribe el COLMAP 0xE496 y la tabla paralela 0xE6EE.
 * Codigos < 0x1B: par (D,E) de la tabla ROM 0x7780 + c*2.
 * 0x1B-0x1E: tabla de variantes - ptr en 0x77B6+(c-0x1B)*2, entrada +D*2.
 * 0x1F: como 0x19. 0x20-0x2F (coleccionables): variante indice 4, entrada
 * +D*2. 0x30-0x3F (enemigos): E=slot*8 con flags, D=0xA8/0x18/0x38.
 * 0x40+: puertas/escaleras D=0xA0 (<0x55) o 0xA2.
 * ========================================================================== */
static void s_5E80(uint8_t h, uint8_t l, uint8_t c, uint8_t d, uint8_t e)
{
    uint8_t cmD, cmE;
    if (c < 0x1Bu) {
        uint16_t t = (uint16_t)(0x7780u + c * 2u);
        cmD = rb(t); cmE = rb((uint16_t)(t + 1u));
    } else if (c < 0x1Fu) {
        uint16_t p = (uint16_t)(rwrd((uint16_t)(0x77B6u + (c - 0x1Bu) * 2u)) + d * 2u);
        cmD = rb(p); cmE = rb((uint16_t)(p + 1u));
    } else if (c == 0x1Fu) {
        uint16_t t = (uint16_t)(0x7780u + 0x19u * 2u);
        cmD = rb(t); cmE = rb((uint16_t)(t + 1u));
    } else if (c < 0x30u) {
        uint16_t p = (uint16_t)(rwrd((uint16_t)(0x77B6u + 4u * 2u)) + d * 2u);
        cmD = rb(p); cmE = rb((uint16_t)(p + 1u));
    } else if (c < 0x40u) {
        uint8_t e8 = (uint8_t)((uint8_t)(e << 3) & 0x7Fu);
        if (c < 0x35u) {
            cmD = 0xA8u;
        } else if (c == 0x35u) {
            if (d < 2u) { e8 |= 0x01u; cmD = 0x18u; }
            else        { cmD = 0xA8u; }
        } else {
            e8 |= 0x80u;
            if (c == 0x36u && d < 2u) { e8 |= 0x04u; cmD = 0x38u; }
            else                      { e8 |= 0x01u; cmD = 0x38u; }
        }
        cmE = e8;
    } else if (c < 0x55u) { cmD = 0xA0u; cmE = e; }
    else                  { cmD = 0xA2u; cmE = e; }

    {   /* sub_49B6: off = fila*30 + col */
        uint16_t off = (uint16_t)(l * 30u + h);
        R(0xE496u + off) = cmD;
        R((uint16_t)(0xE496u + 0x258u + off)) = cmE;
    }
}

/* ==========================================================================
 * Alocador de tiles por tercio (sub_6CB5 / sub_6B02 / sub_6CD9 / sub_6CCA)
 * ========================================================================== */
static uint16_t s_6CB5(uint8_t code, int third)
{
    uint16_t tbl = (uint16_t)(0xE946u + third * 0x60u);
    uint8_t assigned = (uint8_t)(R((uint16_t)(0xEA66u + third)) + 1u);
    R(tbl + code) = assigned;
    return (uint16_t)(((uint16_t)third * 0x100u + assigned) * 8u);
}
static void s_6B02(int third) { R((uint16_t)(0xEA66u + third)) += 1u; }

/* sub_6CD9: un tile de 16 bytes intercalados (pat,col por fila) -> VRAM */
static void s_6CD9(uint16_t *voff, uint16_t *src)
{
    for (int r = 0; r < 8; r++) {
        hal_vdp_write_vram((uint16_t)(PAT_BASE + *voff), mem_rb((*src)++));
        hal_vdp_write_vram((uint16_t)(COL_BASE + *voff), mem_rb((*src)++));
        (*voff)++;
    }
}
/* 8 filas estilo sub_6CCA: patron del src (saltando su byte de color),
 * color = byte constante */
static void s_6CCA_tile(uint16_t *voff, uint16_t *src, uint8_t colbyte)
{
    for (int r = 0; r < 8; r++) {
        hal_vdp_write_vram((uint16_t)(PAT_BASE + *voff), mem_rb((*src)++));
        hal_vdp_write_vram((uint16_t)(COL_BASE + *voff), colbyte);
        (*voff)++; (*src)++;
    }
}
static void load_n(uint16_t *voff, uint16_t src, int n, int third)
{
    for (int i = 0; i < n; i++) { s_6B02(third); s_6CD9(voff, &src); }
}

/* sub_6C9B: clase simple - count tiles del descriptor 0x7BDA + code*3 */
static void s_6C9B(uint8_t code, int third)
{
    uint16_t voff = s_6CB5(code, third);
    uint16_t desc = (uint16_t)(0x7BDAu + code * 3u);
    load_n(&voff, rwrd(desc), rb((uint16_t)(desc + 2u)), third);
}

/* ===== transformaciones de graficos (espejo / corrimiento) ===== */
/* sub_6D5A: espeja un tile: patron bit-reverso, color igual */
static void s_6D5A(uint16_t src, uint16_t dst)
{
    for (int r = 0; r < 8; r++) {
        uint8_t p = mem_rb(src++), m = 0;
        for (int b = 0; b < 8; b++) { m = (uint8_t)((m << 1) | (p & 1u)); p >>= 1; }
        R(dst++) = m;
        R(dst++) = mem_rb(src++);
    }
}
/* sub_6D32 / sub_6D4F: 2 / 3 tiles espejados con el orden invertido */
static void s_6D32(uint16_t src, uint16_t dst)
{
    uint16_t d = (uint16_t)(dst + 0x10u);
    for (int t = 0; t < 2; t++) { s_6D5A(src, d); src += 0x10u; d -= 0x10u; }
}
static void s_6D4F(uint16_t src, uint16_t dst)
{
    uint16_t d = (uint16_t)(dst + 0x20u);
    for (int t = 0; t < 3; t++) { s_6D5A(src, d); src += 0x10u; d -= 0x10u; }
}
/* sub_6D04 / sub_6D1B: 4 (2 pares) / 6 (2 trios) tiles al buffer 0xEA69 */
static void s_6D04(uint16_t src) { s_6D32(src, 0xEA69u); s_6D32((uint16_t)(src + 0x20u), 0xEA89u); }
static void s_6D1B(uint16_t src) { s_6D4F(src, 0xEA69u); s_6D4F((uint16_t)(src + 0x30u), 0xEA99u); }

/* sub_6D98: limpia 4 filas (pat 0 / col 0x11) de 2 tiles (stride 0x10) */
static void s_6D98(uint16_t hl)
{
    for (int c = 0; c < 2; c++) {
        for (int b = 0; b < 4; b++) { R(hl++) = 0x00u; R(hl++) = 0x11u; }
        hl += 8u;
    }
}
/* sub_6DAD: corre 4 filas hacia abajo: x+8..x+F -> x+0x20, x..x+7 -> x+8 */
static void s_6DAD(uint16_t x)
{
    memcpy(&R((uint16_t)(x + 0x20u)), &R((uint16_t)(x + 8u)), 8);
    memcpy(&R((uint16_t)(x + 8u)),    &R(x),                  8);
}
/* sub_6D75: corrimiento de 4px del sprite en el buffer */
static void s_6D75(void)
{
    s_6D98(0xEAB1u);
    {
        uint16_t hl = 0xEA99u;
        for (int b = 0; b < 4; b++) { s_6DAD(hl); hl -= 0x10u; }
    }
    s_6D98(0xEA69u);
}
/* sub_6C73: copia n bytes del src al buffer 0xEA69 */
static void copy_to_buf(uint16_t src, uint16_t n)
{
    for (uint16_t i = 0; i < n; i++) R((uint16_t)(0xEA69u + i)) = mem_rb((uint16_t)(src + i));
}

/* sub_6BC2: graficos de enemigo (codigos 0x30-0x3F) en un tercio */
static void s_6BC2(uint8_t code, int third)
{
    uint16_t voff = s_6CB5(code, third);
    uint16_t src  = rwrd((uint16_t)(0x7BDAu + code * 3u));
    load_n(&voff, src, 4, third);                                   /* sub_6BFA  */
    if (code < 0x36u) {
        load_n(&voff, (uint16_t)(src + 0x40u), 6, third);           /* sub_6C47  */
        copy_to_buf(src, 0x40u);                                    /* sub_6C4F  */
        s_6D75();
        load_n(&voff, 0xEA69u, 6, third);
    } else {
        s_6D04(src);                  load_n(&voff, 0xEA69u, 4, third);          /* 6C08 */
        load_n(&voff, (uint16_t)(src + 0x40u), 4, third);                        /* 6C15 */
        load_n(&voff, (uint16_t)(src + 0x80u), 6, third);                        /* 6C1D */
        s_6D04((uint16_t)(src + 0x40u)); load_n(&voff, 0xEA69u, 4, third);       /* 6C27 */
        s_6D1B((uint16_t)(src + 0x80u)); load_n(&voff, 0xEA69u, 6, third);       /* 6C2D */
        if (code < 0x3Au) {
            copy_to_buf((uint16_t)(src + 0x40u), 0x40u); s_6D75();               /* 6C36 */
            load_n(&voff, 0xEA69u, 6, third);
            s_6D04((uint16_t)(src + 0x40u)); s_6D75();                           /* 6C3E */
            load_n(&voff, 0xEA69u, 6, third);
        }
    }
}

/* sub_6B0B - alocador: carga el grafico del codigo en el/los tercios */
static void s_6B0B(uint8_t e, int third)
{
    if (e == 0u) return;
    if (e >= 0x40u) {                       /* puertas/escaleras de color */
        uint16_t voff = s_6CB5(e, third);
        uint8_t  colb = rb((uint16_t)(0x6DCFu + (e - 0x40u)));
        uint16_t src  = rwrd((uint16_t)(0x7BDAu + e * 3u));
        for (int t = 0; t < 2; t++) { s_6B02(third); s_6CCA_tile(&voff, &src, colb); }
        return;
    }
    if (e >= 0x30u) {                       /* enemigos */
        if (e == 0x36u) { s_6C9B(e, third); return; }
        for (int t = 0; t < 3; t++) s_6BC2(e, t);
        return;
    }
    if (e >= 0x2Au) {                       /* llaves: color de la tabla 0x6DC9 */
        uint16_t voff = s_6CB5(e, third);
        uint8_t  colb = rb((uint16_t)(0x6DC9u + (e - 0x2Au)));
        uint16_t src  = rwrd((uint16_t)(0x7BDAu + e * 3u));
        for (int t = 0; t < 4; t++) { s_6B02(third); s_6CCA_tile(&voff, &src, colb); }
        return;
    }
    if (e >= 0x1Bu && e < 0x1Fu) {          /* tiles "globales": los 3 tercios */
        for (int t = 0; t < 3; t++) s_6C9B(e, t);
        return;
    }
    s_6C9B(e, third);
}

/* ==========================================================================
 * sub_6A7C - escritor central de celda: colmap (5E80) + name table con el
 * tile asignado del tercio (+delta). c=codigo, d=delta, e=registro E del Z80
 * (low byte del count, o slot de objeto - lo consume 5E80).
 * ========================================================================== */
static void s_6A7C(uint8_t h, uint8_t l, uint8_t c, uint8_t d, uint8_t e)
{
    if (l >= 0x14u || h >= 0x1Eu) return;
    s_5E80(h, l, c, d, e);
    {
        int third = (l < 4u) ? 0 : (l < 0x0Cu) ? 1 : 2;
        uint8_t code = (c == 0x0Du) ? 0x0Cu : c;
        uint16_t tbl = (uint16_t)(0xE946u + third * 0x60u);
        if (R(tbl + code) == 0u) s_6B0B(code, third);
        hal_vdp_write_vram(field_name_addr(h, l), (uint8_t)(R(tbl + code) + d));
    }
}

/* ===== avanzadores de cursor (EADB col 0..28 de a 2, EADC fila 0..19) ===== */
static void adv_col(uint8_t *h, uint8_t *l)             /* sub_6A63 */
{
    uint8_t a = (uint8_t)(R(0xEADCu) + 1u);
    R(0xEADCu) = a;
    if (a == 0x14u) { R(0xEADCu) = 0u; R(0xEADBu) += 2u; }
    *h = R(0xEADBu); *l = R(0xEADCu);
}
static void adv_row(uint8_t *h, uint8_t *l)             /* sub_6A3A */
{
    uint8_t a = R(0xEADBu);
    if (a == 0x00u || a == 0x1Cu) { adv_col(h, l); return; }
    a += 2u; R(0xEADBu) = a;
    if (a == 0x1Cu) { R(0xEADBu) = 2u; R(0xEADCu) += 1u; }
    *h = R(0xEADBu); *l = R(0xEADCu);
}

/* ===== lectores de stream ===== */
static uint16_t s_6998(uint16_t de)                     /* count extendido */
{
    for (;;) {
        uint16_t p = (uint16_t)(rw_ram(0xEAD9u) + 1u);
        ww_ram(0xEAD9u, p);
        uint8_t b = mem_rb(p);
        de = (uint16_t)(de + b);
        if (b != 0xFFu) return de;
    }
}
static uint8_t read_6616(void)
{
    uint8_t cbyte = mem_rb(rw_ram(0xEAD9u));
    uint16_t de = (uint16_t)((cbyte & 7u) + 1u);
    if (de == 8u) de = s_6998(de);
    ww_ram(0xEADDu, de);
    ww_ram(0xEAD9u, (uint16_t)(rw_ram(0xEAD9u) + 1u));
    return (uint8_t)(cbyte >> 3);
}
static uint8_t read_6973(void)
{
    uint8_t cbyte = mem_rb(rw_ram(0xEAD9u));
    uint16_t de = (uint16_t)((cbyte & 0x0Fu) + 1u);
    if (de == 0x10u) de = s_6998(de);
    ww_ram(0xEADDu, de);
    ww_ram(0xEAD9u, (uint16_t)(rw_ram(0xEAD9u) + 1u));
    return (uint8_t)(cbyte >> 4);
}

/* ===== pares de tiles desde tablas ROM (sub_6888 / sub_689A) ===== */
static void s_68B7(uint8_t h, uint8_t l, uint16_t t, uint8_t e)
{
    s_6A7C(h, l, rb(t), rb((uint16_t)(t + 1u)), e);
    s_6A7C((uint8_t)(h + 1u), l, rb((uint16_t)(t + 2u)), rb((uint16_t)(t + 3u)), e);
}
static void s_6888(uint8_t h, uint8_t l, uint8_t c, uint8_t e)
{
    s_68B7(h, l, (uint16_t)(0x6DEAu + c * 4u), e);
}
static void s_689A(uint8_t h, uint8_t l, uint8_t c, uint8_t fase, uint8_t e)
{
    s_68B7(h, l, (uint16_t)(0x6E1Eu + c * 12u + fase * 4u), e);
}
/* sub_686C: fase del tramo: 0=inicio (count==original), 2=final (==1), 1=medio */
static uint8_t s_686C(uint16_t de)
{
    if (de == rw_ram(0xEADDu)) return 0u;
    if (de == 1u) return 2u;
    return 1u;
}

/* ===== persistencia (sub_60EB / sub_6100): bit (7-(slot&7)) de base+(slot>>3) ===== */
static uint8_t bitfield_get(uint16_t base, uint8_t slot)
{
    uint8_t b = R((uint16_t)(base + (slot >> 3)));
    return (uint8_t)((b >> (7u - (slot & 7u))) & 1u);
}
static uint8_t room_idx(void)       /* sub_60AA: fila*10 + col */
{
    uint8_t r = R(0xE320u);
    return (uint8_t)((r >> 4) * 10u + (r & 0x0Fu));
}

/* ==========================================================================
 * sub_5F1A - alta de objeto en su tabla + bit de persistencia.
 * Devuelve el bit (0 = ya recogido/muerto -> no se dibuja); *slot_out = slot.
 * ========================================================================== */
static uint8_t s_5F1A(uint8_t code, uint8_t cflags, uint8_t h, uint8_t l, uint8_t *slot_out)
{
    if (code < 0x30u) {                     /* coleccionable -> 0xE3D6 (16x4) */
        uint8_t slot = R(0xE493u); R(0xE493u) = (uint8_t)(slot + 1u);
        uint16_t ix = (uint16_t)(0xE3D6u + slot * 4u);
        uint8_t bit = bitfield_get((uint16_t)(0xE1A7u + room_idx() * 2u), slot);
        R(ix) = bit; R(ix + 1u) = code; R(ix + 2u) = h; R(ix + 3u) = l;
        *slot_out = slot;
        return bit;
    }
    if (code < 0x36u) {                     /* enemigo COLL -> 0xE386 (stride 5) */
        uint8_t slot = R(0xE492u); R(0xE492u) = (uint8_t)(slot + 1u);
        uint16_t ix = (uint16_t)(0xE386u + slot * 5u);
        uint8_t bit = bitfield_get((uint16_t)(0xE0DFu + room_idx() * 2u), slot);
        R(ix) = bit; R(ix + 1u) = code; R(ix + 2u) = h; R(ix + 3u) = l; R(ix + 4u) = 0u;
        *slot_out = slot;
        return bit;
    }
    {                                       /* BAT -> 0xE416 (stride 5) */
        uint8_t slot = R(0xE494u); R(0xE494u) = (uint8_t)(slot + 1u);
        uint16_t ix = (uint16_t)(0xE416u + slot * 5u);
        uint8_t bit = bitfield_get((uint16_t)(0xE26Fu + room_idx()), slot);
        R(ix) = bit; R(ix + 1u) = code; R(ix + 2u) = h; R(ix + 3u) = l;
        R(ix + 4u) = (cflags & 0x20u) ? 3u : 1u;
        *slot_out = slot;
        return bit;
    }
}

/* sub_5FDF - alta de puerta en 0xE346: val=(EADF<<4)|(count-1); persistencia
 * por categoria de columna (0 / 0x1C / resto) en 0xE00D + fila*0x15 + col*2. */
static uint8_t s_5FDF(uint8_t h, uint8_t l, uint8_t e_count)
{
    uint8_t slot = R(0xE491u); R(0xE491u) = (uint8_t)(slot + 1u);
    uint16_t ix = (uint16_t)(0xE346u + slot * 4u);
    uint8_t prev, cat;
    if (h == 0x00u)      { prev = R(0xE48Eu); R(0xE48Eu)++; cat = 0u; }
    else if (h == 0x1Cu) { prev = R(0xE48Fu); R(0xE48Fu)++; cat = 2u; }
    else                 { prev = R(0xE490u); R(0xE490u)++; cat = 1u; }
    {
        uint8_t rm = R(0xE320u);
        uint16_t bf = (uint16_t)(0xE00Du + (rm >> 4) * 0x15u + (rm & 0x0Fu) * 2u + cat);
        uint8_t bit = bitfield_get(bf, prev);
        R(ix) = bit;
        R(ix + 1u) = (uint8_t)((R(0xEADFu) << 4) | (uint8_t)(e_count - 1u));
        R(ix + 2u) = h; R(ix + 3u) = l;
        return bit;
    }
}

/* sub_6055 - alta estructural (rampas/ascensores) en 0xE43E (stride 5).
 * (IX+3) = registro E original = byte bajo del COUNT del tramo (verificado
 * contra e43e_XX.bin). */
static void s_6055(uint8_t a, uint8_t h, uint8_t l, uint16_t de)
{
    uint8_t slot = R(0xE495u); R(0xE495u) = (uint8_t)(slot + 1u);
    uint16_t ix = (uint16_t)(0xE43Eu + slot * 5u);
    uint8_t t;
    R(ix) = a; R(ix + 1u) = h; R(ix + 2u) = l; R(ix + 3u) = (uint8_t)de;
    if (a == 0x0Cu)      t = 3u;
    else if (a == 0x0Du) t = 1u;
    else if (a < 0x1Cu)  t = 0u;
    else if (a < 0x1Fu)  t = 4u;
    else                 t = 1u;
    R(ix + 4u) = t;
}

/* ===== glifos de puerta (caso 6 de sub_6774) ===== */
/* sub_693A: variante de pared en (h, l): busca el tile leido de la name
 * table entre los codigos 1..3 de la tabla del tercio */
static uint8_t s_693A(uint8_t h, uint8_t l)
{
    uint16_t tbl = (l < 4u) ? 0xE946u : (l < 0x0Cu) ? 0xE9A6u : 0xEA06u;
    uint8_t tile = hal_vdp_read_vram(field_name_addr(h, l));
    int b;
    for (b = 3; b >= 1; b--) {
        uint8_t a = R((uint16_t)(tbl + (4 - b)));
        if (a != 0u && (a == tile || (uint8_t)(a + 1u) == tile)) break;
    }
    if (b < 1) b = 0;
    return (uint8_t)(4 - b);
}
/* sub_68CF: fija (0xEADF) = variante de pared (mira la fila anterior) */
static void s_68CF(uint8_t h, uint8_t l)
{
    uint8_t a = s_693A(h, (uint8_t)(l - 1u));
    if (a < 4u) { R(0xEADFu) = a; return; }
    a = (uint8_t)(R(0xEADFu) & 3u);
    R(0xEADFu) = a ? a : 3u;
}
/* sub_68EA: celda superior de la puerta: marco (0x3F+var) y, si no esta
 * abierta (bit!=0), el panel de color (0x41 + (var-1)*6 + count) encima */
static void s_68EA(uint8_t h, uint8_t l, uint8_t e, uint8_t bit)
{
    uint8_t c = (uint8_t)(R(0xEADFu) + 0x3Fu);
    s_6A7C(h, l, c, 0u, e);
    s_6A7C((uint8_t)(h + 1u), l, c, 1u, e);
    if (bit == 0u) return;
    c = (uint8_t)(e + (uint8_t)((uint8_t)(R(0xEADFu) - 1u) * 6u) + 0x41u);
    s_6A7C(h, l, c, 0u, e);
    s_6A7C((uint8_t)(h + 1u), l, c, 1u, e);
}
/* sub_6918: celdas inferiores (0x53+count); E = slot de puerta - 1 */
static void s_6918(uint8_t h, uint8_t l, uint8_t e, uint8_t bit)
{
    if (bit == 0u) return;
    {
        uint8_t c = (uint8_t)(e + 0x53u);
        uint8_t eslot = (uint8_t)(R(0xE491u) - 1u);
        s_6A7C(h, l, c, 0u, eslot);
        s_6A7C((uint8_t)(h + 1u), l, c, 1u, eslot);
    }
}
/* sub_692F: pasaje simple (tile 0x16) */
static void s_692F(uint8_t h, uint8_t l, uint8_t e)
{
    s_6A7C(h, l, 0x16u, 0u, e);
    s_6A7C((uint8_t)(h + 1u), l, 0x16u, 1u, e);
}

/* ===== escritores de glifo de objeto (sub_70A6 / sub_70B6) ===== */
static void s_70A6(uint8_t h, uint8_t l, uint8_t c, uint8_t d, uint8_t e)
{
    s_6A7C(h, l, c, d, e);
    s_6A7C((uint8_t)(h + 1u), l, c, d, e);
    s_6A7C((uint8_t)(h + 1u), (uint8_t)(l + 1u), c, d, e);
    s_6A7C(h, (uint8_t)(l + 1u), c, d, e);
}
static void s_70B6(uint8_t h, uint8_t l, uint8_t c, uint8_t d, uint8_t e)
{
    s_6A7C(h, l, c, d, e);
    s_6A7C((uint8_t)(h + 1u), l, c, (uint8_t)(d + 1u), e);
    s_6A7C(h, (uint8_t)(l + 1u), c, (uint8_t)(d + 2u), e);
    s_6A7C((uint8_t)(h + 1u), (uint8_t)(l + 1u), c, (uint8_t)(d + 3u), e);
}

/* ==========================================================================
 * Pasada de objetos (sub_69AA / sub_69DF)
 * ========================================================================== */
static void s_69DF(uint8_t cflags)
{
    uint8_t h = R(0xEADBu), l = R(0xEADCu);
    uint8_t code = (uint8_t)((cflags & 0x0Fu) + ((cflags & 0x40u) ? 0x20u : 0x30u));
    uint8_t slot = 0u;
    uint8_t bit = s_5F1A(code, cflags, h, l, &slot);
    if (bit == 0u) return;
    if (code == 0x36u) {                    /* sub_6A28 */
        s_6A7C(h, (uint8_t)(l + 1u), code, 0u, slot);
        s_6A7C((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, 1u, slot);
        return;
    }
    if (code == 0x23u) {                    /* sub_6A33 */
        s_70A6(h, l, code, 0u, slot);
        return;
    }
    /* code 0x21 (spawn del jugador): solo posiciona el sprite - sin tiles */
    s_70B6(h, l, code, (uint8_t)((cflags & 0x20u) ? 4u : 0u), slot);
}
static void s_69AA(void)
{
    for (;;) {
        uint16_t p = rw_ram(0xEAD9u);
        uint8_t b0 = mem_rb(p);
        uint8_t b1;
        if (b0 == 0u) return;
        R(0xEADBu) = (uint8_t)((b0 & 0x0Fu) << 1);
        ww_ram(0xEAD9u, (uint16_t)(p + 1u));
        b1 = mem_rb((uint16_t)(p + 1u));
        R(0xEADCu) = (uint8_t)(b1 & 0x1Fu);
        s_69DF((uint8_t)((b0 >> 4) | (b1 & 0xE0u)));
        ww_ram(0xEAD9u, (uint16_t)(rw_ram(0xEAD9u) + 1u));
    }
}

/* ==========================================================================
 * Dispatchers de shape
 * ========================================================================== */
/* sub_66B0 - fila-mayor (avance adv_row) */
static void s_66B0(uint8_t a)
{
    uint8_t h = R(0xEADBu), l = R(0xEADCu);
    uint16_t de = rw_ram(0xEADDu);
    if (a >= 0x0Cu && a != 0x0Eu) s_6055(a, h, l, de);
    if (a == 0u) {
        while (de) { adv_row(&h, &l); de--; }
    } else if (a < 4u) {
        R(0xEADFu) = a;
        while (de) {
            uint8_t d0 = (uint8_t)(l & 1u);
            s_6A7C(h, l, a, d0, (uint8_t)de);
            s_6A7C((uint8_t)(h + 1u), l, a, (uint8_t)(d0 ^ 1u), (uint8_t)de);
            adv_row(&h, &l); de--;
        }
    } else if (a < 7u) {
        uint8_t c = (uint8_t)(a - 3u);
        while (de) { s_6888(h, l, c, (uint8_t)de); adv_row(&h, &l); de--; }
    } else if (a < 9u) {
        uint8_t c = (uint8_t)(a - 7u);
        while (de) { s_689A(h, l, c, s_686C(de), (uint8_t)de); adv_row(&h, &l); de--; }
    } else if (a == 9u) {
        while (de) { s_6888(h, l, 4u, (uint8_t)de); adv_row(&h, &l); de--; }
    } else if (a < 0x0Cu) {                 /* 0xA/0xB: bloque 2x2 */
        while (de) {
            s_6A7C(h, l, a, 0u, (uint8_t)de);
            s_6A7C((uint8_t)(h + 1u), l, a, 0u, (uint8_t)de);
            s_6A7C(h, (uint8_t)(l + 1u), a, 1u, (uint8_t)de);
            s_6A7C((uint8_t)(h + 1u), (uint8_t)(l + 1u), a, 1u, (uint8_t)de);
            adv_row(&h, &l); de--;
        }
    } else if (a < 0x10u) {
        uint8_t c = (uint8_t)(a - 0x0Au);
        while (de) { s_689A(h, l, c, s_686C(de), (uint8_t)de); adv_row(&h, &l); de--; }
    }
}

/* caso puerta (a==6 de sub_6774) */
static void door_case(uint8_t *h, uint8_t *l, uint16_t de)
{
    uint8_t e = (uint8_t)de;
    if (e != 1u) {
        uint8_t bit;
        s_68CF(*h, *l);
        bit = s_5FDF(*h, *l, e);
        s_68EA(*h, *l, e, bit);
        adv_col(h, l);
        s_6918(*h, *l, e, bit);
        adv_col(h, l);
        s_6918(*h, *l, e, bit);
        adv_col(h, l);
        {
            uint8_t b = R(0xEADBu);
            if (b != 0x00u && b != 0x1Cu) return;
        }
    }
    s_692F(*h, *l, e);
    adv_col(h, l);
}

/* caso ascensor (a==0xB de sub_6774) */
static void elevator_case(uint8_t *h, uint8_t *l, uint16_t de)
{
    /* el Z80 incrementa L hasta el fondo del hueco ANTES de leer el tile y
     * de dar de alta el objeto 0x1B (sub_6814: INC L x (count-1)) */
    uint8_t lbot = (uint8_t)(*l + (uint8_t)((uint8_t)de - 1u));
    uint8_t tile = hal_vdp_read_vram(field_name_addr(*h, lbot));
    uint8_t c;
    if (tile == 0u) s_6055(0x1Bu, *h, lbot, de);
    c = tile ? 0x0Bu : 0x07u;
    while (de) { s_689A(*h, *l, c, s_686C(de), (uint8_t)de); adv_col(h, l); de--; }
    if (tile == 0u)
        s_6888(*h, (uint8_t)(*l + 3u), 0x0Bu, 0u);
}

/* sub_6774 - columna-mayor (avance adv_col) */
static void s_6774(uint8_t a)
{
    uint8_t h = R(0xEADBu), l = R(0xEADCu);
    uint16_t de = rw_ram(0xEADDu);
    if (a >= 0x0Cu && a != 0x0Eu) s_6055((uint8_t)(a + 0x10u), h, l, de);
    if (a == 0u) {
        while (de) { adv_col(&h, &l); de--; }
    } else if (a < 5u) {
        uint8_t c = (uint8_t)(a + 4u);
        while (de) { s_6888(h, l, c, (uint8_t)de); adv_col(&h, &l); de--; }
    } else if (a == 5u) {
        while (de) { s_689A(h, l, 6u, s_686C(de), (uint8_t)de); adv_col(&h, &l); de--; }
    } else if (a == 6u) {
        door_case(&h, &l, de);
    } else if (a < 0x0Bu) {
        uint8_t c = (uint8_t)(a + 2u);
        while (de) { s_6888(h, l, c, (uint8_t)de); adv_col(&h, &l); de--; }
    } else if (a == 0x0Bu) {
        elevator_case(&h, &l, de);
    } else if (a < 0x0Fu) {
        uint8_t c = (uint8_t)(a - 4u);
        while (de) { s_689A(h, l, c, s_686C(de), (uint8_t)de); adv_col(&h, &l); de--; }
    } else {
        uint8_t c = (uint8_t)(a - 3u);
        while (de) { s_6888(h, l, c, (uint8_t)de); adv_col(&h, &l); de--; }
    }
}

/* sub_6664: dispatch del decoder de bandas */
static void s_6664(uint8_t shape)
{
    if (shape != 0u && shape < 0x10u) { s_6774(shape); return; }
    s_66B0((uint8_t)(shape & 0x0Fu));
}
/* sub_6616: loop de banda - termina cuando EADB llega a 0x02 / 0x1E */
static void s_6616_loop(void)
{
    for (;;) {
        uint8_t shape = read_6616();
        if (shape == 7u && R(0xEADBu) == 0x1Cu)      shape = 8u;
        else if (shape == 8u && R(0xEADBu) == 0x00u) shape = 7u;
        s_6664(shape);
        {
            uint8_t b = R(0xEADBu);
            if (b == 0x02u || b == 0x1Eu) return;
        }
    }
}
/* sub_6671: cuerpo - 2 pasadas que comparten stream + pasada de objetos */
static void s_6671(uint16_t ptr)
{
    R(0xEADBu) = 2u; R(0xEADCu) = 0u;
    ww_ram(0xEAD9u, ptr); ww_ram(0xEADDu, 0u);
    while (R(0xEADCu) != 0x14u) s_66B0(read_6973());      /* sub_66A2 */
    R(0xEADBu) = 2u; R(0xEADCu) = 0u; ww_ram(0xEADDu, 0u);
    while (R(0xEADBu) != 0x1Cu) s_6774(read_6973());      /* sub_6766 */
    ww_ram(0xEADDu, 0u);
    s_69AA();
}

/* sub_65E1: puntero de stream - tabla en (0xEAD7) (=0x7CF2), fallback ROM */
static uint16_t s_65E1(uint16_t off)
{
    uint16_t p = (uint16_t)(rw_ram(0xEAD7u) + off);
    uint16_t v = (uint16_t)(mem_rb(p) | ((uint16_t)mem_rb((uint16_t)(p + 1u)) << 8));
    if (v == 0u) {
        p = (uint16_t)(0x7CF2u + off);
        v = (uint16_t)(mem_rb(p) | ((uint16_t)mem_rb((uint16_t)(p + 1u)) << 8));
    }
    return v;
}

/* ===== HUD dinamico al cargar sala (sub_5DC0 / sub_5E01 / sub_5E5C) ===== */
static void s_5DDE(uint16_t byte_addr, uint16_t name_off)
{
    uint8_t v = R(byte_addr);
    hal_vdp_write_vram((uint16_t)(NAME_BASE + name_off),      (uint8_t)((v >> 4) + 0x47u));
    hal_vdp_write_vram((uint16_t)(NAME_BASE + name_off + 1u), (uint8_t)((v & 0x0Fu) + 0x47u));
}
static void s_5DC0(void)
{
    for (int i = 0; i < 3; i++) s_5DDE((uint16_t)(0xE33Du + i), (uint16_t)(0x22u + i * 2));
    for (int i = 0; i < 3; i++) s_5DDE((uint16_t)(0xE340u + i), (uint16_t)(0x2Au + i * 2));
}
/* sub_5E01: iconos de llave (fila 2, col 3): por color, count/5 iconos
 * "quinteto" (tile par) + count%5 iconos unidad (tile impar); resto en 0 */
static void s_5E01(void)
{
    uint16_t a = (uint16_t)(NAME_BASE + 0x43u);
    int b = 0x0E;
    uint8_t c = 1u;
    uint16_t de = 0xE337u;
    while (c != 0x0Du) {
        uint8_t cnt = R(de);
        uint8_t d = (uint8_t)(cnt / 5u + 1u), e = (uint8_t)(cnt % 5u + 1u);
        for (;;) {
            if (--d == 0u) break;
            hal_vdp_write_vram(a++, c);
            if (--b == 0) return;
        }
        c++;
        for (;;) {
            if (--e == 0u) break;
            hal_vdp_write_vram(a++, c);
            if (--b == 0) return;
        }
        c++; de++;
    }
    while (b-- > 0) hal_vdp_write_vram(a++, 0x00u);
}
/* sub_5E5C: corazones (fila 3, col 3): vidas-1 (tope 14), resto en 0 */
static void s_5E5C(void)
{
    uint8_t lives = R(0xE336u);
    uint16_t a = (uint16_t)(NAME_BASE + 0x63u);
    int b = 0x0E;
    uint8_t n;
    if (lives == 0u) return;
    n = (uint8_t)(lives - 1u);
    if (n > 0x0Eu) n = 0x0Eu;
    for (uint8_t i = 0; i < n && b > 0; i++, b--) hal_vdp_write_vram(a++, 0x0Du);
    while (b-- > 0) hal_vdp_write_vram(a++, 0x00u);
}

/* ===== HUD estatico del boot (parte de sub_4D52 / sub_64C3 / sub_4ECA) ===== */
static void hud_fill(uint8_t col, uint8_t row, uint8_t w, uint8_t hgt, uint8_t tile)
{
    for (uint8_t r = 0; r < hgt; r++)
        for (uint8_t c = 0; c < w; c++) {
            hal_vdp_write_vram((uint16_t)(NAME_BASE + (uint16_t)(row + r) * 32u + col + c), tile);
            if (tile != 0u) tile++;
        }
}
/* sub_629D / sub_62B0: cadena ASCII (fin 0x40) con base de tiles */
static void draw_string(uint8_t col, uint8_t row, uint16_t addr, uint8_t base)
{
    for (;;) {
        uint8_t ch = rb(addr++);
        uint8_t t;
        if (ch == 0x40u) return;
        if (ch == 0x20u) t = 0u;
        else {
            uint8_t a2 = ch;
            if (a2 < 0x3Au) a2 = (uint8_t)(a2 - 0x30u + 0x5Du);
            t = (uint8_t)(a2 - 0x41u + base);
        }
        if (col < 32u)
            hal_vdp_write_vram((uint16_t)(NAME_BASE + (uint16_t)row * 32u + col), t);
        col++;
    }
}

/* ===== estado base de VRAM al iniciar la partida ===== */
/* sub_64AB simplificado: count tiles desde el descriptor a (tercio<<8|tile) */
static void load_desc(uint16_t desc, uint16_t dest, uint8_t count)
{
    uint16_t src = rwrd(desc);
    uint16_t voff = (uint16_t)(dest * 8u);
    for (uint8_t i = 0; i < count; i++) s_6CD9(&voff, &src);
}
/* sub_4EA2: 12 tiles de llave (2 por color) al tercio 0, tiles 0x01-0x0C;
 * patron de (0x7BD2) (se rebobina por color), color de la tabla 0x6DC9 */
static void s_4EA2(void)
{
    for (int k = 0; k < 6; k++) {
        uint16_t src  = rwrd(0x7BD2u);
        uint16_t voff = (uint16_t)((1u + (uint16_t)k * 2u) * 8u);
        uint8_t  colb = rb((uint16_t)(0x6DC9u + k));
        s_6CCA_tile(&voff, &src, colb);
        s_6CCA_tile(&voff, &src, colb);
    }
}
/* tiles del boot (sub_4D52 0x4DCB-0x4E0E): tercio 0 + font/digitos tercio 1 */
static void boot_tiles(void)
{
    s_4EA2();
    load_desc(0x7BD4u, 0x000Du,  1u);   /* corazon   */
    load_desc(0x7BC6u, 0x000Eu, 28u);   /* mapa HUD  */
    load_desc(0x7BC0u, 0x002Au, 28u);   /* logo HUD  */
    load_desc(0x7BD6u, 0x0046u,  1u);   /* separador */
    load_desc(0x7BCEu, 0x0047u, 10u);   /* digitos   */
    load_desc(0x7BC8u, 0x0051u,  4u);   /* Hi SCORE  */
    load_desc(0x7BCAu, 0x0055u,  2u);   /* Key       */
    load_desc(0x7BCCu, 0x0057u,  2u);   /* Life      */
    load_desc(0x7BD0u, 0x0059u, 26u);   /* font A-Z  */
    load_desc(0x7BD0u, 0x0101u, 28u);   /* sub_4E8E: tercio 1 */
    load_desc(0x7BCEu, 0x011Du, 10u);
}
/* Reproduce la VRAM al arrancar la partida (la secuencia real que vivio la
 * captura de fixtures): INIGRP -> boot (sub_4D52) -> titulo (sub_4AE2 +
 * cargas de sub_4A4A) -> game start (sub_4D52 de nuevo). */
void rl_boot_vram(void)
{
    hal_vdp_fill_vram(PAT_BASE, 0x00u, 0x1800u);    /* INIGRP */
    hal_vdp_fill_vram(COL_BASE, 0x01u, 0x1800u);
    boot_tiles();
    hal_vdp_fill_vram(0x0400u, 0x00u, 0x1400u);     /* sub_4AE2 (titulo) */
    hal_vdp_fill_vram(0x2400u, 0x11u, 0x1400u);
    for (uint16_t t = 0; t < 3u; t++) {             /* logo del titulo x3 tercios */
        load_desc(0x7BC2u, (uint16_t)((t << 8) | 0x73u), 0x23u);
        load_desc(0x7BC4u, (uint16_t)((t << 8) | 0x96u), 0x23u);
    }
    for (uint16_t t = 1; t <= 2u; t++) {            /* font+digitos tercios 1-2 */
        load_desc(0x7BD0u, (uint16_t)((t << 8) | 0x01u), 0x1Cu);
        load_desc(0x7BCEu, (uint16_t)((t << 8) | 0x1Du), 0x0Au);
    }
    boot_tiles();                                   /* game start: sub_4D52 */
}

/* ===== helpers para el motor (capa maqueta) ===== */
void rl_cell_gfx(int srow, int scol, uint8_t out[16])
{
    uint8_t tile = hal_vdp_read_vram((uint16_t)(NAME_BASE + srow * 32 + scol));
    uint16_t off = (uint16_t)((srow / 8) * 0x800 + tile * 8u);
    for (int r = 0; r < 8; r++) {
        out[r * 2]     = hal_vdp_read_vram((uint16_t)(PAT_BASE + off + r));
        out[r * 2 + 1] = hal_vdp_read_vram((uint16_t)(COL_BASE + off + r));
    }
}
void rl_cell_blank(int srow, int scol)
{
    if (srow < 0 || srow >= 24 || scol < 0 || scol >= 32) return;
    hal_vdp_write_vram((uint16_t)(NAME_BASE + srow * 32 + scol), 0x00u);
}

/* ==========================================================================
 * API
 * ========================================================================== */
void rl_reset(void)
{
    memset(ram, 0, sizeof ram);
    ww_ram(0xEAD7u, 0x7CF2u);     /* puntero a la tabla de salas (boot 0x4CEC) */
    /* sub_4D52 (estado): */
    R(0xE324u) = 0x05u;           /* vidas */
    R(0xE320u) = 0x70u;           /* sala inicial */
    R(0xE333u) = 0x06u;
    R(0xE321u) = 0x01u;
    R(0xEACAu) = 0x70u;
    R(0xE322u) = 0x00u;           /* col inicial del jugador */
    R(0xE323u) = 0x11u;           /* fila inicial */
    /* persistencia toda en 1 = todo presente */
    memset(&R(0xE00Du), 0xFF, 0x2C6);

    /* HUD estatico (sub_4D52 0x4E12-0x4E4D + overlay NO/MAP 0x4E60): */
    hud_fill(0x11u, 0u, 7u, 4u, 0x0Eu);   /* mapa  7x4 col 17 */
    hud_fill(0x18u, 0u, 7u, 4u, 0x2Au);   /* logo  7x4 col 24 (sub_4ECA) */
    for (uint8_t r = 0; r < 4u; r++)      /* separador col 31 */
        hal_vdp_write_vram((uint16_t)(NAME_BASE + (uint16_t)r * 32u + 31u), 0x46u);
    hud_fill(1u, 0u, 3u, 1u, 0x52u);      /* SCORE */
    hud_fill(9u, 0u, 4u, 1u, 0x51u);      /* Hi SCORE */
    hud_fill(1u, 2u, 2u, 1u, 0x55u);      /* Key */
    hud_fill(1u, 3u, 2u, 1u, 0x57u);      /* Life */
    draw_string(0x13u, 1u, 0x6472u, 0x59u);   /* "N O" */
    draw_string(0x13u, 2u, 0x6476u, 0x59u);   /* "MAP" */
}

void rl_load_room(uint8_t room)
{
    uint16_t off, ptr;

    R(0xE320u) = room;
    R(0xEAF3u) = 0u;
    /* sub_5327: blanquea sprites 8-10 - sin efecto en fondo/tablas */

    /* sub_659B */
    memset(&R(0xE946u), 0, 0x120);
    memset(&R(0xE346u), 0, 0x150);
    R(0xEA66u) = 0x72u; R(0xEA67u) = 0x1Au; R(0xEA68u) = 0x00u;

    /* blank (descriptor 0x7BDA) al tile 0 de los 3 tercios */
    {
        uint16_t src0 = rwrd(0x7BDAu);
        for (int t = 0; t < 3; t++) {
            uint16_t v = (uint16_t)(t * 0x800u), s = src0;
            s_6CD9(&v, &s);
        }
    }

    /* sub_65C4: limpia name table filas 4-23 + colmap E496-E945 */
    hal_vdp_fill_vram((uint16_t)(NAME_BASE + 0x80u), 0x00u, 0x280u);
    memset(&R(0xE496u), 0, 0x4B0);

    /* LDIR 0xE322 -> 0xE334 (0x11 bytes: estado de sala activo) */
    memcpy(&R(0xE334u), &R(0xE322u), 0x11);

    /* HUD dinamico (score / llaves / corazones) */
    s_5DC0(); s_5E01(); s_5E5C();

    /* streams de la sala: 0x7CF2 + 42*fila + 4*col, offsets +0 / +4 / +2 */
    off = (uint16_t)(42u * (room >> 4) + 4u * (room & 0x0Fu));

    ptr = s_65E1(off);                                  /* banda superior */
    R(0xEADBu) = 0x00u; R(0xEADCu) = 0x00u;
    ww_ram(0xEAD9u, ptr); ww_ram(0xEADDu, 0u);
    s_6616_loop();

    ptr = s_65E1((uint16_t)(off + 4u));                 /* banda inferior */
    R(0xEADBu) = 0x1Cu; R(0xEADCu) = 0x00u;
    ww_ram(0xEAD9u, ptr); ww_ram(0xEADDu, 0u);
    s_6616_loop();

    s_6671(s_65E1((uint16_t)(off + 2u)));               /* cuerpo + objetos */

    /* sub_6F45/sub_6F27: sprites del jugador - sin efecto en fondo.
     * Musica de sala (sub_7769 con flags 0xE343/0xE344) - Fase 6. */
    R(0xEAF6u) = 0u; R(0xEAF7u) = 0u; R(0xEAF8u) = 0u;
}
