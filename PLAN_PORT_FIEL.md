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

**Pendiente para portar `player.c`:** leer el bloque 0x4586-0x49B5 (escaleras
W/S — sub_45AD/4710/4820/4882, daño/muerte), `sub_5053` (transición),
`sub_4499` (salida por puerta), `0x62D8` (render+triggers por frame) y
`0x623C`. Portar sobre el espejo RAM del room_loader y validar frame a frame
contra las trazas (mismo guion → misma posición/fase/patrón).
- Portar el frame loop real (el esqueleto `game_frame()` ya mapea el orden de
  llamadas) y el movimiento/física del jugador del disasm. Transición de sala
  real (`sub_5053`), no la heurística de bordes de `faithful_play()`.
- Input por replay (`CASTLE_MOVES` ya existe) para tests determinísticos.
- **Hecho cuando**: con el mismo guion de inputs, posición del jugador frame a
  frame == traza de openMSX (`trace_jump*.tcl`, `dump_walk.tcl`) durante N
  cientos de frames, incluyendo salto, caída, escaleras, daño.

### Fase 4 — Enemigos y objetos dinámicos (`sub_6F5C` y handlers)
- Portar la IA real por tipo (roller, bat, wall-follower, ascensores, pinchos).
  `enemies.c` tiene parte traducida; validar y completar.
- **Hecho cuando**: posiciones frame a frame == `enemies_paths.c` (4537
  posiciones grabadas) para los 148 enemigos. Entonces `enemies_paths.c` y
  `enemies_port.c` (path-replay) se borran.

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
