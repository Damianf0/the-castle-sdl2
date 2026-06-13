# Changelog

## 2026-06-12 — Fase 5: MINIMAPA del HUD completo

- **Pintor (sub_640F) decodificado y portado**: cada sala del castillo
  (10×10) es un bloque de 4×3 píxeles del minimapa; el "pixel" son 2
  scanlines de la COLOR table (no patterns): la sala de columna PAR vive
  en el nibble FG y la IMPAR en el BG del mismo byte (los chars del
  charset tienen la mitad izquierda en FG). Colores: 4=no visitada,
  7=visitada, 0x0F=sala actual, 9=sala 09 (con E321 bit1).
- **Pickup del mapa (0x22)**: dibuja el canvas de chars (sub_64C3), el
  marco de 6 sprites (sub_638E) y pinta las 100 salas según el bitfield
  de visitadas de 0xE000 + el cursor blanco.
- **Al salir de cada sala con el mapa** (cabeza de sub_5053, hook en
  rl_room_exit): marca el bit de visitada (sub_61E8) y la pinta cyan.
- Validación byte-exacta de la VRAM del HUD (name + colores) contra
  openMSX al final de cada escenario de pickup — la suite pickup pasa de
  3 a 5 escenarios (70map y 70out). El harness carga el charset fijo con
  rl_boot_vram (los colores base del HUD venían de ahí). 14/14 suites.

## 2026-06-12 — Fase 5: PICKUP por celda con efectos reales — muere la maqueta AABB

- **pickup.c nuevo** (sub_5B96 + dispatcher sub_5BB0): pickup por celda
  exacta (colmap bit 0x04 + igualdad con el top-left del jugador, solo
  frames pares) con TODOS los efectos no-cinemáticos: power-ups rojo/verde
  con su música (el rojo no se consume de la sala), reset de puertas
  (0x25), vida extra (0x26), mapa (flag), y el genérico 0x27+: sprite de
  puntos con timer (plano 13, EAF9), score BCD de 6 dígitos con DAA
  (carry del medio = vida extra), hi-score, llaves de color con HUD —
  más los redibujos reales del HUD (dígitos tile 0x47+d, vidas tile 0x0D).
- **Animador de e3d6** (sub_4499/74E9): la llave dorada 0x21 parpadea con
  sprite (plano 11) + tiles alternantes, y se esconde al agarrarla.
- **Maqueta muerta**: keys_port.c/h, items_port.c/h y doors_port.c/h
  BORRADOS; faithful_play corre 100% motor real (quedan actors/camera/
  room/enemies/doors legacy SOLO para el demo del título).
- **Suite `pickup`**: oráculo openMSX con el jugador caminando (spawn vía
  E322/23 + hold; el harness CASTLE_PICKTRACE replica transiciones de
  sala): salas 04 (4 comidas + score), 00 (2 llaves), 02 (vida extra) —
  frame-exactas 301/301. 14/14 suites.
- Hallazgo documentado: caer a un POZO en el juego real termina la
  partida (vuelve a título/demo) — pendiente con el flujo de título.
- Pendientes anotados: secuencias de 0x20/0x21 (la tabla 0x5748 de las
  notas viejas era de sub_518E), minimapa (64C3/638E/640F), color-cycling
  del 0x23.

## 2026-06-12 — Fase 4 motor COMPLETO: pistón 0x1B + partículas reales

- **Pistón vertical 0x1B** (6 salas): nace inerte; se dispara EMPUJANDO la
  trampa COLL 0x34 (empuje izquierda = sube, derecha = baja) mientras ella
  se desliza bajo su techo-marcador (rampa falsa con E6EE bit4). Sube
  cargando al jugador/bloques apoyados en su pie y baja esperando a los
  objetos que aún no caen. Cadena completa sub_61F5→735F→474E portada.
- **Partículas**: el puff de desintegración real (sub_5D63: SFX + 3 frames
  con los patrones 0x2C-0x2E en el plano de sprite 12, bloqueante como el
  original) reemplaza al stub en s_5D47 y en las muertes del movedor BAT;
  sub_6EE1 portado como helper genérico de planos de sprite con tintes.
- **player_tail_frame (sub_623C)**: timer del sprite de partícula (0xEAF9,
  plano 13) y timers de POWER-UP E343/E344 cada 16 frames — con la música
  de aviso (transpose+5, speed 0xFF) y la restauración del tema al expirar.
- Suite e43e ampliada a 10 salas: sala 01 valida el pistón con un
  escenario de EMPUJE real (oráculo openMSX con el jugador libre
  caminando con hold RIGHT — frame-exacto 301/301). Lección del oráculo:
  el force-load pisa E334/E335 (el spawn correcto va por E322/E323).
- 13/13 suites verdes.

## 2026-06-12 — Fase 4: estructurales e43e COMPLETOS (cintas, fuego, trampas 0x1F)

- **442D terminado**: cintas transportadoras 0x0C/0x0D (tira animada de
  2×len celdas con deltas de la tabla ROM 0x77F2, fase EAC9&3 — la 0x0D
  va al revés) y fuego intermitente 0x0F: 32 frames de animación (tabla
  0x7802) y 32 frames letales (EAC9 bit5: colmap=0x10 sin redibujar).
- **4406 portado**: trampas 0x1F — pinchos deslizantes de 2 celdas que
  patrullan horizontal (1 celda cada 2 frames), rebotan contra sólidos
  y se DESACTIVAN si quedan encerradas. Cerebro en frames pares, drawer
  cada frame (transición deltas 2-4 / final 0-1 + blanqueo).
- **Suite nueva `estructurales e43e`**: CASTLE_E43TRACE (port) contra
  tools/tr_e43e.tcl + gen_e43e_fixtures.py (openMSX): los 16 slots e43e
  (tipo,col,fila,f3,estado) frame a frame — 8 salas (00/02/03/06/16/33/
  36/42) × 301 frames EXACTOS al primer intento. 13/13 suites.
- Aclarado: "escaleras" de las notas viejas no existe como mecánica (el
  jugador no lee arriba/abajo del stick); era el bloque 0x4586 (ya en
  los BATs). Queda de la fase: tipo 0x1B (móvil vertical inerte que
  dispara la trampa COLL 0x34 vía sub_61F5/735F/474E) y las partículas.

## 2026-06-12 — Sonido (2): envelope real del canal A — primer oráculo PSG

Reportado por el usuario: el sonido salía "lento/distorsionado". Primer
fixture PSG real (tools/tr_psg.tcl: registra TODAS las escrituras a los
puertos 0xA0/0xA1 en openMSX durante el juego; tools/tr_psgregs.tcl: lee
el estado de los 16 registros en vivo):

- **Oráculo**: por nota el canal A escribe solo R0/R1 + R8=0x10 + R13=0x00;
  el canal B R2/R3 + R9=0x0D. **R11/R12 no se escriben NUNCA** (quedan los
  del BIOS: 0x1C00 → paso de envelope de ~1 s: con shape 0 la nota suena a
  volumen lleno constante y el R13 de cada nota re-dispara la rampa).
  Tick musical: cada 6 VBlanks (EAF3=6 ✓), nota típica 200 ms. R7=0xB8.
  (Las 25k escrituras a R15 son el escaneo de joystick del busy-wait.)
- **Distorsión cazada**: el port inventaba R11/R12=período×2 + shape 0x08
  (sawtooth repetido) → trémolo audible de ~13 Hz en la melodía. Ahora
  play_note replica el oráculo y music_init deja R11/R12 = 0x00/0x1C.
- **Bugs del envelope en el HAL**: corría 16× rápido (un paso del AY son
  256×EP ciclos y la rampa 16 pasos = 4096×EP) y los shapes sin bit de
  continue (0-7) repetían en vez de terminar en 0 y quedarse.
- El tempo medido confirma el modelo del tick (6/4/2 según velocidad).

## 2026-06-12 — Sonido: música de juego CORRECTA, SFX conectados, jingle de muerte

Diagnóstico (el reproductor nunca se validó contra el real — eso sigue
siendo la Fase 6):
- La "música de juego" del port cargaba 0x7A73/0x7A8F, que el RE de la
  muerte (Fase 4) probó que es el JINGLE DE MUERTE: sonaba eso en loop.
- El motor de SFX (sub_76BE) leía variables estáticas mientras los motores
  del juego escriben la RAM espejo (0xEAF6 puff de aplastes): mudos.
- El tempo/transpose por velocidad (62FA → EAF1/EAF3, ayer) tampoco
  llegaba al reproductor.

Arreglos (todos respaldados por disasm):
- **El estado del reproductor ahora vive en la RAM espejo** (0xEAF1-EAF8,
  como el ISR real): SFX, tempo dinámico y mute por CAPS conectados solos.
- **Música in-game real** (0x656B-659A, cola del room loader): el tema
  normal son los MISMOS streams del título (0x78D2/0x7916); con power-up
  rojo (E343) suena 0x79B7/0x79DE y con el verde (E344) 0x7964/0x7993.
  `music_room_start()` se llama en cada carga de sala.
- **Muerte fiel** (sub_5B35/5B56): silencio + 3 frames + jingle
  0x7A73/0x7A8F a tempo 6; la secuencia entera ahora se pacea a velocidad
  real (player_frame_ms) en vez de 60fps.

## 2026-06-12 — Fase 4: motor de ASCENSORES (e43e 0x1C/0x1D) — 10/10 salas BAT

- **sub_442D portado** (player_elev_frame en player.c, primero en el game
  loop como el original): plataformas móviles de 2 (0x1C) y 4 (0x1D)
  celdas que suben/bajan cada 8 frames escribiendo su piso en el colmap
  (colmap por delta del ROM vía 5E80: superficie=piso 0x40, cadenas no).
- **Cerebro sub_4611**: rebote por probes asimétricos (bajar lo frena el
  piso 0x40; subir lo frena el sólido 0x30 — un objeto bit3 no bloquea:
  se empuja). Bajando aplasta objetos (sub_5D47); subiendo CARGA al
  jugador (sub_4701: fila--, sync de sprite) y empuja pilas de bloques
  recursivamente (sub_4744/7575), aplastándolas contra el techo.
- Validación: salas 29 y 81 frame-exactas 301/301 contra los fixtures
  openMSX existentes → BATS_PENDING vacío, **10/10 salas BAT exactas**.
  (La traza valida el ascensor indirectamente: las patrullas 0x38/0x39
  solo se extienden si el piso móvil aparece en el frame justo.)
- Pendiente de 442D: tipos 0x0C/0x0D/0x0F (sub_72CA/72DD/7326) y el 0x1F
  de sub_4406 (sub_47B8/7494).
- Infra: `_buildtools` había sido borrado del disco — repuesto con
  winlibs gcc 16.1, SDL2-devel 2.32.10 y openMSX 21.0 (mismo layout).

## 2026-06-12 — Velocidad real del juego: lenta + CTRL=correr (0x62FA)

Reportado por el usuario: el juego real tiene una velocidad general más
lenta y otra rápida manteniendo una tecla; el port corría siempre rápido.

- **RE**: 0xEAD3/D4 no eran "config de dificultad" (nota vieja del plan,
  falsa): son las filas 6/7 de la matriz de teclado acumuladas por AND de
  SNSMAT en el busy-wait de sub_5128 (bloque 0xEACD..0xEAD5, activo-bajo).
  La rama de juego de 0x62D8 (62FA) decide por fila 6: CTRL suelto →
  EACA=0x70; CTRL → 0x30; CTRL+GRAPH → 0x01; y setea además el transpose
  (0xEAF1=0/7/0x0C) y el tempo de la música (0xEAF3=6/4/2, CAPS = mute).
  El paceo real es CPU-bound: sub_5128 quema EACA llamadas a sub_50E8
  (GTTRIG/GTSTCK/SNSMAT) — no hay espera de vsync en el loop de juego.
- **Medición** (tools/tr_speed.tcl, openMSX, sala 01, ajuste lineal):
  ms/iteración = 54.0 + 1.381×EACA → normal 208.8 ms (~4.8 it/s),
  CTRL 119.9 ms (~8.3 it/s), CTRL+GRAPH 55.6 ms (~18 it/s).
- **Port**: `player_speed_frame()` (62FA fiel, player.c) +
  `hal_wait_game_frame(ms)` (hal_sdl2.c: convierte ms a VBlanks con resto
  fraccional — la música sigue tickeando a 60 Hz como el ISR real) +
  `hal_msx_keyrow6()` (host CTRL→CTRL, ALT→GRAPH). El host LCTRL deja de
  ser fire (queda Z/Space): ahora es la tecla de CORRER, como en el MSX.
- Pendiente de la misma rutina: F1=reiniciar sala, F2=vidas:=1, F4, F5=pausa.

## 2026-06-12 — Fase 4: motor BAT real (enemigos vivos) + suite de fixtures

### sub_438D/43BF/719D + cerebros portados (player.c)
Los enemigos (tabla BAT 0xE416, 8 slots) corren con el motor real:
- **Driver** (sub_438D): pase 1 = cerebro por slot (solo frames PARES);
  pase 2 = cerebro de nuevo si field4&5 y MOVEDOR siempre.
- **Cerebros por tipo** (sub_43BF): 0x36 trampa-murciélago (sub_7279, ciclo
  de 64 frames: colgar/aletear/volar/recolgarse), 0x37 volador perseguidor
  4-direcciones (sub_4901), 0x38 caminador-perseguidor que TREPA en la
  columna del jugador (sub_48AD), 0x39 patrulla con gravedad (sub_4882),
  0x3A/0x3B rebote simple (sub_4872/487A → sub_45AD).
- **Movedor** (sub_719D): 2 fases como los bloques — frame impar dibujo
  final + blanqueo del rastro por dirección; frame par celda nueva +
  transición 3x2/2x3. Caída/subida bloqueada → el BAT MUERE (slot=0,
  blanqueo 2x2, puff 0xEAF6=0x32) — los kamikazes son fieles.
- **Daño y muerte** (sub_5A2D/5AF8/5A63): contacto en frames impares por
  las 4 celdas del cuerpo (bit 0x10 del colmap; saltar esquiva los de
  abajo, 0xE344 los de arriba, 0xE343 = invulnerable), secuencia de muerte
  bloqueante (agonía + caída + vidas-- + commit de persistencia + respawn).
- `player_end_frame()` separado: EAC9++ va al FINAL del frame completo
  (40AF) — bloques/jugador/bats comparten la paridad del mismo frame.

### Bug real encontrado contra el oráculo
En sub_4901 (volador): con "abajo bloqueado, arriba libre, jugador dentro
de ±8", el original solo SUBE si el jugador está ARRIBA (49A2: JP Z/JP C →
horizontal); el port subía con el jugador abajo. Con el fix, la sala 0x98
(4 voladores apilándose contra el jugador) es frame-exacta 301/301 vs
openMSX.

### Validación: suite `bats` nueva (fixtures frescos de openMSX)
- `tools/tr_bats.tcl` + `tools/gen_bats_fixtures.py`: warp a la sala con
  el jugador FIJADO en una celda libre (invulnerable) y traza de los 8
  slots por frame → `tests/fixtures/bats/bats_XX.txt` (10 salas, todos los
  tipos). El harness corre CASTLE_BATTRACE (+CASTLE_PCOL/PROW) y exige
  igualdad frame a frame: **8/10 salas exactas 301/301**; 29 y 81 quedan
  pendientes del motor de ASCENSORES (e43e 0x1D escribe piso en el colmap
  y extiende las patrullas — sub_4406/442D, Fase 4 restante).
- Sala 0x49 probada en openMSX: la trampa colgante 0x35 CAE al cargar la
  sala y aterriza en (20,14) — el BAT rebota contra ella en col 18, igual
  que el port. El path legacy (rebote en 22) era un artefacto de captura.
- Retirado el path-replay legacy: enemies_paths.c/h y enemies_port.c/h
  borrados (+draw_enemies/blit_cell16 en hal, AABB viejo en actors.c);
  faithful_play usa player_bats_frame desde el switchover. Exe 188→174 KB.

Harness: 13/13 suites (loader 700/700, vdp, jugador 6/6, bats 8/8+2
pendientes, título, smoke).

## 2026-06-11 (6) — Motor COLL real: bloques empujables con gravedad

### sub_434A + sub_4820 + sub_710B + sub_4273 portados (player.c)
Los bloques (ollas/ladrillos, tabla 0xE386) ahora son objetos VIVOS del
motor real:
- **Empuje** (sub_4273, integrado en sub_4248/425A tras el chequeo de
  puerta): valida el destino (sub_49DC — un BAT en el destino cuenta como
  libre y es APLASTADO via sub_42F5/sub_5D47), marca field4=3/1 (formato de
  flags de salida) y dispara el primer medio paso.
- **Movedor en 2 fases** (sub_710B): frame PAR actualiza la celda lógica y
  dibuja el gráfico de TRANSICIÓN (3x2 deltas 4-9 horizontal / 2x3 deltas
  10-15 vertical — ¡para eso eran los 16 tiles que el alocador carga por
  bloque!); frame IMPAR dibuja el 2x2 final y blanquea la columna/fila que
  quedó atrás. CLAVE del RE: los escritores Z80 (70B6/70C2/70D5/7103) MUTAN
  los registros H,L,D del caller — los blanqueos solo cierran modelando ese
  flujo de registros.
- **Gravedad y derrape** (sub_4820, por frame vía sub_434A): sin piso 2
  celdas abajo → cae (aplastando BATs); con piso → deriva por pendiente
  (sub_47F5) limitado por paredes (sub_44C2). El colmap se actualiza solo
  (cada celda escrita pasa por sub_5E80) → la colisión del jugador sigue
  al bloque automáticamente.
- Trampas 0x34: estructura del 2º pase de sub_434A lista (partículas
  sub_61F5 — Fase 4).

Verificado: en la sala 0x71 el jugador empuja el ladrillo ~7 columnas por
el corredor con transiciones visuales correctas. Trazas del jugador siguen
6/6 (player_coll_frame en el loop no perturba), harness 11/11.

## 2026-06-11 (5) — Fase 3: SWITCHOVER — el gameplay corre con el jugador REAL

### faithful_play ahora es el motor fiel
- Jugador: `player.c` (sub_40BB+sub_6F5C, validado 6/6 trazas). Colisión por
  el colmap del loader; SPRITE por el VDP nativo (planos 8-10, patrones ROM
  0x9B96→VRAM 0x3800, colores de la tabla (0x7CF0) con tintes de sala
  0xE343/E344). Murieron: la física de actors.c en el loop, draw_actors,
  blit_player_frame y player_sprite.c (el VDP usa los patrones reales).
- Transición de sala REAL: `rl_room_exit` = sub_5053 (sala BCD con DAA,
  entrada por el borde opuesto conservando la perpendicular — lo que la
  maqueta hacía a mano ahora es gratis) + sub_6134 (committea las 4 tablas
  de objetos a los bitfields de persistencia).
- Puertas 100% reales: presionar → rl_door_press (sub_4325/758C) consume la
  llave de la RAM 0xE337+color, abre, redibuja y limpia colisión. La
  persistencia (y las puertas GEMELAS) salen GRATIS del ciclo bitfield:
  abrir → slot[0]=0 → sub_6134 al salir → el loader no la dibuja al volver.
  doors_port (persistencia por posición + open_twin a mano) fuera del loop.
- Pickup integrado al ciclo real: recoger llave = 0xE337+color++ +
  slot[0]=0 + HUD por sub_5E01; ítems = slot[0]=0. Murieron los arrays
  s_ktaken/s_itaken (la persistencia es la del ROM). El AABB del pickup
  sigue siendo maqueta (el real es por celda vía sub_5B96 — Fase 5).
- Verificado end-to-end: caminar → agarrar llave → abrir puerta → cruzarla →
  bloquearse en la siguiente puerta cerrada → con llaves, salir por la
  derecha → "transicion a sala 0x71" (BCD) con entrada a la misma altura.
- Pendiente (documentado): daño/muerte (sub_4406), empuje de bloques
  (sub_4273), escaleras (0x4586+), efectos de ítems (5B96/5C3A) — Fases 4-5.

Harness 11/11. CASTLE_ACTORS reescrito sobre el motor fiel (CASTLE_MOVES con
stick/trigger reales, CASTLE_GIVEKEYS escribe la RAM 0xE337).

## 2026-06-11 (4) — Fase 3 (núcleo): jugador portado, 6/6 trazas frame-perfect

### player.c = sub_40BB + sub_6F5C fieles
El jugador portado reproduce al juego real FRAME A FRAME en las 6 trazas-
oráculo (456 frames: idle, caminata, salto corto/sostenido/en carrera, pared
y choque contra puerta cerrada): posición de sprite, fase de salto (0xEAD6),
patrón de animación e input muestreado, byte-idénticos. Suite `jugador` en
el runner (11/11).

- `sub_40BB`: flags de movimiento en HL — probes de pares de celdas sobre el
  colmap del room loader (sólido 0x80, pasable-especial 0x0A, rampa 0x01 con
  pendiente en 0xE6EE), techo/laterales/paso-doble, fase de salto (0=suelo,
  1-8 sube, 9-16 flota, 0x11 cae; soltar el trigger en el aire → 0x11).
- `sub_6F5C`: aplicador — frames PARES: medio paso (celda±1, píxel a mitad,
  en unidades de 4px) + detección de salida de sala (0xEAE1); IMPARES: snap
  a celda + celda extra si paso doble (rampas). Animación: patrón = frame*12,
  ciclos por bits del contador de frame 0xEAC9.
- Timing de input real (sub_4064/sub_50E8): EACB/EACC se limpian y polean
  SOLO en frames pares (30Hz efectivo). La traza muestrea como el bp del
  oráculo (post-clear, pre-poll). Alineación EAC9 ≡ 2 (mod 4).
- ¡PUERTAS REALES! El bloqueo contra puerta cerrada de la traza `walk` exigió
  portar sub_4325 (localizar puerta: celdas bit 0x02, slot en el 0xE6EE de la
  fila inferior — explica el layout 0xA0 arriba / 0xA2 cuerpo) y sub_758C
  (abrir: consume llave 0xE337+color, slot[0]=0, redibuja marco y blanquea el
  cuerpo → colmap pasa a aire). `rl_door_press()` en room_loader — media
  Fase 5 adelantada.

Pendiente Fase 3: escaleras (bloque 0x4586+, vía sub_434A/442D), transición
de sala (sub_5053), muerte/daño; conmutar faithful_play a player.c.

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
