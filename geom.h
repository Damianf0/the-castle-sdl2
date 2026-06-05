#pragma once
#ifndef CASTLE_GEOM_H
#define CASTLE_GEOM_H
#include <stdint.h>

typedef struct {
    uint8_t type;   /* 0x20-0x2F estructural/salida | 0x30-0x35 coleccionable | >=0x36 enemigo */
    uint8_t row;    /* fila de spawn (0..29) */
    uint8_t col;    /* columna (0..19/31) */
    uint8_t flags;  /* b1 crudo del stream (bit5/bit6 = variantes) */
} GeomObj;

extern GeomObj g_geom_objects[64];
extern int     g_geom_object_count;
extern uint8_t g_geom_room;

/* Decodifica la geometría de la sala -> g_map[row*20+col] (0=aire, !=0=pared)
 * y los objetos -> g_geom_objects[] / g_geom_object_count. */
void geom_decode_room(uint8_t room_x);

#endif
