#!/usr/bin/env python3
"""Diff estructurado de dos volcados de VRAM SCREEN 2 (16KB).
Uso: python tools/diff_vram.py <oraculo.bin> <port.bin>
Reporta diferencias por región y por tile/celda, para iterar la Fase 1."""
import sys

a = open(sys.argv[1], 'rb').read()
b = open(sys.argv[2], 'rb').read()

def diff_tiles(name, base, third):
    off = base + third * 0x800
    bad = [t for t in range(256)
           if a[off+t*8:off+t*8+8] != b[off+t*8:off+t*8+8]]
    if bad:
        print('  %s tercio %d: %d tiles difieren: %s%s' %
              (name, third, len(bad),
               ' '.join('%02X' % t for t in bad[:24]),
               ' ...' if len(bad) > 24 else ''))
    return len(bad)

def diff_name():
    bad = [(i // 32, i % 32) for i in range(768)
           if a[0x1800+i] != b[0x1800+i]]
    if bad:
        print('  name table: %d celdas difieren' % len(bad))
        rows = {}
        for r, c in bad:
            rows.setdefault(r, []).append(c)
        for r in sorted(rows):
            cells = rows[r]
            print('    fila %2d (%d): %s' % (r, len(cells),
                  ' '.join('c%d[%02X!=%02X]' % (c, a[0x1800+r*32+c], b[0x1800+r*32+c])
                           for c in cells[:10]) + (' ...' if len(cells) > 10 else '')))
    return len(bad)

total = 0
for t in range(3):
    total += diff_tiles('pattern', 0x0000, t)
for t in range(3):
    total += diff_tiles('color  ', 0x2000, t)
total += diff_name()
spr = sum(1 for i in range(0x1B00, 0x1B80) if a[i] != b[i])
print('  sprite attr: %d bytes difieren (informativo, no bloquea)' % spr)
print('TOTAL background: %d diferencias' % total)
sys.exit(1 if total else 0)
