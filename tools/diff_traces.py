#!/usr/bin/env python3
"""Compara las trazas del jugador portado contra los oraculos de openMSX.
Corre el exe en modo CASTLE_PTRACE con cada guion de tests/fixtures/traces/
y diffea linea a linea. Uso: python tools/diff_traces.py [nombre]"""
import os, subprocess, sys, glob

FIELDS = ['fc', 'sprY', 'sprX', 'pat', 'EAD6', 'EACB', 'EACC', 'col', 'fila']
ONLY = sys.argv[1] if len(sys.argv) > 1 else None
ok = True

for mf in sorted(glob.glob(os.path.join('tests', 'fixtures', 'traces', 'moves_*.txt'))):
    name = os.path.basename(mf)[6:-4]
    if ONLY and name != ONLY:
        continue
    moves = open(mf).read().strip()
    tf = os.path.join('tests', 'fixtures', 'traces', 'trace_%s.txt' % name)
    out = 'ptrace_out.txt'
    env = dict(os.environ, CASTLE_PTRACE=out, CASTLE_MOVES=moves)
    r = subprocess.run([os.path.abspath('the_castle.exe')], env=env,
                       capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(out):
        print('%-9s: EXE FALLO %s' % (name, r.stderr[:200]))
        ok = False
        continue
    want = [l.split() for l in open(tf) if l.strip()]
    got = [l.split() for l in open(out) if l.strip()]
    bad = []
    for i in range(min(len(want), len(got))):
        if want[i] != got[i]:
            bad.append(i)
    if not bad:
        print('%-9s: OK (%d frames)' % (name, len(want)))
    else:
        ok = False
        print('%-9s: %d/%d frames difieren; primero en f%d:' %
              (name, len(bad), len(want), bad[0]))
        for i in bad[:6]:
            w, g = want[i], got[i]
            ds = ['%s(%s!=%s)' % (FIELDS[k], w[k], g[k])
                  for k in range(min(len(w), len(g))) if w[k] != g[k]]
            print('   f%-3s  %s' % (w[0], '  '.join(ds)))
    if os.path.exists(out):
        os.remove(out)

sys.exit(0 if ok else 1)
