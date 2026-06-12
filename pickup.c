/*
 * THE CASTLE — PICKUP por celda + efectos reales (Fase 5)
 * ========================================================
 * Port fiel de:
 *   sub_5B96 (0x5B96): gate del pickup — solo frames PARES; colmap del
 *     top-left del jugador con bit 0x04 (hay coleccionable) y barrido de
 *     la tabla e3d6 (16 slots de 4: [0]=activo [1]=tipo [2]=col [3]=fila)
 *     buscando posición == top-left del jugador.
 *   sub_5BB0 (dispatcher de efectos por tipo):
 *     0x20  ítem FINAL (sala 09): jingle+commit, secuencia de victoria
 *           (sub_51D9/4F93/4B13/4E8E — PENDIENTE), EAE3=1 (corta el loop).
 *     0x21  ítem especial (salas 50/99): jingle+commit, secuencia
 *           sub_518E (PENDIENTE) y RECARGA de la sala (sub_64DD).
 *     0x22  MAPA (sala 70): SET 3,(0xE321); el dibujo del minimapa
 *           (sub_64C3/638E/63BB/640F) es un subsistema aparte — PENDIENTE.
 *     0x23  power-up ROJO: E343=0x0A, E344=0, música 0x79B7/0x79DE.
 *           ¡NO se borra de la sala! (5BC3 saltea el delete).
 *     0x24  power-up VERDE: E344=0x10, E343=0, música 0x7964/0x7993.
 *     0x25  reset de puertas: EAE2=1 (lo consume rl_room_exit).
 *     0x26  vida extra (sub_5E4B).
 *     0x27+ genérico: sprite de puntos (tabla 0x64A2) en el plano 13 con
 *           timer 0xEAF9=0x10, score BCD (tabla 0x6490, sub_5D87) y si
 *           tipo>=0x2A también llave de color (E337+tipo-0x2A, HUD 5E01).
 *   sub_5CD4: borrar el coleccionable (slot=0 + blanquear 2x2 + SFX
 *     0xEAF7=0x10 + 1 frame de espera; el 0x21 esconde su sprite).
 *   sub_5CB5: jingle de ítem especial (0x7A03/0x7A3C vía sub_5B2F) +
 *     commit del estado (LDIR E334→E322 + sub_6134).
 *   sub_5D87: score BCD 6 dígitos (E33D-E33F) con DAA; carry del byte
 *     medio = VIDA EXTRA (sub_5E4B); actualiza el hi-score (E340-E342)
 *     y redibuja ambos en el HUD (sub_5DC0: dígitos tile 0x47+d en la
 *     name table +0x22 y +0x2A; RLD para partir nibbles).
 *   sub_5E4B/5E5C: vidas++ (E336, cap 0xFF) + contador del HUD (iconos
 *     tile 0x0D desde name+0x63, máx 14, resto blanqueado).
 *   sub_4499 (animador de e3d6, cada frame): tipo 0x21 = sprite plano 11
 *     alternando patrones 0x2A/0x2B + tiles 0x21 deltas 0-3/4-7
 *     (sub_74E9). Tipo 0x23 = color-cycling del tile (sub_7510,
 *     cosmético) — PENDIENTE.
 *
 * Validado frame a frame contra openMSX (suite 'pickup', tools/tr_pick.tcl).
 */
#include <stdint.h>
#include "hal.h"
#include "game.h"
#include "room_loader.h"
#include "player.h"

extern int g_actors_on;

static uint8_t rr(uint16_t a)            { return rl_ram_rb(a); }
static void    wr(uint16_t a, uint8_t v) { rl_ram_wb(a, v); }
static uint8_t rom_rb(uint16_t a)
{
    uint32_t o = (uint32_t)a - 0x4000u;
    return (g_rom && o < g_rom_size) ? g_rom[o] : 0xFFu;
}
static uint8_t cmv(uint8_t b, uint8_t c)
{
    return rr((uint16_t)(0xE496u + (uint16_t)c * 30u + b));
}
/* name table (0x1800) */
static void nt_put(uint16_t off, uint8_t tile)
{
    hal_vdp_write_vram((uint16_t)(0x1800u + off), tile);
}

/* sub_6EE1 (expuesto por player.c): plano de sprite con tintes */
void player_sprite_plane(uint8_t pat, uint8_t plane, int x, int y);

/* sub_5128 dentro de los efectos: 1 frame de juego (solo interactivo) */
static void pk_wait_frame(void)
{
    if (!g_actors_on) return;
    hal_wait_game_frame(player_frame_ms());
    hal_poll_events();
}

/* ===== sub_5DDE/5DD3/5DC0: dígitos BCD del HUD (tile 0x47 + dígito) ===== */
static void hud_bcd3(uint16_t src, uint16_t off)        /* sub_5DD3 */
{
    for (int i = 0; i < 3; i++) {
        uint8_t v = rr((uint16_t)(src + i));
        nt_put((uint16_t)(off + i * 2u),      (uint8_t)(0x47u + (v >> 4)));
        nt_put((uint16_t)(off + i * 2u + 1u), (uint8_t)(0x47u + (v & 0x0Fu)));
    }
}
static void hud_scores(void)                            /* sub_5DC0 */
{
    hud_bcd3(0xE33Du, 0x0022u);     /* score    */
    hud_bcd3(0xE340u, 0x002Au);     /* hi-score */
}

/* ===== sub_5E5C: contador de vidas del HUD (iconos 0x0D, máx 14) ===== */
static void hud_lives(void)
{
    uint8_t v = rr(0xE336u);
    uint8_t n, i;
    if (v == 0u) return;
    n = (uint8_t)(v - 1u);
    if (n > 0x0Eu) n = 0x0Eu;
    for (i = 0u; i < n; i++)        nt_put((uint16_t)(0x63u + i), 0x0Du);
    for (; i < 0x0Eu; i++)          nt_put((uint16_t)(0x63u + i), 0x00u);
}

/* ===== sub_5E4B: vida extra ===== */
static void s_5E4B(void)
{
    uint8_t v = rr(0xE336u);
    if (v == 0xFFu) return;
    wr(0xE336u, (uint8_t)(v + 1u));
    hud_lives();
}

/* suma BCD de un byte con carry (ADC + DAA) */
static uint8_t bcd_add8(uint8_t a, uint8_t b, int *carry)
{
    int lo = (a & 0x0Fu) + (b & 0x0Fu) + *carry;
    int hi = (a >> 4) + (b >> 4) + (lo > 9 ? 1 : 0);
    if (lo > 9) lo -= 10;
    *carry = 0;
    if (hi > 9) { hi -= 10; *carry = 1; }
    return (uint8_t)((hi << 4) | lo);
}

/* ===== sub_5D87: sumar DE (BCD) al score, hi-score y HUD ===== */
static void s_5D87(uint16_t pts)
{
    uint8_t d = (uint8_t)(pts >> 8), e = (uint8_t)(pts & 0xFFu);
    int carry = 0;
    wr(0xE33Fu, bcd_add8(rr(0xE33Fu), e, &carry));
    wr(0xE33Eu, bcd_add8(rr(0xE33Eu), d, &carry));
    if (carry) s_5E4B();                 /* 5D95: carry del medio = vida */
    wr(0xE33Du, bcd_add8(rr(0xE33Du), 0u, &carry));
    /* hi-score: si score > E340-E342 → copiarlo (5DA5) */
    {
        int i, update = 0;
        for (i = 0; i < 3; i++) {
            uint8_t hs = rr((uint16_t)(0xE340u + i));
            uint8_t sc = rr((uint16_t)(0xE33Du + i));
            if (hs < sc) { update = 1; break; }
            if (hs > sc) break;
        }
        if (update)
            for (i = 0; i < 3; i++)
                wr((uint16_t)(0xE340u + i), rr((uint16_t)(0xE33Du + i)));
    }
    hud_scores();
}

/* ===== sub_5CD4: borrar el coleccionable (con SFX y espera) ===== */
static void s_5CD4(uint16_t ix, uint8_t type)
{
    uint8_t h = rr((uint16_t)(ix + 2u)), l = rr((uint16_t)(ix + 3u));
    wr(ix, 0u);
    rl_cell_put(h, l, 0u, 0u, 0u);                       /* sub_70A6 */
    rl_cell_put((uint8_t)(h + 1u), l, 0u, 0u, 0u);
    rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), 0u, 0u, 0u);
    rl_cell_put(h, (uint8_t)(l + 1u), 0u, 0u, 0u);
    if (type == 0x21u)
        player_sprite_plane(0x3Fu, 0x0Bu, 0, 0);         /* esconder llave */
    wr(0xEAF7u, 0x10u);                                  /* SFX pickup */
    pk_wait_frame();                                     /* sub_5128 */
}

/* ===== sub_5CB5: jingle de ítem especial + commit del estado =====
 * sub_5B2F: guarda el tempo, silencia, 3 frames a 0x70, carga
 * 0x7A03/0x7A3C y si había música repone tempo 6. Luego LDIR
 * E334→E322 (0x12) + sub_6134 (persistencia). (sub_4AE2: el flash de
 * pantalla de la secuencia — PENDIENTE con las cutscenes.) */
static void s_5CB5(void)
{
    uint8_t had = rr(0xEAF3u);
    wr(0xEAF3u, 0u); wr(0xEAF4u, 0u); wr(0xEAF2u, 0u);
    wr(0xEAF6u, 0u); wr(0xEAF7u, 0u);
    wr(0xEACAu, 0x70u);
    for (int i = 0; i < 3; i++) pk_wait_frame();
    music_load(0x7A03u, 0x7A3Cu);
    if (had) wr(0xEAF3u, 6u);
    for (int i = 0; i < 0x12; i++)
        wr((uint16_t)(0xE322u + i), rr((uint16_t)(0xE334u + i)));
    rl_persist_commit();                                 /* sub_6134 */
}

/* ===== sub_5B96 + sub_5BB0: el pickup por celda ===== */
void pickup_frame(void)
{
    uint8_t b, c, s;
    uint16_t ix;
    if (rr(0xEAC9u) & 0x01u) return;                     /* solo PARES */
    b = rr(0xE334u); c = rr(0xE335u);
    if (!(cmv(b, c) & 0x04u)) return;                    /* sin coleccionable */
    for (s = 0u, ix = 0xE3D6u; s < 16u; s++, ix += 4u) {
        uint8_t type;
        if (rr((uint16_t)(ix + 2u)) != b || rr((uint16_t)(ix + 3u)) != c)
            continue;
        type = rr((uint16_t)(ix + 1u));
        if (type != 0x23u) s_5CD4(ix, type);
        switch (type) {
        case 0x20u:                       /* ítem FINAL (victoria) */
            s_5CB5();
            /* sub_51D9/4F93/4B13/4E8E: secuencia de victoria — PENDIENTE */
            wr(0xE321u, (uint8_t)(rr(0xE321u) | 0x04u)); /* SET 2 */
            wr(0xEAE3u, 1u);                             /* corta el loop */
            break;
        case 0x21u:                       /* ítem especial: recarga sala */
            s_5CB5();
            /* sub_518E: pantalla especial — PENDIENTE */
            rl_load_room(rr(0xE320u));                   /* sub_64DD */
            player_sync_pixel();
            break;
        case 0x22u:                       /* MAPA */
            wr(0xE321u, (uint8_t)(rr(0xE321u) | 0x08u)); /* SET 3 */
            /* sub_64C3/638E: dibujo del minimapa — PENDIENTE */
            break;
        case 0x23u:                       /* power-up ROJO (no se borra) */
            if (rr(0xE343u) == 0x0Au) break;
            wr(0xE343u, 0x0Au);
            wr(0xE344u, 0u);
            wr(0xEAF2u, 0u); wr(0xEAF4u, 0u);            /* sub_6281 */
            music_load(0x79B7u, 0x79DEu);
            break;
        case 0x24u:                       /* power-up VERDE */
            wr(0xE344u, 0x10u);
            wr(0xE343u, 0u);
            wr(0xEAF2u, 0u); wr(0xEAF4u, 0u);
            music_load(0x7964u, 0x7993u);
            break;
        case 0x25u:                       /* reset de puertas */
            wr(0xEAE2u, 1u);
            break;
        case 0x26u:                       /* vida extra */
            s_5E4B();
            break;
        default:                          /* 0x27+: puntos / llaves */
        {
            uint8_t idx = (uint8_t)(type - 0x27u);
            uint8_t h = rr((uint16_t)(ix + 2u)), l = rr((uint16_t)(ix + 3u));
            /* sprite de puntos (tabla 0x64A2) en el plano 13, timer EAF9 */
            player_sprite_plane(rom_rb((uint16_t)(0x64A2u + idx)), 0x0Du,
                                ((int)h + 1) * 8, ((int)l + 4) * 8 - 1);
            wr(0xEAF9u, 0x10u);
            /* score BCD de la tabla 0x6490 */
            {
                uint16_t pts = (uint16_t)(rom_rb((uint16_t)(0x6490u + idx * 2u)) |
                          ((uint16_t)rom_rb((uint16_t)(0x6491u + idx * 2u)) << 8));
                s_5D87(pts);
            }
            if (type >= 0x2Au) {          /* llave de color */
                uint16_t k = (uint16_t)(0xE337u + (type - 0x2Au));
                if (rr(k) != 0xFFu) {
                    wr(k, (uint8_t)(rr(k) + 1u));
                    rl_keys_hud_redraw();                /* sub_5E01 */
                }
            }
            break;
        }
        }
    }
}

/* ===== sub_4499 + sub_74E9: animador de e3d6 (cada frame) =====
 * Tipo 0x21: sprite plano 11 alternando 0x2A/0x2B + tiles deltas 0-3/4-7.
 * Tipo 0x23: color-cycling del tile (sub_7510, cosmético) — PENDIENTE. */
void pickup_anim_frame(void)
{
    uint16_t ix = 0xE3D6u;
    uint8_t s;
    for (s = 0u; s < 16u; s++, ix += 4u) {
        if (!rr(ix)) continue;
        if (rr((uint16_t)(ix + 1u)) == 0x21u) {
            uint8_t h = rr((uint16_t)(ix + 2u)), l = rr((uint16_t)(ix + 3u));
            uint8_t ph = (uint8_t)(rr(0xEAC9u) & 0x01u);
            uint8_t d = (uint8_t)(ph * 4u);
            player_sprite_plane((uint8_t)(0x2Au + ph), 0x0Bu,
                                ((int)h + 1) * 8, ((int)l + 4) * 8 - 1);
            rl_cell_put(h, l, 0x21u, d, 0u);             /* sub_70B6 */
            rl_cell_put((uint8_t)(h + 1u), l, 0x21u, (uint8_t)(d + 1u), 0u);
            rl_cell_put(h, (uint8_t)(l + 1u), 0x21u, (uint8_t)(d + 2u), 0u);
            rl_cell_put((uint8_t)(h + 1u), (uint8_t)(l + 1u), 0x21u,
                        (uint8_t)(d + 3u), 0u);
        }
    }
}
