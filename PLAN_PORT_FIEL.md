# Plan — Port fiel desde el disasm (opción B)

> Decisión 2026-06-11: el proyecto se reorienta a un port fiel función-por-función
> desde el disasm. Todo lo capturado de openMSX deja de ser FUENTE DE DATOS del
> runtime y pasa a ser ORÁCULO DE TESTS (golden fixtures). Nada de heurísticas
> nuevas: si algo no se entiende, se traza en openMSX y se porta del disasm.

## Principio rector

- **Una sola fuente de verdad en runtime**: la ROM + lógica portada del disasm
  (`the_castle_disasm.asm`, 10.5k líneas).
- **openMSX = oráculo**: cada fase tiene un criterio de "hecho" verificable
  byte-a-byte o pixel-a-pixel contra dumps del juego real. Los ~580 KB de tablas
  horneadas (`map_real.c`, `colmap_data.c`, `*_data.c`, `enemies_paths.c`) se
  convierten en fixtures de regresión y SE BORRAN del ejecutable.
- **Portar literal, después refactorizar**: registros → variables, mismo orden de
  operaciones. Primero idéntico, después lindo.

## Diagnóstico que motiva el orden (2026-06-11)

1. **Video roto por diseño, no por bug puntual.** Hoy conviven TRES modelos de
   render: `vdp_render()` con `vram[]` (sprites), `screen.c` con
   `g_screen_buf`/`g_bg_tiles` planos, y el blit directo de `ROOM_NT` para el
   modo fiel. El CHANGELOG 2026-05-24 registra que el modelo de tercios se
   ABANDONÓ ("flat g_bg_tiles[256][16]... all third-specific writes collapse to
   the same destination") — pero SCREEN 2 tiene pattern/color table
   independientes por tercio y el título DEPENDE de eso (créditos en tercios 1-2
   con tiles distintos al tercio 0, ver AGENTS.md). Ese flatten es la causa raíz
   probable de los tiles corruptos de la intro. La paleta TMS9918A en
   `hal_sdl2.c` tiene los valores canónicos correctos; el problema es el
   ruteo/compositing, no las constantes.
2. **El gameplay actual es maqueta sobre snapshots** (colisión = RAM 0xE496
   capturada, visual = name tables capturadas, enemigos = paths grabados,
   física = aproximada a mano). No escala a lógica de juego (efectos de ítems,
   ascensores, contacto, score, final).
3. **Pivote identificado**: todos los dumps se generaron force-calleando
   `sub_64DD` (room loader) en openMSX. Portar esa rutina y sus hijas elimina
   todas las tablas horneadas de una vez.

## Fases

> Estado: Fase 0 ✅ · Fase 1 ✅ (título byte-idéntico) · Fase 2 ✅ COMPLETA
> (2026-06-11: room_loader.c = sub_64DD fiel, 700/700 fixtures; runtime
> conmutado, tablas horneadas BORRADAS, exe 403→173 KB) · Fase 3 ← SIGUIENTE
> (jugador desde el disasm: sub_4064/sub_40BB, validar contra trazas)

### Fase 0 — Harness de verificación + saneamiento ✅
- Runner de comparación: corre el port en modo headless (`CASTLE_SHOT`/dump de
  estado) y diffea contra fixtures (`colmap_XX.bin`, `e346/e3d6_XX.bin`, name
  tables, trazas de salto/enemigos). Un comando = pasa/falla.
- Mover fixtures a `tests/fixtures/`, scripts `.tcl`/`.py` del oráculo a
  `tools/`; borrar los one-off muertos.
- Unificar build (hoy `CMakeLists.txt` NO compila los `*_port.c`; el build real
  es `build.ps1` con glob). Una sola definición de fuentes.
- Branch/tag del estado actual como referencia jugable antes de demoler.

### Fase 1 — VDP SCREEN 2 fiel (arregla intro: tiles + paleta) ✅
- UN solo modelo de video: VRAM emulada de 16 KB + registros VDP. Render
  SCREEN 2 con **3 tercios reales** (pattern table 0x0000-0x17FF y color table
  0x2000-0x37FF completas, tercio según fila), name table 0x1800, sprites TMS
  (incl. regla de 4-por-línea si el juego la explota), color 0 = backdrop
  (reg 7).
- Eliminar `screen.c` (compositor plano), el blit de `ROOM_NT` y los buffers
  paralelos. Todo escribe VRAM; el render lee SOLO VRAM.
- **Hecho cuando**: screenshots del port en frames clave de la intro/título ==
  screenshots de openMSX, pixel-perfect (fixture nuevo: capturas de la intro).

### Fase 2 — Room loader real (`sub_64DD` y rutinas hijas)
- Portar el decode completo de sala: puebla 0xE000 (colisión stride 20), 0xE496
  (tilemap stride 30), 0xE346 (puertas), 0xE3D6 (coleccionables), 0xE43E
  (rampas/escaleras), 0xE6EE (índice celda→objeto) y dibuja la sala en VRAM.
- Punto de partida: `geom.c` (ya porta parte del stream 0x7CF2 / sub_6616 /
  sub_66A2 / sub_69AA) — completarlo contra el disasm, no contra el mapa PNG.
- **Hecho cuando**: para las 100 salas, las 6 tablas + name table generadas ==
  fixtures capturados, byte-exacto. Entonces se BORRAN `map_real.c`,
  `colmap_data.c`, `keys/doors/items/blocks_data.c` del runtime.

### Fase 3 — Game loop + jugador (`sub_4064`, `sub_40BB`, `sub_5053`) — EN CURSO

Avance 2026-06-11 — MAPA ESTRUCTURAL COMPLETO del núcleo del jugador:

**Arquitectura por frame (game loop sub_4064/4070):**
1. `sub_4064`: en frames PARES (bit0 de 0xEAC9 == 0, test sub_5D5D) limpia
   `0xEACB` (stick) y `0xEACC` (trigger).
2. `sub_5128` → `sub_50E8` × (0xEACA veces, regulador de velocidad; juego =
   0x70): **GTTRIG(2,1,0) → 0xEACC y GTSTCK(2,1,0) → 0xEACB, escribiendo
   SOLO en frames pares** (input efectivo a 30Hz — la "alternancia" vista en
   las trazas es artefacto del punto de muestreo del bp). Además SNSMAT filas
   0-8 acumuladas con AND en la cola 0xEACD (9 bytes; "any key" del título).
   En DEMO: keyframes de (0xEAE5): [input,duración]; EACB=byte&0xF,
   EACC=bit4; fin de stream vs 0x7BC0 → (0xEAE3)=1.
3. `sub_40BB` = SOLO computa FLAGS de colisión/intención en HL (no mueve):
   - lee EACB (2-4=der → D=+1,L=3; 6-8=izq → D=-1,L=1), EACC (salto),
     EAD6 = fase de salto/caída (0=suelo; 1..8 subiendo; 9..0x10 flote;
     0x11 = caída/terminal).
   - PROBES de pares de celdas sobre el colmap 0xE496 en (0xE334=col,
     0xE335=fila) ± offsets — bits del colmap: **0x80=sólido, 0x40=?,
     0x30=clase, 0x20=ocupado, 0x08=celda-objeto (E6EE>>3 = slot),
     0x04=trigger coleccionable, 0x01=rampa (E6EE da pendiente)**.
     Familia: 4515 (par horiz &0x80, fila>0x13 → libre = salida abajo),
     44EF (par vert vía 4A06: &0x0A pasable-especial sino &0x80),
     4502 (&0x20), 452D (&0x40), 44C2 (vert &0x30 + modo), 4541 (full+bit3),
     4566 (&0x30), 49DC (dest de bloque), 47F5 = PENDIENTE bajo los pies
     (±1 por celda vía 4A38: celda&0x01 → byte E6EE &0x03).
   - Empuje de bloques: `sub_4273` con TRAMPOLÍN AUTO-MODIFICANTE en 0xEAFA
     (JP a "INC B; INC B; RET" o "DEC B; RET" según dirección) + sub_468A
     (lookup de objeto por celda: &0x08 → slot E6EE>>3, bit4 = tabla
     COLL/BAT) + sub_710B (re-render del bloque movido).
   - Puertas: `sub_42FF` → sub_4325 (localiza puerta por celda) →
     sub_758C (apertura real, Fase 5).
4. `sub_6F5C(DE=HL de 40BB)` = APLICADOR de movimiento + render:
   - frames IMPARES (5D5D NZ): E334 ±1 según bits D (4=mover,0=dir...),
     `sub_6F45` (pixel = ((col+1)*8, (fila+4)*8-1)), anim 6F94.
   - frames PARES (→ 6FF0): detección de SALIDA: E334==0+izq → (0xEAE1)=7;
     ==0x1C+der → 3; E335==0xFE → 1 (arriba); ==0x14 → 5 (abajo). Si no:
     medio-paso: H=(E334+1)*2, L=(E335+4)*2 en unidades de 4px; bits D
     ajustan B/C (celda) y H/L (±1 = ±4px, bit4 = paso doble en rampa);
     guarda B→E334, C→E335; pixel = H*4, L*4-1. → anim 6F94.
   - Anim: frame A → patrón sprite A*3*4 (3 planos, sprites 8-10 vía
     6F27/6EE1). Confirmado vs trazas: caminata der = ciclo 8,6,7
     (96/72/84); idle=0; salto=5 (60); caída=7 / 9+(EAC9&3).
5. `sub_5B96` = ¡dispatcher de TRIGGERS de celda! (el famoso sub_5BB0):
   celda&0x04 → busca el slot en 0xE3D6 por posición y despacha por val:
   0x20=salida especial, 0x21=salida+reload (llama sub_64DD), 0x22=revela
   mapa, 0x23+=power-ups/puntos (Fase 5).
6. `sub_5B5F` = caída (BIT 2,H): frames pares E335+1 con pixel a medio paso,
   anim 7 / 9+. (0xEAE1) la consume sub_5053 (transición de sala).
7. Muerte/respawn: sub_518E (limpia fila, posiciones de respawn por sala en
   tabla ROM 0x5748).

✅ Trazas-oráculo en `tests/fixtures/traces/` (6 guiones por frame).

✅ **player.c PORTADO y validado: 6/6 trazas frame-perfect (456 frames)** —
suite `jugador` en el runner. Incluye sub_4325+sub_758C (puertas:
rl_door_press) porque la traza walk choca contra la puerta cerrada.

**sub_5053 (transición de sala) YA MAPEADO** (0x5053): sala BCD con DAA
(arriba -0x10, der +1, abajo +0x10, izq -1), posición de entrada = borde
opuesto (E335=0x11 / E334=0 / E335=0 / E334=0x1C), LDIR E334-E345 →
E322-E333 (commit del estado activo: llaves E337+, score E33D+), sub_6134 =
commit de los slots de puertas a los bitfields (vía sub_6110 set/clear bit),
minimapa sub_63FD si E321 bit3.

✅ **SWITCHOVER hecho**: faithful_play corre con player.c + rl_room_exit
(sub_5053+sub_6134 portados) + sprite por VDP (patrones 0x9B96, colores
(0x7CF0) con tintes 0xE343/E344) + puertas/persistencia/gemelas 100% reales
vía el ciclo bitfield. Verificado end-to-end: llave→puerta→cruce→transición
BCD a 0x71. Murieron: física de actors en el loop, doors_port del loop,
player_sprite.c, s_ktaken/s_itaken.

**Restos de Fase 3 (van con Fases 4-5):** daño/muerte (sub_4406 + sub_518E,
respawns tabla 0x5748); empuje de bloques (sub_4273, trampolín 0xEAFA);
escaleras (bloque 0x4586+ vía sub_434A/442D); pickup por celda (sub_5B96 +
handlers 0x5C3A+) en vez del AABB maqueta; 0x62D8/0x623C (render/triggers).
- Portar el frame loop real (el esqueleto `game_frame()` ya mapea el orden de
  llamadas) y el movimiento/física del jugador del disasm. Transición de sala
  real (`sub_5053`), no la heurística de bordes de `faithful_play()`.
- Input por replay (`CASTLE_MOVES` ya existe) para tests determinísticos.
- **Hecho cuando**: con el mismo guion de inputs, posición del jugador frame a
  frame == traza de openMSX (`trace_jump*.tcl`, `dump_walk.tcl`) durante N
  cientos de frames, incluyendo salto, caída, escaleras, daño.

### Fase 4 — Enemigos y objetos dinámicos — MAPA COMPLETO (2026-06-11)

**Motor COLL ✅ portado** (bloques empujables: ver CHANGELOG (6)).

**Motor BAT ✅ portado y validado (2026-06-12)**: driver+cerebros+movedor+
daño/muerte en player.c; suite `bats` del harness compara CASTLE_BATTRACE
(+CASTLE_PCOL/PROW) frame a frame contra fixtures frescos de openMSX
(tests/fixtures/bats/, 10 salas vía tools/gen_bats_fixtures.py +
tools/tr_bats.tcl): **10/10 exactas 301/301** (29/81 incluidas con los
ascensores). El criterio original (enemies_paths.c) se descartó:
la sala 49 probó que el path legacy era un artefacto (la trampa 0x35 cae al
cargar y el BAT real rebota contra ella); enemies_paths.c y enemies_port.c
BORRADOS. Bug real cazado: sub_4901 solo sube si el jugador está ARRIBA
(JP Z/JP C en 49A2 → horizontal).

**Motor de ASCENSORES ✅ portado y validado (2026-06-12)** — sub_442D en
player.c (player_elev_frame, ANTES de 434A en el loop): e43e tipos 0x1C
(ancho 2) / 0x1D (ancho 4), actúan cada 8 frames (EAC9&7==0). Entrada:
[1]=col, [2]=fila (la mueve el DRAWER 73FB/743D), [4]=estado (bit2=activo,
bit3=subiendo). Cerebro sub_4611: probes sub_452D (bajar: piso 0x40
bloquea) / sub_4541 (subir: sólido 0x30 sin bit3-objeto bloquea) con
rebote; bajando APLASTA objetos (468A modo 6 → 5D47); subiendo EMPUJA la
pila (468A modo 7 con bit0=JUGADOR: pies en fila c ⇔ top-left==(b,c-1) o
(b-1,c-1); sub_4701: jugador = E335--+6F45+6F27(0xFF), objeto = sub_4744
recursivo encima + sub_7575 redibuja 2x2 una fila arriba; techo → 5D47).
El colmap por delta sale del ROM vía 5E80 (0x1B-0x1E: tabla 0x77B6):
superficie=piso, cadenas no. Salas 29/81 frame-exactas 301/301.

Pendiente de la fase: partículas sub_61F5/5D63, escaleras, pickup por
celda, tipos 0x0C/0x0D/0x0F de 442D (sub_72CA/72DD/7326) y 0x1F de
sub_4406 (sub_47B8/7494, trampas estructurales).

Mapa original (referencia):
- Driver `sub_438D`: tabla 0xE416 (8 slots). Pasada 1: cerebro `sub_43BF`
  para cada activo. Pasada 2: si (field4 & 5) → 43BF de nuevo; SIEMPRE
  `sub_719D` (movedor).
- Cerebro `sub_43BF` (solo frames PARES): despacho por tipo:
  0x36 = estático (flags sin cambio; mover→sub_7279 partículas/trampa).
  0x37 → `sub_4901`: volador 4-direcciones que PERSIGUE al jugador
    (45D0 = ¿jugador a ±8 cols?, 45EF/45F8 = comparar col/fila del jugador,
    4586 = probes vertical (B,C+2)/(B,C-1) con 4566 (&0x30)).
  0x38 → `sub_48AD`: caminador-perseguidor: gravedad (4502 &0x20 debajo);
    horizontal con rebote (45AD); cerca del jugador → SET 2+3 (¡SUBE!) si
    arriba libre.
  0x39 → `sub_4882`: patrulla horizontal con gravedad y rebote; D de 45AD
    por cercanía (45D0 carry).
  0x3A → `sub_487A` y 0x3B+ → `sub_4872`: wrappers de `sub_45AD`-family
    (releer 4868-4880: son 2 wrappers casi idénticos con D=0 → 45AD-modo).
- Helpers: `sub_45AD(flags, modo)` = REBOTE: D'=44C2(modo) en (B+2,C) der,
  E'=44C2 en (B-1,C) izq (via sub_4599); der libre+izq bloq → SET 1 (der);
  der bloq+izq libre → RES 1; ambos → RES 0 (parar). `sub_45D0` = jugador a
  ±8 columnas (carry=no). `sub_4586` = par vertical abajo(C+2)/arriba(C-1)
  con 4566.
- Movedor `sub_719D` (gemelo de 710B con verticales completos y ANIMACIÓN
  por dirección): finales deltas 0-3 (D=0) o 4-7 (D=4, espejado/variante);
  transiciones: derecha D=0x16(22), izquierda D=0x0C(12) (3x2 deltas +0..5);
  abajo D=0x1C(28), arriba D=0x22(34) (2x3). Frame PAR baja: si las 2 celdas
  de abajo (4566 modo 1) BLOQUEAN → EL BAT MUERE (slot[0]=0 + blank 2x2 +
  puff sub_5D63) — los que se lanzan en picada. Subida: celda C-1 con 4566;
  bloqueada → 725D (leer 724B-7279). 0x36 → sub_7279 (trampa murciélago,
  partículas). Falta leer: 4868-4880 (wrappers 3A/3B) y 7250-72C9.
- **Daño por contacto `sub_5A2D` (mapeado)**: solo frames IMPARES; si
  (0xE343)≠0 → INVULNERABLE (power-up 0x23, el del tinte parpadeante);
  4 probes `sub_5AF8` en las celdas del cuerpo: celda con bit 0x10 (peligro:
  0x38 BAT / 0x18 trampa) y sin 0x80 → flags D=E6EE&7 (saltando RES 2;
  (0xE344) anula otro bit — leer 5B23-5B2E) → ≠0 = MUERTE `sub_5A63`:
  música muerte (0x7A73/0x7A8F — ¡eran esto, no música de juego!), 16
  frames de caída (5B5F), sigue cayendo hasta fila ≥0x12, frame 0x0D, 32
  frames de pausa, DEC vidas (E324+E336), EAE0=1, LDIR E336→E324 (commit),
  sub_6134. El caller: EAE0 → si vidas>0 recargar la sala (respawn en el
  punto de entrada E322/E323); si 0 → game over al título.
- `0x62D8` (rama juego = 62FA) — **VELOCIDAD ✅ portada (2026-06-12)**:
  0xEAD3/D4 NO son config (nota vieja falsa): son las filas 6 y 7 de la
  MATRIZ DE TECLADO acumuladas (AND de SNSMAT) en el busy-wait de sub_5128
  (EACD..EAD5, activo-bajo). Fila 6: bit1=CTRL suelto → EACA=0x70 (normal);
  CTRL → 0x30 (correr); CTRL+GRAPH (bit2) → 0x01 (turbo); cada modo setea
  también transpose (0xEAF1=0/7/0x0C) y tempo música (0xEAF3=6/4/2; CAPS
  bit3 → 0 = mute). El paceo real es CPU-bound (sub_5128 = EACA×sub_50E8,
  sin vsync); medido en openMSX (tools/tr_speed.tcl, sala 01):
  ms/iter = 54.0 + 1.381×EACA → 0x70=208.8ms, 0x30=119.9ms, 0x01=55.6ms.
  Port: player_speed_frame() + hal_wait_game_frame() (vblanks fraccionales).
  Pendiente de la misma rutina: F1=reiniciar sala (EAE0=1), F2=vidas:=1,
  fila 7 (0xEAD4): F4→sub_4F93, F5→pausa sub_6358.
- **Hecho cuando**: posiciones frame a frame == `enemies_paths.c` (4537
  posiciones) para los 148 enemigos → borrar enemies_paths.c y el
  path-replay de enemies_port.c (el render pasa a ser por celdas reales
  del movedor 719D — el overlay muere).

### Fase 5 — Sistemas de juego
- Puertas/llaves (`sub_442D`, `sub_438D`, `sub_4499`, `sub_758C`),
  coleccionables y EFECTOS reales (`sub_434A`, dispatcher `sub_5BB0`: mapa,
  power-ups, comida, tesoro, score), HUD (`sub_5A2D`), vidas/muerte/game over,
  demo mode, condición de final.
- Al completarse, muere el motor maqueta entero: `actors.c`, `*_port.c`,
  `faithful_play()`, stubs de `main.c`.

### Fase 6 — Música/PSG verificada
- No confiar en lo actual de oído (ya hubo datos mal direccionados: 0x7ABE vs
  0x7A73, CHANGELOG 2026-05-25). Fixture nuevo: dump de escrituras a registros
  PSG por frame en openMSX (intro + in-game) y comparación registro-a-registro
  con `music.c`. Corregir tempo/transpose/ISR hasta igualar.

## Orden razonado

Video primero porque TODAS las demás fases se verifican mirando la pantalla;
loader segundo porque elimina las tablas y desbloquea salas reales; jugador →
enemigos → sistemas siguen la dependencia natural; música al final por ser
independiente.

## Reglas

1. Ninguna heurística nueva. Lo que no se entiende, se traza y se porta.
2. Ningún dato capturado en el runtime. Capturas = solo tests.
3. Cada fase termina con su test del oráculo en verde antes de empezar la
   siguiente.
4. Borrar código superado en la misma fase que lo reemplaza (no acumular
   motores paralelos otra vez).
