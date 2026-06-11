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

/* Devuelve y CONSUME el borde de salida (0xEAE1): 0=no, 1=arriba, 3=der,
 * 5=abajo, 7=izq. */
uint8_t player_take_exit(void);

/* Recalcula el pixel desde 0xE334/0xE335 (tras cargar sala) y resetea el
 * sprite (sub_6F45 + sub_6F27 con frame 0). */
void player_sync_pixel(void);

#endif
