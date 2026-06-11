# AGENTS.md — The Castle MSX → SDL2 Port

## Project

Port of *The Castle* (ASCII, 1986) MSX game from Z80 assembly to C99 with SDL2.
Entrypoint: `main.c:main()`. All game variables defined in `main.c`, declared `extern` in `game.h`.

## Build & Run

Windows (this machine, canonical): `powershell -ExecutionPolicy Bypass -File .\build.ps1` → `the_castle.exe` (MinGW + SDL2 under `..\_buildtools\`). CMake is NOT installed here; `CMakeLists.txt` mirrors the same source list for other platforms:

```
cmake -B build [-DPAL_TIMING=ON] [-DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON]
cmake --build build
build/the_castle [path/to/the_castle.rom]
```

Defaults: RelWithDebInfo build type. ROM is copied to `build/the_castle.rom` by CMake if present.

## Tests (harness del oráculo) — correr SIEMPRE antes de commitear

```
python tests/run_tests.py            # compila + 5 suites
python tests/run_tests.py --no-build
```

- `tests/fixtures/` = dumps byte-exactos de openMSX (ver su README). Son la
  verdad de referencia; NUNCA fuente de datos del runtime.
- `CASTLE_DUMP=dir the_castle.exe` vuelca colmap/doors/keys/items por sala en
  formato canónico (sin SDL); el runner lo compara contra los fixtures.
- `tools/` = scripts de captura (.tcl para openMSX) y generadores `gen_*.py`
  (corren desde la raíz del repo).
- Rumbo del proyecto: `PLAN_PORT_FIEL.md` (port fiel desde el disasm, los
  fixtures pasan de datos a tests).

## Architecture

- `hal.h` — pure interface (portability layer). `hal_sdl2.c` implements it.
- Core logic (`the_castle.c`, `room.c`, `camera.c`, etc.) calls only `hal.h` + `game.h`.
- To add a platform: write `hal_<platform>.c`, add to CMakeLists.txt.

## Video (Fase 1 — VDP fiel, 2026-06-11)

- **Un solo modelo de video**: VRAM emulada de 16KB + registros en `hal_sdl2.c`. `hal_vdp_write_vram`/`read_vram` van DIRECTO a `vram[]`; `vdp_render()` interpreta SCREEN 2 con **3 tercios reales** (pattern 0x0000+t*0x800, color 0x2000+t*0x800, name 0x1800). Color 0 = backdrop (reg 7). `screen.c` (compositor plano) fue eliminado.
- **`g_palette[16]`** (static en `hal_sdl2.c`) se empaqueta con `SDL_MapRGB()` — nunca hardcodear literales `0xAARRGGBB`.
- Verificación: suite `vdp` (render == referencia Python, pixel-exacto) y suite `título` (VRAM del port == captura openMSX `vram_title.bin`, byte-exacto) en `tests/run_tests.py`.

## Key Gotchas

- **ROM required at runtime.** The original 32KB `.rom` file must be available. It provides music data, room scripts, tile descriptors.
- **`music_isr_tick()` lives in `hal_wait_vsync()`**, NOT in the game loop — mimics the MSX VBlank ISR.
- **`sub_6EE1`/`sub_6EAE` is a SPRITE attribute setter**, not a name-table writer. It sets sprite `B` at pixel position `(X=H, Y=L)` via BIOS `WRTVRM` (0x004D). The port translates this to `put_tile(col, row, tile)` because SDL2 has no sprite hardware. Enemies, effects and blanking in the Z80 all use sprites; the port writes directly to the name table instead.
- **`CALL 0x004D`** = BIOS `WRTVRM` (MSX Wiki confirmed). Takes HL=VRAM address, A=value. Used extensively by `sub_6EAE` for sprite attribute writes.
- **`LD HL,(0xF3CD)`** loads `GRPATR` (sprite attribute table base, set by INIGRP/SETGRP at init). Used in `sub_6EAE` to calculate sprite entry address.
- **`sub_4B07`** in Z80: positions sprites 8-13 at pixel (Y=0, X=0) with blank pattern — does NOT write to name table. No SDL equivalent (commented out in `intro_prepare_vram()` and `intro_cleanup()`).
- **Init order matters:** `hal_init` → `tiles_load_from_rom` → `game_init` → `enemies_init` → `particles_init` → `doors_init` → `music_init` → `camera_init` → `main_loop`.
- **`char_to_tile` (sub_62B0) — CORREGIDO 2026-06-11 contra disasm + oráculo:** el caso dígito (`chr < 0x3A`: `SUB 0x30; ADD 0x5D`) CAE sin RET en el caso letra (`SUB 0x41; ADD A,C`). Neto: dígito → `chr-0x30+0x1C+base`, letra → `chr-0x41+base`. El "RET NC" documentado antes NO existe (verificado contra `vram_title.bin`). Dos copias: `title.c:char_to_tile()` y `camera.c:camera_draw_string()`.
- **Tiles de texto del título (thirds 1-2, base C=0x01):** `'A'..'Z'` → 0x01-0x1A, `'['` → 0x1B, `'0'..'9'` → 0x1D-0x26. Cargados por `load_title_tiles()` fiel a sub_4A4A: font 28 tiles (descriptor 0x7BD0 → ROM 0x8796) a 0x01+, dígitos 10 (descriptor 0x7BCE → 0x86F6) a 0x1D+, SOLO tercios 1-2 (sub_4E8E/sub_4E91). El HUD usa base C=0x59 (letras sobre WALLS, tercio 0).
- **Tabla de descriptores 0x7BC0:** words simples (direcciones ROM), SIN formato compacto: 0x7BC0=0x7E96 hudlogo, 0x7BC2=0x8056 logo-izq, 0x7BC4=0x8286 logo-der, 0x7BC6=0x84B6 map, 0x7BC8=0x8676, 0x7BCA=0x86B6, 0x7BCC=0x86D6, 0x7BCE=0x86F6 dígitos, 0x7BD0=0x8796 font, 0x7BD2=0x9A56 llave, 0x7BD4=0x9A76 corazón, 0x7BD6=0x9A86 separador, 0x7BDA=0x8956 blank. Loader: `tiles_load_from_desc()` (= sub_64AB; dest `(tercio<<8)|tile`, encadenable).
- **Logo del título**: 70 tiles 0x73-0xB8 cargados POR TERCIO (3×) desde los descriptores 0x7BC2/0x7BC4 (datos contiguos en 0x8056). Posición final del logo: name table (col 9, fila 6), bloque 14×5 (mitades 0x73+/0x96+).
- **Llaves HUD (sub_4EA2):** 12 tiles 0x01-0x0C; patrón de (0x7BD2)=0x9A56, byte de color DIRECTO de la tabla ROM **0x6DC9** = {0x41,0x81,0xD1,0x21,0x71,0xA1} (ink 4,8,13,2,7,10 — ¡el color 1 es 8, no 6!).
- **Tile 0x00 no se carga en el boot**: conserva el estado INIGRP del BIOS (patrón 0, color 0x01).
- **HUD estático vs dinámico:** `draw_hud()` = solo lo que dibuja el boot (labels/map/logo/NO-MAP); score/llaves/corazones = `draw_hud_dynamic()` (sub_5A2D, por frame de juego). En el título NO se ven (oráculo).
- **Stubs in `main.c`:** `update_roller_by_pos()` and `update_bat_by_slot()` are temporary wrappers in `main.c` that `doors.c` depends on.
- **Two map layers:** `g_map[0x400]` (20×30 collision map) and `g_tilemap[]` (30×30 visual map).
- **BCD room coords:** `g_room_x` uses BCD (hi-nibble=row, lo-nibble=column). Arithmetic is DAA-style, not binary.
- **Tiles loaded from ROM at runtime.** `tiles.c` reads raw 16-byte interleaved tiles from the game ROM using `TILE_MAP[]`. Hardcoded `VRAM_TILES[]` and `vram_tiles.c` removed.
- **Tile data is NOT compressed.** All verified tiles in ROM are raw 16-byte format (pattern/color interleaved).
- (OBSOLETO 2026-06-11: la afirmación previa de que la tabla 0x7BC0 usa "formato compacto" desde 0x7BC8 era FALSA — son words simples, ver la entrada de descriptores arriba. La historia del "BG1_MAIN border 0x73-0x76" del título también era incorrecta: el logo completo se carga por tercio desde los descriptores 0x7BC2/0x7BC4.)
- **Per-third tile model (REAL, verificado contra dumps):** cada tercio tiene pattern/color table propia y el juego lo explota (en gameplay difieren 150-220 tiles entre tercios). `tiles_load_from_rom` (boot) duplica el tileset a los 3 tercios; los loads específicos (título, salas) escriben tercios individuales vía `tiles_load_from_desc`.
- **No external BIOS ROM dependency.** `tiles_load_bios_rom()` has been removed. All tiles come from the game ROM.
- **`vram_tiles.c` deleted** — removed from build.

## Controls

Arrows/WASD = move, Z/Space/Ctrl = fire, X = fire2, Esc = quit.

## Windows Build

```
cmake -B build -DSDL2_DIR="C:\SDL2\cmake"
cmake --build build --config Release
```

MSVC flags: `/W4 /WX- /wd4996`. GCC/Clang: `-Wall -Wextra -Wno-unused-parameter -Wno-unused-function`.

## HUD Tile Mapping (Pattern Table 0x00-0x72)

| Range | Count | Description |
|-------|-------|-------------|
| 0x00 | 1 | Blank |
| 0x01-0x0C | 12 | Key icons |
| 0x0D | 1 | Heart (life icon) |
| 0x0E-0x29 | 28 | Map graphic (7×4) |
| 0x2A-0x45 | 28 | Logo graphic (7×4, not title screen 14×5) |
| 0x46 | 1 | Vertical separator line (col 31, all 4 rows) |
| 0x47-0x50 | 10 | Digits 0-9 (ANIM_BG) |
| 0x51 | 1 | "Hi" label (HiScore) |
| 0x52-0x54 | 3 | "SCORE" label |
| 0x55-0x56 | 2 | "Key" label |
| 0x57-0x58 | 2 | "Life" label |
| 0x59-0x72 | 26 | Letters A-Z (WALLS/font, used as dynamic overlays) |

**Name table layout (rows 0-3):**
- Row 0: `blk SCORE blk blk blk Hi SCORE blk blk blk blk [MAP row 1] [LOGO row 1] |`
- Row 1: `blk ... [MAP row 2] *NN* *OO* [MAP end] [LOGO row 2] |`
- Row 2: `blk Key ... [MAP row 3] *MM* *AA* *PP* [MAP end] [LOGO row 3] |`
- Row 3: `blk Life ... [MAP row 4] [LOGO row 4] |`

**`draw_hud()` in camera.c** now replicates Z80 sub_4E0C + sub_64C3 logic:
- `hud_fill_rect()` = sub_64C3: writes incrementing tiles to a rectangle
- Calls in order: MAP(17,0,7,4,0x0E), LOGO(24,0,7,4,0x2A), separator col31 rows0-3, SCORE(1,0,3,1,0x52), HiSCORE(9,0,4,1,0x51), Key(1,2,2,1,0x55), Life(1,3,2,1,0x57)
- Then dynamic overlays: 6 score digits using 0x47-0x50, key icons 0x01-0x0C at row2 col3+, hearts 0x0D at row3 col3+
- Tiles 0x00-0x72 in **tercio 0 never change** — loaded once from ROM via TILE_MAP

**Init order:** `hal_init` → `tiles_load_from_rom` → `game_init` → `enemies_init` → `particles_init` → `doors_init` → `music_init` → `camera_init` → `main_loop`
