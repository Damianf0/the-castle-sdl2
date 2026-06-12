#ifndef PLAYER_H
#define PLAYER_H
#include <stdint.h>

/* Port fiel del JUGADOR (Fase 3): sub_40BB (flags de colision/intencion) +
 * sub_6F5C (aplicador de movimiento en medios pasos de 4px) + el timing real
 * de input (sub_4064/sub_50E8: poll GTSTCK/GTTRIG solo en frames pares).
 * Opera sobre el espejo de RAM del room_loader (0xE334/0xE335 col/fila,
 * 0xEAD6 fase de salto, 0xEAC9 contador de frame, 0xEAE1 salida de sala).
 * Validado frame a frame contra tests/fixtures/traces/. */

/* Estado leido por el render (posicion del sprite, igual que la attr table) */
extern int g_plr_px;      /* sprite X (pixeles) */
extern int g_plr_py;      /* sprite Y (= pixel display - 1, como el TMS) */
extern int g_plr_frame;   /* frame de animacion 0..9 (patron sprite = f*12) */

/* Un frame completo del jugador.
 * stick = GTSTCK (0=nada 1=N 3=E 5=S 7=O), trig = GTTRIG (0/1).
 * El poll real solo toma efecto en frames pares (30Hz). */
void player_frame(uint8_t stick, uint8_t trig);

/* Incremento del contador de frame 0xEAC9 (40AF) — llamar al FINAL del
 * frame completo (tras bloques/bats/daño, que comparten la paridad). */
void player_end_frame(void);

/* 0x62FA: velocidad por teclas de sistema (row6 = fila 6 de la matriz MSX,
 * activo-bajo). CTRL=correr (EACA 0x70→0x30), CTRL+GRAPH=turbo (0x01);
 * escribe también transpose (0xEAF1) y tempo (0xEAF3) de la música. */
void player_speed_frame(uint8_t row6);

/* Duración real de una iteración del game loop según 0xEACA (sub_5128,
 * medida en openMSX con tools/tr_speed.tcl): para hal_wait_game_frame(). */
double player_frame_ms(void);

/* Devuelve y CONSUME el borde de salida (0xEAE1): 0=no, 1=arriba, 3=der,
 * 5=abajo, 7=izq. */
uint8_t player_take_exit(void);

/* Recalcula el pixel desde 0xE334/0xE335 (tras cargar sala) y resetea el
 * sprite (sub_6F45 + sub_6F27 con frame 0). */
void player_sync_pixel(void);

/* sub_442D: motor por frame de los estructurales e43e: cintas 0x0C/0x0D,
 * fuego intermitente 0x0F (cada frame) y ASCENSORES 0x1C/0x1D (cada 8
 * frames: mueven su piso por el colmap, cargan al jugador/bloques y
 * aplastan). Llamar ANTES de player_coll_frame (orden del game loop real). */
void player_elev_frame(void);

/* sub_4406: trampas móviles 0x1F (pinchos deslizantes de 2 celdas).
 * Llamar DESPUÉS de player_frame y ANTES de player_bats_frame. */
void player_traps_frame(void);

/* sub_434A: motor por frame de los objetos COLL (bloques empujables con
 * gravedad y derrape por rampas, trampas 0x34). Llamar ANTES de
 * player_frame en cada frame (orden del game loop real). */
void player_coll_frame(void);

/* sub_438D: motor por frame de los ENEMIGOS (tabla BAT 0xE416): cerebros
 * por tipo + movedor con animación. Llamar DESPUÉS de player_frame. */
void player_bats_frame(void);

/* sub_5A2D: detección de contacto letal (frames impares, 4 celdas del
 * cuerpo, invulnerable con el power-up 0xE343). 1 = murió. */
int player_check_death(void);

/* sub_5A63: secuencia de muerte (bloqueante, consume frames con vsync):
 * agonía + caída + vidas-- + commit + EAE0=1. Tras llamar: si quedan vidas
 * (0xE324), limpiar 0xEAE0 y recargar la sala (respawn en la entrada). */
void player_death_run(void);

#endif
