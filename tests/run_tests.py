#!/usr/bin/env python3
"""
Harness de regresión contra el oráculo (dumps de openMSX en tests/fixtures/).

Uso (desde la raíz del repo):
    python tests/run_tests.py            # compila + corre todo
    python tests/run_tests.py --no-build # usa el exe ya compilado

Qué verifica:
  1. build       : build.ps1 compila sin errores.
  2. colmap      : CASTLE_DUMP colmap_XX.bin == fixture colmap/colmap_XX.bin[:600]
                   (campo de colisión 20x30, RAM 0xE496) para las 100 salas.
  3. doors       : CASTLE_DUMP doors_XX.txt == decode del fixture e346/ (reglas
                   de sub_758C: color=(val&0xF)-1, pos=(col+1,row+4), 2x3, count 1).
  4. keys        : keys_XX.txt == decode de e3d6/ (val>=0x2A, color=val-0x2A, 2x2).
  5. items       : items_XX.txt == decode de e3d6/ (0x22<=val<=0x29).
  6. screenshot  : CASTLE_SHOT produce un BMP válido y no uniforme (smoke).

Hoy los dumps salen de tablas generadas DESDE los fixtures, así que 2-5 son
verdes por construcción. El punto es que cuando el room loader portado
(sub_64DD, Fase 2 del plan) reemplace esas tablas, esto DEBE seguir verde.
"""
import os, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIX = os.path.join(ROOT, 'tests', 'fixtures')
EXE = os.path.join(ROOT, 'the_castle.exe')

def room_name(idx):
    """idx 0..99 -> nombre BCD 'XX' usado en los archivos (hi=fila, lo=col)."""
    return '%02X' % (((idx // 10) << 4) | (idx % 10))

def decode_doors(e346):
    out = []
    for s in range(16):
        active, val, col, row = e346[s*4:s*4+4]
        if active and val:
            color = (val & 0x0F) - 1
            if color < 0 or color > 5:
                continue
            dcol, drow = col + 1, row + 4
            if dcol > 30 or drow > 21:
                continue
            out.append('%d %d 2 3 %d 1' % (dcol, drow, color))
    return out

def decode_keys(e3d6):
    out = []
    for s in range(16):
        active, val, col, row = e3d6[s*4:s*4+4]
        if active and val >= 0x2A:
            scol, srow = col + 1, row + 4
            if scol > 30 or srow > 22:
                continue
            out.append('%d %d 2 2 %d' % (scol, srow, val - 0x2A))
    return out

def decode_items(e3d6):
    out = []
    for s in range(16):
        active, val, col, row = e3d6[s*4:s*4+4]
        if active and 0x22 <= val <= 0x29:
            scol, srow = col + 1, row + 4
            if scol > 30 or srow > 22:
                continue
            out.append('%d %d 0x%02X' % (scol, srow, val))
    return out

def read_lines(path):
    with open(path) as f:
        return [ln.strip() for ln in f if ln.strip()]

def run(results):
    def check(name, fn):
        try:
            errs = fn()
        except Exception as e:
            errs = ['excepción: %r' % e]
        results.append((name, errs))
        tag = 'PASS' if not errs else 'FAIL'
        print('[%s] %s' % (tag, name))
        for e in errs[:8]:
            print('       ' + e)
        if len(errs) > 8:
            print('       ... y %d más' % (len(errs) - 8))

    # --- 1. build -----------------------------------------------------------
    if '--no-build' not in sys.argv:
        def t_build():
            r = subprocess.run(
                ['powershell', '-ExecutionPolicy', 'Bypass', '-File',
                 os.path.join(ROOT, 'build.ps1')],
                cwd=ROOT, capture_output=True, text=True)
            return [] if r.returncode == 0 else [r.stdout[-2000:] + r.stderr[-2000:]]
        check('build', t_build)
        if results[-1][1]:
            return

    if not os.path.exists(EXE):
        results.append(('exe', ['no existe %s' % EXE]))
        print('[FAIL] exe no encontrado')
        return

    # --- 2-5. CASTLE_DUMP vs fixtures ---------------------------------------
    with tempfile.TemporaryDirectory(prefix='castle_dump_') as dump:
        env = dict(os.environ, CASTLE_DUMP=dump)
        r = subprocess.run([EXE], cwd=ROOT, env=env, capture_output=True, text=True)
        if r.returncode != 0:
            results.append(('CASTLE_DUMP', ['exit %d: %s' % (r.returncode, r.stderr[:500])]))
            print('[FAIL] CASTLE_DUMP no corrió')
            return

        def t_colmap():
            errs = []
            for idx in range(100):
                xx = room_name(idx)
                want = open(os.path.join(FIX, 'colmap', 'colmap_%s.bin' % xx), 'rb').read()[:600]
                got = open(os.path.join(dump, 'colmap_%s.bin' % xx), 'rb').read()
                if want != got:
                    nbad = sum(1 for a, b in zip(want, got) if a != b)
                    errs.append('sala %s: %d celdas difieren' % (xx, nbad))
            return errs
        check('colmap (100 salas, RAM 0xE496)', t_colmap)

        def make_table_test(prefix, fixdir, decoder):
            def t():
                errs = []
                for idx in range(100):
                    xx = room_name(idx)
                    raw = open(os.path.join(FIX, fixdir, '%s_%s.bin' % (fixdir, xx)), 'rb').read()
                    want = decoder(raw)
                    got = read_lines(os.path.join(dump, '%s_%s.txt' % (prefix, xx)))
                    if want != got:
                        errs.append('sala %s: esperado %r != dump %r' % (xx, want, got))
                return errs
            return t
        check('doors (tabla 0xE346)', make_table_test('doors', 'e346', decode_doors))
        check('keys  (tabla 0xE3D6)', make_table_test('keys', 'e3d6', decode_keys))
        check('items (tabla 0xE3D6)', make_table_test('items', 'e3d6', decode_items))

    # --- 6. screenshot smoke --------------------------------------------------
    # Usa el modo CASTLE_ACTORS (render fiel de sala + actores). El CASTLE_SHOT
    # "pelado" hoy rinde uniforme porque el render de fondo solo se activa con
    # g_actors_on — eso lo resuelve la Fase 1 (VDP fiel) y entonces se podrá
    # comparar pixel-perfect contra tests/fixtures/vram/.
    def t_shot():
        with tempfile.TemporaryDirectory(prefix='castle_shot_') as d:
            shot = os.path.join(d, 'shot')
            env = dict(os.environ, CASTLE_SHOT=shot, CASTLE_ROOM='70',
                       CASTLE_ACTORS='1', CASTLE_FRAMES='1')
            r = subprocess.run([EXE], cwd=ROOT, env=env, capture_output=True,
                               text=True, timeout=60)
            if r.returncode != 0:
                return ['exit %d: %s' % (r.returncode, r.stderr[:500])]
            bmp = shot + '_00.bmp'
            if not os.path.exists(bmp):
                return ['no se generó %s' % bmp]
            data = open(bmp, 'rb').read()
            if len(data) < 1000 or data[:2] != b'BM':
                return ['BMP inválido (%d bytes)' % len(data)]
            off = struct.unpack_from('<I', data, 10)[0]
            px = data[off:]
            distinct = len(set(px[i:i+4] for i in range(0, len(px) - 4, 4 * 7)))
            if distinct < 8:
                return ['imagen casi uniforme (%d colores muestreados): render roto' % distinct]
            return []
    check('screenshot smoke (sala 0x70, modo actores)', t_shot)

def main():
    results = []
    run(results)
    nfail = sum(1 for _, errs in results if errs)
    print('\n%d/%d suites OK' % (len(results) - nfail, len(results)))
    sys.exit(1 if nfail else 0)

if __name__ == '__main__':
    main()
