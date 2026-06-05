# The Castle (MSX → C/SDL2) — Traspaso: subsistema de objetos por sala

> Documento de handoff. Resume la ingeniería inversa, el formato decodificado, el
> cargador implementado, el censo, la metodología de validación y qué falta.
> Pensado para que otra persona u otra IA retome el trabajo sin reconstruir nada.
>
> **Convención de confianza:** ✅ validado (contra ROM/emulador) · 🟡 inferido del
> desensamblado · ⚠️ no confirmado / hipótesis.

---

## 1. Contexto

- **Proyecto:** port de *The Castle* (ASCII, 1986, ROM MSX1 de 32KB, ORG `0x4000`)
  a C99 + SDL2. Repo: `github.com/vrgimenez/TheCastleMSX_SDL2`, branch `OC-BigPickle`.
- **Enfoque del port:** reimplementación *semántica* (no emulación del Z80). HAL
  abstracta (`hal.h` + `hal_sdl2.c`). Requiere el `.rom` original en runtime.
- **Objetivo de esta sesión:** entender y completar el **cargado de objetos por
  sala** (enemigos, coleccionables, puertas/salidas), que el port **no tenía**.

### Hallazgo central
El port traduce la *lógica* de cada objeto (update/draw) pero **nunca poblaba las
tablas de objetos desde los datos de la ROM** → el castillo corría vacío. Toda la
lógica de `update_doors`/`update_collectibles`/`check_key_pickup`/enemigos operaba
sobre tablas siempre en cero. **El eslabón que faltaba es el cargador**, que acá se
decodificó y se implementó (`objects_load_from_rom`).

---

## 2. Notas de arquitectura del port (para contexto del que retome)

- **Modelo de tiles "plano":** `g_bg_tiles[256][16]` colapsa los 3 tercios del VDP
  (TMS9918A) en un único banco de 256. En `hal_vdp_write_vram` el tercio se descarta
  (`ignore third`). 🟡
  - Medido empíricamente (instrumentando el HAL con un shadow por-tercio): en
    gameplay el vocabulario total es **134 tiles** y el presupuesto máximo por frame
    **62** → capacidad de sobra. **Pero hay reuso del mismo índice en tercios
    distintos** (HUD vs playfield) en ~7 índices (`0x31,0x32,0x39,0x3D,0x3E,0x3F,0x40`,
    rango del logo `0x2A-0x45`) → el banco plano corrompe uno de los dos. ✅
  - Conclusión: el fix nativo correcto NO es reproducir los tercios (eso es portar
    una limitación de HW); es **des-plegar las regiones** en un banco más grande con
    remapeo en la carga. (Pendiente, no urgente para los objetos.)
- **Sprites → name table:** los enemigos se dibujan como tiles, no como sprites HW.
  El HAL igual conserva un renderer de sprites real (lee attr table a granularidad de
  pixel). El movimiento de enemigos en el original es **por casilleros** (no sub-pixel),
  así que el grid-snap del port es fiel. El jugador sí tiene X sub-tile (`0xE320`). 🟡

---

## 3. Sistema de objetos — DECODIFICADO

### 3.1 Tres tablas (en `doors.c`)
| Tabla RAM original | Array en port | Slots | Struct (port) |
|---|---|---|---|
| `0xE43E` | `g_objects`      | 16 | `{type, param, col, row, state}` |
| `0xE386` | `g_collectibles` | 16 | `{type, param, col, row, anim}` |
| `0xE416` | `g_bat_slots`    |  8 | `{type, param, col, row, move_flags}` |
| `0xE3D6` | `g_exit_doors`   |  – | `{active, type, col, row}` |

> El orden de bytes en la RAM original difiere del struct del port (el port reordena).
> En el dump de RAM, un slot BAT se ve `[param, type, col, row, flags]` (ej. `01 3B 0E 11 01`).

### 3.2 Dispatch por tipo (sub_5F1D del Z80) ✅
```
type <  0x30  -> tabla OBJ  (estructurales / salidas; 0x21=salida izq, 0x23=salida der)
0x30 <= type < 0x36 -> tabla COLL (coleccionables; 0x34 = roller)
type >= 0x36  -> tabla BAT  (enemigos; 0x36..0x3F)
```

### 3.3 Tabla de punteros y formato del stream ✅ (validado en openMSX)
- **Base de la tabla de punteros:** ROM `0x7CF2` (= valor runtime de `0xEAD7`).
- **Índice por sala:** `offset = 42*row + 4*col`
  - `row = g_room_x >> 4`, `col = g_room_x & 0x0F` (coords BCD).
  - El `42` y el `4` salen del multiplicador 16×16 `sub_5D24`.
  - Cada entrada de sala = puntero de 16 bits **little-endian** al stream.
- **Stream de objetos:** secuencia de entradas de **2 bytes** `[b0, b1]`, terminador
  `b0 == 0`. Decodificación:
  ```
  row  = (b0 & 0x0F) * 2          ; fila de SPAWN (pares 0..30; tilemap = 30 filas)
  col  =  b1 & 0x1F               ; 0..31
  type = (b0 >> 4) + ((b1 & 0x40) ? 0x20 : 0x30)
  ```
- **Cómo se consume en el original:** `sub_69AA` recorre el stream (puntero en
  `0xEAD9`), decodifica cada entrada y `sub_69DF` → `0x5F1A` la spawnea. **NO hay
  opcodes de control**: lo que parecían opcodes eran objetos con fila alta.

### 3.4 Validación ✅
- Calibrado contra openMSX: room `0x70` → puntero real `0xB7C4`; room `0x71` → `0xB803`.
  Confirma `offset = 42*7 + 4*0 = 294`, base `0x7CF2`.
- Room `0x70` decodificada incluye dos enemigos `0x3B` (en `(17,14)` y `(12,18)`),
  que coinciden con los dos slots `0x3B` vistos en el dump de RAM del emulador. ✅

### 3.5 Flujo de transición de sala (para destrabar navegación) 🟡
```
loop principal (~0x4064..0x40B9):
  LD A,(0xEAE3); OR A; RET NZ      ; compuerta de modo
  CALL sub_442D / sub_434A / sub_6F5C / sub_4406 / sub_438D / sub_4499  ; lógica + objetos
  LD A,(0xEAE0); OR A; RET NZ      ; compuerta
  LD A,(0xEAE1)                    ; DIRECCIÓN DE SALIDA (set por colisión con borde)
  CALL sub_5053                    ; despachador: si A=0 RET; CP 0x01/03/05/07 -> nav
     ; nav: g_room_x += {-0x10,+1,+0x10,-1} con DAA  (wrap del castillo)
     ; recarga la sala
```
- **Loop de la sala 100:** la navegación es BCD + `DAA`; al pasar de `0x99` envuelve a
  `0x00` → reentrás al castillo (lo que el jugador percibe como "recorrerlo de nuevo").
  El "salas cambiadas" en la 2da vuelta sale de flags de estado (candidatos
  `0xEAE2`/`0xEAE3`, bit 3 de `g_room_y`). ⚠️ no confirmado cuál.

---

## 4. El cargador implementado

`objects_load_from_rom(uint8_t room_x)` (en `doors.c`). Ya integrado en el árbol
del zip. **3 ediciones** respecto al repo original:

1. `doors.c`: la función `objects_load_from_rom()` + helper `rom_rw()` (tras `rom_rb`).
2. `game.h`: `void objects_load_from_rom(uint8_t room_x);`
3. `room.c`: llamada `objects_load_from_rom(g_room_x);` al final de `room_full_load()`.

```c
void objects_load_from_rom(uint8_t room_x)
{
    memset(g_objects,0,sizeof(g_objects)); memset(g_collectibles,0,sizeof(g_collectibles));
    memset(g_bat_slots,0,sizeof(g_bat_slots)); memset(g_exit_doors,0,sizeof(g_exit_doors));
    uint8_t  row = room_x >> 4, col = room_x & 0x0F;
    uint16_t ptr = rom_rw(0x7CF2u + 42u*row + 4u*col);   // rom_rw = read16 little-endian
    if (ptr < ROM_ORG) return;
    int io=0, ic=0, ib=0;
    for (int g=0; g<64; g++) {
        uint8_t b0 = rom_rb(ptr); if (b0==0) break;
        uint8_t b1 = rom_rb(ptr+1);
        uint8_t type = (b0>>4) + ((b1&0x40)?0x20:0x30);
        uint8_t c = b1 & 0x1F, r = (b0 & 0x0F) * 2;
        ptr += 2;
        if      (type < 0x30) { if(io<OBJ_SLOTS){ g_objects[io].type=type; g_objects[io].col=c; g_objects[io].row=r; io++; } }
        else if (type < 0x36) { if(ic<COLL_SLOTS){ g_collectibles[ic].type=type; g_collectibles[ic].col=c; g_collectibles[ic].row=r; ic++; } }
        else                  { if(ib<BAT_SLOTS){ g_bat_slots[ib].type=type; g_bat_slots[ib].col=c; g_bat_slots[ib].row=r; g_bat_slots[ib].move_flags=(b1&0x20)?0x03:0x01; ib++; } }
    }
}
```

### Self-test (validado contra el ROM) ✅
```
room 0x00 -> COLL=10 BAT=1          room 0x70 -> OBJ=4 COLL=16(cap) BAT=4
room 0x39 -> COLL=16 BAT=3          room 0x01 -> OBJ=14 COLL=16 BAT=8(cap)
room 0x04 -> vacío (correcto)       room 0x80 -> BAT=1
```
Los caps (16/16/8) truncan salas muy pobladas — fiel a los límites de slots del HW.

---

## 5. Censo (archivo aparte: `CENSO_objetos.txt`)
- **100/100 salas, 2067 objetos, 86 con enemigos.**
- Tipos: enemigos `0x38`×119, `0x39`×105, `0x3c`×46, `0x3a`×43, `0x3f`×30, `0x3b`×23,
  `0x3d`×14, `0x36`×12, `0x3e`×7, `0x37`×6 · coleccionables `0x30`×644, `0x31`×370,
  `0x32`×202, `0x33`×144, `0x35`×20, roller `0x34`×8 · salidas/puertas `0x20-0x2f`.
- ⚠️ **Caveats:** 4 salas "runaway" (sin terminador en <64) sobre-leen; algunas salas
  con conteos altos pueden inflar por sobre-lectura. **Tipos y presencia de enemigos
  por sala = sólidos; conteos exactos en salas grandes = aproximados.** Las posiciones
  son de SPAWN (la celda final pasa por post-proceso `sub_60AA/60EB`, no portado).

---

## 6. Metodología de validación (reproducible)

### 6.1 Instrumentación del port (headless)
Se parchea `hal_sdl2.c` con un modelo-sombra por-tercio y análisis por frame; se corre
headless con `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` y env vars
(`CASTLE_FAST`, `CASTLE_AUTOPLAY`, `CASTLE_INST_FRAMES`, `CASTLE_OBJLOG`,
`CASTLE_SELFTEST`). *(Este andamiaje NO está en el zip limpio; fue para análisis.)*

### 6.2 openMSX como segunda vía (clave)
```bash
apt-get install -y openmsx           # trae C-BIOS (cbios), no necesita BIOS propietaria
# correr headless y volcar RAM via Tcl:
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  openmsx -machine C-BIOS_MSX1 -cart the_castle.rom -script dump.tcl
```
- En Tcl: `set throttle off`, `debug read memory 0xADDR`, `keymatrixdown 8 0x01`
  (space para arrancar), `after time <s> {...}` para agendar volcados, `exit`.
- Para entrar a gameplay: presionar space (`keymatrixdown/up 8 0x01`).
- **Limitación:** los screenshots salen negros headless (sin GL); **solo el volcado de
  RAM es confiable**. La navegación headless no se logró driblear bien (las transiciones
  necesitan condición de borde + gates `0xEAE0/0xEAE3`).
- Direcciones útiles en RAM para calibrar: `0xEAD7` (base tabla, = `0x7CF2`),
  `0xEAD9` (puntero de stream actual), `0xE320` (g_room_x), tablas `0xE386/0xE416/0xE43E`.

---

## 7. Tabla de referencia (direcciones)

### RAM (work area)
| Addr | Significado |
|---|---|
| `0xE320` | `g_room_x` — coord BCD de sala (hi=fila, lo=col) |
| `0xE321` | `g_room_y` / flags (bit 3 = sala especial / has_map) |
| `0xE334/E335` | `g_player_col` / `g_player_row` |
| `0xE386` | tabla COLL (coleccionables) |
| `0xE416` | tabla BAT (enemigos) |
| `0xE43E` | tabla OBJ (estructurales) |
| `0xE3D6` | tabla ExitDoor |
| `0xEAD7` | base de tabla de punteros de objetos (= `0x7CF2`) |
| `0xEAD9` | puntero actual al stream de objetos |
| `0xEADB/EADC` | fila/col en curso del stream |
| `0xEAE0/EAE3` | compuertas del loop |
| `0xEAE1` | dirección de salida (`0x01`↑ `0x03`→ `0x05`↓ `0x07`←) |
| `0xEAE4` | `g_intro_active` (1=título/demo, 0=gameplay) |

### ROM
| Addr | Contenido |
|---|---|
| `0x7CF2` | tabla de punteros de objetos por sala (offset `42*row+4*col`) |
| `0x5748` | tabla de salas especiales `[room_x,col,row,script]` + default tras `0xFF` |
| `0x5887` | BG script (dibujo de sala) |
| `0x53D4` | loader estándar de sala (setea punteros fijos) |
| `0x69AA / 0x69DF / 0x5F1A` | intérprete/spawn de objetos |
| `0x5D24` | multiplicador 16×16 (índice de tabla) |
| `0x5053` | despachador de transición de sala |

### Códigos de tipo de objeto
| Código | Categoría | Qué es |
|---|---|---|
| `0x20-0x2F` | OBJ | estructurales/salidas (`0x21`=salida izq, `0x23`=salida der) |
| `0x30-0x33` | COLL | coleccionables (llaves/ítems) |
| `0x34` | COLL | roller (se desliza horizontal, rebota) |
| `0x35` | COLL | coleccionable |
| `0x36/0x37` | BAT | murciélagos |
| `0x38/0x39` | BAT | enemigos tipo-tile (mov. con test de colisión) |
| `0x3A/0x3B` | BAT | enemigos tipo-sprite (mov. que sondea celdas vecinas) |
| `0x3C-0x3F` | BAT | enemigos (variantes) |

> ⚠️ El port C tenía 2 defectos detectados: (a) `0x21/0x23` mal comentados como
> "llave" además de "salida" (son salidas); (b) enemigos `0x38-0x3B` aplastados a
> "key/powerup genérico" — no reproducen sus handlers distintos del original.

---

## 8. Estado: hecho / pendiente

**Hecho ✅**
- Formato de objetos decodificado y validado (tabla, índice, entrada, dispatch).
- `objects_load_from_rom()` implementado, integrado y probado (self-test OK).
- Censo de las 100 salas (`CENSO_objetos.txt`).
- Builds: Linux x64 y Windows x64 (cross-MinGW + SDL2 de fuente).

**Pendiente / próximos pasos (en orden de impacto)**
1. **Ver los objetos en pantalla.** Hoy el `.exe` corre pero arranca en sala que puede
   estar vacía y el dibujado depende del game-loop (stub). Pasos:
   - Forzar `g_room_x` a una sala poblada (ej. `0x70`) al init para test visual.
   - Cablear el game-loop para que llame update+draw de las tablas pobladas.
2. **Navegación entre salas.** Destrabar el disparador de `sub_5053` (gates `0xEAE0/EAE3`
   + flag `0xEAE1` + condición de borde). Sin esto no se recorre el castillo.
3. **Post-proceso de posición** (`sub_60AA/60EB`): pasar de fila de spawn a celda final
   exacta. Para que las posiciones del censo/loader sean 100% fieles.
4. **4 streams "runaway"** del censo: confirmar el terminador real (¿hay un 2do criterio
   además de `b0==0`?) revisando esas salas en el emulador.
5. **Enemigos `0x38-0x3B`:** portar sus handlers reales (`sub_48AD/4882/487A/4872/45AD`)
   en vez del genérico actual.
6. **2da vuelta (post sala 100):** identificar el flag que cambia el contenido de salas.

---

## 9. Build

### Windows (cross desde Linux, lo que se usó)
```bash
apt-get install -y mingw-w64
git clone --depth 1 --branch release-2.30.9 https://github.com/libsdl-org/SDL.git SDL2src
cmake -B sdlbuild SDL2src -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF -DCMAKE_INSTALL_PREFIX=sdl2-win -DCMAKE_BUILD_TYPE=Release
cmake --build sdlbuild -j4 && cmake --install sdlbuild
x86_64-w64-mingw32-gcc -O2 -DSDL_MAIN_HANDLED -static-libgcc *.c \
  -Isdl2-win/include -Isdl2-win/include/SDL2 -Lsdl2-win/lib -lSDL2 -o the_castle.exe
# distribuir: the_castle.exe + sdl2-win/bin/SDL2.dll + the_castle.rom
```
`mingw-toolchain.cmake`: setea `CMAKE_SYSTEM_NAME=Windows`, compiladores
`x86_64-w64-mingw32-{gcc,g++,windres}`, `CMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32`.

### Windows (nativo, con MSYS2/MinGW o Visual Studio)
```
cmake -B build -DCMAKE_BUILD_TYPE=Release   # con SDL2 instalado / SDL2_DIR apuntando a sus cmake
cmake --build build
```

### Linux
```bash
sudo apt install libsdl2-dev cmake
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/the_castle the_castle.rom
```

---

## 10. Cómo contribuir al repo (vrgimenez/TheCastleMSX_SDL2)
- Es repo activo de otro (commits diarios). Fork → branch desde `OC-BigPickle` → PR chico.
- **No hay LICENSE** en el repo → abrir issue pidiendo una antes de mandar código.
- No se pudo listar issues/PRs (rate-limit). Chequear antes de empezar para no pisar trabajo.
- El estilo del mantenedor: `CHANGELOG.md` (Keep-a-Changelog), commits `fix:`/`feat:` con cuerpo.
- El loader de objetos es una contribución de alto valor (enciende todo el subsistema),
  pero coordinarla por issue antes (toca el modelo de carga de sala).

---

*Generado como traspaso de la sesión de RE del subsistema de objetos de The Castle.*
