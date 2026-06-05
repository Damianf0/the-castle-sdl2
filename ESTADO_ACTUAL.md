# The Castle (port C/SDL2) — Estado actual

> Documento de estado honesto. Separa lo SÓLIDO (del ROM, validado) de lo que es
> DEMO/maqueta (se ve bien pero no es el port fiel). Para retomar sin reconstruir.
> Fecha: 2026-06-04.

---

## TL;DR

- **El juego compila** a `.exe` nativo de Windows (`build.ps1`).
- **Tenemos oráculo de ground-truth:** openMSX corre el ROM real y volcamos VRAM + tablas de objetos byte-exactas.
- **Datos dinámicos EXACTOS de las 100 salas** (enemigos/objetos/rampas) en `castle_objects.json`.
- **Lo visual del viewer es una MAQUETA** (tiles sacados de la imagen del mapa + jugador/enemigos hechos a mano), NO el motor de render del ROM. Se ve bien pero no es fiel-desde-ROM.

---

## 1. Herramientas (todo instalado y funcionando)

| Tool | Ruta | Para qué |
|---|---|---|
| MinGW+SDL2 | `..\_buildtools\mingw`, `..\_buildtools\sdl2` | compilar |
| Build | `build.ps1` (1 comando) | → `the_castle.exe` |
| openMSX | `..\_buildtools\openmsx\openmsx.exe` | ground truth (VRAM/RAM del ROM real) |
| Python 3.14 | `C:\Program Files\Python314` | scripts/PIL |

---

## 2. SÓLIDO — del ROM, validado ✅

- **Decode de enemigos** (`objects_load_from_rom` / `geom.c`): da los enemigos EXACTOS, validado contra la RAM real de openMSX (salas 0x70/0x01/0x80).
- **openMSX como oráculo:** `force_dump.tcl` fuerza cada sala (force-call de `sub_64DD` @0x64DD) y vuelca sus tablas. `render_vram.py` reproduce SCREEN 2 exacto.
- **Censo real (castle_objects.json):** 148 enemigos (61 salas), 156 coleccionables, 181 estructural/rampas/ascensores — origen exacto de cada uno. Corrige el censo viejo inflado.
- **Confirmado:** el mapa `Castle-SG-All.png` SÍ matchea el ROM (comparado sala 0x70 real vs mapa).
- **Correcciones de RE:** `0x5758`/`sub_53D4` = animación del TÍTULO (no geometría). Cada sala tiene 3 punteros de stream en `0x7CF2 + 42*row + 4*col`.

## 3. GEOMETRÍA FIEL — del ROM real (2026-06-05) ✅

- **`map_real.c`** (277 tiles): la VRAM REAL del ROM, capturada de openMSX (force-call `sub_64DD` dibuja la sala). 100 name tables 32x24 byte-exactas. El render de la app es IDÉNTICO al juego. Esto resuelve la queja de fondo (es output del ROM real, no un screenshot).
- Generación: `capture_vram.tcl` (vuelca 100 salas) → `gen_realtiles.py` → `map_real.c/.h`.
- Mapeo coord objeto→pantalla calibrado: `screen = (game_row+3, game_col)`.
- (El `map_tiles.c` de la imagen quedó ELIMINADO.)

## 3c. SPRITE DEL JUGADOR — del ROM real (2026-06-05) ✅

- **`player_sprite.c`**: sprite REAL del jugador extraído byte-exacto del ROM (pattern table @`0x9b96`, copiada a VRAM 0x3800 por `LDIRVM` en `0x4D02`). El jugador = 3 planos OR (rojo/amarillo/cyan), identificados volcando la sprite attribute table (0x1B00) en gameplay real (`dump_player.tcl`/`dump_walk.tcl`). 6 frames: parado, caminata izq×2, caminata der×3 (patrones separados, no espejados). `draw_actors()` lo blitea con selección por facing+animación. Reemplaza al verde hecho a mano.

## 3b. MAQUETA — todavía no fiel ⚠️

- **Física del jugador** (`actors.c`): gravedad/salto/colisión AABB hechos a mano (no el movimiento real del disasm). El SPRITE ya es fiel (ver 3c); falta el movimiento fiel.
- **Enemigos dinámicos:** desactivados (los reales ya están en la geometría VRAM, estáticos). El movimiento real = capa de comportamiento pendiente.
- **Modo viewer** (`ver_castillo.bat`, `CASTLE_VIEW`): camino paralelo que saltea el game-loop real. Es una maqueta jugable.
- **Decode RLE de geometría** (`geom.c` run_6616/66A2): intento de leer paredes del ROM, dio aproximado. NO se usa para el render.

## 4. Del trabajo previo (no tocar)

- **Intro** (`title.c`) y **sonido** (`music.c`) ya estaban resueltos. Intactos.
- Dos capas de mapa: `g_map` (0xE000, stride 20) = colisión; `g_tilemap` (0xE496, stride 30) = visual.

---

## 5. Las 3 capas de un port fiel y dónde estamos

| Capa | Estado | Fuente fiel |
|---|---|---|
| Geometría visual de salas | ✅ **FIEL** (`map_real.c`, VRAM real capturada de openMSX) | output del ROM real |
| Origen+tipo de enemigos/objetos/rampas | ✅ EXACTO (`castle_objects.json`) | tablas reales del ROM |
| Comportamiento (movimiento de enemigos/ascensores/rollers) | pendiente | disasm (`enemies.c` tiene parte) |
| Jugador — sprite | ✅ **FIEL** (`player_sprite.c`, extraído del ROM 0x9b96) | sprite pattern table del ROM |
| Jugador — física/movimiento | maqueta (hecho a mano) | disasm |

---

## 6. Próximos pasos (al carril fiel)

1. **Clasificar tipos estructurales:** cuál de `0x1C`/`0x1D`/`0x1F`/`0x0C`/`0x0D`/`0x0F`/`0x1B` es rampa, ascensor, puerta — mirando sus `p3`/`p4` y los handlers en el disasm.
2. **Geometría fiel:** decidir capturar las 100 name tables de openMSX (fiel, confiable) vs portar el motor de glifos del ROM (independencia del emu, mucho más trabajo).
3. **Comportamiento:** portar los handlers de movimiento del disasm, validando frame a frame contra la RAM de openMSX.

---

## 7. Archivos generados esta sesión

**Código C:** `geom.c/.h` (decode objetos+geom RLE), `map_tiles.c/.h` (tiles del mapa), `actors.c/.h` (jugador/enemigos), cambios en `doors.c` (fix puntero objetos), `hal_sdl2.c` (overlay+screenshot+actores), `main.c` (modos viewer/shot).
**Datos:** `castle_objects.json` (objetos exactos 100 salas).
**Scripts:** `build.ps1`, `force_dump.tcl` (dump 100 salas), `dump.tcl`, `render_vram.py`, `gen_tiles.py`, `gen_mapdata.py`, `extract2.py`, `parse_all.py`, `compare_objs.py`, `decode_geom.py`.
**Lanzador:** `ver_castillo.bat` (maqueta jugable).
**Imágenes de referencia:** `real_vs_mapimg.png`, `mapd_vs_real.png`, `real_vs_map.png`, `player2.gif`, `tour_real.gif`.
