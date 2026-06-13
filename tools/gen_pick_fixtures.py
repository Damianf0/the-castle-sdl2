#!/usr/bin/env python3
"""Genera fixtures-oraculo del PICKUP por celda (sub_5B96/5BB0): el jugador
spawnea al lado de los items y camina sobre ellos (hold). Traza por frame los
slots e3d6 + score + llaves + vidas -> tests/fixtures/pick/pick_XX.txt.
Primera linea: "# room XX pcol P prow R hold H from F".
Uso: python tools/gen_pick_fixtures.py [salaXX ...]"""
import os, subprocess, sys

OPENMSX = os.path.join('..', '_buildtools', 'openmsx', 'openmsx.exe')
FRAMES = 300

# nombre -> (sala, pcol, prow, dir, holdfrom, givemap):
#   04:    4 comidas 0x27 en fila 16 (score BCD + sprite de puntos)
#   00:    2 llaves 0x2F en fila 17 (inventario E337+ y HUD)
#   02:    vida extra 0x26 en (26,17)
#   70map: pickup del MAPA 0x22 (spawn sobre el item) → minimapa completo
#   70out: salir de la sala con el mapa → marca de visitada (cyan)
# Cada escenario también valida la VRAM del HUD (name filas 0-4 + colores
# de los chars 0x00-0x3F, minimapa incluido) al final de la traza.
SCENARIOS = {
    '04':    ('04', 0, 16, 'R', 4, 0),
    '00':    ('00', 18, 17, 'R', 4, 0),
    '02':    ('02', 20, 17, 'R', 4, 0),
    '70map': ('70', 24, 5, 'R', 4, 0),
    '70out': ('70', 2, 17, 'L', 4, 1),
}


def main():
    names = sys.argv[1:] or sorted(SCENARIOS)
    outdir = os.path.join('tests', 'fixtures', 'pick')
    os.makedirs(outdir, exist_ok=True)
    for name in names:
        xx, pcol, prow, hdir, hfrom, gmap = SCENARIOS[name]
        bcd = int(xx, 16)
        with open('tr_pick_in.txt', 'w') as f:
            f.write('%d %d %d %d %s %d %d'
                    % (bcd, pcol, prow, FRAMES, hdir, hfrom, gmap))
        for fn in ('tr_pick_out.txt', 'tr_pick_done.txt', 'tr_pick_err.txt',
                   'tr_pick_vram.txt'):
            if os.path.exists(fn):
                os.remove(fn)
        print('%s (sala %s): jugador en (%d,%d) hold %s%s...'
              % (name, xx, pcol, prow, hdir, ' +mapa' if gmap else ''),
              flush=True)
        subprocess.run([OPENMSX, '-carta', 'the_castle.rom',
                        '-script', os.path.join('tools', 'tr_pick.tcl')],
                       capture_output=True)
        if os.path.exists('tr_pick_err.txt'):
            raise SystemExit('%s: %s' % (name, open('tr_pick_err.txt').read()))
        if not os.path.exists('tr_pick_out.txt'):
            raise SystemExit('%s: openMSX no produjo salida' % name)
        out = os.path.join(outdir, 'pick_%s.txt' % name)
        with open(out, 'w') as f:
            f.write('# room %s pcol %d prow %d hold %s from %d map %d\n'
                    % (xx, pcol, prow, hdir, hfrom, gmap))
            f.write(open('tr_pick_out.txt').read())
        with open(os.path.join(outdir, 'pick_%s_vram.txt' % name), 'w') as f:
            f.write(open('tr_pick_vram.txt').read())
        n = sum(1 for _ in open(out)) - 1
        print('  -> %s (%d frames + vram)' % (out, n), flush=True)
    for fn in ('tr_pick_in.txt', 'tr_pick_out.txt', 'tr_pick_done.txt',
               'tr_pick_vram.txt'):
        if os.path.exists(fn):
            os.remove(fn)


if __name__ == '__main__':
    main()
