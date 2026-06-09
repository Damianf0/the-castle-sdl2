#!/usr/bin/env python3
"""
Genera colmap_data.c desde los volcados REALES del tilemap de colision 0xE496.

Entrada : colmap_XX.bin (100 archivos, 900 bytes; los primeros 600 = el campo
          20 filas x 30 cols; los otros 300 = tabla paralela 0xE6EE celda->objeto).
Salida  : colmap_data.c — COLMAP[100][20][30].

Semantica REAL de las celdas (verificada salas 0x70 y 0x00 contra todas las
tablas de objetos):
  0x00 aire | 0xE0 pared/piso | 0xA0/0xA2 puerta(2x3)/escalon | 0xA8 bloque
  0x38 enemigo | 0x24/0x20 recogible (llave/item)
  bit 0x80 = BLOQUEA al jugador (E0, A0, A2, A8 lo tienen; 38, 24, 20 no).
Mapeo a pantalla: screen_col = field_col + 1, screen_row = field_row + 4.

Uso: python gen_colmap_data.py
"""
import glob, os

maps = {}
for f in sorted(glob.glob('colmap_*.bin')):
    nm = os.path.basename(f)[7:9]
    hi, lo = int(nm[0]), int(nm[1])
    if hi > 9 or lo > 9:
        continue
    idx = hi * 10 + lo
    maps[idx] = open(f, 'rb').read()[:600]

assert len(maps) == 100, f'faltan salas: {100-len(maps)}'

out = ['#include "colmap_data.h"', '',
       'const uint8_t COLMAP[100][20][30] = {']
for idx in range(100):
    d = maps[idx]
    out.append('  { /* sala %d%d (0x%X%X) */' % (idx//10, idx%10, idx//10, idx%10))
    for r in range(20):
        row = d[r*30:(r+1)*30]
        out.append('    {' + ','.join('0x%02X' % b for b in row) + '},')
    out.append('  },')
out.append('};')
open('colmap_data.c', 'w').write('\n'.join(out) + '\n')
print('colmap_data.c escrito (100 salas x 20x30)')
