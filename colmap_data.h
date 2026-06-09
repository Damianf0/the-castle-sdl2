#ifndef COLMAP_DATA_H
#define COLMAP_DATA_H
#include <stdint.h>
/* Tilemap de colisión REAL del ROM (RAM 0xE496), campo 20 filas x 30 cols.
 * Pantalla: screen_col = field_col + 1, screen_row = field_row + 4.
 * Celdas: 0x00 aire, 0xE0 pared/piso, 0xA0/0xA2 puerta/escalón, 0xA8 bloque,
 * 0x38 enemigo, 0x24/0x20 recogible. bit 0x80 = bloquea al jugador. */
#define CM_ROWS 20
#define CM_COLS 30
#define CM_SOLID 0x80u   /* bit que bloquea al jugador */
#define CM_BLOCK 0xA8u   /* celda de bloque empujable */
extern const uint8_t COLMAP[100][CM_ROWS][CM_COLS];
#endif
