#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "hal.h"
#include "game.h"

#define VRAM_PAT_BASE   0x0000u
#define VRAM_COL_BASE   0x2000u
#define VRAM_NAME_BASE  0x1800u

static uint8_t g_tiles[256][16];

#define ROM_ADDR(a)  ((uint16_t)(a))

static const struct {
    uint16_t vram_idx;
    uint16_t rom_addr;
    uint8_t  count;
} TILE_MAP[] = {
    /* (el tile 0x00 NO se carga en el boot: queda el estado INIGRP del BIOS,
     * patrón 0 / color 0x01 — verificado contra vram_title.bin) */
    { 0x01, ROM_ADDR(0x9A56), 2 },    /* KEY (dark blue)*/
    { 0x0D, ROM_ADDR(0x9A76), 1 },    /* HEART */
    { 0x0E, ROM_ADDR(0x84B6), 28 },   /* MAP Area 7x4, rows 0-3, BG3 (tiles 0x0E-0x29) (file 0x44B6-0x4675) */
    { 0x2A, ROM_ADDR(0x7E96), 28 },   /* LOGO Area 7x4, rows 0-3 (tiles 0x2A-0x45) (file 0x3E96-0x4055) */
    { 0x46, ROM_ADDR(0x9A86),  1 },   /* Vertical Separator */
    { 0x47, ROM_ADDR(0x86F6), 10 },   /* Digits 0-9 - ANIM_BG (0x47-0x50) */
    { 0x51, ROM_ADDR(0x8676), 4 },    /* "Hi""SCORE" - BG4_A (0x51-0x54) */
    { 0x55, ROM_ADDR(0x86B6), 2 },    /* "Key" - BG4_B (0x55-0x56) */
    { 0x57, ROM_ADDR(0x86D6), 2 },    /* "Life" - BG4_C (0x57-0x58) */
    { 0x59, ROM_ADDR(0x8796), 26 },   /* Font A-Z */
    { 0x73, ROM_ADDR(0x8056), 70 },   /* LOGO Body 14x5, rows 0-4 (tiles 0x73-0xB8) (file 0x4056-0x44B5) */
//  { 0x73, ROM_ADDR(0x89C6), 2 },    /* wall tiles 0x73-0x74 */
//  { 0x75, ROM_ADDR(0x8966), 2 },    /* wall tiles 0x75-0x76 */
};
#define N_MAPS (sizeof(TILE_MAP)/sizeof(TILE_MAP[0]))

/* Escribe un tile a los 3 tercios de la pattern/color table (SCREEN 2 real:
 * cada tercio tiene tablas propias; la carga "global" del tileset duplica el
 * dato en los 3, como hace el loader del ROM con LDIRVM por tercio). */
static void write_tile_to_vdp(uint8_t src_idx, uint8_t dst_idx)
{
    for (uint16_t t = 0u; t < 3u; t++) {
        uint16_t pat = (uint16_t)(VRAM_PAT_BASE + t * 0x800u + dst_idx * 8u);
        uint16_t col = (uint16_t)(VRAM_COL_BASE + t * 0x800u + dst_idx * 8u);
        for (uint8_t r = 0u; r < 8u; r++) {
            hal_vdp_write_vram((uint16_t)(pat + r), g_tiles[src_idx][r * 2u]);
            hal_vdp_write_vram((uint16_t)(col + r), g_tiles[src_idx][r * 2u + 1u]);
        }
    }
}

/* ==========================================================================
 * sub_4EA2 — GENERAR TILES DE LLAVES (0x01-0x0C)
 * Los 2 patrones base están en (0x7BD2)=0x9A56; los 6 BYTES DE COLOR salen
 * de la tabla ROM 0x6DC9 = {0x41,0x81,0xD1,0x21,0x71,0xA1} y se escriben
 * DIRECTOS (las 8 filas iguales), no ink<<4|paper_del_tile.
 * ========================================================================== */
static void tiles_load_keys(void)
{
    uint32_t base_off = 0x5A56u;   /* (0x7BD2) = 0x9A56 → file 0x5A56 */
    uint32_t ink_off  = 0x2DC9u;   /* tabla de colores 0x6DC9 */
    if (base_off + 32u > g_rom_size || ink_off + 6u > g_rom_size) return;

    for (int k = 0; k < 6; k++) {
        uint8_t vram_idx = (uint8_t)(0x01u + (uint8_t)k * 2u);
        uint8_t colbyte  = g_rom[ink_off + (uint32_t)k];

        for (int t = 0; t < 2; t++) {
            uint8_t idx = (uint8_t)(vram_idx + t);
            uint32_t src = base_off + (uint32_t)t * 16u;

            for (int r = 0; r < 8; r++) {
                g_tiles[idx][r * 2u]      = g_rom[src + (uint32_t)r * 2u];
                g_tiles[idx][r * 2u + 1u] = colbyte;
            }
        }
    }
}

void tiles_load_from_rom(const uint8_t *rom_data, uint32_t rom_size)
{
    g_rom      = rom_data;
    g_rom_size = rom_size;

    memset(g_tiles, 0, sizeof(g_tiles));
    for (uint16_t i = 0; i < 256; i++)
        for (int r = 0; r < 8; r++)
            g_tiles[i][r * 2u + 1u] = 0x11u;

    for (uint8_t m = 0u; m < (uint8_t)N_MAPS; m++) {
        uint16_t idx  = TILE_MAP[m].vram_idx;
        uint32_t foff = (uint32_t)TILE_MAP[m].rom_addr - 0x4000u;
        for (uint8_t t = 0u; t < TILE_MAP[m].count; t++) {
            if (foff + (uint32_t)t * 16u + 16u > rom_size) break;
            uint16_t gi = (uint16_t)(idx + t);
            memcpy(g_tiles[gi], rom_data + foff + (uint32_t)t * 16u, 16);
        }
    }

    tiles_load_keys();

    /* Estado post-INIGRP del BIOS: patrón 0, color table llena de 0x01
     * (FORCLR=15... el byte observado en el oráculo es 0x01), name table 0. */
    hal_vdp_fill_vram(VRAM_PAT_BASE,  0x00u, 0x1800u);
    hal_vdp_fill_vram(VRAM_COL_BASE,  0x01u, 0x1800u);
    hal_vdp_fill_vram(VRAM_NAME_BASE, 0x00u, 768u);

    /* tile 0x00 NO se escribe: conserva el estado INIGRP (ver TILE_MAP) */
    for (uint16_t i = 1u; i < 256; i++)
        write_tile_to_vdp((uint8_t)i, (uint8_t)i);

    /* patrones de SPRITE: ROM 0x9B96 -> VRAM 0x3800 (LDIRVM del boot 0x4D02).
     * El jugador (sprites 8-10, escritos por player.c) los necesita. */
    for (uint16_t i = 0u; i < 0x800u; i++)
        hal_vdp_write_vram((uint16_t)(0x3800u + i),
                           (rom_size > 0x5B96u + i) ? rom_data[0x5B96u + i] : 0u);
}

void tiles_reload_all(void)
{
    for (uint16_t i = 0u; i < 256; i++)
        write_tile_to_vdp((uint8_t)i, (uint8_t)i);
}

void tiles_reload_walls_and_anim(void)
{
    for (uint8_t i = 0u; i < 26u; i++)
        write_tile_to_vdp((uint8_t)(0x59u + i), (uint8_t)(0x59u + i));
    for (uint8_t i = 0u; i < 10u; i++)
        write_tile_to_vdp((uint8_t)(0x47u + i), (uint8_t)(0x47u + i));
}

void tiles_animate(uint8_t frame_counter)
{
    if ((frame_counter & 0x03u) != 0u) return;
    uint8_t slot     = (uint8_t)((frame_counter >> 2u) % 10u);
    uint8_t next_src = (uint8_t)(0x47u + ((slot + 1u) % 10u));
    uint8_t dst      = (uint8_t)(0x47u + slot);
    write_tile_to_vdp(next_src, dst);
}

/* sub_6CD9 — copia UN tile (16 bytes intercalados pat,col por fila) del ROM a
 * la VRAM: patrón en vram_off, color en vram_off+0x2000. vram_off ya codifica
 * el tercio: ((tercio<<8)|tile)*8. */
static void tiles_blit16(uint16_t vram_off, uint16_t rom_addr)
{
    uint32_t off = (uint32_t)rom_addr - 0x4000u;
    if (!g_rom || off + 16u > g_rom_size) return;
    for (uint8_t r = 0; r < 8u; r++) {
        hal_vdp_write_vram((uint16_t)(vram_off + r),           g_rom[off + (uint32_t)r * 2u]);
        hal_vdp_write_vram((uint16_t)(0x2000u + vram_off + r), g_rom[off + (uint32_t)r * 2u + 1u]);
    }
}

/* sub_64AB — carga `count` tiles consecutivos usando la tabla de descriptores
 * de 0x7BC0: *desc_addr apunta al descriptor (word = dirección ROM del primer
 * tile); *dest = (tercio<<8)|tile_inicial. Igual que el Z80, al volver avanza
 * *desc_addr al siguiente descriptor y *dest al tile siguiente al último —
 * las llamadas se encadenan. */
void tiles_load_from_desc(uint16_t *desc_addr, uint16_t *dest, uint8_t count)
{
    uint32_t doff = (uint32_t)*desc_addr - 0x4000u;
    if (!g_rom || doff + 2u > g_rom_size) return;
    uint16_t src = (uint16_t)(g_rom[doff] | ((uint16_t)g_rom[doff + 1u] << 8));
    for (uint8_t i = 0; i < count; i++)
        tiles_blit16((uint16_t)((*dest + i) * 8u), (uint16_t)(src + i * 16u));
    *dest      = (uint16_t)(*dest + count);
    *desc_addr = (uint16_t)(*desc_addr + 2u);
}

void tiles_rom_to_vram(uint32_t rom_file_off, uint8_t vram_start,
                       uint8_t count)
{
    if (rom_file_off >= 0x4000u) rom_file_off -= 0x4000u;
    if (rom_file_off + (uint32_t)count * 16u > g_rom_size) return;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t  tile_idx = (uint8_t)(vram_start + i);
        uint32_t off      = rom_file_off + (uint32_t)i * 16u;
        for (uint8_t row = 0; row < 8u; row++) {
            g_tiles[tile_idx][row * 2u]     = g_rom[off + (uint32_t)row * 2u];
            g_tiles[tile_idx][row * 2u + 1u] = g_rom[off + (uint32_t)row * 2u + 1u];
        }
        write_tile_to_vdp(tile_idx, tile_idx);
    }
}

void tiles_vram_from_rom(uint32_t rom_file_off, uint8_t vram_start,
                         uint8_t count)
{
    if (rom_file_off >= 0x4000u) rom_file_off -= 0x4000u;
    if (rom_file_off + (uint32_t)count * 16u > g_rom_size) return;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t  tile_idx = (uint8_t)(vram_start + i);
        uint32_t off      = rom_file_off + (uint32_t)i * 16u;
        for (uint16_t t = 0u; t < 3u; t++) {
            uint16_t pat = (uint16_t)(VRAM_PAT_BASE + t * 0x800u + tile_idx * 8u);
            uint16_t col = (uint16_t)(VRAM_COL_BASE + t * 0x800u + tile_idx * 8u);
            for (uint8_t row = 0; row < 8u; row++) {
                hal_vdp_write_vram((uint16_t)(pat + row), g_rom[off + (uint32_t)row * 2u]);
                hal_vdp_write_vram((uint16_t)(col + row), g_rom[off + (uint32_t)row * 2u + 1u]);
            }
        }
    }
}

void tiles_dump_vram(const char *label)
{
    char fname[64];
    uint8_t vram[0x4000];
    FILE *fp;

    snprintf(fname, sizeof(fname), "vram_%s.bin", label);
    hal_vdp_copy_from_vram(0x0000u, vram, 0x4000);
    fp = fopen(fname, "wb");
    if (fp) {
        fwrite(vram, 1, 0x4000, fp);
        fclose(fp);
    }
}

uint8_t tiles_vram_idx_blank(void)        { return 0x00u; }
uint8_t tiles_vram_idx_door(void)         { return 0x0Du; }
uint8_t tiles_vram_idx_bg3(uint8_t n)     { return (uint8_t)(0x0Eu + n); }
uint8_t tiles_vram_idx_key(void)          { return 0x46u; }
uint8_t tiles_vram_idx_anim_bg(uint8_t n) { return (uint8_t)(0x47u + n); }
uint8_t tiles_vram_idx_bg4(uint8_t n)     { return (uint8_t)(0x51u + n); }
uint8_t tiles_vram_idx_wall(uint8_t n)    { return (uint8_t)(0x59u + n); }
uint8_t tiles_vram_idx_space(void)        { return 0x3Fu; }
