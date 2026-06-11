/*
 * THE CASTLE — Jugador + enemigos sobre la geometría del mapa (modo viewer).
 * Física simple de plataformas: gravedad, salto, colisión contra tiles sólidos.
 * La solidez se deriva del tileset real (ratio de píxeles no-negros por tile).
 */
#include <string.h>
#include <stdlib.h>
#include "actors.h"
#include "room_loader.h"
#include "enemies_port.h"
#include "doors_port.h"
#include "blocks_port.h"
#include "keys_port.h"
#include "items_port.h"

/* Geometría = VRAM real del ROM: 32x24 tiles, HUD en las filas 0..2. */
#define ROOM_W (RT_COLS * 8)   /* 256 */
#define ROOM_H (RT_ROWS * 8)   /* 192 */
#define HUD_ROWS 3

/* ---- estado público (leído por el renderer) ---- */
int g_player_px, g_player_py, g_player_face = 1;
int g_player_anim;
int g_player_moving;
int g_player_lives = 4;     /* vidas (los corazones del HUD) */
int g_player_invuln = 0;    /* frames de invulnerabilidad/parpadeo tras un golpe */
int g_enemy_n;
int g_enemy_px[MAX_ENEMIES], g_enemy_py[MAX_ENEMIES], g_enemy_dir[MAX_ENEMIES];

static int g_room_idx;           /* sala actual (ry*10+rx) */

/* Copia de trabajo del TILEMAP DE COLISIÓN REAL del ROM (0xE496) para la sala
 * actual. Es el dato que usa el juego original: bit 0x80 = bloquea al jugador
 * (paredes E0, puertas A0/A2, bloques A8); enemigos 0x38 y recogibles 0x24/0x20
 * NO bloquean. Se muta al abrir puertas (se limpian sus celdas). */
static uint8_t s_cm[CM_ROWS][CM_COLS];

/* Limpia celdas del tilemap de trabajo (en tiles de PANTALLA). Lo llama
 * doors_port al abrir/restaurar una puerta abierta. */
void actors_cm_clear(int scol, int srow, int w, int h)
{
    for (int r = srow; r < srow + h; r++)
        for (int c = scol; c < scol + w; c++) {
            int fr = r - 4, fc = c - 1;
            if (fr >= 0 && fr < CM_ROWS && fc >= 0 && fc < CM_COLS)
                s_cm[fr][fc] = 0;
        }
}

/* --- Estado del salto FIEL (capturado del ROM vía openMSX) ---------------
 * Caminar = 4 px/frame. Salto = SPACE: sube 4 px/frame mientras se mantiene
 * (tope 8 pasos = 32 px), flota en el apex hasta 8 frames mientras se
 * mantiene, y cae 4 px/frame. Es un arco scripteado, sin aceleración. */
#define MOVE_STEP  4
#define RISE_MAX   8   /* frames de subida (×4px = 32px de altura máx) */
#define HOVER_MAX  8   /* frames de flotación en el apex */
int g_player_air;                /* 1 si está en el aire (leído por el render) */
static int p_phase;              /* 0=subiendo 1=flotando 2=cayendo */
static int p_rise_left;          /* frames de subida restantes */
static int p_hover_left;         /* frames de flotación restantes */
static int p_jump_prev;          /* estado anterior de la tecla de salto (flanco) */

/* Solidez del campo en coords de CAMPO (fila 0..19, col 0..29), con espejado
 * en los bordes: la pantalla tiene 32 cols pero el campo 30 (la col 0 y 31 de
 * pantalla espejan la col 0/29 del campo: si hay puerta abierta es pasaje).
 * Arriba del campo (HUD) espeja la fila 0 (permite salir por pozos verticales);
 * debajo del campo = salida (caer a la sala de abajo). */
static int cm_solid(int fr, int fc)
{
    if (fc < 0) fc = 0; else if (fc >= CM_COLS) fc = CM_COLS - 1;
    if (fr < 0) fr = 0;
    if (fr >= CM_ROWS) return 0;
    return (s_cm[fr][fc] & CM_SOLID) != 0;
}

static int solid_at(int x, int y)
{
    if (x < 0 || x >= ROOM_W) return 0;     /* borde lateral = salida, no sólido */
    if (y < 0 || y >= ROOM_H) return 0;     /* arriba/abajo del todo = salida */
    int c = x / 8, r = y / 8;
    if (block_solid(r, c)) return 1;        /* bloque empujable (pos actual din.) */
    return cm_solid(r - 4, c - 1);          /* TILEMAP REAL del ROM (0xE496) */
}

/* solidez del tile base en tiles de PANTALLA (sin el bloque dinámico) —
 * la usa blocks_port para decidir a dónde puede moverse un bloque. */
int actors_tile_solid(int sr, int sc)
{
    if (sc < 0 || sc >= RT_COLS || sr < 0 || sr >= RT_ROWS) return 1;
    return cm_solid(sr - 4, sc - 1);
}

/* AABB del jugador: 16x16 = el BORDE del sprite real (no el centro).
 * Los probes usan 1px de margen por lado para no engancharse en pasajes
 * de exactamente 2 tiles (16px). */
#define PW 16
#define PH 16
static int box_solid(int x, int y)
{
    return solid_at(x + 1, y) || solid_at(x + PW - 2, y) ||
           solid_at(x + 1, y + PH - 1) || solid_at(x + PW - 2, y + PH - 1) ||
           solid_at(x + PW/2, y + PH - 1);
}

/* Encuentra el piso más cercano debajo de (x) para spawnear */
static int floor_below(int x, int y0)
{
    for (int y = y0; y < ROOM_H - PH; y++)
        if (solid_at(x + PW/2, y + PH)) return y;
    return ROOM_H - PH - 1;
}

/* Coloca al jugador en una celda parable cerca del borde de entrada. */
static void spawn_player(int entry_edge)
{
    int want_col = (RT_COLS / 2);
    if      (entry_edge == 7) want_col = RT_COLS - 3;  /* entró por la derecha */
    else if (entry_edge == 3) want_col = 2;

    /* Buscar una celda PARABLE: 2 tiles abiertos (hueco para el sprite de 16px)
     * con piso sólido debajo. Elegir la más cercana a want_col, evitando la
     * cornisa pegada al HUD (filas altas). */
    int best_c = -1, best_r = -1, best_d = 9999;
    for (int r = HUD_ROWS + 2; r < RT_ROWS - 1; r++) {
        for (int c = 1; c < RT_COLS - 2; c++) {
            /* hueco de 2x2 tiles (el sprite es 16x16) con piso debajo,
             * según el TILEMAP REAL (cm_solid trabaja en coords de campo) */
            int open_here  = !cm_solid(r - 4, c - 1) && !cm_solid(r - 4, c);
            int open_above = !cm_solid(r - 5, c - 1) && !cm_solid(r - 5, c);
            int floor_blw  =  cm_solid(r - 3, c - 1);
            if (open_here && open_above && floor_blw) {
                int d = (c - want_col) * (c - want_col) - r;  /* cerca de col, prefiere filas bajas */
                if (d < best_d) { best_d = d; best_c = c; best_r = r; }
            }
        }
    }
    if (best_c >= 0) {
        g_player_px = best_c * 8;
        g_player_py = (best_r + 1) * 8 - PH;   /* pies sobre el piso */
    } else {
        g_player_px = want_col * 8;
        g_player_py = HUD_ROWS * 8;
    }
    g_player_air = 0; p_phase = 0; p_rise_left = 0; p_hover_left = 0; p_jump_prev = 0;
}

void actors_init_room(unsigned char room, int entry_edge)
{
    int ry = room >> 4, rx = room & 0x0F;
    g_room_idx = ry * 10 + rx;

    /* Cargar el TILEMAP DE COLISIÓN REAL de la sala (copia de trabajo) desde
     * la RAM del room loader (0xE496, recién decodificada del ROM).
     * Las celdas 0xA8 (bloques empujables) se limpian: los bloques se simulan
     * dinámicos (block_solid) y se mueven; el resto queda tal cual el ROM.
     * Las puertas (A0/A2) quedan sólidas; doors_room_init limpia las abiertas. */
    for (int r = 0; r < CM_ROWS; r++)
        for (int c = 0; c < CM_COLS; c++) {
            uint8_t v = rl_ram_rb((uint16_t)(0xE496u + r * 30 + c));
            s_cm[r][c] = (v == CM_BLOCK) ? 0 : v;
        }

    /* Al CRUZAR de sala se CONSERVA la coordenada perpendicular al borde
     * (salís por una puerta a media altura -> entrás a la misma altura, no en
     * cualquier otra puerta). Sólo el spawn inicial / tras un golpe usa la
     * heurística de spawn_player. */
    if (entry_edge == 3) {                       /* entró desde la izquierda */
        g_player_px = 1;                         /* py se conserva */
    } else if (entry_edge == 7) {                /* entró desde la derecha */
        g_player_px = ROOM_W - PW - 1;
    } else if (entry_edge == 5) {                /* entró desde arriba (cayó) */
        g_player_py = HUD_ROWS * 8 + 1;          /* px se conserva */
    } else if (entry_edge == 1) {                /* entró desde abajo (saltó) */
        g_player_py = ROOM_H - PH - 1;
    } else {
        spawn_player(0);
    }
    if (entry_edge) {
        /* destrabar si el punto de entrada cae en sólido: corrimientos cortos
         * sobre el mismo eje; si no hay caso, heurística vieja como fallback */
        if (box_solid(g_player_px, g_player_py)) {
            int lateral = (entry_edge == 3 || entry_edge == 7);
            int fixed = 0;
            for (int d = 4; d <= 32 && !fixed; d += 4) {
                int x1 = g_player_px - (lateral ? 0 : d), y1 = g_player_py - (lateral ? d : 0);
                int x2 = g_player_px + (lateral ? 0 : d), y2 = g_player_py + (lateral ? d : 0);
                if      (!box_solid(x1, y1)) { g_player_px = x1; g_player_py = y1; fixed = 1; }
                else if (!box_solid(x2, y2)) { g_player_px = x2; g_player_py = y2; fixed = 1; }
            }
            if (!fixed) spawn_player(entry_edge);
        }
        p_jump_prev = 0;   /* el arco de salto en curso continúa en la sala nueva */
    }
    g_player_invuln = 80;   /* gracia al entrar (el spawn puede caer cerca de un enemigo) */

    /* Los enemigos reales los maneja enemies_port (path-replay del ROM). */
    g_enemy_n = 0;
}

/* Devuelve el borde de salida (0=ninguno,1=arriba,3=der,5=abajo,7=izq).
 * 'jump' = tecla de salto (SPACE en el original). */
int actors_update(int left, int right, int jump)
{
    /* --- horizontal: g_hspeed px/frame (ajustable con CASTLE_SPEED) --- */
    static int g_hspeed = 0;
    if (g_hspeed == 0) {
        const char *s = getenv("CASTLE_SPEED");
        g_hspeed = s ? atoi(s) : 3;     /* default 3 px/frame (un poco más lento) */
        if (g_hspeed < 1) g_hspeed = 1;
    }
    g_player_moving = 0;
    int dir = (right ? 1 : 0) - (left ? 1 : 0);
    if (dir) {
        g_player_face = (dir > 0) ? 1 : 0;
        int moved = 0;
        for (int i = 0; i < g_hspeed; i++) {
            int nx = g_player_px + dir;
            if (box_solid(nx, g_player_py)) break;
            g_player_px = nx; g_player_moving = 1; moved = 1;
        }
        /* si quedó trabado contra un bloque, intentar empujarlo */
        if (!moved) {
            if (blocks_push(g_player_px, g_player_py, PH, dir)) g_player_moving = 1;
        }
        if (g_player_px < -2)               return 7;   /* salida izquierda */
        if (g_player_px > ROOM_W - PW + 2)  return 3;   /* salida derecha   */
    }

    /* --- vertical: arco de salto FIEL (sin gravedad continua) --- */
    int on_ground = box_solid(g_player_px, g_player_py + 1);
    int jump_edge = jump && !p_jump_prev;   /* flanco de subida: re-pulsar */
    p_jump_prev = jump;

    if (!g_player_air) {
        if (on_ground && jump_edge) {       /* iniciar salto */
            g_player_air = 1; p_phase = 0;
            p_rise_left = RISE_MAX; p_hover_left = HOVER_MAX;
        } else if (!on_ground) {            /* se cayó de una repisa */
            g_player_air = 1; p_phase = 2;
        }
    }

    if (g_player_air) {
        if (p_phase == 0) {                 /* subiendo */
            if (jump && p_rise_left > 0) {
                for (int i = 0; i < MOVE_STEP; i++) {
                    if (box_solid(g_player_px, g_player_py - 1)) { p_rise_left = 1; break; }
                    g_player_py--;
                }
                if (--p_rise_left <= 0) p_phase = 1;  /* llegó al tope -> flotar */
            } else {
                p_phase = 2;                /* soltó antes -> caer (altura variable) */
            }
        } else if (p_phase == 1) {          /* flotando en el apex */
            if (jump && p_hover_left > 0) p_hover_left--;
            else p_phase = 2;
        }
        if (p_phase == 2) {                 /* cayendo 4 px/frame */
            for (int i = 0; i < MOVE_STEP; i++) {
                if (box_solid(g_player_px, g_player_py + 1)) {
                    g_player_air = 0; p_phase = 0; break;
                }
                g_player_py++;
            }
        }
    }

    if (g_player_py > ROOM_H + 4) return 5;   /* cayó -> sala de abajo */
    if (g_player_py < -4)         return 1;   /* subió -> sala de arriba */

    if (g_player_moving) g_player_anim = (g_player_anim + 1) & 0x3F;

    /* --- daño: tocar un enemigo cuesta una vida --- */
    if (g_player_invuln > 0) {
        g_player_invuln--;
    } else {
        /* AABB del jugador (un poco encogido para que sea justo) */
        int plx = g_player_px + 1, ply = g_player_py + 1;
        int prx = g_player_px + PW - 1, pry = g_player_py + PH - 1;
        for (int e = 0; e < g_pen_n; e++) {
            if (!g_pen[e].active) continue;
            /* enemigo en pantalla: col=byte2+2, fila=byte3+4, 2x2 tiles (16x16),
             * encogido 3px por lado */
            int ex = (g_pen[e].row + 2) * 8 + 3;
            int ey = (g_pen[e].col + 4) * 8 + 3;
            int ex2 = ex + 16 - 6, ey2 = ey + 16 - 6;
            if (plx < ex2 && prx > ex && ply < ey2 && pry > ey) {
                /* GOLPE */
                if (g_player_lives > 0) g_player_lives--;
                else g_player_lives = 3;            /* viewer: reinicia el contador */
                g_player_invuln = 90;               /* ~1.5s de parpadeo/invuln */
                spawn_player(0);                    /* reaparece en el centro */
                break;
            }
        }
    }
    return 0;
}
