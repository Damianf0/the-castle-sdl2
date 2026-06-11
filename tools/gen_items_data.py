#!/usr/bin/env python3
"""
Genera items_data.c desde los volcados REALES de la tabla 0xE3D6.

Items = val 0x22-0x29 (sub_5BB0 @0x5BB0): 0x22 revela el mapa, 0x23-0x26
power-ups, 0x27-0x29 puntos (comida/tesoro). En el juego real se recogen al
TOCARLOS y desaparecen â€” NO son solidos. (0x20/0x21 = salidas, NO se tocan
aca; >=0x2A = llaves, van en keys_data.)

Posicion pantalla = (col+1, row+4), tamano 2x2 (igual que llaves).
Uso: python gen_items_data.py
"""
import glob, os
from collections import Counter

rooms = [[] for _ in range(100)]
vals = Counter()
for f in sorted(glob.glob('tests/fixtures/e3d6/e3d6_*.bin')):
    nm = os.path.basename(f)[5:7]
    hi, lo = int(nm[0]), int(nm[1])
    if hi > 9 or lo > 9:
        continue
    idx = hi * 10 + lo
    e = open(f, 'rb').read()
    for s in range(16):
        active, val, col, row = e[s*4:s*4+4]
        if active and 0x22 <= val <= 0x29:
            scol, srow = col + 1, row + 4
            if scol > 30 or srow > 22:
                continue
            rooms[idx].append((scol, srow, val))
            vals[val] += 1

total = sum(len(r) for r in rooms)
print(f'items: {total}  salas: {sum(1 for r in rooms if r)}  '
      f'max/sala: {max(len(r) for r in rooms)}')
print('por val:', {hex(k): v for k, v in sorted(vals.items())})

out = ['#include "items_data.h"', '', 'const uint8_t ITEM_COUNT[100] = {']
ln = '    '
for i in range(100):
    ln += str(len(rooms[i])) + ','
    if (i + 1) % 20 == 0:
        out.append(ln); ln = '    '
out += ['};', '', 'const ItemSpawn ITEM_DATA[100][ITEM_MAXPER] = {']
for i in range(100):
    if rooms[i]:
        cells = ','.join('{%d,%d,0x%02X}' % it for it in rooms[i])
        out.append('    [%d]={%s},' % (i, cells))
out.append('};')
open('items_data.c', 'w').write('\n'.join(out) + '\n')
print('items_data.c escrito')
