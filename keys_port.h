#ifndef KEYS_PORT_H
#define KEYS_PORT_H
#include <stdint.h>

/* Llaves de colores: tiles con forma de llave repartidos por los cuartos
 * (detectados de la VRAM real, keys_data.c). Se recogen al tocarlas y se
 * acumulan por color en el inventario/HUD. NO son la tabla COLL (esos son
 * bloques empujables). */
#define KEY_MAX    10
#define KEY_COLORS 6   /* 0AM 1CY 2VE 3AZ 4RO 5MA */

typedef struct {
    uint8_t active;          /* 1 = sin recoger (visible/horneada) */
    uint8_t color;           /* índice de color (0AM 1CY 2VE 3AZ) */
    int     scol, srow;      /* tile de pantalla (esquina sup-izq) */
    int     sw, sh;          /* tamaño en tiles */
} PortKey;

extern PortKey g_pkey[KEY_MAX];
extern int     g_pkey_n;
extern int     g_key_inv[KEY_COLORS];     /* llaves recogidas por color */
extern uint8_t g_key_color[KEY_COLORS];   /* color MSX por índice (HUD) */
extern const uint8_t  KEY_COLMSX[KEY_COLORS]; /* color MSX real por color lógico */
extern const uint16_t KEY_BMP[16];        /* bitmap 16x16 de la llave (real ROM) */

void keys_room_init(unsigned char room);
void keys_update(int px, int py, int pw, int ph);
int  key_cell(int srow, int scol);   /* 1 si la celda es una llave (nunca sólida) */

#endif
