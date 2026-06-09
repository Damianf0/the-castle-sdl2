#ifndef KEYS_DATA_H
#define KEYS_DATA_H
#include <stdint.h>
typedef struct { uint8_t scol,srow,sw,sh,color; } KeySpawn;
#define KEY_MAXROOM 100
#define KEY_MAXPER 10
extern const uint8_t KEY_COUNT[100];
extern const KeySpawn KEY_DATA[100][KEY_MAXPER];
#endif
