/*
 * THE CASTLE — Estado global compartido entre módulos
 * ====================================================
 * Todas las variables que antes eran "static" en the_castle.c
 * y que otros módulos necesitan via "extern" se declaran aquí.
 *
 * Incluir este header en cualquier .c que necesite acceder al
 * estado del juego.
 */

#pragma once
#ifndef CASTLE_GAME_H
#define CASTLE_GAME_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * ESTADO DEL JUGADOR Y DEL JUEGO (RAM MSX 0xE320..0xEAFF)
 * ========================================================================== */

extern uint8_t g_state_flags;   /* 0xEAC9 — frame counter + flags de modo   */
extern uint8_t g_anim_frame;    /* 0xEACB — frame de animación del jugador   */
extern uint8_t g_facing;        /* 0xEACC — dirección (0=der, 0xFF=izq)      */
extern uint8_t g_player_speed;  /* 0xEACA — velocidad/contador sub-pixel     */
extern uint8_t g_transition;    /* 0xEAD6 — contador de transición 0x00..0x11*/
extern uint8_t g_game_over;     /* 0xEAE0 — flag de game over                */
extern uint8_t g_room_exit;     /* 0xEAE1 — flag/código de salida de sala    */
extern uint8_t g_restart_flag;  /* 0xEAE3 — solicitud de restart de sala     */
extern uint8_t g_intro_active;  /* 0xEAE4 — flag de intro/título activo      */
extern uint8_t g_enemies_active;/* 0xEAE8 — semáforo de actualización enemigos*/

extern uint8_t g_player_col;    /* 0xE334 — columna del jugador (0-19)       */
extern uint8_t g_player_row;    /* 0xE335 — fila del jugador (0-29)          */
extern uint8_t g_player_x;      /* 0xE320 — posición X en pixels             */
extern uint8_t g_player_y;      /* 0xE321 — posición Y en tiles              */

extern uint8_t g_lives;         /* 0xE324 — vidas restantes                  */
extern uint8_t g_room_number;   /* 0xE333 — número de sala actual            */

extern uint8_t g_score[3];      /* 0xE33D — puntuación BCD (3 bytes)         */
extern uint8_t g_hiscore[3];    /* 0xE340 — hi-score BCD                     */

/* Mapa de colisión/tiles: 20 columnas × 30 filas */
extern uint8_t g_map[0x400];    /* 0xE000 */

/* ROM data pointer (inicializado por tiles_load_from_rom) */
extern const uint8_t *g_rom;       /* puntero al buffer de la ROM            */
extern uint32_t       g_rom_size;  /* tamaño del buffer                      */

extern uint8_t g_keyframe_queue[9];   /* 0xEACD — cola de keyframes legacy */

/* ==========================================================================
 * API DE CADA MÓDULO
 * ========================================================================== */

/* --- tiles.c: carga del charset/tiles del ROM a la VRAM emulada --- */
void    tiles_load_from_rom(const uint8_t *rom_data, uint32_t rom_size);
void    tiles_load_from_desc(uint16_t *desc_addr, uint16_t *dest, uint8_t count);

/* --- music.c (reproductor PSG fiel) --- */
void music_init(void);
void music_isr_tick(void);
void music_load(uint16_t music_a_addr, uint16_t music_b_addr);
void music_set_tempo(uint8_t counter, uint8_t value);
void music_set_transpose(uint8_t fine, uint8_t coarse);
void music_sfx_trigger(uint8_t sfx_id, uint8_t volume);
void music_play_title(void);
void music_play_game(void);
void music_room_start(void);   /* 0x656B: tema por sala según power-ups */
void music_play_death(void);   /* sub_5B35/5B56: jingle 0x7A73/0x7A8F */
void music_stop(void);

/* --- hud.c: HUD estático del boot (labels + áreas mapa/logo + overlay) --- */
void draw_hud(void);

/* --- pickup.c: minimapa del HUD --- */
void minimap_draw_full(void);      /* sub_64C3+638E: pickup del mapa 0x22 */
void minimap_room_exit_mark(void); /* sub_61E8+5053: al salir de la sala  */

/* --- cheats.c: cheats de QA (F5-F9) en el juego interactivo. Actualiza
 * *room si teletransporta; devuelve 1 si lo hizo (saltar el resto del frame). */
int cheats_frame(uint8_t *room);

/* --- title.c --- */
void title_screen(void);

/* --- main.c: juego fiel (render VRAM real + jugador/enemigos/llaves/
 * puertas/objetos). Corre hasta que se cierra la ventana. start_room en
 * hex (0x70 = arranque). */
void faithful_play(uint8_t start_room);

/* DEMO real (4AA4-4AC7): partida con el input grabado del ROM (0x7ABE)
 * y EAE4=1. Sale por fin del stream (EAE4 queda 1 → volver al título) o
 * por tecla (EAE4=0 → arrancar el juego). */
void faithful_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* CASTLE_GAME_H */
