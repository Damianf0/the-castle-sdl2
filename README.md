# The Castle (ASCII, 1986) — Port fiel a Windows

Port nativo a Windows (C + SDL2) del juego de MSX **The Castle** (ASCII
Corporation, 1986), reconstruido **función por función desde el desensamblado
de la ROM original**. No es una recreación "de oído" ni una maqueta: cada
sistema del motor está portado del código Z80 real y **validado frame a frame
(y registro a registro, en el caso de la música) contra el emulador openMSX**.

---

## Crédito y herencia del proyecto

Este proyecto es **heredado del trabajo que hizo Víctor primero**. La base
inicial —el andamiaje del port, la primera aproximación al motor y el arranque
de la reconstrucción— es obra suya. Lo que sigue (la reorientación a *port
fiel* desde el disasm y todo lo documentado abajo) se construyó **sobre esa
base**. El reconocimiento a Víctor por sentar los cimientos es parte del
proyecto.

---

## De dónde venimos

El proyecto arrancó como una **maqueta**: snapshots de pantalla "horneados"
como datos, física del jugador aproximada y enemigos por *path-replay*
(repetían trayectorias grabadas en vez de pensar). Funcionaba para mirar, pero
no *era* el juego: cualquier interacción nueva se salía del molde.

En **junio de 2026** se tomó la decisión de reorientar a un **port fiel
(opción B)**: portar el motor real función por función desde
`the_castle_disasm.asm`, y usar las capturas de openMSX **solo como oráculo de
tests**, nunca como datos del runtime. La regla de oro: *ninguna heurística
nueva — lo que no se entiende, se traza en openMSX y se porta del disasm.*

---

## Qué hicimos

El motor completo del juego está portado y validado. Resumen por sistema:

- **Video (VDP TMS9918A → SDL2)**: render SCREEN 2 fiel; la intro y los tiles
  por tercios de pantalla salen byte-idénticos a la pantalla real.
- **Room loader (`sub_64DD`)**: las 100 salas se decodifican desde la ROM
  (colmap, tablas de objetos, name table, VRAM completa). Sin tablas
  horneadas — todo sale del ROM.
- **Jugador (`sub_40BB`/`6F5C`)**: movimiento, salto, gravedad, colisión y
  transiciones de sala, validado contra trazas frame-perfect.
- **Bloques empujables, ascensores, cintas, fuego, trampas y pistones**
  (motores COLL y `sub_442D`/`4406`): física real con gravedad, empuje,
  aplastamiento y la mecánica oculta del pistón disparado por la trampa.
- **Enemigos (`sub_438D`)**: cerebros por tipo + movedor con animación;
  validado 10/10 salas frame-exactas (murió el path-replay de la maqueta).
- **Daño, muerte, respawn y GAME OVER** (`sub_5A2D`/`5A63`/`4F16`).
- **Pickup por celda con efectos reales** (`sub_5B96`/`5BB0`): llaves, comida,
  tesoros, score BCD, vidas extra, power-ups y el **minimapa** del HUD.
- **Demo mode real**: la demo del título es una partida jugada por el motor
  con el input grabado en la ROM — reproduce **1288 frames exactos** y sirve
  de prueba integral de todo el motor a la vez.
- **Velocidad fiel**: el juego tiene dos velocidades (normal y, manteniendo
  **CTRL**, "correr"), paceadas como en el MSX real (medido en openMSX).
- **Música (PSG AY-3-8910)**: reproductor portado del ISR real y **validado
  registro a registro** — la secuencia de notas de melodía y bajo coincide
  100% con el original (114/114 + 126/126 notas).

Todo esto se verifica con una **suite de 17 tests** (`tests/run_tests.py`) que
compara el port contra openMSX byte/frame/registro a registro.

> Cola pendiente (cosmética, documentada en `PLAN_PORT_FIEL.md`): las dos
> cinemáticas raras de fin de juego (ítem de victoria y el ítem especial de
> teletransporte), el *shimmer* de color del power-up rojo, y el detalle fino
> de los volúmenes de los efectos de sonido.

---

## Cómo jugar

1. Asegurate de tener los tres archivos juntos en la misma carpeta:
   `the_castle.exe`, `SDL2.dll` y `the_castle.rom`.
2. Doble clic en `the_castle.exe` (o corrélo desde una consola).

### Controles del juego

| Tecla | Acción |
|-------|--------|
| Flechas / WASD | Mover al jugador |
| Z / Espacio | Saltar / acción |
| **CTRL** (mantener) | **Correr** (velocidad rápida) |
| **CTRL + ALT** | Turbo |
| Esc | Salir |

### Teclas de función nuevas

Estas teclas **no existen en el MSX original** — se agregaron para QA y
comodidad. Los *cheats* solo funcionan dentro del juego; el control de
volumen funciona en todos lados (título, demo y juego).

| Tecla | Función |
|-------|---------|
| **F5** | God mode (toggle): invulnerable + vidas al máximo (nunca game over; si caés a un pozo, reaparecés) |
| **F6** | Dar 9 llaves de cada color (abre cualquier puerta) |
| **F7** | Dar el mapa (dibuja el minimapa completo) |
| **F8 / F9** | Saltar a la sala anterior / siguiente (recorre las 100 salas, ignorando paredes y conexiones) |
| **F10** | Mute (silenciar / reactivar el sonido) |
| **F11 / F12** | Bajar / subir el volumen (8 niveles) |

> Las teclas F1–F4 quedan reservadas para las teclas de sistema reales del
> MSX (reinicio de sala, pausa, etc.), aún no portadas.

---

## Recompilar desde el código fuente

El código completo está en `fuente/`. Build canónico en Windows (MinGW-w64 +
SDL2):

```
powershell -ExecutionPolicy Bypass -File build.ps1
```

`build.ps1` compila todos los `.c` y deja `the_castle.exe`. Requiere el
toolchain MinGW-w64 y los headers/libs de SDL2 (las rutas están al principio
de `build.ps1`). El resultado es un único ejecutable + `SDL2.dll`.

---

## Contenido del paquete

```
the_castle.exe        El juego (ejecutable nativo de Windows)
SDL2.dll              Runtime de SDL2 (requerido por el exe)
the_castle.rom        ROM original de The Castle (MSX, 32 KB) — necesaria
README.md             Este archivo
CHANGELOG.md          Historial detallado de todo lo portado, paso a paso
PLAN_PORT_FIEL.md     El plan de reconstrucción + mapa de RE de cada sistema
fuente/               Código fuente C completo + build.ps1 para recompilar
```

> **Nota sobre la ROM**: `the_castle.rom` es el juego original de ASCII
> Corporation (1986) y es necesaria para ejecutar el port (el motor decodifica
> todo desde ahí). Su distribución corresponde a quien tenga los derechos del
> juego original.

---

*Port fiel reconstruido desde el desensamblado, validado contra openMSX.
Heredado del trabajo inicial de Víctor.*
