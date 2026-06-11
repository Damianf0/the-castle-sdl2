#ifndef ITEMS_PORT_H
#define ITEMS_PORT_H
#include <stdint.h>

/* Ítems recogibles de la tabla 0xE3D6 (sub_5BB0): comida/tesoros/power-ups.
 * En el juego real se recogen al TOCARLOS y desaparecen — nunca son sólidos.
 * (Las llaves van aparte en keys_port; 0x20/0x21 = salidas, no se tocan acá.) */
#define ITEM_MAX 12

typedef struct {
    uint8_t active;       /* 1 = sin recoger (visible/horneado) */
    uint8_t val;          /* tipo 0xE3D6 (0x22..0x29) */
    uint8_t slot;         /* slot en la tabla 0xE3D6 (persistencia) */
    int     scol, srow;   /* tile de pantalla (esquina sup-izq, 2x2) */
} PortItem;

extern PortItem g_pitem[ITEM_MAX];
extern int      g_pitem_n;

void items_room_init(unsigned char room);
void items_update(int px, int py, int pw, int ph);
int  item_cell(int srow, int scol);   /* 1 si la celda es un ítem (nunca sólido) */

#endif
