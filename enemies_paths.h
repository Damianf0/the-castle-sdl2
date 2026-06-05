#ifndef ENEMIES_PATHS_H
#define ENEMIES_PATHS_H
#include <stdint.h>
typedef struct { uint8_t type,sr,sc,plen; uint16_t poff; } PathEnemy;
extern const uint16_t PATH_ROOM_OFF[100];
extern const uint8_t  PATH_ROOM_CNT[100];
extern const PathEnemy PATH_ENEMIES[148];
extern const uint8_t PATH_POS[4537][2];
#endif
