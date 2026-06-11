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

int g_plr_px, g_plr_py, g_plr_frame;

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
        /* sub_4248/sub_425A -> sub_42FF (solo frames PARES, 5D5D): puerta en
         * la celda del trampolin (der: b+2, izq: b-1); si hay, intenta abrir
         * (rl_door_press = sub_4325+sub_758C) y bloquea el paso ese frame.
         * Empuje de bloques (sub_4273) pendiente - Fase 5. */
        if ((rr(0xEAC9u) & 0x01u) == 0u) {
            if (h & 0x01u) {
                if (rl_door_press((uint8_t)(b + 2u), c)) h &= (uint8_t)~0x01u;
            }
            if (h & 0x02u) {
                if (rl_door_press((uint8_t)(b - 1u), c)) h &= (uint8_t)~0x02u;
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

    wr(0xEAC9u, (uint8_t)(fc + 1u));       /* 40AF: contador de frame */
}

uint8_t player_take_exit(void)
{
    uint8_t v = rr(0xEAE1u);
    if (v) wr(0xEAE1u, 0u);
    return v;
}
