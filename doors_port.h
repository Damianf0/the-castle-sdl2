#ifndef DOORS_PORT_H
#define DOORS_PORT_H
#include <stdint.h>

/* Puertas con requisito de llaves (detectadas del mapa real, doors_data.c).
 * Mientras está cerrada bloquea (sólida); se abre al tocarla con las llaves
 * del color y cantidad requeridos (que se descuentan). */
#define ROOMDOOR_MAX 6

typedef struct {
    uint8_t dcol, drow, dw, dh;  /* bbox en tiles de pantalla */
    uint8_t color;               /* índice de color requerido (0AM 1CY 2VE 3AZ) */
    uint8_t count;               /* llaves requeridas */
    uint8_t open;                /* 1 = ya abierta */
} RoomDoor;

extern RoomDoor g_door[ROOMDOOR_MAX];
extern int      g_door_n;

void doors_room_init(unsigned char room);
int  door_block(int srow, int scol);     /* 1=cerrada(sólida) -1=abierta(pasable) 0=no hay */
void doors_update(int px, int py, int pw, int ph);

#endif
