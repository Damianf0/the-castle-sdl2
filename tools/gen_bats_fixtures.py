#!/usr/bin/env python3
"""Genera fixtures-oraculo del motor BAT: corre openMSX (tools/tr_bats.tcl)
por sala con el jugador fijado en una celda libre (elegida del colmap) y
guarda la traza frame a frame de los 8 slots (col,fila,flags) en
tests/fixtures/bats/bats_XX.txt. Primera linea: "# room XX pcol P prow R".
Uso: python tools/gen_bats_fixtures.py [salaXX ...]"""
import os, subprocess, sys, time

OPENMSX = os.path.join('..', '_buildtools', 'openmsx', 'openmsx.exe')
ROOMS = ['01', '09', '16', '18', '29', '32', '43', '49', '81', '98']
FRAMES = 300


def pick_cell(xx):
    """celda libre 2x2 con piso debajo, lejos de los bordes"""
    d = open('tests/fixtures/colmap/colmap_%s.bin' % xx, 'rb').read()
    cm = lambda b, c: d[c * 30 + b]
    best = None
    for c in range(17, 0, -1):          # de abajo hacia arriba
        for b in range(3, 26):
            if any(cm(b + db, c + dc) for db in (0, 1) for dc in (0, 1)):
                continue
            if not (cm(b, c + 2) & 0x40 and cm(b + 1, c + 2) & 0x40):
                continue
            # preferir celdas centradas
            cand = (abs(b - 14), b, c)
            if best is None or cand < best:
                best = cand
        if best:
            return best[1], best[2]
    raise SystemExit('sala %s: sin celda libre para el jugador' % xx)


def main():
    rooms = sys.argv[1:] or ROOMS
    outdir = os.path.join('tests', 'fixtures', 'bats')
    os.makedirs(outdir, exist_ok=True)
    for xx in rooms:
        pcol, prow = pick_cell(xx)
        bcd = int(xx, 16)
        with open('tr_bats_in.txt', 'w') as f:
            f.write('%d %d %d %d' % (bcd, pcol, prow, FRAMES))
        for fn in ('tr_bats_out.txt', 'tr_bats_done.txt', 'tr_bats_err.txt'):
            if os.path.exists(fn):
                os.remove(fn)
        print('sala %s: jugador en (%d,%d)...' % (xx, pcol, prow), flush=True)
        subprocess.run([OPENMSX, '-carta', 'the_castle.rom',
                        '-script', os.path.join('tools', 'tr_bats.tcl')],
                       capture_output=True)
        if os.path.exists('tr_bats_err.txt'):
            raise SystemExit('sala %s: %s' % (xx, open('tr_bats_err.txt').read()))
        if not os.path.exists('tr_bats_out.txt'):
            raise SystemExit('sala %s: openMSX no produjo salida' % xx)
        out = os.path.join(outdir, 'bats_%s.txt' % xx)
        with open(out, 'w') as f:
            f.write('# room %s pcol %d prow %d\n' % (xx, pcol, prow))
            f.write(open('tr_bats_out.txt').read())
        n = sum(1 for _ in open(out)) - 1
        print('  -> %s (%d frames)' % (out, n), flush=True)
    for fn in ('tr_bats_in.txt', 'tr_bats_out.txt', 'tr_bats_done.txt'):
        if os.path.exists(fn):
            os.remove(fn)


if __name__ == '__main__':
    main()
