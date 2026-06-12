#!/usr/bin/env python3
"""
Harness de regresión contra el oráculo (dumps de openMSX en tests/fixtures/).

Uso (desde la raíz del repo):
    python tests/run_tests.py            # compila + corre todo
    python tests/run_tests.py --no-build # usa el exe ya compilado

Qué verifica:
  1. build        : build.ps1 compila sin errores.
  2. loader *     : el ROOM LOADER PORTADO (room_loader.c = sub_64DD fiel)
                    decodifica las 100 salas desde el ROM; sus tablas RAW
                    (colmap+E6EE, e346, e3d6, e43e, objs, name table y la
                    VRAM completa de tiles) == dumps de openMSX, byte a byte.
  3. vdp          : render SCREEN 2 del exe (CASTLE_VRAMIN) == renderer de
                    referencia Python, pixel-exacto, muestra de salas.
  4. título       : VRAM del port en sub_4AD7 == captura openMSX.
  5. screenshot   : smoke del modo actores.
"""
import os, struct, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIX = os.path.join(ROOT, 'tests', 'fixtures')
EXE = os.path.join(ROOT, 'the_castle.exe')

def room_name(idx):
    """idx 0..99 -> nombre BCD 'XX' usado en los archivos (hi=fila, lo=col)."""
    return '%02X' % (((idx // 10) << 4) | (idx % 10))

# Paleta TMS9918A canónica (la misma de hal_sdl2.c y tools/render_vram.py)
TMS_PAL = [(0,0,0),(0,0,0),(33,200,66),(94,220,120),(84,85,237),(125,118,252),
           (212,82,77),(66,235,245),(252,85,84),(255,121,120),(212,193,84),
           (230,206,128),(33,176,59),(201,91,186),(204,204,204),(255,255,255)]

def render_screen2_reference(vram_path):
    """Render SCREEN 2 de referencia: name table 0x1800, pattern/color por
    tercio, color 0 = backdrop (negro, reg7=0x01 en CASTLE_VRAMIN).
    Devuelve filas de tuplas (r,g,b), 192x256."""
    v = open(vram_path, 'rb').read()
    backdrop = TMS_PAL[1]
    fb = [[None] * 256 for _ in range(192)]
    for row in range(24):
        toff = (row >> 3) * 0x800
        for col in range(32):
            tile = v[0x1800 + row * 32 + col]
            for yy in range(8):
                p = v[toff + tile * 8 + yy]
                c = v[0x2000 + toff + tile * 8 + yy]
                fg = TMS_PAL[c >> 4] if c >> 4 else backdrop
                bg = TMS_PAL[c & 15] if c & 15 else backdrop
                for xx in range(8):
                    fb[row * 8 + yy][col * 8 + xx] = fg if p & (0x80 >> xx) else bg
    return fb

def compare_bmp_to_reference(bmp_path, ref):
    """Compara un BMP 32bpp de SDL (BGRX, bottom-up) contra el render de
    referencia. Devuelve nº de píxeles distintos."""
    d = open(bmp_path, 'rb').read()
    off = struct.unpack_from('<I', d, 10)[0]
    w, h = struct.unpack_from('<ii', d, 18)
    if (w, h) != (256, 192):
        return 256 * 192
    bad = 0
    for y in range(192):
        rowoff = off + (191 - y) * 256 * 4
        rowref = ref[y]
        for x in range(256):
            i = rowoff + x * 4
            if (d[i+2], d[i+1], d[i]) != rowref[x]:
                bad += 1
    return bad

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

    # --- 2-5. ROOM LOADER PORTADO (sub_64DD) vs fixtures ----------------------
    # CASTLE_DUMP decodifica las 100 salas desde el ROM con room_loader.c y
    # vuelca las tablas RAW; se comparan byte a byte contra los dumps de
    # openMSX. La suite 'vram' incluye TODOS los datos de tile (alocador por
    # tercio + espejos/corrimientos de gráficos) y el residuo del boot+título.
    with tempfile.TemporaryDirectory(prefix='castle_dump_') as dump:
        env = dict(os.environ, CASTLE_DUMP=dump)
        r = subprocess.run([EXE], cwd=ROOT, env=env, capture_output=True, text=True)
        if r.returncode != 0:
            results.append(('CASTLE_DUMP', ['exit %d: %s' % (r.returncode, r.stderr[:500])]))
            print('[FAIL] CASTLE_DUMP no corrió')
            return

        DUMP_SETS = [
            ('colmap', 900, None),   # 0xE496 colmap + 0xE6EE índice
            ('e346',    64, None),   # puertas
            ('e3d6',    64, None),   # coleccionables
            ('e43e',    80, None),   # estructurales
            ('objs',   288, None),   # 0xE380-0xE49F (enemigos + contadores)
            ('ont',    768, None),   # name table
            # vram: pattern+name+color; 0x1B00-0x1FFF = sprite attr viva → fuera
            ('vram', 0x3800, list(range(0, 0x1B00)) + list(range(0x2000, 0x3800))),
        ]
        def make_raw_test(name, size, rng):
            def t():
                errs = []
                for idx in range(100):
                    xx = room_name(idx)
                    a = open(os.path.join(FIX, name, '%s_%s.bin' % (name, xx)), 'rb').read()[:size]
                    b = open(os.path.join(dump, '%s_%s.bin' % (name, xx)), 'rb').read()[:size]
                    idxs = rng if rng else range(min(len(a), len(b)))
                    nbad = sum(1 for i in idxs if a[i] != b[i])
                    if nbad:
                        errs.append('sala %s: %d bytes difieren' % (xx, nbad))
                return errs
            return t
        for name, size, rng in DUMP_SETS:
            check('loader %-6s (100 salas, raw)' % name, make_raw_test(name, size, rng))

    # --- 6. vdp: render SCREEN 2 vs referencia --------------------------------
    # Muestra diversa de salas (las 100 tardarían ~2 min; estas cubren las 4
    # esquinas del castillo, salas con/sin puertas y la sala inicial 0x70).
    VDP_SAMPLE = ['00', '09', '18', '27', '35', '46', '54', '63', '70', '81', '99']
    def t_vdp():
        errs = []
        with tempfile.TemporaryDirectory(prefix='castle_vdp_') as d:
            for xx in VDP_SAMPLE:
                fix = os.path.join(FIX, 'vram', 'vram_%s.bin' % xx)
                bmp = os.path.join(d, 'vdp_%s.bmp' % xx)
                env = dict(os.environ, CASTLE_VRAMIN=fix, CASTLE_SHOT=bmp)
                r = subprocess.run([EXE], cwd=ROOT, env=env, capture_output=True,
                                   text=True, timeout=60)
                if r.returncode != 0 or not os.path.exists(bmp):
                    errs.append('sala %s: exe falló (%d)' % (xx, r.returncode))
                    continue
                bad = compare_bmp_to_reference(bmp, render_screen2_reference(fix))
                if bad:
                    errs.append('sala %s: %d píxeles difieren' % (xx, bad))
        return errs
    check('vdp SCREEN 2 (%d salas, pixel-exacto)' % len(VDP_SAMPLE), t_vdp)

    # --- 6b. jugador vs trazas-oráculo (Fase 3) -------------------------------
    # player.c (sub_40BB + sub_6F5C portados) corre cada guion de input y debe
    # reproducir frame a frame la traza del juego real (posición, fase de
    # salto, patrón de animación, input muestreado).
    def t_player():
        import glob as _g
        errs = []
        for mf in sorted(_g.glob(os.path.join(FIX, 'traces', 'moves_*.txt'))):
            name = os.path.basename(mf)[6:-4]
            moves = open(mf).read().strip()
            out = os.path.join(tempfile.gettempdir(), 'ptrace_%s.txt' % name)
            env = dict(os.environ, CASTLE_PTRACE=out, CASTLE_MOVES=moves)
            r = subprocess.run([EXE], cwd=ROOT, env=env, capture_output=True,
                               text=True, timeout=60)
            if r.returncode != 0 or not os.path.exists(out):
                errs.append('%s: exe falló' % name)
                continue
            want = [l.split() for l in
                    open(os.path.join(FIX, 'traces', 'trace_%s.txt' % name)) if l.strip()]
            got = [l.split() for l in open(out) if l.strip()]
            bad = sum(1 for i in range(min(len(want), len(got))) if want[i] != got[i])
            os.remove(out)
            if bad:
                errs.append('%s: %d/%d frames difieren' % (name, bad, len(want)))
        return errs
    check('jugador (trazas frame a frame)', t_player)

    # --- 6c. enemigos BAT vs trazas-oráculo (Fase 4) ---------------------------
    # player.c (sub_438D/43BF/719D y cerebros 4882/48AD/4901/7279 portados)
    # corre cada sala con el jugador fijado en la celda de la captura y debe
    # reproducir frame a frame las posiciones de los 8 slots BAT (incluye la
    # interacción con los objetos COLL: trampas 0x35 que caen, etc.).
    # Salas cuyo ciclo BAT depende del motor de ASCENSORES (e43e código 0x1D,
    # sub_4406/442D — aún no portado): la plataforma móvil escribe bits de piso
    # en el colmap y extiende las patrullas. Se saltan hasta portarlo.
    BATS_PENDING = {'29', '81'}
    def t_bats():
        import glob as _g
        errs = []
        for bf in sorted(_g.glob(os.path.join(FIX, 'bats', 'bats_*.txt'))):
            lines = open(bf).read().splitlines()
            hdr = lines[0].split()        # "# room XX pcol P prow R"
            xx, pcol, prow = hdr[2], hdr[4], hdr[6]
            if xx in BATS_PENDING:
                continue
            out = os.path.join(tempfile.gettempdir(), 'battrace_%s.txt' % xx)
            env = dict(os.environ, CASTLE_BATTRACE=out, CASTLE_ROOM=xx,
                       CASTLE_FRAMES=str(len(lines) - 1),
                       CASTLE_PCOL=pcol, CASTLE_PROW=prow)
            r = subprocess.run([EXE], cwd=ROOT, env=env, capture_output=True,
                               text=True, timeout=60)
            if r.returncode != 0 or not os.path.exists(out):
                errs.append('sala %s: exe falló' % xx)
                continue
            def positions(ls):
                # "fc s:col,fila[/fXX] ..." -> [(fc, {slot: (col,fila)})]
                seq = []
                for l in ls:
                    if not l.strip() or l.startswith('#'):
                        continue
                    p = l.split()
                    slots = {}
                    for it in p[1:]:
                        if it.startswith('|'):
                            break
                        s, rest = it.split(':')
                        slots[int(s)] = rest.split('/')[0]
                    seq.append(slots)
                return seq
            want = positions(lines[1:])
            got = positions(open(out).read().splitlines())
            os.remove(out)
            n = min(len(want), len(got))
            bad = sum(1 for i in range(n) if want[i] != got[i])
            if bad:
                first = next(i for i in range(n) if want[i] != got[i])
                errs.append('sala %s: %d/%d frames difieren (primero f%d: '
                            'oráculo %s vs port %s)' %
                            (xx, bad, n, first, want[first], got[first]))
        return errs
    check('enemigos BAT (trazas frame a frame; %d salas pendientes de '
          'ascensores)' % len(BATS_PENDING), t_bats)

    # --- 7. título vs oráculo -------------------------------------------------
    # CASTLE_TITLEDUMP corre la intro real (rápido) y vuelca la VRAM en el
    # mismo momento que tools/cap_title.tcl capturó la del juego real en
    # openMSX (entrada a sub_4AD7, fase "esperar input"). Byte-exacto en
    # pattern/name/color; los sprites no se comparan (el port no los usa aún).
    def t_title():
        fix = os.path.join(FIX, 'vram_title.bin')
        with tempfile.TemporaryDirectory(prefix='castle_title_') as d:
            out = os.path.join(d, 'title.bin')
            env = dict(os.environ, CASTLE_TITLEDUMP=out, CASTLE_FAST='1')
            r = subprocess.run([EXE], cwd=ROOT, env=env, capture_output=True,
                               text=True, timeout=300)
            if r.returncode != 0 or not os.path.exists(out):
                return ['exe falló (%d): %s' % (r.returncode, r.stderr[:300])]
            a = open(fix, 'rb').read()
            b = open(out, 'rb').read()
            errs = []
            for nm, lo, hi in [('pattern', 0x0000, 0x1800),
                               ('name',    0x1800, 0x1B00),
                               ('color',   0x2000, 0x3800)]:
                bad = sum(1 for i in range(lo, hi) if a[i] != b[i])
                if bad:
                    errs.append('%s table: %d bytes difieren' % (nm, bad))
            return errs
    check('título (VRAM == openMSX en sub_4AD7)', t_title)

    # --- 8. screenshot smoke --------------------------------------------------
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
