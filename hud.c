/*
 * THE CASTLE — HUD estático del boot (sub_4D52 → sub_4E0C + sub_64C3 ×5)
 * ======================================================================
 * Dibuja la fila superior fija: labels SCORE/Hi SCORE/Key/Life, las áreas
 * de MAPA (tiles 0x0E-0x29) y LOGO (0x2A-0x45), el separador, y el overlay
 * "NO MAP" (o "DEMO GAME" en attract). Validado byte-exacto por la suite
 * del título (vram_title.bin). Los elementos DINÁMICOS (score, vidas,
 * llaves, minimapa) los dibuja el juego real (pickup.c / sub_5A2D).
 */
#include <stdint.h>
#include <stdbool.h>
#include "hal.h"
#include "game.h"
#include "room_loader.h"

#define NAME_BASE 0x1800u

/* sub_64C3 (variante de boot): rectángulo de tiles con incremento */
static void hud_fill_rect(uint8_t col, uint8_t row,
                          uint8_t width, uint8_t height, uint8_t start_tile)
{
    uint8_t tile = start_tile;
    for (uint8_t r = 0u; r < height; r++) {
        uint16_t base = (uint16_t)(NAME_BASE + (uint16_t)(row + r) * 32u + col);
        for (uint8_t c = 0u; c < width; c++) {
            hal_vdp_write_vram((uint16_t)(base + c),
                               (start_tile == 0u) ? 0u : tile);
            if (start_tile != 0u) tile++;
        }
    }
}

/* sub_62B0: string del ROM (fin 0x40, espacio 0x20, dígitos y letras) */
static void hud_draw_string(uint8_t col, uint8_t row,
                            uint16_t rom_str_addr, uint8_t tile_base)
{
    uint16_t addr = rom_str_addr;
    uint8_t c = col;
    if (!g_rom) return;
    for (;;) {
        uint32_t off = (uint32_t)addr - 0x4000u;
        uint8_t byte;
        if (off >= g_rom_size) break;
        byte = g_rom[off];
        addr++;
        if (byte == 0x40u) break;
        {
            uint8_t tile;
            if (byte == 0x20u) {
                tile = 0u;
            } else {
                uint8_t a = byte;
                if (a < 0x3Au) a = (uint8_t)(a - 0x30u + 0x5Du);
                tile = (uint8_t)(a - 0x41u + tile_base);
            }
            hal_vdp_write_vram((uint16_t)(NAME_BASE + (uint16_t)row * 32u + c),
                               tile);
            c++;
        }
    }
}

void draw_hud(void)
{
    /* MAP area (tiles 0x0E-0x29), LOGO (0x2A-0x45), separador 0x46 */
    hud_fill_rect(17u, 0u, 7u, 4u, 0x0Eu);
    hud_fill_rect(24u, 0u, 7u, 4u, 0x2Au);
    for (uint8_t r = 0u; r < 4u; r++)
        hal_vdp_write_vram((uint16_t)(NAME_BASE + (uint16_t)r * 32u + 31u),
                           0x46u);
    /* labels: SCORE / Hi SCORE / Key / Life */
    hud_fill_rect(1u, 0u, 3u, 1u, 0x52u);
    hud_fill_rect(9u, 0u, 4u, 1u, 0x51u);
    hud_fill_rect(1u, 2u, 2u, 1u, 0x55u);
    hud_fill_rect(1u, 3u, 2u, 1u, 0x57u);

    /* Overlay del área de mapa (sub_4E60/4E76): en attract "DEMO GAME",
     * sin mapa (E321 bit3=0) "NO MAP"; con mapa queda el minimapa. */
    if (g_intro_active) {
        hud_draw_string(18u, 1u, 0x56CAu, 0x59u);   /* "DEMO" */
        hud_draw_string(19u, 2u, 0x56CFu, 0x59u);   /* "GAME" */
    } else if (!(rl_ram_rb(0xE321u) & 0x08u)) {
        hud_draw_string(19u, 1u, 0x6472u, 0x59u);   /* "NO"  */
        hud_draw_string(19u, 2u, 0x6476u, 0x59u);   /* "MAP" */
    }
}

/* sub_4F16: pantalla GAME OVER (lives==0). Refleja vidas=0 en el HUD,
 * limpia el recuadro central (cols 9-22, filas 13-15) y escribe
 * "GAME OVER" (string ROM 0x6467) en (col 11, fila 14), tile-base 1
 * (letras = chr-0x41+1, verificado contra openMSX). El caller hace la
 * espera de 16 frames y vuelve al título. */
void draw_game_over(void)
{
    rl_ram_wb(0xE336u, rl_ram_rb(0xE324u));         /* E336 = vidas (=0) */
    hud_fill_rect(9u, 13u, 14u, 3u, 0u);            /* sub_628C ×3: limpiar */
    hud_draw_string(11u, 14u, 0x6467u, 1u);         /* sub_629D: "GAME OVER" */
}
