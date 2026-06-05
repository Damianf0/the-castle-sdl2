@echo off
REM The Castle - viewer jugable: recorres el castillo con el JUGADOR REAL del ROM.
REM   Flechas izq/der = mover    Flecha arriba / Espacio = saltar
REM   Salir por un borde de la pantalla = pasar a la sala contigua
REM   Esc = salir
REM Geometria = VRAM real del ROM + sprite real del jugador (player_sprite.c).
cd /d "%~dp0"
set CASTLE_SPEED=1
set CASTLE_VIEW=1
set CASTLE_GEOMDBG=1
set CASTLE_ROOM=70
the_castle.exe the_castle.rom
