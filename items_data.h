#ifndef ITEMS_DATA_H
#define ITEMS_DATA_H
#include <stdint.h>
/* scol,srow = esquina sup-izq en tiles de pantalla (2x2); val = tipo 0xE3D6
 * (0x22 mapa, 0x23-0x26 power-ups, 0x27-0x29 puntos/comida) */
typedef struct { uint8_t scol, srow, val; } ItemSpawn;
#define ITEM_MAXPER 12
extern const uint8_t ITEM_COUNT[100];
extern const ItemSpawn ITEM_DATA[100][ITEM_MAXPER];
#endif
