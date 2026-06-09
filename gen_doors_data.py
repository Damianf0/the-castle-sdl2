#!/usr/bin/env python3
"""
Genera doors_data.c desde los volcados REALES de la tabla 0xE346 (puertas/gates).

Entrada : e346_XX.bin  (100 archivos, 64 bytes c/u = 16 slots x 4 bytes
          [active, val, col, row]); XX = valor de sala en HEX (00..99).
          Generados por cap_e346.tcl en openMSX.
Salida  : doors_data.c (DOOR_COUNT[100] + DOOR_DATA[100][DOOR_MAXPER]).

Modelo (convertido del código del ROM, sub_758C @0x758C; la puerta se localiza
        via sub_4325: tile-puerta -> indice en 0xE6EE -> objeto en 0xE346):
  color requerido = (val & 0x0F) - 1     (mismo espacio que la llave: 0xE337[color])
  count = 1 SIEMPRE (sub_758C hace un solo DEC)
  high-nibble (val>>4) = variante/orientacion del grafico
  Posicion en pantalla = (col+1, row+4)  [misma formula que llaves/jugador]
  Tamano = 2x3 tiles (16x24 px)
  Las gates en col0/col28 = pasajes laterales con candado entre salas.

Colores logicos compartidos llave<->puerta (0xE337):
  0=azul 1=(sin llaves) 2=magenta 3=verde 4=cyan 5=amarillo

Uso: python gen_doors_data.py
"""
import glob, os
from collections import Counter

rooms = [[] for _ in range(100)]
for f in sorted(glob.glob('e346_*.bin')):
    nm = os.path.basename(f)[5:7]
    hi, lo = int(nm[0]), int(nm[1])
    if hi > 9 or lo > 9:
        continue
    idx = hi * 10 + lo
    e = open(f, 'rb').read()
    for s in range(16):
        active, val, col, row = e[s*4:s*4+4]
        if active and val:
            color = (val & 0x0F) - 1
            if color < 0 or color > 5:
                continue
            dcol, drow = col + 1, row + 4
            if dcol > 30 or drow > 21:
                continue
            rooms[idx].append((dcol, drow, 2, 3, color, 1))

total = sum(len(r) for r in rooms)
cc = Counter(g[4] for rm in rooms for g in rm)
print(f'gates: {total}  salas: {sum(1 for r in rooms if r)}  '
      f'max/sala: {max(len(r) for r in rooms)}  colores: {dict(sorted(cc.items()))}')

out = ['#include "doors_data.h"', '', 'const uint8_t DOOR_COUNT[100] = {']
ln = '    '
for i in range(100):
    ln += str(len(rooms[i])) + ','
    if (i + 1) % 20 == 0:
        out.append(ln); ln = '    '
out += ['};', '', 'const DoorDef DOOR_DATA[100][DOOR_MAXPER] = {']
for i in range(100):
    if rooms[i]:
        cells = ','.join('{%d,%d,%d,%d,%d,%d}' % g for g in rooms[i])
        out.append('    [%d]={%s},' % (i, cells))
out.append('};')
open('doors_data.c', 'w').write('\n'.join(out) + '\n')
print('doors_data.c escrito')
