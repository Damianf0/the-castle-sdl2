#ifndef DOORS_DATA_H
#define DOORS_DATA_H
#include <stdint.h>
/* dcol,drow = esquina sup-izq en tiles de pantalla; dw,dh tamaño; color=indice(0AM 1CY 2VE 3AZ); count=llaves requeridas */
typedef struct { uint8_t dcol,drow,dw,dh,color,count; } DoorDef;
#define DOOR_MAXROOM 100
#define DOOR_MAXPER 6
extern const uint8_t DOOR_COUNT[100];
extern const DoorDef DOOR_DATA[100][DOOR_MAXPER];
#endif
