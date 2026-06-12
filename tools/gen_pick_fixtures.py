#!/usr/bin/env python3
"""Genera fixtures-oraculo del PICKUP por celda (sub_5B96/5BB0): el jugador
spawnea al lado de los items y camina sobre ellos (hold). Traza por frame los
slots e3d6 + score + llaves + vidas -> tests/fixtures/pick/pick_XX.txt.
Primera linea: "# room XX pcol P prow R hold H from F".
Uso: python tools/gen_pick_fixtures.py [salaXX ...]"""
import os, subprocess, sys

OPENMSX = os.path.join('..', '_buildtools', 'openmsx', 'openmsx.exe')
FRAMES = 300

# sala -> (pcol, prow, dir, holdfrom). Escenarios SIN pozos ni salidas:
#   04: 4 comidas 0x27 en fila 16 (score BCD + sprite de puntos)
#   00: 2 llaves 0x2F en fila 17 (inventario E337+ y HUD)
#   02: vida extra 0x26 en (26,17)
SCENARIOS = {
    '04': (0, 16, 'R', 4),
    '00': (18, 17, 'R', 4),
    '02': (20, 17, 'R', 4),
}


def main():
    rooms = sys.argv[1:] or sorted(SCENARIOS)
    outdir = os.path.join('tests', 'fixtures', 'pick')
    os.makedirs(outdir, exist_ok=True)
    for xx in rooms:
        pcol, prow, hdir, hfrom = SCENARIOS[xx]
        bcd = int(xx, 16)
        with open('tr_pick_in.txt', 'w') as f:
            f.write('%d %d %d %d %s %d' % (bcd, pcol, prow, FRAMES, hdir, hfrom))
        for fn in ('tr_pick_out.txt', 'tr_pick_done.txt', 'tr_pick_err.txt'):
            if os.path.exists(fn):
                os.remove(fn)
        print('sala %s: jugador en (%d,%d) hold %s...' % (xx, pcol, prow, hdir),
              flush=True)
        subprocess.run([OPENMSX, '-carta', 'the_castle.rom',
                        '-script', os.path.join('tools', 'tr_pick.tcl')],
                       capture_output=True)
        if os.path.exists('tr_pick_err.txt'):
            raise SystemExit('sala %s: %s' % (xx, open('tr_pick_err.txt').read()))
        if not os.path.exists('tr_pick_out.txt'):
            raise SystemExit('sala %s: openMSX no produjo salida' % xx)
        out = os.path.join(outdir, 'pick_%s.txt' % xx)
        with open(out, 'w') as f:
            f.write('# room %s pcol %d prow %d hold %s from %d\n'
                    % (xx, pcol, prow, hdir, hfrom))
            f.write(open('tr_pick_out.txt').read())
        n = sum(1 for _ in open(out)) - 1
        print('  -> %s (%d frames)' % (out, n), flush=True)
    for fn in ('tr_pick_in.txt', 'tr_pick_out.txt', 'tr_pick_done.txt'):
        if os.path.exists(fn):
            os.remove(fn)


if __name__ == '__main__':
    main()
