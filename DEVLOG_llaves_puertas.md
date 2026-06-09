# DEVLOG — Llaves y Puertas (conversión fiel desde el ROM)

> Sesión 2026-06-08. Autor: Claude (Opus) + Damián.
> Documento de traspaso para que otra persona/IA continúe **sin reconstruir**.
> Convención de confianza: ✅ validado contra ROM/emulador · 🟡 inferido del
> desensamblado · ⚠️ hipótesis / pendiente de verificar.

---

## 0. TL;DR

Las **llaves de colores** y las **puertas con candado** ahora salen de las
**tablas reales del ROM** (convertidas del código, no adivinadas):

| Subsistema | Tabla RAM | Dato generado | Estado |
|---|---|---|---|
| Llaves | `0xE3D6` | `keys_data.c` — 156 llaves / 64 salas | ✅ |
| Puertas (gates) | `0xE346` | `doors_data.c` — 259 gates / 98 salas | ✅ |
| Escaleras/rampas | `0xE43E` | (ya estaba; confirmado que NO son puertas) | ✅ |

Pipeline reproducible: **openMSX vuelca las tablas → Python genera los .c → build.ps1 compila.**

```
cap_e3d6.tcl ─→ e3d6_XX.bin ─→ gen_keys_data.py  ─→ keys_data.c
cap_e346.tcl ─→ e346_XX.bin ─→ gen_doors_data.py ─→ doors_data.c
```

---

## 1. El problema que veníamos arrastrando

Antes las llaves/puertas se **detectaban por forma** sobre la imagen del mapa y la
VRAM renderizada (component detection). Daba números inventados (233 "llaves",
57 "puertas" con colores y counts mal). El usuario lo cortó en seco:

> *"me parece que estamos solamente adivinando y la idea era convertir el código"*

→ Pivot: **convertir la lógica real del disassembly** (`the_castle_disasm.asm`)
y leer las tablas reales con openMSX como oráculo.

---

## 2. Lo que se descubrió en el código (rutas exactas)

Tres tablas en RAM, todas formato **`[active, val, col, row]`** (4 bytes/slot,
16 slots), con **posición de pantalla = `(col+1, row+4)`** — misma fórmula que el
jugador (`sub_6F4D`). `col/row` son coords de juego.

### 2.1 Llaves e ítems — tabla `0xE3D6`, dispatcher `sub_5BB0` (@0x5BB0) ✅
Loop sobre 16 slots; detecta colisión con el jugador (`sub_5D1E`) y despacha por
`(IX+1)` = `val`:

| `val` | Acción |
|---|---|
| `0x20` | especial (set bit 0xe321) |
| `0x21` | **salida "NEXT"** → `CALL sub_64DD` recarga sala (sin llave) |
| `0x22` | revela el **MAPA** (el "NO MAP" del HUD) |
| `0x23 / 0x24 / 0x25` | power-ups (escriben 0xe343 / 0xe344 / 0xeae2) |
| `0x26` | efecto (`sub_5E4B`) |
| `0x27 / 0x28 / 0x29` | **puntos** (comida/tesoro): `sub_5D87` BCD, sin contador |
| **`>= 0x2A`** | **LLAVE**: `0x5C95: SUB 0x2A; HL=0xE337+idx; INC(HL); CALL sub_5E01` |

→ **color de llave = `val − 0x2A`**. El contador por color vive en `0xE337[14]`
(`sub_5E01` = refresca el HUD de llaves).

### 2.2 Puertas con candado — tabla `0xE346`, abre `sub_758C` (@0x758C) ✅
Cómo se localiza la puerta (`sub_42FF` → `sub_4325` @0x4325):
el tile-puerta del tilemap lógico (`0xE496`) indexa un array paralelo en
**`0xE6EE`** → da un índice de objeto → `sub_5D24` calcula la dirección en
**`0xE346`** (`BC=0xe346`, stride 4).

`sub_758C` (al empujar contra la puerta con la dirección correcta):
```
A = (IX+1)            ; val de la puerta
AND 0x0F : DEC A      ; color requerido = (val & 0x0F) - 1
HL = 0xE337 + color
if (HL) == 0  -> RET  ; no tenés la llave -> no abre
DEC (HL)              ; consume 1 llave   (=> count SIEMPRE 1)
CALL sub_5E01         ; refresca HUD
(IX+0) = 0            ; marca abierta
A = (IX+1); AND 0xF0; SRL x4; ADD 0x3F  ; tile de puerta ABIERTA (redibuja)
```
→ **color requerido = `(val & 0x0F) − 1`**, **count = 1**, `val>>4` = variante.
Las gates en **col0 / col28** = **pasajes laterales con candado** entre salas.

### 2.3 Escaleras/rampas — tabla `0xE43E` (stride **5**), init `sub_4490` ✅
NO son puertas. room00 = 5 segmentos en diagonal = la escalera de tiles cyan.
(`0xE386`, stride 5 = bloques empujables/COLL; `val 0x34` especial.)

### 2.4 Espacio de color compartido (llave ⟷ puerta) ✅
Llave `val−0x2A` y puerta `(val&0xF)−1` indexan **el mismo `0xE337[color]`**:

| color | MSX | nota |
|---|---|---|
| 0 | azul (4) | 9 puertas, 6 llaves |
| 1 | (6) | **5 puertas, 0 llaves** ⚠️ (ver §6) |
| 2 | magenta (13) | |
| 3 | verde (2) | |
| 4 | **cyan (7)** | la más común junto a la 5 |
| 5 | amarillo (10) | la más común (121 puertas) |

Tabla MSX para HUD/sintético: `KEY_COLMSX = {4, 6, 13, 2, 7, 10}`.
Verificado visualmente: room00 llaves verde/magenta/amarillo; room70 gate cyan = color4.

---

## 3. Cómo se capturó (openMSX como oráculo)

Patrón común de los `cap_*.tcl` (force-load de las 100 salas):
1. Entrar a gameplay real: simular 2 toques de espacio
   (`keymatrixdown/up 8 1` en t≈6 y t≈8) hasta que dispara el loop principal `0x4070`.
2. Por cada sala (valor 0x00..0x99, sólo dígitos 0-9): escribir `0xE320 = sala`,
   empujar `0x4070` a la pila, `reg PC = 0x64DD` (force-load), y en el bp de `0x4070`
   volcar la tabla a `XX.bin` (XX = valor de sala en **hex**).

> ⚠️ **NO** pokear la posición del jugador (`0xE334/0xE335`): rompe el loop `0x4070`
> (descubierto 2 veces). Por eso el force-load por sala, no el barrido en vivo.

Correr (desde la carpeta del proyecto):
```powershell
..\_buildtools\openmsx\openmsx.exe -machine C-BIOS_MSX1 -cart the_castle.rom -script cap_e3d6.tcl
..\_buildtools\openmsx\openmsx.exe -machine C-BIOS_MSX1 -cart the_castle.rom -script cap_e346.tcl
..\_buildtools\openmsx\openmsx.exe -machine C-BIOS_MSX1 -cart the_castle.rom -script cap_e43e.tcl
```
Cada uno crea `XX_done.txt` al terminar y sale solo.

---

## 4. Calibraciones que costaron (no repetir el error)

- **Posición en pantalla:** primero calibré mal a `(col, row+3)`; lo correcto es
  **`(col+1, row+4)`** (lo confirmó room00: la llave verde caía 1 tile abajo-derecha
  del primer marcador). Es la MISMA fórmula del jugador → coherente.
- **Render de llaves = SINTÉTICO, no horneado.** El name table horneado en
  `map_real` es **inconsistente entre salas**: room00/room70 tienen la llave
  horneada, pero room17 NO (ahí hay pared donde el dato dice llave). Por eso se
  dibuja la llave con un **bitmap real 16×16 (`KEY_BMP`)** extraído de la llave
  verde de room00, recoloreado por color lógico. Las **puertas SÍ** están horneadas
  consistentes → se dejan y sólo se blanquean al abrir.
- **color4 = cyan, no poción.** Me confundió que en algunas salas el horneado
  mostraba pociones/vacío en la posición; pero el código (`val≥0x2A`) y la gate
  cyan de room70 confirman que color4 (val `0x2E`) es una llave cyan válida.

---

## 5. Archivos tocados/creados esta sesión

### Datos generados (NO editar a mano; regenerar con los .py)
- `keys_data.c`  ← `gen_keys_data.py`  (156 llaves)
- `doors_data.c` ← `gen_doors_data.py` (259 gates)

### Código de puerto
- `keys_port.c/.h` — añadido `KEY_COLMSX[6]` (colores reales) y `KEY_BMP[16]`
  (bitmap 16×16 de la llave). `keys_room_init` setea `g_key_color[]` desde
  `KEY_COLMSX`. Tamaño llave 2×2. Colección por AABB, `g_key_inv[6]`, persistencia
  `s_ktaken[100][KEY_MAX]`. (`KeySpawn.color = val−0x2A`.)
- `doors_port.c` — sin cambios de lógica (ya tenía count=1 forzado, persistencia
  `s_dopen`, consumo de `g_key_inv[color]`, `door_block` sólido/abierto). Sólo
  cambió el DATO (`doors_data.c`).
- `hal_sdl2.c` — (a) blanquea SIEMPRE el horneado de las celdas de llave (antes
  sólo las recogidas); (b) dibuja la **llave sintética** `KEY_BMP` recoloreada por
  `KEY_COLMSX` en la posición de cada llave activa (bucle nuevo al inicio del bloque
  `if (g_actors_on)` en `debug_draw_geom`). HUD de llaves usa `g_key_color[]`.

### Herramientas nuevas
- `cap_e346.tcl`, `cap_e43e.tcl` (capturadores; `cap_e3d6.tcl` ya existía).
- `gen_keys_data.py`, `gen_doors_data.py` (generadores reproducibles).

### Memoria persistente actualizada
- `…/memory/thecastle-objects-census.md` — entrada **"★ LLAVES Y PUERTAS — MODELO
  FIEL DEFINITIVO"** (tablas 0xE3D6/0xE346/0xE43E + rutas del código). El enfoque
  viejo por detección de forma quedó marcado **[SUPERADO]**.

---

## 6. Pendiente / riesgos conocidos

- ✅ **Apertura probada en vivo (2026-06-09, harness):** abre con la llave y consume 1
  (inv 9→8), bloquea sin llave, persiste al volver, se blanquea al abrir.
- ✅ **Puertas GEMELAS (2026-06-09):** las gates de borde son el mismo pasaje en las
  dos salas contiguas → al abrir una se abre la gemela (`open_twin()` en doors_port:
  lateral col0/col28 matchea por drow±1; vertical row tope/base por dcol±1; el grid
  envuelve). Verificado room70 (29,15) → room71 (1,15) OPEN.
- ✅ **ÍTEMS 0xE3D6 (2026-06-09):** vals 0x22-0x29 ahora son recogibles (tabla
  `items_data.c` ×256, `items_port.c`): se recogen al tocar, desaparecen, persisten,
  y NUNCA son sólidos (antes eran obstáculos horneados sólidos que había que saltar
  — el "mapa de colisiones mal" reportado). TODO fiel: puntos (0x27-29, sub_5D87),
  revelar mapa (0x22), efectos power-ups (0x23-26).
- ⚠️ **color1: 5 puertas, 0 llaves.** Hay 5 gates que piden color1 pero no existe
  ninguna llave color1 en `0xE3D6`. Puede ser: (a) puertas dead-end intencionales,
  (b) ruido de slot, o (c) decode de `val` levemente off para esos casos. Revisar
  qué salas/valores son (filtrar `(val&0xF)==2` en los `e346_*.bin`).
- ⚠️ **Salidas `0x21` (NEXT) vs gates `0xE346`.** Las salidas auto-recarga (sin
  llave) están en `0xE3D6`; las gates con candado en `0xE346`. Verificar que la
  transición de sala (room_transition) y las gates de borde no se pisen.
- **Bloques empujables** no persisten su posición al reentrar a la sala (vuelven al
  spawn). Enemigos (path-replay) atraviesan bloques movidos. (Heredado.)
- **Tile de puerta ABIERTA:** el ROM redibuja `(val>>4)+0x3F`; el port sólo blanquea
  (queda hueco transitable). Si se quiere fiel, dibujar ese tile.
- **Harness:** `CASTLE_MOVES` (guion R/L/U/D/A/. por frame) y `CASTLE_DOORROOM=XX`
  (a f25 carga las puertas de otra sala para inspeccionar gemelas).

---

## 7. Cómo reconstruir todo desde cero

```powershell
cd C:\Users\Soporte\Desktop\TheCastle_con_loader
# 1) (si faltan los .bin) capturar tablas reales
..\_buildtools\openmsx\openmsx.exe -machine C-BIOS_MSX1 -cart the_castle.rom -script cap_e3d6.tcl
..\_buildtools\openmsx\openmsx.exe -machine C-BIOS_MSX1 -cart the_castle.rom -script cap_e346.tcl
# 2) generar los .c
python gen_keys_data.py
python gen_doors_data.py
# 3) compilar
.\build.ps1
# 4) probar (sala inicial 0x70)
.\the_castle.exe
# screenshot headless: $env:CASTLE_SHOT="x.bmp"; $env:CASTLE_ROOM="70"; $env:CASTLE_ACTORS="1"; $env:CASTLE_GEOMDBG="1"; .\the_castle.exe
```
