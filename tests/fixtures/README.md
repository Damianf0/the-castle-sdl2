# Fixtures del oráculo (openMSX)

Dumps byte-exactos del juego REAL corriendo en openMSX. Son la verdad de
referencia del port: cualquier decoder portado del disasm debe reproducirlos
exactamente. **Nunca** son fuente de datos del runtime — solo tests.

Generación: los scripts `tools/*.tcl` se corren dentro de openMSX (cwd = raíz
del repo). Casi todos fuerzan cada sala con un force-call a `sub_64DD`
(room loader @0x64DD) y vuelcan RAM/VRAM. `XX` en los nombres = número de sala
BCD (00..99, hi=fila, lo=columna del castillo 10x10).

| Set | Archivos | Bytes c/u | Contenido | Script |
|---|---|---|---|---|
| `colmap/` | 100 | 900 | RAM 0xE496: tilemap de colisión 20x30 (bytes 0-599) + tabla 0xE6EE celda→objeto (600-899). bit 0x80 = bloquea | `capture_colmap.tcl` |
| `e346/` | 100 | 64 | RAM 0xE346: puertas/gates, 16 slots × 4 bytes [active, val, col, row] | `cap_e346.tcl` |
| `e3d6/` | 100 | 64 | RAM 0xE3D6: coleccionables (llaves val≥0x2A, items 0x22-0x29), 16 slots × 4 | `cap_e3d6.tcl` |
| `e43e/` | 100 | 80 | RAM 0xE43E: rampas/escaleras/objetos estructurales, 16 slots × 5 | `cap_e43e.tcl` |
| `objs/` | 100 | 288 | RAM 0xE380-0xE4A0: todas las tablas de objetos (COLL 0xE386, BAT 0xE416, OBJ 0xE43E) | `force_dump.tcl` |
| `ont/` | 100 | 768 | Name table original 32×24 (sin deduplicar) por sala | `capture_nt.tcl` |
| `vram/` | 100 | 16384 | VRAM completa por sala (pattern/color/name/sprites) | `capture_vram.tcl` |
| `ram.bin` | 1 | 4096 | RAM 0xE000-0xF000 en gameplay (diagnóstico) | `dump.tcl` |
| `vram.bin` | 1 | 16384 | VRAM en gameplay sala 0x70 | `dump.tcl` |
| `vram_title.bin` | 1 | 16384 | VRAM del TÍTULO real en la entrada a `sub_4AD7` (fase "esperar input": logo en (9,6) + créditos completos). Oráculo de la Fase 1 | `cap_title.tcl` |

> Eliminados 2026-06-11: `vram_title_init.bin` y `vram_demo.bin` — NO eran
> capturas de openMSX sino dumps del propio port (los escribía
> `tiles_dump_vram()` en title.c con el modelo de video viejo). Contaminados;
> el oráculo real del título es `vram_title.bin`.
| `castle_objects.json` | 1 | — | Censo decodificado de `objs/`: 148 enemigos, 156 coleccionables, 181 estructurales | `parse_all.py` |

Verificado 2026-06-11: los generadores `tools/gen_*.py` reproducen
byte-idéntico `colmap_data.c`, `doors_data.c`, `keys_data.c`, `items_data.c` y
`map_real.c` desde estos fixtures (mientras esas tablas sigan existiendo; el
plan es reemplazarlas por decoders portados — ver `PLAN_PORT_FIEL.md`).

Trazas de comportamiento (para Fases 3-4): se generan con `tools/trace_*.tcl`
y `tools/dump_*.tcl` (salto, caminata, enemigos frame a frame).
