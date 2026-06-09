#!/usr/bin/env python3
"""
Genera keys_data.c desde los volcados REALES de la tabla 0xE3D6 (llaves/ítems).

Entrada : e3d6_XX.bin  (100 archivos, 64 bytes c/u = 16 slots x 4 bytes
          [active, val, col, row]); XX = valor de sala en HEX (00..99).
          Generados por cap_e3d6.tcl en openMSX.
Salida  : keys_data.c  (KEY_COUNT[100] + KEY_DATA[100][KEY_MAXPER]).

Modelo (convertido del código del ROM, sub_5BB0 @0x5BB0):
  val >= 0x2A  => LLAVE; color logico = val - 0x2A
  (val 0x20-0x29 = salidas/mapa/power-ups/puntos -> NO son llaves, se ignoran)
  Posicion en pantalla = (col+1, row+4)  [misma formula que el jugador sub_6F4D]
  Tamano = 2x2 tiles (16x16 px)

Uso: python gen_keys_data.py
"""
import glob, os

rooms = [[] for _ in range(100)]
for f in sorted(glob.glob('e3d6_*.bin')):
    nm = os.path.basename(f)[5:7]          # "70" -> sala 0x70
    hi, lo = int(nm[0]), int(nm[1])
    if hi > 9 or lo > 9:
        continue
    idx = hi * 10 + lo                      # indice de sala 0..99 (ry*10+rx)
    e = open(f, 'rb').read()
    for s in range(16):
        active, val, col, row = e[s*4:s*4+4]
        if active and val >= 0x2A:           # LLAVE
            scol, srow = col + 1, row + 4
            if scol > 30 or srow > 22:
                continue
            rooms[idx].append((scol, srow, 2, 2, val - 0x2A))

total = sum(len(r) for r in rooms)
print(f'llaves: {total}  salas con llaves: {sum(1 for r in rooms if r)}  '
      f'max/sala: {max(len(r) for r in rooms)}')

out = ['#include "keys_data.h"', '', 'const uint8_t KEY_COUNT[100] = {']
ln = '    '
for i in range(100):
    ln += str(len(rooms[i])) + ','
    if (i + 1) % 20 == 0:
        out.append(ln); ln = '    '
out += ['};', '', 'const KeySpawn KEY_DATA[100][KEY_MAXPER] = {']
for i in range(100):
    if rooms[i]:
        cells = ','.join('{%d,%d,%d,%d,%d}' % k for k in rooms[i])
        out.append('    [%d]={%s},' % (i, cells))
out.append('};')
open('keys_data.c', 'w').write('\n'.join(out) + '\n')
print('keys_data.c escrito')
