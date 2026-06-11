# Changelog

## 2026-06-11 (3) — Fase 2 COMPLETA: runtime conmutado al room loader; −580 KB de tablas

### El juego ahora decodifica TODO desde el ROM en runtime
`faithful_play()` llama `rl_load_room()` (sub_64DD portado): la VRAM emulada
queda con la sala real y el VDP la renderiza nativo. ELIMINADOS del repo:
`colmap_data.c` (319 KB), `map_real.c` (220 KB), `keys/doors/items/
blocks_data.c` y sus generadores `gen_*.py`. El exe pasó de 403 KB a 173 KB.

### Recableado de la capa maqueta (muere en Fases 3-5)
- `actors.c`: colisión desde la RAM del loader (0xE496) — antes COLMAP[][].
- `keys_port`/`items_port`: tablas 0xE3D6 del loader; al recoger se blanquea
  la celda en VRAM (el gráfico horneado real desaparece). Persistencia por
  slot. Fuera el bitmap sintético KEY_BMP: las llaves se ven con su gráfico
  real del ROM.
- `doors_port`: tabla 0xE346; persistencia de abiertas por POSICIÓN (cubre a
  la gemela de la sala vecina sin necesitar su tabla). Al abrir: blanqueo en
  VRAM + clear de colisión.
- `blocks_port`: bloques desde la tabla COLL 0xE386 (códigos 0x30-0x35; el
  0x34 son trampas — Fase 4). Gráfico 2x2 capturado de la VRAM al cargar.
- `enemies_port`: gráfico capturado de la VRAM (fuera ROOM_NT/RT_TILES);
  spawn blanqueado; movimiento sigue siendo path-replay (Fase 4).
- `hal_sdl2.c`: `debug_draw_geom` quedó reducido a overlay de actores
  (bloques/jugador/enemigos/inventario); el fondo es el render del VDP.

Harness 10/10 (las suites del loader validan el mismo código que ahora corre
el juego). Verificación visual: sala 0x70 jugable con llave/puerta/olla/
arqueros desde el ROM.

## 2026-06-11 (2) — Fase 2 (núcleo): ROOM LOADER portado, 700/700 fixtures byte-exactos

### room_loader.c = sub_64DD fiel, instrucción a instrucción
Las 100 salas decodificadas desde el ROM en runtime, verificadas contra los
dumps de openMSX (`python tests/run_tests.py`, 11/11):
- **colmap** 0xE496 (20×30) + tabla 0xE6EE celda→objeto: 900 B × 100 ✓
- **e346** puertas (val=(variante<<4)|(color+1), persistencia por categoría
  de columna en 0xE00D+) ✓  **e3d6** coleccionables ✓  **e43e** estructurales
  (byte 3 = count del tramo) ✓  **objs** 0xE380-0xE49F (enemigos COLL 0xE386 +
  BAT 0xE416 + contadores) ✓
- **ont** name table ✓ — valida también el ALOCADOR de tiles por tercio
  (sub_6B0B: tablas 0xE946/E9A6/EA06, contadores 0xEA66+ desde 0x72/0x1A/0)
- **vram** pattern+color completas ✓ — espejos (sub_6D5A bit-reverso, orden
  invertido), corrimiento de 4px (sub_6D75), variantes por clase (llaves 4
  tiles color de 0x6DC9; puertas/escaleras 2 tiles color de 0x6DCF; enemigos
  16 o 28/40 tiles ×3 tercios), y el residuo del boot+título (rl_boot_vram).

### Estructura descifrada (lo importante para el resto del port)
- Streams: 0x7CF2 + 42*fila + 4*col → +0 banda sup, +4 banda inf, +2 cuerpo
  (2 pasadas que comparten puntero + pasada de objetos sub_69AA).
- Cursor: EADB=col 0..28 (de a 2), EADC=fila 0..19. Celdas de a pares.
- sub_5E80: colmap codes desde tablas ROM 0x7780/0x77B6.
- Tabla de descriptores de tiles 0x7BDA: entradas de 3 bytes (addr, count),
  índice = código de tile lógico (IY = 0x7BDA + 3*E).
- Persistencia: bitfields por sala — puertas 0xE00D (21 B/fila de castillo),
  COLL 0xE0DF (2 B/sala), items 0xE1A7 (2 B/sala), BAT 0xE26F (1 B/sala).
- HUD: sub_5DC0/5E01/5E5C = score/llaves/corazones al cargar sala (los 6
  dígitos SIEMPRE — la supresión de ceros del título es porque no se llama).

### Harness
- CASTLE_DUMP ahora corre el decoder portado y vuelca tablas RAW + VRAM por
  sala; el runner compara los 7 sets × 100 salas contra fixtures.
- tools/diff_dump.py para iterar diffs por sala/offset.

Pendiente de Fase 2: conmutar el runtime de la maqueta (colmap_data.c,
map_real.c, keys/doors/items_data.c → room_loader) y borrar ~580 KB de tablas.

## 2026-06-11 — Fase 1 COMPLETA: VDP SCREEN 2 fiel; título byte-idéntico a openMSX

### El criterio de cierre se cumplió
`tests/run_tests.py` 7/7, incluyendo dos suites nuevas:
- **vdp**: render del exe (CASTLE_VRAMIN) == renderer de referencia Python,
  pixel-exacto, sobre 11 dumps de VRAM reales.
- **título**: la VRAM del port al entrar a la fase "esperar input" ==
  `vram_title.bin` capturado de openMSX con bp en sub_4AD7 (`cap_title.tcl`),
  byte-exacto en pattern/name/color (de 1046 diffs iniciales a 0).

### Video (causa raíz de los tiles rotos de la intro)
- UN solo modelo: VRAM 16KB + registros; `vdp_render()` lee SCREEN 2 con los
  3 tercios reales; color 0 = backdrop. ELIMINADOS: `screen.c` (compositor
  plano que colapsaba los tercios — el flatten del 2026-05-24 era incompatible
  con cómo el juego usa SCREEN 2), ruteo de VRAM a buffers paralelos.
- En gameplay los tercios difieren en 150-220 tiles (verificado contra
  `vram_XX.bin`) — el modelo plano nunca pudo ser correcto.

### Título portado FIEL del disasm (sub_4A4A y rutinas hijas)
- `sub_62B0` (char→tile): el caso dígito CAE en el caso letra (sin RET).
  El "fix Z80" del 2026-05-18 inventaba un RET NC — revertido. Créditos:
  letras 0x01+, dígitos 0x1D+ (base C=1); HUD: base C=0x59.
- `sub_64AB` portado como `tiles_load_from_desc()`: tabla de descriptores
  0x7BC0 = words simples (el "formato compacto" documentado era falso).
- Carga del título fiel: logo 70 tiles por tercio (desc 0x7BC2/0x7BC4);
  font+dígitos SOLO a tercios 1-2 (sub_4E8E/4E91). El load viejo pisaba
  las letras del tileset de juego (0x5D-0x72 tercio 0) — eso era la
  corrupción visible.
- `sub_4AE2` fiel: FILVRM pattern+0x400=0x00 / color+0x400=0x11.
- Animación del logo fiel (sub_4B54/4B7F/4BC4): 1ª pasada deja rastro,
  2ª lo limpia retrazando, seq2 sube limpiando la fila inferior; final (9,6).
- Llaves HUD (sub_4EA2): color directo de la tabla ROM 0x6DC9 — el color
  lógico 1 es ink 8 (rojo), no 6; corregido también KEY_COLMSX.
- Tile 0x00 no se carga (estado INIGRP del BIOS: color 0x01).
- HUD partido: `draw_hud()` estático (boot) vs `draw_hud_dynamic()`
  (sub_5A2D, por frame); el título no muestra score/llaves/corazones.

### Harness
- `CASTLE_VRAMIN` + `CASTLE_TITLEDUMP` + `CASTLE_FAST` (modos headless).
- `tools/cap_title.tcl` (oráculo del título), `tools/diff_vram.py`.
- Borrados los pseudo-fixtures `vram_title_init.bin`/`vram_demo.bin`: los
  generaba el propio port (tiles_dump_vram), no openMSX. Contaminados.

## 2026-06-09 (3) — Collision now uses the ROM's REAL tilemap (0xE496), no more pixel heuristics

### The deep fix (user demanded: "understand the ROM, stop guessing")
Player/map collision no longer derives solidity from rendered pixels (the
40%-nonblack-tile heuristic — the root cause of every phantom-collision class
of bug). It now uses the game's OWN collision tilemap at RAM 0xE496, captured
per-room (colmap_XX.bin → `colmap_data.c`, 100×20×30).

Cell semantics REVERSE-ENGINEERED and verified against every known object in
rooms 0x70 and 0x00 (gates, stairs, keys, items, block, both archers, walls):
  0x00 air | 0xE0 wall/floor | 0xA0/0xA2 door(2x3)/stair | 0xA8 pushable block
  0x38 enemy | 0x24/0x20 collectible (key/item)
  **bit 0x80 = blocks the player** (E0/A0/A2/A8 have it; 38/24/20 don't —
  that's why you walk THROUGH keys/items/enemies and not walls/doors).
Bytes 600-899 of the dump are the parallel 0xE6EE table (cell → object index)
— matches gate slot indices; documented for future use.

### Implementation
- `gen_colmap_data.py` → `colmap_data.c/.h` (COLMAP[100][20][30], field→screen
  offset (+1,+4)).
- actors.c: working copy `s_cm` per room; solid = `cell & 0x80`. 0xA8 cells
  cleared at load (blocks simulated dynamically via block_solid). Doors stay
  solid until opened: doors_port now calls `actors_cm_clear()` on open AND when
  restoring a persisted-open door (incl. twins). DELETED: compute_solid pixel
  heuristic, tile_solid[], and ALL spawn/key/item-cell exclusion hacks — the
  real map simply doesn't mark those as blocking.
- Edges: HUD rows mirror field row 0 (vertical shaft exits now possible),
  below-field = fall exit, screen cols 0/31 mirror field cols 0/29 (open edge
  doors are real passages).
- spawn_player scans the real map for a 2x2 hole with real floor below.

### Verified (harness battery on real map)
walk/feet-on-floor ✓, door blocks flush without key ✓, opens+consumes with
key ✓, block push (now reaches col 18 — heuristic had invented a wall) ✓,
key collect ✓, jump arc intact incl. real ceiling hit under mid platform ✓.

## 2026-06-09 (2) — Edge-based hitbox (16x16) + room transitions preserve position

### Fixed (user-reported)
- **Hitbox = sprite borders**: player physics box was 8x14 CENTERED inside the
  16x16 sprite (drawn at px-4,py-2) — collisions referenced the "center", the
  sprite visually overlapped walls/floors by 4px, and the player could wedge into
  1-tile gaps where the sprite didn't fit ("stuck mid-map" after some jumps).
  Now PW/PH = 16x16 = the sprite edge, drawn at (px,py) with no offset; probes
  keep 1px inset per side so exact 2-tile (16px) passages don't snag. Applies to
  walls/platforms, doors (blocks flush at the door edge), enemy contact (full
  16x16 boxes), keys/items/doors AABBs (16,16), block pushing (lead = px+16), and
  spawn search (needs a 2x2-tile hole now).
- **Room transitions preserve position**: crossing an edge now KEEPS the
  coordinate perpendicular to it (exit through a mid-height side door → enter the
  next room at the SAME height; falling/jumping through top/bottom keeps the
  column). Before, spawn_player()'s heuristic picked "the best standable cell",
  often teleporting the player to a different door (e.g., the one previously used
  to enter). Heuristic now only used for initial spawn / after damage; if the
  entry point lands in solid, short same-axis nudges (±4..32px) then fallback.
  In-flight jump arcs continue across rooms.

### Verified (harness)
- Door blocks flush at sprite edge without key (px+15 == door edge), opens+
  consumes with key, walk-through after. Block push (15→17) and item pickup
  walkthrough still work. Feet rest exactly on floor line.

## 2026-06-09 — Twin doors across rooms + collectible items (0xE3D6)

### Fixed (user-reported)
- **Twin-door pairing**: edge gates (col0/col28, row0/row17) are the SAME physical
  door in two adjacent rooms. Opening one now also opens its twin in the neighbor
  room (`open_twin()` in doors_port.c, lateral match by drow ±1, vertical by dcol ±1,
  grid wraps like the viewer). Before, passing through an open door landed you
  against/on the closed twin — perceived as a phantom collision area to jump over.
  Verified: opening room70's (29,15) gate marks room71's (1,15) twin OPEN.
- **Items are now collectible** (were solid baked obstacles you had to jump):
  0xE3D6 vals 0x22-0x29 (map/power-ups/food/treasure per dispatcher `sub_5BB0`)
  → `items_data.c` (256 items / 70 rooms, gen_items_data.py), `items_port.c/.h`:
  collected on touch (AABB), vanish (blanked), persist collected, NEVER solid
  (item_cell exclusion in actors solid_at / actors_tile_solid).
  TODO: faithful effects (score for 0x27-29, map reveal for 0x22, power-ups).
- Verified blocks' spawn exclusion was already correct (push & return repro: no
  phantom at the old position in room 0x70).

### Added
- Harness: `CASTLE_MOVES` (per-frame movement script: R/L/U/D/A/.) and
  `CASTLE_DOORROOM=XX` (swap door set at f25 to inspect a neighbor room's doors).

## 2026-06-08 — exe boots intro → faithful game (no more launcher .bat needed)

### Changed
- `main()` default path now plays the **intro** and, on key press, enters the
  **faithful gameplay** directly. Before, the playable/faithful build was gated
  behind `CASTLE_VIEW=1` (set by `Jugar.bat`); the default path ran the
  incomplete skeleton `game_frame()` loop.
- Extracted the viewer gameplay loop into `faithful_play(uint8_t start_room)`
  (`main.c`, declared in `game.h`). `CASTLE_VIEW` now just calls it too.
- `title.c` `game_start:` now calls `faithful_play(0x70)` instead of the skeleton
  `while(!g_game_over){game_frame();}` loop.
- `hal_vdp_present()`: faithful geometry render (`debug_draw_geom`) now triggers on
  `g_actors_on` (during faithful play) OR `CASTLE_GEOMDBG=1` — so the screenshot
  ACTORS mode and the in-game render no longer need the env var.
- `Jugar.bat` / `LEEME.txt`: updated — double-clicking `the_castle.exe` is enough;
  the .bat is now optional (speed tuning / skip-intro dev mode).

## 2026-06-08 — Keys & doors converted from real ROM tables (not shape-detection)

### Changed
- **Keys** now come from RAM table `0xE3D6` (dispatcher `sub_5BB0` @0x5BB0):
  `val >= 0x2A` ⇒ key, color = `val - 0x2A`. → `keys_data.c` (156 keys / 64 rooms).
- **Doors/gates** now come from RAM table `0xE346` (open routine `sub_758C` @0x758C,
  located via `sub_4325` → `0xE6EE` index): required color = `(val & 0x0F) - 1`,
  consumes exactly 1 key. → `doors_data.c` (259 gates / 98 rooms).
- Confirmed `0xE43E` (stride 5) = stairs/ramps, NOT doors.
- Screen mapping for both = `(col+1, row+4)` (same as player `sub_6F4D`).
- Shared color space key⟷door (both index `0xE337`): 0=blue 2=magenta 3=green
  4=cyan 5=yellow (`KEY_COLMSX = {4,6,13,2,7,10}`).

### Added
- `cap_e346.tcl`, `cap_e43e.tcl` — openMSX table dumpers (→ `eXXX_YY.bin`).
- `gen_keys_data.py`, `gen_doors_data.py` — reproducible C-table generators.
- `KEY_BMP[16]` synthetic key sprite (baked name table is inconsistent across rooms).
- Docs: `DEVLOG_llaves_puertas.md`, `ASSETS_llaves_puertas.md`.

### Removed (superseded)
- Shape/map-image detection of keys/doors (was "guessing"; wrong colors/counts).

## 2026-06-03 — Identified sub_6EE1 as sprite-setter, not name-table write

### Fixed

- **title.c** (`intro_prepare_vram`, `intro_cleanup`): Removed incorrect blank-tile
  writes to name table row 0 cols 8-13. Z80 `sub_4B07` positions sprites 8-13 at
  pixel (Y=0, X=0) with blank pattern — it does NOT write to the name table. No
  SDL equivalent needed.

### Changed

- **README.md**: Port status table now includes `sub_6EE1` entry. Key Gotchas
  section documents `sub_6EE1` as a sprite attribute setter via BIOS `WRTVRM`
  (`CALL 0x004D`), not a name-table writer.

### Research

- **`CALL 0x004D`** confirmed as BIOS `WRTVRM` (Write VRAM) per MSX Wiki.
- **`sub_6EE1`/`sub_6EAE`**: Uses `LD HL,(GRPATR)` (sprite attribute table base at
  0xF3CD, set by INIGRP) to calculate sprite entry address = GRPATR + sprite_num*4,
  then calls WRTVRM 4 times to set Y=lo(HL), X=hi(HL), Pattern=tile*4, Color=translated_tile.
  Port reimplements this as `put_tile(col, row, tile)` writing directly to the name table
  since SDL2 has no sprite hardware — a semantic translation, not 1:1 bytecode.
- **Z80 enemies/effects render via sprites**, not name-table tiles. The port's
  direct name-table writes are functionally equivalent but architecturally different.

## 2026-05-25 — Per-third cleanup + music fix + credit strip comments

### Fixed

- **music.c** (`music_play_game`): Game music data now loaded from `0x7A73`
  (channel A) and `0x7A8F` (channel B) instead of `0x7ABE` — `0x7ABE` contains
  demo AI keyframe data, not music.

- **tiles.c**: Removed all `-Wtype-limits` warnings from `TILES_PER_TERCIO`:
  bounds checks comparing `uint8_t` against `256` were always true/false.

### Changed

- **tiles.c**: `TILES_PER_TERCIO`, `VRAM_THIRD_SIZE` defines removed. All
  VRAM write functions (`write_tile_to_vdp`, `tiles_rom_to_vram`,
  `tiles_vram_from_rom`) no longer take per-third parameters — single flat
  write only. Removed `tiles_write_range_to_thirds()` (unused).
  `tiles_dump_vram()` simplified (no per-third dump).

- **game.h**: Updated declarations for `tiles_rom_to_vram`,
  `tiles_vram_from_rom`; removed `tiles_write_range_to_thirds`.

- **room.c** (`load_tileset`): Removed `all_thirds` parameter.

- **room.c**, **title.c**: All callers updated, per-third comments cleaned up.

- **title.c**: CREDIT_STRIPS defines, array, and comments now include the
  decoded text: `[ 1985  ISAO YOSHIDA`, `[ 1986 KEISUKE IWAKURA`,
  `PRESENTED`, `BY`, `ASCII CORPORATION`.

## 2026-05-24 — Flat tile architecture + screen compositor + SDL_MapRGB palette

### Added

- **tiledata.h/c**: New module — overlay tile arrays loaded from ROM:
  `g_font[28]` (A-Z + symbols @ 0x8796), `g_digits[10]` @ 0x86F6,
  `g_title_logo[70]` @ 0x8056, `g_hud_logo[28]` @ 0x7E96+,
  `g_hud_map[28]` @ 0x84B6, `g_wall_variants[4]`, `g_door[1]`,
  `g_key_base[2]`, plus `g_keys[12]` generated at runtime for 6 ink colors.

- **screen.h/c**: New module — flat screen buffer `g_screen_buf[24][32]`
  + background tiles `g_bg_tiles[256][16]`. `screen_render()` composites
  background from these arrays. Overlay tile renderers: `screen_put_tile()`
  and `screen_put_tile_array()`.

### Changed

- **tiles.c**: `g_tiles` reduced from 768 to 256 entries (only third 0).
  `tiles_load_from_rom()` loads TILE_MAP only to third 0.
  `tiles_reload_all()` writes only third 0.
  Added `tiles_vram_from_rom()` — writes ROM data directly to VRAM
  without touching g_tiles.

- **hal_sdl2.c**: VRAM writes in pattern table (0x0000-0x17FF) and color
  table (0x2000-0x37FF) route to `g_bg_tiles[]`, ignoring the third.
  Name table writes (0x1800-0x1AFF) route to `g_screen_buf[]`.
  `vdp_render()` now calls `screen_render()` then `vdp_render_sprites()`.
  Removed dead per-third render code. Old border_rgba and sprite pixel
  packing replaced with `g_palette[color_idx]` from SDL_MapRGB.

- **title.c**: `load_title_tiles()` replaces 3 old loaders — loads
  BG1_MAIN (70 tiles, 0x73-0xB8) to all 3 tercios, font A-Z (0x01-0x1C)
  to tercios 1-2, digits (0x1D-0x26) to tercios 1-2, credit digits
  (0x5D-0x66) to tercios 1-2, credit font (0x6E-0x89) to tercios 1-2.
  Removed redundant credit re-load inside cycle loop.

- **CMakeLists.txt**: Added tiledata.c and screen.c.

### Fixed

- **hal_sdl2.c**: Palette packed via `SDL_MapRGB()` instead of hardcoded
  `0xAARRGGBB` literals — fixes pink/magenta tint on SDL_RGBA8888
  little-endian. Also applied to border color and sprite rendering.

- **tiledata.h/c**: Added 2×2 door tile system:
  - `g_door_base[4]` loaded from file offset 0x59F6 (4 tiles forming a
    2×2 door: white frame top, dark blue panel bottom).
  - `g_door_open[2]` loaded from file offset 0x5A36 (open door frame
    upper tiles, bottom 2 = blank).
  - `g_door_variants[6][4]` generated at runtime via
    `tiledata_generate_doors()` — 6 ink colors for the door panel.
  - Renamed old `g_door[1]` → `g_heart[1]` (heart/life tile at file
    0x5A76, was mislabeled as door).

- **main.c**: Init order now includes `tiledata_load_from_rom()` and
  `screen_init()` before `tiles_load_from_rom()`.

### Removed

- Per-third pattern table model abandoned — flat `g_bg_tiles[256][16]`
  with no per-third differentiation. All third-specific writes collapse
  to the same destination.

## 2026-05-18 — Z80 char_to_tile match + per-third font loading

### Fixed

- **title.c / camera.c** (`char_to_tile`): Now matches the Z80 original exactly:
  `chr - 0x30 + 0x5D` for ALL characters ≥ 0x30 (both digits and letters). The
  old formula `chr - 0x30 + 0x1C + tile_base` was incorrect — the Z80's
  `RET NC` after `ADD 0x5D` means the letter path (`SUB 0x41, ADD C`) is ONLY
  reached for chr < 0x30 (punctuation). This means:
  - `'0'..'9'` → VRAM **0x5D..0x66**
  - `'A'..'Z'` → VRAM **0x6E..0x87**

- **title.c**: Added `load_credit_digit_tiles()` — loads digit tile patterns
  from ROM **0x86F6** (same data as ANIM_BG) to VRAM 0x5D-0x66 in thirds 1-2 only.

- **title.c**: Added `load_credit_font_tiles()` — loads font letter patterns
  from ROM **0x8796** (same data as WALLS, 28 tiles) to VRAM 0x6E-0x87 in
  thirds 1-2 only. The two extra tiles (0x8936-0x8956) provide the custom
  `'['` → "(c)" and `'\'` → "?" symbols used in credits.

- **title.c** (`title_screen`): Calls both loading functions after
  `load_title_border_tiles()` so credit text renders correctly in thirds 1-2
  while third 0 retains WALLS data at the same VRAM indices.

- **tiles.c**: Removed embedded `FONT_DIGITS` const arrays (tiles 0x1B, 0x1D-0x26)
  — digits now come from ROM 0x86F6 to the correct Z80-mapped positions 0x5D-0x66.

- **main.c / game.h**: Removed `tiles_load_bios_rom()` — no external
  `msxbios.rom` needed. All tiles come from the game ROM.

### Changed

- **AGENTS.md**: Updated char_to_tile docs, added per-third credit tile map,
  documented digit source (ROM 0x86F6), removed BIOS font references.

## 2026-05-17 — Title screen VRAM fix + BIOS font + char encoding

### Added

- **tiles.c**: `tiles_reload_all()` — reloads all tiles from `g_tiles[]` to all
  3 VRAM thirds (undoes `intro_prepare_vram()` clearing).

- **tiles.c**: `tiles_load_bios_rom()` — loads MSX1 BIOS charset from external
  `msxbios.rom`, mapping letters A-Z to VRAM 0x01-0x1A and digits 0-9 to
  0x1D-0x26 for credit text rendering (`tile_base=0x01`).

- **title.c**: `load_title_border_tiles()` — loads 4 decorative border tiles
  from BG1_MAIN (ROM 0x8056) to VRAM 0x73-0x76 for the logo frame.

### Fixed

- **title.c / camera.c** (`char_to_tile`): Digit formula now correctly adds
  `tile_base`: `chr - 0x30 + 0x1C + tile_base`. The old formula
  `chr - 0x30 + 0x5D` ignored `tile_base`, causing inconsistent VRAM indices
  when `tile_base ≠ 0x41`. Matches Z80 original: `SUB 0x30` → `ADD 0x5D` →
  fall through → `SUB 0x41` → `ADD C`.

- **title.c** (`draw_credit_row`): Changed `tile_base` from `0x73` to `0x01`,
  correct for credit text.

- **title.c** (`title_screen`): Replaced `tiles_reload_walls_and_anim()` with
  `tiles_reload_all()` + `load_title_border_tiles()`. `intro_prepare_vram()`
  clears pattern/color tables for tiles ≥0x80 in third 0, and all tiles in
  thirds 1-2. The old code only restored WALLS+ANIM_BG, leaving title logo
  blocks (0x77-0xB8) and BIOS font tiles (0x01-0x26) cleared in thirds 1-2 —
  causing corrupted title logo and credit text in those screen bands.

- **tiles.c** (`TILE_MAP`): Replaced title block entries (A: 0x9116, B: 0x81A6,
  C: 0x8286) with single 66-tile logo body entry from ROM 0x8096 (tile #5+ of
  the full logo dataset at 0x8056). The entire title logo (70 tiles, 0x73-0xB8)
  is a contiguous block starting at ROM 0x8056, not three separate blocks.

- **tiles.c** (`tiles_reload_walls_and_anim`): Loop count corrected from 28 to
  26 (was writing indices 0x59-0x74, clobbering wall variant tiles 0x73-0x74).

- **main.c**: Moved `tiles_load_bios_rom()` after `tiles_load_from_rom()` so
  the BIOS font properly overwrites the game charset at indices 0x01-0x02
  instead of being overwritten by it.

### Changed

- **AGENTS.md**: Updated with gotchas on `char_to_tile` digit formula,
  credit `tile_base`, title border tile loading, and init order.
  Removed `vram_tiles.c` and `vram_tiles.h` references.

## 2025-05-16 — Tile loading rewrite

### Changed

- **tiles.c**: Replaced hardcoded `VRAM_TILES[185][16]` array (from openMSX VRAM dumps)
  with runtime loading from the game ROM file. A lookup table maps each VRAM index to
  its ROM address. Unmapped tiles fall back to BLANK.

- **room.c (load_tileset)**: Fixed tile data reading to use raw 16-byte interleaved
  format from ROM directly, instead of incorrect pointer-indirection through a
  non-existent pointer table.

### Fixed

- **tiles.c (TILE_MAP)**: Corrected title block B ROM address from `0x9926` to `0x81A6`
  (verified against title-screen VRAM dump).

- **tiles.c (TILE_MAP)**: Added missing gameplay wall tiles `0x73-0x74` at `0x89C6`
  and `0x76` at `0x8976` (these sit after the 26-tile WALLS block and were previously
  unmapped, rendering as BLANK).

### Removed

- **vram_tiles.c**: Deleted (duplicate VRAM dump data, was already commented out
  in CMakeLists.txt).

### Notes

- All tiles in ROM are raw 16-byte interleaved MSX format (8 pattern bytes +
  8 color bytes). NOT compressed as previously assumed.
- ROM descriptor table: 0x7BC0-0x7BC6 store full 16-bit addresses; 0x7BC8+ use
  compact format (context-dependent hi byte).
- **All 185 tiles now mapped** to verified ROM addresses (19-entry TILE_MAP).
- Title-screen tile blocks: A (0x77-0x87 @ 0x9116), B (0x88-0x95 @ 0x81A6),
  C (0x96-0xB8 @ 0x8286), decorative borders use BG1_MAIN (0x8056) at 0x73-0x76.
- Gameplay wall tiles: main WALLS block 0x59-0x72 @ 0x8796 (26 tiles), plus
  0x73-0x74 @ 0x89C6, 0x75-0x76 @ 0x8966 (not contiguous with main block).
