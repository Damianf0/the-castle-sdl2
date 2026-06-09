#ifndef BLOCKS_DATA_H
#define BLOCKS_DATA_H
#include <stdint.h>
typedef struct { uint8_t scol,srow; } BlockSpawn;  /* esquina sup-izq en tiles de pantalla, bloque 2x2 */
#define BLOCK_MAXPER 8
extern const uint8_t BLOCK_COUNT[100];
extern const BlockSpawn BLOCK_DATA[100][BLOCK_MAXPER];
#endif
