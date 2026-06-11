#!/usr/bin/env python3
"""Compara el dump del decoder portado contra los fixtures de openMSX.
Uso: python tools/diff_dump.py <dumpdir> [salaXX]"""
import sys, os

DUMP = sys.argv[1]
ONLY = sys.argv[2] if len(sys.argv) > 2 else None
FIX = os.path.join('tests', 'fixtures')

SETS = [('colmap', 'colmap', 900), ('e346', 'e346', 64), ('e3d6', 'e3d6', 64),
        ('e43e', 'e43e', 80), ('objs', 'objs', 288), ('ont', 'ont', 768),
        ('vram', 'vram', 0x3800)]   # pattern+name+color (los sprites 0x3800+ no)

tot_bad = {}
for idx in range(100):
    xx = '%02X' % (((idx // 10) << 4) | (idx % 10))
    if ONLY and xx != ONLY:
        continue
    for name, fdir, size in SETS:
        a = open(os.path.join(FIX, fdir, '%s_%s.bin' % (fdir, xx)), 'rb').read()[:size]
        b = open(os.path.join(DUMP, '%s_%s.bin' % (name, xx)), 'rb').read()[:size]
        if name == 'vram':
            # solo pattern (0-0x17FF), name (0x1800-0x1AFF) y color (0x2000+):
            # 0x1B00-0x1FFF tiene la sprite attr table viva del oraculo
            rng = list(range(0, 0x1B00)) + list(range(0x2000, min(len(a), len(b))))
            bad = [i for i in rng if a[i] != b[i]]
        else:
            bad = [i for i in range(min(len(a), len(b))) if a[i] != b[i]]
        if bad:
            tot_bad.setdefault(name, []).append((xx, len(bad)))
            if ONLY:
                print('%s sala %s: %d bytes difieren' % (name, xx, len(bad)))
                for i in bad[:20]:
                    print('   off %3d (0x%03X): oraculo %02X != port %02X' % (i, i, a[i], b[i]))
                if len(bad) > 20:
                    print('   ... y %d mas' % (len(bad) - 20))

if not ONLY:
    ok = True
    for name, _, _ in SETS:
        rooms = tot_bad.get(name, [])
        if rooms:
            ok = False
            worst = sorted(rooms, key=lambda r: -r[1])[:6]
            print('%-7s: %3d salas con diffs; peores: %s' %
                  (name, len(rooms), ' '.join('%s(%d)' % w for w in worst)))
        else:
            print('%-7s: 100/100 OK' % name)
    sys.exit(0 if ok else 1)
