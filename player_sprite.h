#ifndef PLAYER_SPRITE_H
#define PLAYER_SPRITE_H
#include <stdint.h>
/* Sprites reales del jugador, extraidos del ROM (pattern table @0x9b96).
 * Cada uno = 3 planos OR con sus colores MSX. 16x16, indice de color (0=transparente). */
#define PL_FRAMES 8
#define PLF_STAND 0
#define PLF_WALK_L0 1
#define PLF_WALK_L1 2
#define PLF_WALK_R0 3
#define PLF_WALK_R1 4
#define PLF_WALK_R2 5
#define PLF_JUMP_L 6
#define PLF_JUMP_R 7
extern const uint8_t PLAYER_SPR[PL_FRAMES][16][16];
#endif
