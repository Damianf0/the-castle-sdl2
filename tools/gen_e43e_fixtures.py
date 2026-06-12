#!/usr/bin/env python3
"""Genera fixtures-oraculo de los ESTRUCTURALES e43e (cintas 0x0C/0x0D,
fuego 0x0F, ascensores 0x1C/0x1D, trampas 0x1F): corre openMSX
(tools/tr_e43e.tcl) por sala con el jugador fijado y guarda la traza por
frame de los 16 slots e43e en tests/fixtures/e43e_tr/e43e_tr_XX.txt.
Primera linea: "# room XX pcol P prow R".
Uso: python tools/gen_e43e_fixtures.py [salaXX ...]"""
import os, subprocess, sys

OPENMSX = os.path.join('..', '_buildtools', 'openmsx', 'openmsx.exe')
ROOMS = ['00', '01', '02', '03', '06', '07', '16', '33', '36', '42']
FRAMES = 300

# Escenarios de EMPUJE (pistones 0x1B): sala -> (pcol, prow, dir, holdfrom).
# El jugador arranca al lado de la trampa COLL 0x34 y mantiene la direccion.
PUSH = {'01': (10, 5, 'R', 4)}


def pick_cell(xx):
    """celda libre 2x2 con piso debajo, lejos de los bordes (= gen_bats)"""
    d = open('tests/fixtures/colmap/colmap_%s.bin' % xx, 'rb').read()
    cm = lambda b, c: d[c * 30 + b]
    best = None
    for c in range(17, 0, -1):
        for b in range(3, 26):
            if any(cm(b + db, c + dc) for db in (0, 1) for dc in (0, 1)):
                continue
            if not (cm(b, c + 2) & 0x40 and cm(b + 1, c + 2) & 0x40):
                continue
            cand = (abs(b - 14), b, c)
            if best is None or cand < best:
                best = cand
        if best:
            return best[1], best[2]
    raise SystemExit('sala %s: sin celda libre para el jugador' % xx)


def main():
    rooms = sys.argv[1:] or ROOMS
    outdir = os.path.join('tests', 'fixtures', 'e43e_tr')
    os.makedirs(outdir, exist_ok=True)
    for xx in rooms:
        hold = PUSH.get(xx)
        if hold:
            pcol, prow = hold[0], hold[1]
        else:
            pcol, prow = pick_cell(xx)
        bcd = int(xx, 16)
        with open('tr_e43e_in.txt', 'w') as f:
            f.write('%d %d %d %d' % (bcd, pcol, prow, FRAMES))
            if hold:
                f.write(' %s %d' % (hold[2], hold[3]))
        for fn in ('tr_e43e_out.txt', 'tr_e43e_done.txt', 'tr_e43e_err.txt'):
            if os.path.exists(fn):
                os.remove(fn)
        print('sala %s: jugador en (%d,%d)...' % (xx, pcol, prow), flush=True)
        subprocess.run([OPENMSX, '-carta', 'the_castle.rom',
                        '-script', os.path.join('tools', 'tr_e43e.tcl')],
                       capture_output=True)
        if os.path.exists('tr_e43e_err.txt'):
            raise SystemExit('sala %s: %s' % (xx, open('tr_e43e_err.txt').read()))
        if not os.path.exists('tr_e43e_out.txt'):
            raise SystemExit('sala %s: openMSX no produjo salida' % xx)
        out = os.path.join(outdir, 'e43e_tr_%s.txt' % xx)
        with open(out, 'w') as f:
            hdr = '# room %s pcol %d prow %d' % (xx, pcol, prow)
            if hold:
                hdr += ' hold %s from %d' % (hold[2], hold[3])
            f.write(hdr + '\n')
            f.write(open('tr_e43e_out.txt').read())
        n = sum(1 for _ in open(out)) - 1
        print('  -> %s (%d frames)' % (out, n), flush=True)
    for fn in ('tr_e43e_in.txt', 'tr_e43e_out.txt', 'tr_e43e_done.txt'):
        if os.path.exists(fn):
            os.remove(fn)


if __name__ == '__main__':
    main()
