# ASSETS — Llaves y Puertas (inventario de la sesión 2026-06-08)

Todo lo que se usó/generó para la conversión fiel de llaves y puertas. Ver
`DEVLOG_llaves_puertas.md` para el porqué y el cómo.

---

## 1. Fuentes (entrada)

| Asset | Ruta | Qué es | Notas |
|---|---|---|---|
| ROM original | `the_castle.rom` (= `The_Castle_-_ASCII__1986___GoodMSX___356_.rom`) | MSX1 32 KB, ORG 0x4000 | **Copyright — NO commitear** (`.gitignore *.rom`). Requerido en runtime. |
| Disassembly Z80 | `the_castle_disasm.asm` | 11.503 líneas, anotado | Fuente de verdad para "convertir el código". |
| Emulador oráculo | `..\_buildtools\openmsx\openmsx.exe` | openMSX headless | `-machine C-BIOS_MSX1 -cart the_castle.rom -script X.tcl` |
| Toolchain build | `..\_buildtools\mingw`, `..\_buildtools\sdl2` | MinGW gcc + SDL2 portable | via `build.ps1` |
| Python + PIL | `C:\Program Files\Python314` | scripts/render de validación | |

### Direcciones RAM clave (del ROM)
```
0xE320  sala actual                 0xE334/35  col/row del jugador (NO pokear)
0xE337  contador de llaves [14]     0xE346  PUERTAS/gates (16x4)
0xE3D6  LLAVES/items/salidas (16x4) 0xE43E  escaleras/rampas (16x5)
0xE386  bloques empujables/COLL (16x5)   0xE416  enemigos/BAT (8x5)
0xE496  tilemap lógico colisión (stride 30, sólido si &0x30)
0xE6EE  cell->indice de objeto (para localizar gate)
```
### Rutas de código clave
```
0x4070  loop principal (dispara en gameplay real)   0x64DD  force-load de sala
0x5BB0  sub_5BB0  dispatcher de 0xE3D6 (recoger llave/item)
0x758C  sub_758C  abre gate (consume llave)          0x4325  localiza gate
0x5E01  refresca HUD de llaves    0x5D87  suma score BCD    0x6F4D  pos->pantalla
```

---

## 2. Volcados del ROM (.bin) — generados con openMSX

| Asset | Archivos | Tamaño c/u | Tabla | Generado por |
|---|---|---|---|---|
| `e3d6_XX.bin` | 100 | 64 B (16×4) | **Llaves/ítems 0xE3D6** | `cap_e3d6.tcl` |
| `e346_XX.bin` | 100 | 64 B (16×4) | **Puertas 0xE346** | `cap_e346.tcl` |
| `e43e_XX.bin` | 100 | 80 B (16×5) | Escaleras 0xE43E | `cap_e43e.tcl` |
| `ont_XX.bin` | 100 | 768 B | Name table original (VRAM 0x1800) | `capture_nt.tcl` |
| `colmap_XX.bin` | 100 | 900 B | Tilemap colisión 0xE496 | `capture_colmap.tcl` |

> `XX` = valor de sala en **hex** (00..99, sólo dígitos 0-9). Son la "fuente de
> verdad" intermedia: si se borran, re-correr los `cap_*.tcl`.

---

## 3. Capturadores TCL (openMSX)

| Asset | Salida | Estado |
|---|---|---|
| `cap_e3d6.tcl` | `e3d6_XX.bin` | ✅ (ya existía) |
| `cap_e346.tcl` | `e346_XX.bin` | ✅ nuevo esta sesión |
| `cap_e43e.tcl` | `e43e_XX.bin` | ✅ nuevo esta sesión |
| `capture_nt.tcl` | `ont_XX.bin` | ✅ |
| `capture_colmap.tcl` | `colmap_XX.bin` | ✅ |
| `trace_keydoor.tcl` | `keydoor_log.txt` | trazado en vivo (bp 0x5CA3/0x75A0) |

Patrón: force-load sala por sala vía `0x64DD`, bp en `0x4070`, volcar; crean
`*_done.txt` al terminar. **No pokean al jugador** (rompe el loop).

---

## 4. Generadores (Python → C)

| Asset | Entrada | Salida | Resultado |
|---|---|---|---|
| `gen_keys_data.py` | `e3d6_*.bin` | `keys_data.c` | 156 llaves / 64 salas |
| `gen_doors_data.py` | `e346_*.bin` | `doors_data.c` | 259 gates / 98 salas |

Reglas embebidas: llave `val≥0x2A` color=`val−0x2A`; puerta color=`(val&0xF)−1`
count=1; posición `(col+1,row+4)`; llave 2×2, puerta 2×3.

---

## 5. Datos y gráficos generados

| Asset | Qué es | Origen |
|---|---|---|
| `keys_data.c` | `KEY_COUNT[100]` + `KEY_DATA[100][10]` | `gen_keys_data.py` |
| `doors_data.c` | `DOOR_COUNT[100]` + `DOOR_DATA[100][6]` | `gen_doors_data.py` |
| `KEY_BMP[16]` (en `keys_port.c`) | bitmap 16×16 de la llave | extraído de la llave verde de room00 (VRAM) |
| `KEY_COLMSX[6]` (en `keys_port.c`) | `{4,6,13,2,7,10}` colores MSX por color lógico | muestreo VRAM + verificación visual |
| `keymask.json` | máscara 16×16 de la llave (intermedio) | descartable, sólo para extraer `KEY_BMP` |

---

## 6. Código fuente del puerto (modificado)

| Asset | Cambio |
|---|---|
| `keys_port.c/.h` | `KEY_COLMSX`, `KEY_BMP`, colores reales, llave 2×2 |
| `hal_sdl2.c` | blanqueo total del horneado de llaves + dibujo sintético `KEY_BMP` recoloreado |
| `doors_port.c` | sin cambios de lógica (sólo cambió el dato `doors_data.c`) |
| `build.ps1` | sin cambios (auto-globa `*.c`) |

---

## 7. Salida / distribución

- `the_castle.exe` — binario (322 KB). Necesita `SDL2.dll` + `the_castle.rom`.
- Distribuible para colega: `..\TheCastle_Prueba\` (+ `TheCastle_Prueba.zip`) —
  **regenerar** tras estos cambios (todavía tiene la versión vieja de llaves/puertas).

---

## 8. Archivos efímeros (se pueden borrar)

`*_done.txt` (flags de fin de captura), `keymask.json` (intermedio), cualquier
`*.bmp`/`*.png` de validación visual (ya limpiados). Los `.bin` NO son efímeros
(regenerarlos cuesta una corrida de openMSX por tabla).
