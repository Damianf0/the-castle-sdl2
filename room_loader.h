#ifndef ROOM_LOADER_H
#define ROOM_LOADER_H
#include <stdint.h>

/* Port fiel del ROOM LOADER del ROM (sub_64DD @0x64DD) y sus rutinas hijas.
 * Decodifica una sala completa desde el ROM a:
 *   - el espejo de RAM del juego (tablas 0xE496 colmap, 0xE6EE índice,
 *     0xE346 puertas, 0xE3D6 coleccionables, 0xE386/0xE416 enemigos,
 *     0xE43E estructurales, contadores, bitfields de persistencia)
 *   - la VRAM emulada (tiles por tercio asignados dinámicamente + name table)
 * Verificable byte-a-byte contra tests/fixtures/ (dumps de openMSX). */

/* Espejo de RAM 0xE000-0xEAFF. */
uint8_t  rl_ram_rb(uint16_t addr);
void     rl_ram_wb(uint16_t addr, uint8_t v);

/* sub_4D52 (solo la parte de estado): resetea contadores y bitfields de
 * persistencia (0xE000-0xE00C = 0, 0xE00D..+0x2C5 = 0xFF, vidas, sala 0x70,
 * posición inicial del jugador, score). */
void rl_reset(void);

/* sub_64DD: carga la sala (BCD hi=fila, lo=columna) — tablas RAM + VRAM. */
void rl_load_room(uint8_t room_bcd);

/* Estado base de VRAM al iniciar partida (boot + título + game-start):
 * INIGRP, tileset del boot, logo del título — lo que las cargas de sala
 * van pisando. Necesario para comparar la VRAM completa contra fixtures. */
void rl_boot_vram(void);

/* Helpers para el motor (capa maqueta hasta Fases 3-5):
 * lee los 16 bytes (8 patrón + 8 color) del tile visible en una celda de
 * pantalla, resolviendo el tercio; y blanquea una celda (tile 0). */
void rl_cell_gfx(int srow, int scol, uint8_t out[16]);
void rl_cell_blank(int srow, int scol);

/* sub_4325 + sub_758C: el jugador presiona contra la celda de campo (b,c).
 * Si (b,c) y (b,c+1) son celdas de puerta (bit 0x02), intenta abrirla:
 * con llave del color (0xE337+color) la consume, marca el slot 0xE346 como
 * abierto y redibuja (marco arriba, cuerpo blanqueado — colmap pasa a aire).
 * Devuelve 1 si había puerta (el movimiento se bloquea ese frame). */
int rl_door_press(uint8_t b, uint8_t c);

/* sub_5053: transición de sala por borde (1=arriba 3=der 5=abajo 7=izq):
 * committea la persistencia de las 4 tablas a los bitfields (sub_6134),
 * ajusta la sala BCD y la posición de entrada, y guarda el estado activo.
 * Después llamar rl_load_room(rl_ram_rb(0xE320)). */
void rl_room_exit(uint8_t edge);

/* redibuja los iconos de llave del HUD (sub_5E01) tras un pickup */
void rl_keys_hud_redraw(void);

#endif
