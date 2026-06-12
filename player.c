/*
 * THE CASTLE - JUGADOR fiel (Fase 3).
 * ====================================
 * Port instruccion-a-instruccion de:
 *   sub_4064 (0x4064): limpia EACB/EACC en frames pares (bit0 de 0xEAC9).
 *   sub_50E8 (0x50E8): poll de input - escribe EACB (GTSTCK) y EACC (GTTRIG)
 *                      SOLO en frames pares (input efectivo a 30Hz).
 *   sub_40BB (0x40BB): computa los FLAGS de movimiento en HL a partir del
 *                      input y de probes de pares de celdas sobre el colmap
 *                      0xE496 en (0xE334 col, 0xE335 fila). No mueve.
 *   sub_4210 (0x4210): empaqueta H interno -> D de salida:
 *                      bit0=mover horiz, bit1=derecha, bit2=mover vert,
 *                      bit3=arriba, bit4=paso doble (rampa).
 *   sub_6F5C (0x6F5C): APLICA el movimiento: frames pares = medio paso
 *                      (celda +-1 con pixel a mitad de camino, deteccion de
 *                      salida de sala en 0xEAE1); impares = snap a celda
 *                      (sub_6F45) y celda extra si paso doble. Elige el frame
 *                      de animacion (sub_6F94; patron sprite = frame*12).
 *
 * Fase de salto 0xEAD6: 0=suelo; 1..8 subiendo (4px/frame); 9..0x10 flote;
 * 0x11 = cayendo. Trigger soltado en el aire -> 0x11. Piso + fase>=0x11 -> 0.
 *
 * Bits del colmap: 0x80 solido, 0x0A "pasable especial", 0x08 celda-objeto,
 * 0x04 trigger coleccionable, 0x01 rampa (pendiente en 0xE6EE & 3).
 *
 * Pendiente (no afecta a las trazas): empuje de bloques (sub_4248/4273 via
 * trampolin 0xEAFA) y apertura de puertas (sub_42FF/4325/758C) - Fase 5;
 * escaleras (bloque 0x4586+) - se agrega al portar sub_434A/442D.
 *
 * Validado frame a frame contra tests/fixtures/traces/ (suite 'player').
 */
#include <stdint.h>
#include "player.h"
#include "room_loader.h"
#include "hal.h"
#include "game.h"

int g_plr_px, g_plr_py, g_plr_frame;

/* ===== sub_6F27 + sub_6EE1/sub_6EAE: sprites del jugador (planos 8-10) =====
 * patron = frame*3+plano (el attr guarda patron*4, sprites 16x16); color de
 * la tabla ROM en (0x7CF0) indexada por patron, con tintes de sala: si
 * (0xE343) el rojo (8) parpadea a blanco; si (0xE344) a verde. El VDP
 * emulado los renderiza nativo desde la attr table 0x1B00. */
static uint8_t rom_rb_p(uint16_t a)
{
    uint32_t o = (uint32_t)a - 0x4000u;
    return (g_rom && o < g_rom_size) ? g_rom[o] : 0xFFu;
}
static void s_6F27(uint8_t frame);
static uint8_t s_4273(uint8_t b, uint8_t c, int dir);

/* ===== acceso al espejo RAM del loader ===== */
static uint8_t rr(uint16_t a)            { return rl_ram_rb(a); }
static void    wr(uint16_t a, uint8_t v) { rl_ram_wb(a, v); }

/* celda del colmap 0xE496 (lectura CRUDA, sin bounds: el Z80 lee igual y las
 * filas >0x13 caen en la tabla 0xE6EE contigua - mismo layout en el espejo) */
static uint8_t cm(uint8_t b, uint8_t c)
{
    return rr((uint16_t)(0xE496u + (uint16_t)c * 30u + b));
}
static uint8_t e6ee(uint8_t b, uint8_t c)
{
    return rr((uint16_t)(0xE6EEu + (uint16_t)c * 30u + b));
}

/* ===== probes (los wrappers de pares de celdas de 0x44C2-0x4A49) =====
 * Convencion: devuelven 1 = NZ del Z80, 0 = Z. */

/* sub_4515: par horizontal (b,c),(b+1,c) & 0x80. NZ = piso/solido presente.
 * fila 0xFF -> NZ; fila > 0x13 -> Z (debajo de la pantalla = libre). */
static uint8_t p_4515(uint8_t b, uint8_t c)
{
    if (c == 0xFFu) return 1;
    if (c > 0x13u) return 0;
    if (cm(b, c) & 0x80u) return 1;
    if (cm((uint8_t)(b + 1u), c) & 0x80u) return 1;
    return 0;
}

/* sub_4A05 (ap=0) / sub_4A06 (ap=A'): celda (b,c). NZ = bloqueada.
 * Fuera de rango (b>0x1D o c>0x13) -> NZ. ap!=0: si celda & 0x0A -> Z
 * (pasable especial); sino & 0x80. */
static uint8_t p_4A06(uint8_t ap, uint8_t b, uint8_t c)
{
    if (b > 0x1Du || c > 0x13u) return 1;
    if (ap) {
        if (cm(b, c) & 0x0Au) return 0;
    }
    return (cm(b, c) & 0x80u) ? 1 : 0;
}

/* sub_44EF: par vertical (b,c),(b,c+1) via 4A06. NZ = bloqueado. */
static uint8_t p_44EF(uint8_t ap, uint8_t b, uint8_t c)
{
    if (p_4A06(ap, b, c)) return 1;
    if (p_4A06(ap, b, (uint8_t)(c + 1u))) return 1;
    return 0;
}

/* sub_41DF(A'): laterales en la fila c: (b-1,c) bloqueada -> RES 1,H (izq);
 * (b+2,c) bloqueada -> RES 0,H (der). */
static void s_41DF(uint8_t ap, uint8_t b, uint8_t c, uint8_t *h)
{
    if (p_4A06(ap, (uint8_t)(b - 1u), c)) *h &= (uint8_t)~0x02u;
    if (p_4A06(ap, (uint8_t)(b + 2u), c)) *h &= (uint8_t)~0x01u;
}

/* sub_4235: laterales con modo segun estado: en el aire (fase!=0) o cayendo
 * (bit3 H) -> modo 0 (solo & 0x80); en suelo -> modo 1 (0x0A pasable). */
static void s_4235(uint8_t b, uint8_t c, uint8_t *h)
{
    uint8_t a;
    if (rr(0xEAD6u) != 0u)      a = 0u;
    else if (*h & 0x08u)        a = 0u;
    else                        a = 1u;
    s_41DF(a, b, c, h);
}

/* sub_4A38: si la celda es rampa (bit 0x01) devuelve su byte 0xE6EE, sino 0 */
static uint8_t p_4A38(uint8_t b, uint8_t c)
{
    if (!(cm(b, c) & 0x01u)) return 0;
    return e6ee(b, c);
}

/* sub_47F5: pendiente bajo los pies: suma +-1 por celda (b,c),(b+1,c) segun
 * bits 0/1 del byte E6EE; 2 -> 1, -2 -> -1. */
static uint8_t s_47F5(uint8_t b, uint8_t c)
{
    uint8_t h = 0;
    for (int i = 0; i < 2; i++) {
        uint8_t a = (uint8_t)(p_4A38(b, c) & 0x03u);
        if (a) {
            if (a & 0x01u) h++;
            else           h--;
        }
        b++;
    }
    if (h == 0x02u) h = 0x01u;
    if (h == 0xFEu) h = 0xFFu;
    return h;
}

/* sub_41F6: intencion horizontal -> bits de H:
 * 0xFE = doble-izq (SET 5 y trata como -1); 0xFF = izq (SET 1);
 * 0x02 = doble-der (SET 4 y trata como +1); 0x01 = der (SET 0). */
static void s_41F6(uint8_t a, uint8_t *h)
{
    if (a == 0xFEu) { *h |= 0x20u; a = 0xFFu; }
    if (a == 0xFFu) *h |= 0x02u;
    if (a == 0x02u) { *h |= 0x10u; a = 0x01u; }
    if (a == 0x01u) *h |= 0x01u;
}

/* sub_4210: H interno -> D de salida para 6F5C */
static uint8_t s_4210(uint8_t h)
{
    uint8_t out = 0;
    if (h & 0x30u) out |= 0x10u;                       /* paso doble */
    if (h & 0x0Cu) { out |= 0x04u; if (h & 0x04u) out |= 0x08u; }  /* vert/up */
    if (h & 0x03u) { out |= 0x01u; if (h & 0x01u) out |= 0x02u; }  /* horiz/der */
    return out;
}

/* ==========================================================================
 * sub_40BB - flags del frame. Devuelve D (movimiento) y E (animacion = L).
 * ========================================================================== */
static void s_40BB(uint8_t *outD, uint8_t *outE)
{
    uint8_t h = 0, l = 0;          /* HL = 0x0000 */
    uint8_t d = 0, e = 0xFFu;      /* DE = 0x00FF */
    uint8_t b = rr(0xE334u);
    uint8_t c = (uint8_t)(rr(0xE335u) + 2u);   /* fila bajo los pies */
    uint8_t a, floor1;

    /* piso + fase de salto/caida (0xEAD6) */
    floor1 = p_4515(b, c);
    a = rr(0xEAD6u);
    if (!floor1) { if (a == 0u) a = 0x11u; }   /* sin piso: empezar a caer */
    else         { if (a >= 0x11u) a = 0u; }   /* aterrizo */
    wr(0xEAD6u, a);

    /* stick: 2-4 = derecha, 6-8 = izquierda */
    a = rr(0xEACBu);
    if (a >= 2u && a < 5u)      { d = 0x01u; l = 0x03u; }
    else if (a >= 6u)           { d = 0xFFu; l = 0x01u; }

    /* trigger: mantener = subir fase; soltar en el aire = caer (0x11) */
    {
        uint8_t trig = rr(0xEACCu);
        a = rr(0xEAD6u);
        if (trig == 0u) {
            if (a != 0u) { a = 0x11u; wr(0xEAD6u, a); }
        } else {
            a++;
            wr(0xEAD6u, a);
            if (a < 0x11u) e = (a < 9u) ? 0x01u : 0x00u;  /* sube / flota */
        }
    }

    /* 4121: intencion horizontal (en el aire = stick; en suelo += pendiente) */
    a = rr(0xEAD6u);
    {
        uint8_t mv = (a != 0u) ? d : (uint8_t)(s_47F5(b, c) + d);
        s_41F6(mv, &h);
    }
    if (e == 0x01u) h |= 0x04u;        /* SET 2,H: subiendo */
    if (e == 0xFFu) h |= 0x08u;        /* SET 3,H: cayendo (default) */
    if (p_4515(b, c)) h &= (uint8_t)~0x08u;   /* piso -> no cae */
    if (h & 0x08u) s_41DF(0u, b, c, &h);      /* cayendo: laterales al pie */

    /* 414B: paso doble bloqueado por los laterales del cuerpo (filas c-2,c-1) */
    c -= 2u; b += 2u;
    if (p_44EF(0u, b, c)) h &= (uint8_t)~0x10u;   /* doble-der */
    b -= 3u;
    if (p_44EF(0u, b, c)) h &= (uint8_t)~0x20u;   /* doble-izq */
    b += 1u;

    /* 4162: si NO esta cayendo: laterales cuerpo-arriba + techo */
    if (!(h & 0x08u)) {
        s_4235(b, c, &h);
        c -= 1u;                                   /* fila sobre la cabeza */
        if (!(h & 0x01u)) {                        /* sin intencion derecha */
            if (p_4A06(0u, b, c)) h &= (uint8_t)~0x04u;   /* techo (col) */
            b -= 1u;
            if ((h & 0x02u) && (h & 0x04u)) {
                if (p_4A06(0u, b, c)) h &= (uint8_t)~(0x04u | 0x02u);
            }
            b += 1u;
        }
        if (!(h & 0x02u)) {                        /* sin intencion izquierda */
            b += 1u;
            if (p_4A06(0u, b, c)) h &= (uint8_t)~0x04u;   /* techo (col+1) */
            b += 1u;
            if ((h & 0x01u) && (h & 0x04u)) {
                if (p_4A06(0u, b, c)) h &= (uint8_t)~(0x04u | 0x01u);
            }
            b -= 2u;
        }
        c += 1u;                                   /* 41A9 */
    }
    c += 1u;                                       /* 41AA: fila inferior cuerpo */
    if (!(h & 0x04u)) s_4235(b, c, &h);
    c -= 1u;

    /* 41B2: flag de animacion "en el aire" + interaccion puerta/bloque */
    a = rr(0xEAD6u);
    if (a != 0u)            l |= 0x04u;
    else if (h & 0x08u)     l |= 0x04u;
    else {
        /* sub_4248/sub_425A (solo frames PARES, 5D5D): primero puerta
         * (sub_42FF -> rl_door_press), si no, empuje de bloque (sub_4273).
         * Cualquiera de los dos en "bloqueado" corta el avance del frame. */
        if ((rr(0xEAC9u) & 0x01u) == 0u) {
            if (h & 0x01u) {
                if (rl_door_press((uint8_t)(b + 2u), c))      h &= (uint8_t)~0x01u;
                else if (!s_4273(b, c, +1))                   h &= (uint8_t)~0x01u;
            }
            if (h & 0x02u) {
                if (rl_door_press((uint8_t)(b - 1u), c))      h &= (uint8_t)~0x02u;
                else if (!s_4273(b, c, -1))                   h &= (uint8_t)~0x02u;
            }
        }
    }

    *outD = s_4210(h);
    *outE = l;
}

/* ==========================================================================
 * sub_6F45 / sub_6F94 / sub_6F5C - aplicador de movimiento + animacion
 * ========================================================================== */
static void s_6F45_pixel(void)
{
    g_plr_px = ((int)rr(0xE334u) + 1) * 8;
    g_plr_py = ((int)rr(0xE335u) + 4) * 8 - 1;
}

static void s_6F94_anim(uint8_t e)
{
    uint8_t a = rr(0xEAD6u);
    uint8_t fc = rr(0xEAC9u);
    uint8_t frame;
    if (a != 1u && ((e & 0x10u) || (e & 0x04u))) {     /* en el aire */
        if (!(e & 0x01u))      frame = 5u;             /* salto neutro */
        else if (e & 0x02u)    frame = 9u;             /* salto derecha */
        else                   frame = 4u;             /* salto izquierda */
    } else {
        if (!(e & 0x01u))      frame = 0u;             /* parado */
        else if (!(e & 0x02u)) {                       /* caminando izq */
            if (fc & 0x01u)      frame = 1u;
            else if (fc & 0x02u) frame = 3u;
            else                 frame = 2u;
        } else {                                       /* caminando der */
            if (fc & 0x01u)      frame = 6u;
            else if (fc & 0x02u) frame = 8u;
            else                 frame = 7u;
        }
    }
    g_plr_frame = frame;
}

/* sub_6FF0: camino de frames PARES: salidas de sala + medio paso */
static void s_6FF0(uint8_t d, uint8_t e)
{
    uint8_t col = rr(0xE334u);
    uint8_t row;

    /* salida IZQUIERDA: col 0 + caminando izq (E bit0 sin bit1) */
    if (col == 0u && (e & 0x01u) && !(e & 0x02u)) {
        if ((rr(0xE320u) & 0x0Fu) != 0u) {
            s_6F45_pixel();
            g_plr_frame = 1u;
            wr(0xEAE1u, 7u);
            return;
        }
    }
    /* salida DERECHA: col 0x1C + caminando der */
    if (col == 0x1Cu && (e & 0x01u) && (e & 0x02u)) {
        if ((rr(0xE320u) & 0x0Fu) != 9u) {
            s_6F45_pixel();
            g_plr_frame = 6u;
            wr(0xEAE1u, 3u);
            return;
        }
    }
    /* 7039: salidas verticales */
    row = rr(0xE335u);
    if (row == 0xFEu) { wr(0xEAE1u, 1u); return; }     /* arriba  */
    if (row == 0x14u) { wr(0xEAE1u, 5u); return; }     /* abajo   */

    /* medio paso: H/L en unidades de 4px */
    {
        uint8_t b = col, c = row;
        int hh = ((int)col + 1) * 2;
        int ll = ((int)row + 4) * 2;
        if (d & 0x01u) {
            if (d & 0x02u) { b++; hh++; if (d & 0x10u) hh++; }
            else           { b--; hh--; if (d & 0x10u) hh--; }
        }
        if (d & 0x04u) {
            if (d & 0x08u) { c--; ll--; }              /* sube */
            else           { c++; ll++; }              /* baja */
        }
        wr(0xE334u, b);
        wr(0xE335u, c);
        g_plr_px = hh * 4;
        g_plr_py = ll * 4 - 1;
    }
    s_6F94_anim(e);
}

static void s_6F5C(uint8_t d, uint8_t e)
{
    /* (0xEAF8) = disparo de sonido de salto (fase 1) - Fase 6 */
    if ((rr(0xEAC9u) & 0x01u) == 0u) {     /* frame PAR -> 6FF0 */
        s_6FF0(d, e);
        return;
    }
    /* frame IMPAR: celda extra solo con paso doble; pixel = celda exacta */
    if ((d & 0x10u) && (d & 0x01u)) {
        uint8_t col = rr(0xE334u);
        col = (d & 0x02u) ? (uint8_t)(col + 1u) : (uint8_t)(col - 1u);
        wr(0xE334u, col);
    }
    s_6F45_pixel();
    s_6F94_anim(e);
}

/* ==========================================================================
 * MOTOR DE OBJETOS COLL (bloques empujables / rollers / trampas 0x34).
 * Port fiel de: sub_434A (driver por frame) + sub_4820 (gravedad/derrape) +
 * sub_710B (movedor en 2 fases con tiles de transicion: el alocador cargo
 * 16 tiles por bloque = 4 finales + 6 medio-paso horizontal (deltas 4-9) +
 * 6 medio-paso vertical (10-15)) + sub_4273 (empuje del jugador, trampolin
 * 0xEAFA) + sub_468A (lookup de objeto por celda) + sub_42F5/sub_5D47
 * (aplastar BATs). Los escritores Z80 (70B6/70C2/70D5/7103) MUTAN H,L,D del
 * caller - se modela con variables corridas.
 * ========================================================================== */

/* sub_468A: objeto en la celda (b,c). mode 2 = solo COLL (E386), mode 4 =
 * solo BAT (E416). Devuelve 1 (NZ) con *ix/*slot si lo encontro. */
static uint8_t s_468A(uint8_t mode, uint8_t b, uint8_t c,
                      uint16_t *ix, uint8_t *slot)
{
    uint8_t v, s;
    if (b >= 0x1Eu || c >= 0x14u) return 0;
    v = cm(b, c);
    if (!(v & 0x08u)) return 0;
    s = (uint8_t)(e6ee(b, c) >> 3);
    if (!(s & 0x10u)) {                     /* tabla COLL */
        if (!(mode & 0x02u)) return 0;
        if (ix)   *ix = (uint16_t)(0xE386u + s * 5u);
        if (slot) *slot = s;
        return 1;
    }
    if (!(mode & 0x04u)) return 0;          /* tabla BAT */
    s &= 0x0Fu;
    if (ix)   *ix = (uint16_t)(0xE416u + s * 5u);
    if (slot) *slot = s;
    return 1;
}

/* sub_49DC: destino de empuje/caida libre? NZ(1)=bloqueado. Un BAT en el
 * destino cuenta como libre (sera aplastado por sub_42F5). */
static uint8_t p_49DC(uint8_t b, uint8_t c)
{
    if (b >= 0x1Eu) return 1;
    if (c >= 0x14u) return 0;
    if (s_468A(0x04u, b, c, 0, 0)) return 0;
    return (cm(b, c) & 0x30u) ? 1 : 0;
}

/* sub_44C2(A'): par vertical &0x30 en (b,c),(b,c+1); modo para (b,c+2):
 * 2 = nada mas, 0 = &0x40 debe estar, otro = &0x20 debe estar. NZ=bloqueado */
static uint8_t p_44C2(uint8_t ap, uint8_t b, uint8_t c)
{
    if (b > 0x1Du) return 1;
    if (cm(b, c) & 0x30u) return 1;
    if (cm(b, (uint8_t)(c + 1u)) & 0x30u) return 1;
    if (ap == 2u) return 0;
    if (ap == 0u) return (cm(b, (uint8_t)(c + 2u)) & 0x40u) ? 0 : 1;
    return (cm(b, (uint8_t)(c + 2u)) & 0x20u) ? 0 : 1;
}

/* sub_5D47: mata el objeto (entry): slot[0]=0, blanquea su 2x2 y dispara el
 * sonido (0xEAF6=0x32). (El "puff" de sprite de sub_5D63 - Fase 4.) */
static void s_5D47(uint16_t ix)
{
    uint8_t h = rr((uint16_t)(ix + 2u)), l = rr((uint16_t)(ix + 3u));
    wr(ix, 0u);
    rl_cell_put(h, l, 0u, 0u, 0u);
    rl_cell_put((uint8_t)(h + 1u), l, 0u, 0u, 0u);
    rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), 0u, 0u, 0u);
    rl_cell_put(h, (uint8_t)(l + 1u), 0u, 0u, 0u);
    wr(0xEAF6u, 0x32u);
}

/* sub_42F5: si hay un BAT en (b,c) lo aplasta */
static void s_42F5(uint8_t b, uint8_t c)
{
    uint16_t ix;
    if (s_468A(0x04u, b, c, &ix, 0)) s_5D47(ix);
}

/* sub_710B: movedor/redibujador del objeto COLL segun sus flags (entry[4]):
 * frame PAR: actualiza la celda logica y dibuja el grafico de TRANSICION
 * (3x2 deltas 4-9 horizontal / 2x3 deltas 10-15 vertical);
 * frame IMPAR: dibuja el 2x2 final (deltas 0-3) y blanquea lo que quedo. */
static void s_710B(uint16_t ix, uint8_t slot)
{
    uint8_t code = rr((uint16_t)(ix + 1u));
    uint8_t h = rr((uint16_t)(ix + 2u)), l = rr((uint16_t)(ix + 3u));
    uint8_t f = rr((uint16_t)(ix + 4u));
    uint8_t e = slot;
    if (code == 0x34u) {
        /* sub_61F5: trampa (particulas) - Fase 4 */
    }
    if (rr(0xEAC9u) & 0x01u) {
        /* IMPAR: paso final */
        if (f & 0x01u) {
            if (!(f & 0x02u)) {
                /* 7126 izquierda: 70B6 deja H=h+1,L=l+1 -> blanquea col h+2 */
                rl_cell_put(h, l, code, 0u, e);
                rl_cell_put((uint8_t)(h + 1u), l, code, 1u, e);
                rl_cell_put(h, (uint8_t)(l + 1u), code, 2u, e);
                rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, 3u, e);
                rl_cell_put((uint8_t)(h + 2u), (uint8_t)(l + 1u), 0u, 0u, e);
                rl_cell_put((uint8_t)(h + 2u), l, 0u, 0u, e);
            } else {
                /* 7137 derecha: blanquea col h-1 y dibuja el final */
                rl_cell_put((uint8_t)(h - 1u), l, 0u, 0u, e);
                rl_cell_put((uint8_t)(h - 1u), (uint8_t)(l + 1u), 0u, 0u, e);
                rl_cell_put(h, l, code, 0u, e);
                rl_cell_put((uint8_t)(h + 1u), l, code, 1u, e);
                rl_cell_put(h, (uint8_t)(l + 1u), code, 2u, e);
                rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, 3u, e);
            }
        } else if (f & 0x04u) {
            /* 7155 abajo: blanquea fila l-1 y dibuja el final */
            rl_cell_put(h, (uint8_t)(l - 1u), 0u, 0u, e);
            rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l - 1u), 0u, 0u, e);
            rl_cell_put(h, l, code, 0u, e);
            rl_cell_put((uint8_t)(h + 1u), l, code, 1u, e);
            rl_cell_put(h, (uint8_t)(l + 1u), code, 2u, e);
            rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, 3u, e);
        }
        return;
    }
    /* PAR: actualizar celda + grafico de transicion */
    if (f & 0x01u) {
        uint8_t hg;
        if (!(f & 0x02u)) { h--; wr((uint16_t)(ix + 2u), h); hg = h; }       /* izq */
        else              { wr((uint16_t)(ix + 2u), (uint8_t)(h + 1u)); hg = h; } /* der */
        /* sub_70C2: 3x2 deltas 4-9 en (hg..hg+2, l..l+1) */
        rl_cell_put(hg, l, code, 4u, e);
        rl_cell_put((uint8_t)(hg + 1u), l, code, 5u, e);
        rl_cell_put((uint8_t)(hg + 2u), l, code, 6u, e);
        rl_cell_put(hg, (uint8_t)(l + 1u), code, 7u, e);
        rl_cell_put((uint8_t)(hg + 1u), (uint8_t)(l + 1u), code, 8u, e);
        rl_cell_put((uint8_t)(hg + 2u), (uint8_t)(l + 1u), code, 9u, e);
    } else if (f & 0x04u) {
        wr((uint16_t)(ix + 3u), (uint8_t)(l + 1u));                          /* baja */
        /* sub_70D5: 2x3 deltas 10-15 en (h..h+1, l..l+2) */
        rl_cell_put(h, l, code, 10u, e);
        rl_cell_put((uint8_t)(h + 1u), l, code, 11u, e);
        rl_cell_put(h, (uint8_t)(l + 1u), code, 12u, e);
        rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, 13u, e);
        rl_cell_put(h, (uint8_t)(l + 2u), code, 14u, e);
        rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 2u), code, 15u, e);
    }
}

/* sub_4820: recomputa los flags del objeto (solo frames PARES):
 * sin piso 2 celdas abajo -> CAE (aplastando BATs); sino derrape por
 * pendiente (sub_47F5) limitado por las paredes (sub_44C2). */
static uint8_t s_4820(uint8_t f, uint8_t b, uint8_t c)
{
    uint8_t h = f;
    if (rr(0xEAC9u) & 0x01u) return h;          /* impar: sin cambios */
    c += 2u;
    if (!p_49DC(b, c) && !p_49DC((uint8_t)(b + 1u), c)) {
        s_42F5(b, c); s_42F5((uint8_t)(b + 1u), c);
        h &= (uint8_t)~0x01u;                   /* RES 0 */
        h |= 0x04u;                             /* SET 2: cae */
        h &= (uint8_t)~0x08u;                   /* RES 3 */
        return h;
    }
    /* 4847: derrape por pendiente */
    {
        uint8_t mv = s_47F5(b, c);
        h = 0u;
        s_41F6(mv, &h);
        c -= 2u; b += 2u;
        if (p_44C2(2u, b, c)) h &= (uint8_t)~0x01u;
        b -= 3u;
        if (p_44C2(2u, b, c)) h &= (uint8_t)~0x02u;
    }
    return s_4210(h);
}

/* sub_434A: driver por frame de la tabla COLL (16 slots).
 * Pasada 1: flags (4820) + mover (710B) los que NO son trampas;
 * Pasada 2: mover las trampas 0x34. */
void player_coll_frame(void)
{
    for (int s = 0; s < 16; s++) {
        uint16_t ix = (uint16_t)(0xE386u + s * 5u);
        if (!rr(ix)) continue;
        {
            uint8_t b = rr((uint16_t)(ix + 2u)), c = rr((uint16_t)(ix + 3u));
            wr((uint16_t)(ix + 4u), s_4820(rr((uint16_t)(ix + 4u)), b, c));
        }
        if (rr((uint16_t)(ix + 1u)) != 0x34u) s_710B(ix, (uint8_t)s);
    }
    for (int s = 0; s < 16; s++) {
        uint16_t ix = (uint16_t)(0xE386u + s * 5u);
        if (!rr(ix)) continue;
        if (rr((uint16_t)(ix + 1u)) == 0x34u) s_710B(ix, (uint8_t)s);
    }
}

/* sub_4273: intento de empuje en la celda del trampolin (der: b+2, izq: b-1).
 * Devuelve 1 (NZ) = el jugador puede avanzar; 0 (Z) = bloqueado. */
static uint8_t s_4273(uint8_t b, uint8_t c, int dir)
{
    uint16_t ix1 = 0, ix2 = 0;
    uint8_t s1 = 0, s2 = 0, f1, f2;
    uint8_t bb = (uint8_t)(dir > 0 ? b + 2 : b - 1);
    f1 = s_468A(0x02u, bb, c, &ix1, &s1);
    f2 = s_468A(0x02u, bb, (uint8_t)(c + 1u), &ix2, &s2);
    if (!f1 && !f2) return 1;                  /* sin objeto: avanza */
    if (f1 && f2 && s1 != s2) return 0;        /* dos objetos distintos */
    {
        uint16_t ix = f2 ? ix2 : ix1;          /* 42A6: prioriza el segundo */
        uint8_t f4 = rr((uint16_t)(ix + 4u));
        uint8_t ob, oc, db;
        if (f4 & 0x04u) return 0;              /* bit2: no empujable ahora */
        if (f4 & 0x01u) return 1;              /* bit0: atravesable */
        ob = rr((uint16_t)(ix + 2u));          /* sub_431C */
        oc = rr((uint16_t)(ix + 3u));
        db = (uint8_t)(dir > 0 ? ob + 2 : ob - 1);
        if (p_49DC(db, oc)) return 0;
        if (p_49DC(db, (uint8_t)(oc + 1u))) return 0;
        s_42F5(db, (uint8_t)(oc + 1u));        /* aplasta BATs en el destino */
        s_42F5(db, oc);
        wr((uint16_t)(ix + 4u), (uint8_t)(dir > 0 ? 3u : 1u));   /* 0xEAFD */
        s_710B(ix, f2 ? s2 : s1);              /* primer medio paso */
        return 1;
    }
}

/* ==========================================================================
 * MOTOR BAT (enemigos, tabla 0xE416): sub_438D (driver) + sub_43BF (cerebro
 * por tipo) + sub_719D (movedor con animacion por direccion).
 * Deltas de tile del alocador (28/40 por enemigo): 0-3 final-A, 4-7 final-B
 * (variante por bit1), 8-11/18-21 idle aleteo, 0x0C trans-izq, 0x16
 * trans-der, 0x1C trans-abajo, 0x22 trans-arriba. Los que chocan techo o
 * piso en vertical MUEREN (slot=0 + blank + puff sub_5D63).
 * ========================================================================== */

/* sub_4502: par horizontal &0x20 en (b,c),(b+1,c); fila>0x13 = libre.
 * NZ(1) = bloqueado/piso presente. */
static uint8_t p_4502(uint8_t b, uint8_t c)
{
    if (c > 0x13u) return 0;
    if (cm(b, c) & 0x20u) return 1;
    if (cm((uint8_t)(b + 1u), c) & 0x20u) return 1;
    return 0;
}
/* sub_4566(ap): par horizontal &0x30; fuera de pantalla: ap==0 -> bloqueado,
 * ap!=0 -> libre. NZ(1)=bloqueado. */
static uint8_t p_4566(uint8_t ap, uint8_t b, uint8_t c)
{
    if (c > 0x13u) return ap ? 0 : 1;
    if (cm(b, c) & 0x30u) return 1;
    if (cm((uint8_t)(b + 1u), c) & 0x30u) return 1;
    return 0;
}
/* sub_45D0: posicion del jugador vs columna b: 0 = misma col, 1 = dentro de
 * +-8 cols, 2 = fuera. */
static uint8_t p_45D0(uint8_t b)
{
    uint8_t pc = rr(0xE334u);
    if (pc == b) return 0;
    if (pc > (uint8_t)(b + 8u) && (uint8_t)(b + 8u) >= b) return 2;
    if (b >= 8u && pc < (uint8_t)(b - 8u)) return 2;
    return 1;
}
/* sub_45EF: B vs col del jugador: -1 = jugador a la DERECHA (carry),
 * 0 = igual, +1 = jugador a la izquierda. */
static int p_45EF(uint8_t b)
{
    uint8_t pc = rr(0xE334u);
    if (b == pc) return 0;
    return (b < pc) ? -1 : 1;
}
/* sub_45F8: C vs fila del jugador: -1 = jugador ABAJO (carry), 0 = igual,
 * +1 = jugador arriba. */
static int p_45F8(uint8_t c)
{
    uint8_t pr = rr(0xE335u);
    if (c == pr) return 0;
    return (c < pr) ? -1 : 1;
}

/* sub_4599 + sub_45AD: rebote horizontal: probes laterales (b+2 / b-1) con
 * sub_44C2(modo); der libre+izq bloq -> derecha; der bloq+izq libre ->
 * izquierda; ambos bloq -> parar. */
static uint8_t s_45AD(uint8_t f, uint8_t mode, uint8_t b, uint8_t c)
{
    uint8_t dR = p_44C2(mode, (uint8_t)(b + 2u), c);
    uint8_t eL = p_44C2(mode, (uint8_t)(b - 1u), c);
    if (dR == 0u) {
        if (eL != 0u) f |= 0x02u;
    } else {
        if (eL == 0u) f &= (uint8_t)~0x02u;
        else          f &= (uint8_t)~0x01u;
    }
    return f;
}

/* sub_4882 (tipo 0x39): patrulla horizontal con gravedad; cerca del jugador
 * usa probes en modo 1 (mas permisivo). */
static uint8_t br_4882(uint8_t f, uint8_t b, uint8_t c)
{
    uint8_t d;
    if (!p_4502(b, (uint8_t)(c + 2u)))
        return (uint8_t)((f & ~(0x01u | 0x08u)) | 0x04u);   /* cae */
    f &= (uint8_t)~0x04u;
    if (p_45D0(b) == 2u) d = 0u;
    else { f |= 0x01u; d = 1u; }
    return s_45AD(f, d, b, c);
}

/* sub_48AD (tipo 0x38): caminador-perseguidor; en la columna del jugador
 * SUBE si el techo esta libre. */
static uint8_t br_48AD(uint8_t f, uint8_t b, uint8_t c)
{
    uint8_t prox;
    if (!p_4502(b, (uint8_t)(c + 2u)))
        return (uint8_t)((f & ~(0x01u | 0x08u)) | 0x04u);   /* cae */
    if (!(f & 0x04u) && (f & 0x01u)) {           /* en marcha horizontal */
        uint8_t d = 0u;
        if (p_45D0(b) != 2u) {
            d = 1u;
            if (p_45F8(c) < 0) d = 2u;           /* jugador abajo */
        }
        return s_45AD(f, d, b, c);
    }
    prox = p_45D0(b);
    if (prox == 0u) {                            /* misma columna */
        f |= 0x04u | 0x08u;                      /* subir */
        if (p_4502(b, (uint8_t)(c - 1u))) f &= (uint8_t)~0x04u;
        return f;
    }
    f &= (uint8_t)~0x04u;
    f |= 0x01u;
    if (prox == 1u) {                            /* dentro de +-8 */
        uint8_t d = 1u;
        if (p_45F8(c) < 0) d = 2u;
        return s_45AD(f, d, b, c);
    }
    f &= (uint8_t)~0x01u;                        /* fuera: quieto */
    return f;
}

/* sub_4901 (tipo 0x37): volador perseguidor 4-direcciones. Port literal con
 * los saltos del Z80. l = "ya intente la otra direccion". */
static uint8_t br_4901(uint8_t f, uint8_t b, uint8_t c)
{
    uint8_t h = f, l = 0;
    uint8_t dR, eL, dD, eU;
    if (h & 0x04u) goto v495F;
h490B:
    h &= (uint8_t)~0x04u; h |= 0x01u; h &= (uint8_t)~0x08u;
    if (p_45F8(c) >= 0) h |= 0x08u;              /* jugador arriba/igual: subir */
    dR = p_44C2(2u, (uint8_t)(b + 2u), c);       /* sub_4599 modo 2 */
    eL = p_44C2(2u, (uint8_t)(b - 1u), c);
    if (dR == 0u) {
        if (eL == 0u) goto done;                 /* ambos libres */
        if (l == 0u) {
            if (p_45D0(b) != 2u) goto v495E;     /* dentro: probar vertical */
            h |= 0x02u; goto done;               /* fuera: derecha */
        }
        if (p_45EF(b) < 0) { h |= 0x02u; goto done; }
        goto stop49AD;
    }
    if (eL != 0u) {                              /* ambos bloqueados */
        if (l == 0u) goto v495E;
        goto stop49AD;
    }
    if (l == 0u) {                               /* solo derecha bloqueada */
        if (p_45D0(b) != 2u) goto v495E;
        h &= (uint8_t)~0x02u; goto done;         /* fuera: izquierda */
    }
    {
        int r = p_45EF(b);
        if (r == 0) goto stop49AD;
        if (r > 0) { h &= (uint8_t)~0x02u; goto done; }
        goto stop49AD;
    }
v495E:
    l++;
v495F:
    h &= (uint8_t)~0x01u; h |= 0x04u; h &= (uint8_t)~0x02u;
    if (p_45EF(b) < 0) h |= 0x02u;               /* cara hacia el jugador */
    dD = p_4566(0u, b, (uint8_t)(c + 2u));       /* sub_4586 modo 0 */
    eU = p_4566(0u, b, (uint8_t)(c - 1u));
    if (dD == 0u) {
        if (eU != 0u) {                          /* solo abajo libre */
            if (p_45D0(b) == 2u) { h &= (uint8_t)~0x08u; goto done; }
            if (p_45F8(c) >= 0) goto h490B;      /* jugador no-abajo: horizontal */
            h &= (uint8_t)~0x08u; goto done;     /* bajar */
        }
        if (p_45D0(b) == 2u) goto done;          /* ambos libres, lejos: sigue */
        if (p_45F8(c) != 0) goto done;
        goto h490B;                              /* misma fila: horizontal */
    }
    if (eU != 0u) goto h490B;                    /* ambos bloqueados */
    if (p_45D0(b) == 2u) { h |= 0x08u; goto done; }   /* lejos: subir */
    if (p_45F8(c) <= 0) goto h490B;              /* misma fila o jugador abajo */
    h |= 0x08u; goto done;                       /* jugador arriba: subir (49A8) */
stop49AD:
    h &= (uint8_t)~(0x01u | 0x04u);
done:
    return h;
}

/* sub_43BF: cerebro (solo frames PARES) */
static uint8_t s_43BF(uint16_t ix)
{
    uint8_t code = rr((uint16_t)(ix + 1u));
    uint8_t b = rr((uint16_t)(ix + 2u)), c = rr((uint16_t)(ix + 3u));
    uint8_t f = rr((uint16_t)(ix + 4u));
    if (rr(0xEAC9u) & 0x01u) return f;
    switch (code) {
        case 0x36u: return f;
        case 0x37u: return br_4901(f, b, c);
        case 0x38u: return br_48AD(f, b, c);
        case 0x39u: return br_4882(f, b, c);
        default:    return s_45AD(f, 0u, b, c);   /* 0x3A (487A) y 0x3B+ (4872) */
    }
}

/* sub_7279: trampa murcielago (0x36): ciclo de 64 frames — cuelga (par de
 * celdas en l+1, deltas 0-1/2-3), aletea volando (2x2 deltas 4-7/8-11) y
 * vuelve a colgarse. */
static void s_7279(uint16_t ix, uint8_t slot)
{
    uint8_t h = rr((uint16_t)(ix + 2u)), l = rr((uint16_t)(ix + 3u));
    uint8_t t = (uint8_t)(rr(0xEAC9u) & 0x3Fu);
    uint8_t d;
    if (t < 0x10u) d = 0u;
    else if (t < 0x18u) d = 2u;
    else if (t < 0x38u) {
        d = (t & 0x01u) ? 8u : 4u;
        rl_cell_put(h, l, 0x36u, d, slot);
        rl_cell_put((uint8_t)(h + 1u), l, 0x36u, (uint8_t)(d + 1u), slot);
        rl_cell_put(h, (uint8_t)(l + 1u), 0x36u, (uint8_t)(d + 2u), slot);
        rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), 0x36u, (uint8_t)(d + 3u), slot);
        return;
    } else if (t == 0x38u) {
        rl_cell_put(h, l, 0u, 0u, slot);
        rl_cell_put((uint8_t)(h + 1u), l, 0u, 0u, slot);
        d = 2u;
    } else d = 2u;
    rl_cell_put(h, (uint8_t)(l + 1u), 0x36u, d, slot);
    rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), 0x36u, (uint8_t)(d + 1u), slot);
}

/* sub_719D: movedor BAT */
static void s_719D(uint16_t ix, uint8_t slot)
{
    uint8_t code = rr((uint16_t)(ix + 1u));
    uint8_t h, l, f, d;
    if (code == 0x36u) { s_7279(ix, slot); return; }
    h = rr((uint16_t)(ix + 2u)); l = rr((uint16_t)(ix + 3u));
    f = rr((uint16_t)(ix + 4u));
    if (rr(0xEAC9u) & 0x01u) {
        /* IMPAR: paso final / idle */
        if (f & 0x01u) {
            if (!(f & 0x02u)) {                       /* izq (7126, D=0) */
                rl_cell_put(h, l, code, 0u, slot);
                rl_cell_put((uint8_t)(h + 1u), l, code, 1u, slot);
                rl_cell_put(h, (uint8_t)(l + 1u), code, 2u, slot);
                rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, 3u, slot);
                rl_cell_put((uint8_t)(h + 2u), (uint8_t)(l + 1u), 0u, 0u, slot);
                rl_cell_put((uint8_t)(h + 2u), l, 0u, 0u, slot);
            } else {                                  /* der (71BC: 7139 D=4) */
                rl_cell_put((uint8_t)(h - 1u), l, 0u, 0u, slot);
                rl_cell_put((uint8_t)(h - 1u), (uint8_t)(l + 1u), 0u, 0u, slot);
                rl_cell_put(h, l, code, 4u, slot);
                rl_cell_put((uint8_t)(h + 1u), l, code, 5u, slot);
                rl_cell_put(h, (uint8_t)(l + 1u), code, 6u, slot);
                rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, 7u, slot);
            }
        } else if (f & 0x04u) {
            d = (f & 0x02u) ? 4u : 0u;                /* variante por cara */
            if (f & 0x08u) {                          /* ARRIBA (71D4) */
                rl_cell_put(h, l, code, d, slot);
                rl_cell_put((uint8_t)(h + 1u), l, code, (uint8_t)(d + 1u), slot);
                rl_cell_put(h, (uint8_t)(l + 1u), code, (uint8_t)(d + 2u), slot);
                rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, (uint8_t)(d + 3u), slot);
                rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 2u), 0u, 0u, slot);
                rl_cell_put(h, (uint8_t)(l + 2u), 0u, 0u, slot);
            } else {                                  /* abajo (7155) */
                rl_cell_put(h, (uint8_t)(l - 1u), 0u, 0u, slot);
                rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l - 1u), 0u, 0u, slot);
                rl_cell_put(h, l, code, d, slot);
                rl_cell_put((uint8_t)(h + 1u), l, code, (uint8_t)(d + 1u), slot);
                rl_cell_put(h, (uint8_t)(l + 1u), code, (uint8_t)(d + 2u), slot);
                rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, (uint8_t)(d + 3u), slot);
            }
        } else {                                      /* quieto (71E3) */
            d = (f & 0x02u) ? 4u : 0u;
            rl_cell_put(h, l, code, d, slot);
            rl_cell_put((uint8_t)(h + 1u), l, code, (uint8_t)(d + 1u), slot);
            rl_cell_put(h, (uint8_t)(l + 1u), code, (uint8_t)(d + 2u), slot);
            rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, (uint8_t)(d + 3u), slot);
        }
        return;
    }
    /* PAR (71F4): celda nueva + grafico de transicion */
    if (f & 0x01u) {
        uint8_t hg;
        if (!(f & 0x02u)) { d = 0x0Cu; h--; wr((uint16_t)(ix + 2u), h); hg = h; }
        else              { d = 0x16u; wr((uint16_t)(ix + 2u), (uint8_t)(h + 1u)); hg = h; }
        rl_cell_put(hg, l, code, d, slot);
        rl_cell_put((uint8_t)(hg + 1u), l, code, (uint8_t)(d + 1u), slot);
        rl_cell_put((uint8_t)(hg + 2u), l, code, (uint8_t)(d + 2u), slot);
        rl_cell_put(hg, (uint8_t)(l + 1u), code, (uint8_t)(d + 3u), slot);
        rl_cell_put((uint8_t)(hg + 1u), (uint8_t)(l + 1u), code, (uint8_t)(d + 4u), slot);
        rl_cell_put((uint8_t)(hg + 2u), (uint8_t)(l + 1u), code, (uint8_t)(d + 5u), slot);
    } else if (f & 0x04u) {
        d = (f & 0x02u) ? 0x22u : 0x1Cu;
        if (!(f & 0x08u)) {
            /* ABAJO: si el piso bloquea (4566 modo 1) el BAT MUERE */
            if (p_4566(1u, h, (uint8_t)(l + 2u))) {
                wr(ix, 0u);
                rl_cell_put(h, l, 0u, 0u, slot);
                rl_cell_put((uint8_t)(h + 1u), l, 0u, 0u, slot);
                rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), 0u, 0u, slot);
                rl_cell_put(h, (uint8_t)(l + 1u), 0u, 0u, slot);
                wr(0xEAF6u, 0x32u);                   /* puff (sub_5D63) */
                return;
            }
            wr((uint16_t)(ix + 3u), (uint8_t)(l + 1u));
            rl_cell_put(h, l, code, d, slot);
            rl_cell_put((uint8_t)(h + 1u), l, code, (uint8_t)(d + 1u), slot);
            rl_cell_put(h, (uint8_t)(l + 1u), code, (uint8_t)(d + 2u), slot);
            rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, (uint8_t)(d + 3u), slot);
            rl_cell_put(h, (uint8_t)(l + 2u), code, (uint8_t)(d + 4u), slot);
            rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 2u), code, (uint8_t)(d + 5u), slot);
        } else {
            /* ARRIBA: techo bloquea (4566 modo 1 en l-1) -> MUERE (724B) */
            if (p_4566(1u, h, (uint8_t)(l - 1u))) {
                wr(ix, 0u);
                rl_cell_put(h, l, 0u, 0u, slot);
                rl_cell_put((uint8_t)(h + 1u), l, 0u, 0u, slot);
                rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), 0u, 0u, slot);
                rl_cell_put(h, (uint8_t)(l + 1u), 0u, 0u, slot);
                wr(0xEAF6u, 0x32u);
                return;
            }
            l--;
            wr((uint16_t)(ix + 3u), l);               /* 725D */
            rl_cell_put(h, l, code, d, slot);
            rl_cell_put((uint8_t)(h + 1u), l, code, (uint8_t)(d + 1u), slot);
            rl_cell_put(h, (uint8_t)(l + 1u), code, (uint8_t)(d + 2u), slot);
            rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, (uint8_t)(d + 3u), slot);
            rl_cell_put(h, (uint8_t)(l + 2u), code, (uint8_t)(d + 4u), slot);
            rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 2u), code, (uint8_t)(d + 5u), slot);
        }
    } else {
        /* quieto (7268): aleteo idle, deltas 8-11 / 0x12-0x15 */
        d = (f & 0x02u) ? 0x12u : 0x08u;
        rl_cell_put(h, l, code, d, slot);
        rl_cell_put((uint8_t)(h + 1u), l, code, (uint8_t)(d + 1u), slot);
        rl_cell_put(h, (uint8_t)(l + 1u), code, (uint8_t)(d + 2u), slot);
        rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, (uint8_t)(d + 3u), slot);
    }
}

/* sub_438D: driver por frame de los BATs */
void player_bats_frame(void)
{
    for (int s = 0; s < 8; s++) {
        uint16_t ix = (uint16_t)(0xE416u + s * 5u);
        if (!rr(ix)) continue;
        wr((uint16_t)(ix + 4u), s_43BF(ix));
    }
    for (int s = 0; s < 8; s++) {
        uint16_t ix = (uint16_t)(0xE416u + s * 5u);
        if (!rr(ix)) continue;
        if (rr((uint16_t)(ix + 4u)) & 0x05u)
            wr((uint16_t)(ix + 4u), s_43BF(ix));
        s_719D(ix, (uint8_t)s);
    }
}

/* ==========================================================================
 * MOTOR DE ASCENSORES (e43e tipos 0x1C ancho 2 / 0x1D ancho 4) — sub_442D.
 * Cada 8 frames (EAC9&7==0): cerebro sub_4611 (probes con rebote) y drawer
 * sub_73FB/743D (mueve la fila de la ENTRADA e43e y redibuja; el colmap por
 * delta sale del ROM vía 5E80: superficie deltas 0/1/2 = piso, cadena no).
 * Bajando APLASTA objetos (sub_5D47); subiendo EMPUJA la pila de bloques y
 * al jugador (sub_4701/4744/7575, recursivo) y aplasta contra el techo.
 * Pendiente del driver 442D: tipos 0x0C/0x0D/0x0F (sub_72CA/72DD/7326).
 * ========================================================================== */

/* sub_452D: ¿bajada bloqueada? piso (0x40) en (b,c) o (b+1,c) */
static uint8_t p_452D(uint8_t b, uint8_t c)
{
    if (c >= 0x14u) return 1u;
    if (cm(b, c) & 0x40u) return 1u;
    return (cm((uint8_t)(b + 1u), c) & 0x40u) ? 1u : 0u;
}

/* sub_4541: ¿subida bloqueada? sólido (0x30) SIN objeto (bit3, será
 * empujado) en (b,c) o (b+1,c) */
static uint8_t p_4541(uint8_t b, uint8_t c)
{
    uint8_t i, v;
    if (c >= 0x14u) return 1u;
    for (i = 0u; i < 2u; i++) {
        v = cm((uint8_t)(b + i), c);
        if (!(v & 0x08u) && (v & 0x30u)) return 1u;
    }
    return 0u;
}

/* sub_468A completo (con bit0 = jugador): 0=nada, 1=jugador (sus pies
 * están en la fila c: top-left == (b,c-1) o (b-1,c-1)), 2=objeto */
static uint8_t s_468A_pl(uint8_t mode, uint8_t b, uint8_t c,
                         uint16_t *ix, uint8_t *slot)
{
    if (b >= 0x1Eu || c >= 0x14u) return 0u;
    if (mode & 0x01u) {
        uint8_t pc = rr(0xE334u), pr = rr(0xE335u);
        if (pr == (uint8_t)(c - 1u) &&
            (pc == b || pc == (uint8_t)(b - 1u))) return 1u;
    }
    return s_468A((uint8_t)(mode & 0x06u), b, c, ix, slot) ? 2u : 0u;
}

static void s_4744(uint8_t b, uint8_t c);

/* sub_4701 (rama objeto): empujar el 2x2 una fila arriba. Techo sólido →
 * APLASTADO (5D47). Antes empuja lo que tenga encima (recursivo) y al
 * final lo redibuja una fila arriba (sub_7575: 2x2 + blanqueo abajo). */
static void s_4701_obj(uint16_t ix, uint8_t slot)
{
    uint8_t b = rr((uint16_t)(ix + 2u));
    uint8_t c = (uint8_t)(rr((uint16_t)(ix + 3u)) - 1u);
    if (p_4541(b, c)) { s_5D47(ix); return; }
    s_4744(b, c);
    s_4744((uint8_t)(b + 1u), c);
    {   /* sub_7575 */
        uint8_t code = rr((uint16_t)(ix + 1u));
        uint8_t h = rr((uint16_t)(ix + 2u));
        uint8_t l = (uint8_t)(rr((uint16_t)(ix + 3u)) - 1u);
        wr((uint16_t)(ix + 3u), l);
        rl_cell_put(h, l, code, 0u, slot);
        rl_cell_put((uint8_t)(h + 1u), l, code, 1u, slot);
        rl_cell_put(h, (uint8_t)(l + 1u), code, 2u, slot);
        rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), code, 3u, slot);
        rl_cell_put(h, (uint8_t)(l + 2u), 0u, 0u, 0u);
        rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 2u), 0u, 0u, 0u);
    }
}

/* sub_4701 (rama jugador): fila--, sync de pixel y redibujo del sprite
 * (6F27 con A=0xFF = misma animación, posición nueva) */
static void s_4701_player(void)
{
    wr(0xE335u, (uint8_t)(rr(0xE335u) - 1u));
    s_6F45_pixel();
    s_6F27((uint8_t)g_plr_frame);
}

/* sub_4744: si hay ocupante en (b,c), empujarlo una fila arriba */
static void s_4744(uint8_t b, uint8_t c)
{
    uint16_t ix;
    uint8_t slot;
    uint8_t occ = s_468A_pl(7u, b, c, &ix, &slot);
    if (occ == 1u) s_4701_player();
    else if (occ == 2u) s_4701_obj(ix, slot);
}

/* sub_4611: cerebro del ascensor. a = estado (bit3 = subiendo), (b,c) =
 * esquina izquierda de la plataforma, d = ancho en pares de celdas.
 * Devuelve el estado nuevo (el rebote re-entra en el sentido contrario,
 * como los JR del original). */
static uint8_t s_4611(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    uint8_t i, w = (uint8_t)(d * 2u);
    if (a & 0x08u) goto up;
down:
    for (i = 0u; i < d; i++)
        if (p_452D((uint8_t)(b + i * 2u), (uint8_t)(c + 1u))) {
            a |= 0x08u;                       /* 4643: SET 3 y probar subir */
            goto up;
        }
    for (i = 0u; i < w; i++) {                /* aplastar objetos debajo */
        uint16_t ix;
        if (s_468A_pl(6u, (uint8_t)(b + i), (uint8_t)(c + 1u), &ix, 0) == 2u)
            s_5D47(ix);
    }
    return a;
up:
    for (i = 0u; i < d; i++)
        if (p_4541((uint8_t)(b + i * 2u), (uint8_t)(c - 1u))) {
            a &= (uint8_t)~0x08u;             /* 467B: RES 3 y probar bajar */
            goto down;
        }
    for (i = 0u; i < w; i++) {                /* cargar la pila de arriba */
        for (;;) {
            uint16_t ix;
            uint8_t slot;
            uint8_t occ = s_468A_pl(7u, (uint8_t)(b + i),
                                    (uint8_t)(c - 1u), &ix, &slot);
            if (occ == 0u) break;
            if (occ == 1u) { s_4701_player(); continue; }   /* re-probar */
            s_4701_obj(ix, slot);
            break;
        }
    }
    return a;
}

/* sub_73FB (0x1C) / sub_743D (0x1D): el drawer mueve la fila de la entrada
 * y redibuja. bit2 del estado = activo. Superficie: deltas 0,1[,1],2;
 * colgante en la fila de abajo al subir: 2,3 (0x1C) / _,3,4,_ (0x1D). */
static void s_elev_draw(uint16_t ix, uint8_t t)
{
    uint8_t h = rr((uint16_t)(ix + 1u)), l = rr((uint16_t)(ix + 2u));
    uint8_t st = rr((uint16_t)(ix + 4u));
    if (!(st & 0x04u)) return;
    if (t == 0x1Cu) {
        if (st & 0x08u) {                                      /* sube */
            l--; wr((uint16_t)(ix + 2u), l);
            rl_cell_put(h, l, 0x1Cu, 0u, 0u);
            rl_cell_put((uint8_t)(h + 1u), l, 0x1Cu, 1u, 0u);
            rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), 0x1Cu, 3u, 0u);
            rl_cell_put(h, (uint8_t)(l + 1u), 0x1Cu, 2u, 0u);
        } else {                                               /* baja */
            wr((uint16_t)(ix + 2u), (uint8_t)(l + 1u));
            rl_cell_put(h, l, 0u, 0u, 0u);
            rl_cell_put((uint8_t)(h + 1u), l, 0u, 0u, 0u);
            rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), 0x1Cu, 1u, 0u);
            rl_cell_put(h, (uint8_t)(l + 1u), 0x1Cu, 0u, 0u);
        }
        return;
    }
    if (st & 0x08u) {                                          /* 0x1D sube */
        l--; wr((uint16_t)(ix + 2u), l);
        rl_cell_put(h, l, 0x1Du, 0u, 0u);
        rl_cell_put((uint8_t)(h + 1u), l, 0x1Du, 1u, 0u);
        rl_cell_put((uint8_t)(h + 2u), l, 0x1Du, 1u, 0u);
        rl_cell_put((uint8_t)(h + 3u), l, 0x1Du, 2u, 0u);
        rl_cell_put((uint8_t)(h + 3u), (uint8_t)(l + 1u), 0u, 0u, 0u);
        rl_cell_put((uint8_t)(h + 2u), (uint8_t)(l + 1u), 0x1Du, 4u, 0u);
        rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), 0x1Du, 3u, 0u);
        rl_cell_put(h, (uint8_t)(l + 1u), 0u, 0u, 0u);
    } else {                                                   /* baja */
        wr((uint16_t)(ix + 2u), (uint8_t)(l + 1u));
        rl_cell_put(h, l, 0u, 0u, 0u);
        rl_cell_put((uint8_t)(h + 1u), l, 0u, 0u, 0u);
        rl_cell_put((uint8_t)(h + 2u), l, 0u, 0u, 0u);
        rl_cell_put((uint8_t)(h + 3u), l, 0u, 0u, 0u);
        rl_cell_put((uint8_t)(h + 3u), (uint8_t)(l + 1u), 0x1Du, 2u, 0u);
        rl_cell_put((uint8_t)(h + 2u), (uint8_t)(l + 1u), 0x1Du, 1u, 0u);
        rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), 0x1Du, 1u, 0u);
        rl_cell_put(h, (uint8_t)(l + 1u), 0x1Du, 0u, 0u);
    }
}

/* sub_442D: driver de la tabla e43e (16 slots de 5 bytes). Los ascensores
 * solo actúan cada 8 frames (EAC9&7==0). */
void player_elev_frame(void)
{
    uint16_t hl = 0xE43Eu;
    uint8_t s;
    for (s = 0u; s < 16u; s++, hl += 5u) {
        uint8_t t = rr(hl);
        if (t != 0x1Cu && t != 0x1Du) continue;
        if (rr(0xEAC9u) & 0x07u) continue;
        wr((uint16_t)(hl + 4u),
           s_4611(rr((uint16_t)(hl + 4u)), rr((uint16_t)(hl + 1u)),
                  rr((uint16_t)(hl + 2u)), (t == 0x1Cu) ? 1u : 2u));
        s_elev_draw(hl, t);
    }
}

/* ==========================================================================
 * DANIO POR CONTACTO + MUERTE (sub_5A2D / sub_5AF8 / sub_5A63)
 * ========================================================================== */
/* sub_5AF8: peligro en (b,c)? celda con bit 0x10 y sin 0x80; flags del byte
 * E6EE&7 (saltando anula bit2; el power-up 0xE344 anula bit1). */
static uint8_t p_5AF8(uint8_t b, uint8_t c)
{
    uint8_t v, d, ph;
    if (c >= 0x14u) return 0;
    v = cm(b, c);
    if (v & 0x80u) return 0;
    if (!(v & 0x10u)) return 0;
    d = (uint8_t)(e6ee(b, c) & 0x07u);
    ph = rr(0xEAD6u);
    if (ph >= 2u && ph < 0x11u) d &= (uint8_t)~0x04u;
    if (rr(0xE344u)) d &= (uint8_t)~0x02u;
    return d;
}

/* sub_5A2D (deteccion): solo frames IMPARES; invulnerable con el power-up
 * (0xE343); 4 probes en las celdas del cuerpo. 1 = murio. */
int player_check_death(void)
{
    uint8_t b, c;
    if ((rr(0xEAC9u) & 0x01u) == 0u) return 0;
    if (rr(0xEAE0u)) return 1;
    if (rr(0xE343u)) return 0;
    b = rr(0xE334u); c = rr(0xE335u);
    if (p_5AF8(b, c)) return 1;
    if (p_5AF8((uint8_t)(b + 1u), c)) return 1;
    if (p_5AF8((uint8_t)(b + 1u), (uint8_t)(c + 1u))) return 1;
    if (p_5AF8(b, (uint8_t)(c + 1u))) return 1;
    return 0;
}

/* anim de muerte (sub_5B66): frame 7 o 10-12 por bits del contador */
static void death_anim(void)
{
    uint8_t fc = rr(0xEAC9u);
    uint8_t fr = ((fc & 0x03u) == 0u) ? 7u : (uint8_t)(9u + (fc & 0x03u));
    g_plr_frame = fr;
    s_6F27(fr);
}

/* sub_5A63: secuencia de muerte (bloqueante: consume frames con vsync).
 * 16 frames de agonia, caida hasta fila 0x12, frame 0x0D + 32 frames,
 * vidas--, commit del estado (LDIR E336->E324 + sub_6134), EAE0=1.
 * (Musica de muerte 0x7A73/0x7A8F - Fase 6.) */
void player_death_run(void)
{
    /* sub_5B35/5B56: silenciar (EAF3/F4/F2 y SFX = 0), velocidad 0x70,
     * 3 frames de pausa y cargar el jingle de muerte a tempo 6 */
    wr(0xEAF3u, 0u); wr(0xEAF4u, 0u); wr(0xEAF2u, 0u);
    wr(0xEAF6u, 0u); wr(0xEAF7u, 0u);
    wr(0xEACAu, 0x70u);
    for (int i = 0; i < 3; i++) {
        hal_wait_game_frame(player_frame_ms());
        hal_poll_events();
    }
    music_play_death();

    wr(0xEAC9u, 0u);
    for (int i = 0; i < 16; i++) {
        death_anim();
        wr(0xEAC9u, (uint8_t)(rr(0xEAC9u) + 1u));
        hal_wait_game_frame(player_frame_ms());
        hal_poll_events();
    }
    for (;;) {
        uint8_t d, e;
        wr(0xEACBu, 0u); wr(0xEACCu, 0u);
        s_40BB(&d, &e);
        if (!(d & 0x04u)) break;
        if (rr(0xE335u) >= 0x12u) break;
        if ((rr(0xEAC9u) & 0x01u) == 0u) {       /* sub_5B78: medio paso */
            uint8_t row = (uint8_t)(rr(0xE335u) + 1u);
            wr(0xE335u, row);
            g_plr_px = ((int)rr(0xE334u) + 1) * 8;
            g_plr_py = (((int)row + 4) * 2 + 1) * 4 - 1;
        } else {
            s_6F45_pixel();
        }
        death_anim();
        wr(0xEAC9u, (uint8_t)(rr(0xEAC9u) + 1u));
        hal_wait_game_frame(player_frame_ms());
        hal_poll_events();
    }
    s_6F45_pixel();
    g_plr_frame = 0x0D;
    s_6F27(0x0Du);
    for (int i = 0; i < 32; i++) {
        hal_wait_game_frame(player_frame_ms());
        hal_poll_events();
    }
    wr(0xE324u, (uint8_t)(rr(0xE324u) - 1u));
    wr(0xE336u, (uint8_t)(rr(0xE336u) - 1u));
    if (!rr(0xEAE0u)) {
        wr(0xEAE0u, 1u);
        for (int i = 0; i < 0x0D; i++)
            wr((uint16_t)(0xE324u + i), rr((uint16_t)(0xE336u + i)));
        if (rr(0xE331u)) wr(0xE331u, (uint8_t)(rr(0xE331u) + 4u));
        if (rr(0xE332u)) wr(0xE332u, (uint8_t)(rr(0xE332u) + 4u));
        rl_persist_commit();                      /* sub_6134 */
    }
}

/* sub_6F27: setea los 3 planos del jugador (sprites 8,9,10) */
static void s_6F27(uint8_t frame)
{
    uint16_t coltab = (uint16_t)(rom_rb_p(0x7CF0u) | ((uint16_t)rom_rb_p(0x7CF1u) << 8));
    uint8_t d = (uint8_t)(frame * 3u);
    for (int b = 8; b < 11; b++) {
        uint16_t attr = (uint16_t)(0x1B00u + b * 4);
        uint8_t pat = (uint8_t)(d + (b - 8));
        uint8_t col = rom_rb_p((uint16_t)(coltab + pat));
        /* tintes de sala (sub_6EE1): el color 8 parpadea blanco/verde */
        if (col == 8u) {
            uint8_t t = rr(0xE343u);
            if (t) {
                uint8_t blink = (t < 3u) ? (uint8_t)(rr(0xEAC9u) & 1u)
                                         : (uint8_t)(rr(0xEAC9u) & 4u);
                if (blink) col = 0x0Fu;
            } else if ((t = rr(0xE344u)) != 0u) {
                uint8_t blink = (t < 3u) ? (uint8_t)(rr(0xEAC9u) & 1u)
                                         : (uint8_t)(rr(0xEAC9u) & 4u);
                if (blink) col = 0x02u;
            }
        }
        hal_vdp_write_vram(attr,                 (uint8_t)g_plr_py);
        hal_vdp_write_vram((uint16_t)(attr + 1), (uint8_t)g_plr_px);
        hal_vdp_write_vram((uint16_t)(attr + 2), (uint8_t)(pat * 4u));
        hal_vdp_write_vram((uint16_t)(attr + 3), col);
    }
}

/* ==========================================================================
 * API
 * ========================================================================== */
void player_frame(uint8_t stick, uint8_t trig)
{
    uint8_t fc = rr(0xEAC9u);
    uint8_t d, e;

    /* sub_4064: limpiar input en frames pares */
    if ((fc & 0x01u) == 0u) {
        wr(0xEACBu, 0u);
        wr(0xEACCu, 0u);
        /* sub_50E8: el poll solo escribe en frames pares */
        if (trig)  wr(0xEACCu, 0xFFu);
        if (stick) wr(0xEACBu, stick);
    }

    s_40BB(&d, &e);
    s_6F5C(d, e);
    s_6F27((uint8_t)g_plr_frame);          /* sprites 8-10 en la attr table */
}

/* 40AF: el contador de frame se incrementa al FINAL del frame completo —
 * después de bloques/bats/daño, que comparten la paridad del frame. */
void player_end_frame(void)
{
    wr(0xEAC9u, (uint8_t)(rr(0xEAC9u) + 1u));
}

/* ==========================================================================
 * 0x62D8 (rama 62FA, juego real) — velocidad/teclas de sistema por frame.
 * C = (0xEAD3) = fila 6 de la matriz acumulada en el busy-wait (activo-bajo).
 *
 *   6318: BIT 1,C (CTRL)  suelto      → A=0x70 D=0x00 E=6  (normal)
 *   631C: BIT 2,C (GRAPH) suelto      → A=0x30 D=0x07 E=4  (CTRL: correr)
 *   6320: CTRL+GRAPH                  → A=0x01 D=0x0C E=2  (turbo)
 *   6336: BIT 3,C (CAPS) pulsado      → E=0 (música muda)
 *   633C: (0xEACA)=A velocidad, (0xEAF1)=D transpose, (0xEAF3)=E tempo
 *
 * Pendiente de portar (misma rutina): F1=reiniciar sala, F2=vidas:=1,
 * y fila 7 (0xEAD4): F4→sub_4F93, F5→pausa sub_6358.
 * ========================================================================== */
/* sub_5128: duración real de una iteración del game loop, medida en
 * openMSX (tools/tr_speed.tcl): ms = 54.0 + 1.381 × EACA. */
double player_frame_ms(void)
{
    return 54.0 + 1.381 * (double)rr(0xEACAu);
}

void player_speed_frame(uint8_t row6)
{
    uint8_t a, d, e;
    if (row6 & 0x02u)        { a = 0x70u; d = 0x00u; e = 6u; }
    else if (row6 & 0x04u)   { a = 0x30u; d = 0x07u; e = 4u; }
    else                     { a = 0x01u; d = 0x0Cu; e = 2u; }
    if ((row6 & 0x08u) == 0u) e = 0u;
    wr(0xEACAu, a);
    wr(0xEAF1u, d);
    wr(0xEAF3u, e);
}

void player_sync_pixel(void)
{
    g_plr_px = ((int)rl_ram_rb(0xE334u) + 1) * 8;
    g_plr_py = ((int)rl_ram_rb(0xE335u) + 4) * 8 - 1;
    g_plr_frame = 0;
    s_6F27(0u);
}

uint8_t player_take_exit(void)
{
    uint8_t v = rr(0xEAE1u);
    if (v) wr(0xEAE1u, 0u);
    return v;
}
