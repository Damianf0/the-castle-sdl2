/*
 * THE CASTLE — Jugador + enemigos sobre la geometría del mapa (modo viewer).
 * Física simple de plataformas: gravedad, salto, colisión contra tiles sólidos.
 * La solidez se deriva del tileset real (ratio de píxeles no-negros por tile).
 */
#include <string.h>
#include <stdlib.h>
#include "actors.h"
#include "map_real.h"

/* Geometría = VRAM real del ROM: 32x24 tiles, HUD en las filas 0..2. */
#define ROOM_W (RT_COLS * 8)   /* 256 */
#define ROOM_H (RT_ROWS * 8)   /* 192 */
#define HUD_ROWS 3

/* ---- estado público (leído por el renderer) ---- */
int g_player_px, g_player_py, g_player_face = 1;
int g_player_anim;
int g_player_moving;
int g_enemy_n;
int g_enemy_px[MAX_ENEMIES], g_enemy_py[MAX_ENEMIES], g_enemy_dir[MAX_ENEMIES];

static int g_room_idx;           /* sala actual (ry*10+rx) */
static int tile_solid[RT_COUNT]; /* 1 si el tile es piso/pared sólida */
static int solid_ready;

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

/* Un tile es sólido si >=40% de sus píxeles visibles son no-negros.
 * RT_TILES[t] = 8 bytes patrón + 8 bytes color (fg<<4|bg por fila). */
static void compute_solid(void)
{
    for (int t = 0; t < RT_COUNT; t++) {
        int nb = 0;
        for (int row = 0; row < 8; row++) {
            uint8_t pat = RT_TILES[t][row], col = RT_TILES[t][8 + row];
            uint8_t fg = col >> 4, bg = col & 0x0F;
            for (int b = 0; b < 8; b++) {
                uint8_t pi = (pat & (0x80u >> b)) ? fg : bg;
                if (pi > 1) nb++;
            }
        }
        tile_solid[t] = (nb >= 26);
    }
    solid_ready = 1;
}

static int solid_at(int x, int y)
{
    if (x < 0 || x >= ROOM_W) return 0;     /* borde lateral = salida, no sólido */
    if (y < HUD_ROWS * 8) return 1;         /* HUD arriba = techo sólido */
    if (y >= ROOM_H) return 1;              /* piso del mundo */
    int c = x / 8, r = y / 8;
    return tile_solid[ROOM_NT[g_room_idx][r][c]];
}

/* AABB del jugador: 8 ancho x 14 alto */
#define PW 8
#define PH 14
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

void actors_init_room(unsigned char room, int entry_edge)
{
    if (!solid_ready) compute_solid();
    int ry = room >> 4, rx = room & 0x0F;
    g_room_idx = ry * 10 + rx;

    /* columna deseada de entrada según el borde por el que entró */
    int want_col = (RT_COLS / 2);
    if      (entry_edge == 7) want_col = RT_COLS - 3;  /* entró por la derecha */
    else if (entry_edge == 3) want_col = 2;

    /* Buscar una celda PARABLE: 2 tiles abiertos (hueco para el sprite de 16px)
     * con piso sólido debajo. Elegir la más cercana a want_col, evitando la
     * cornisa pegada al HUD (filas altas). */
    int best_c = -1, best_r = -1, best_d = 9999;
    for (int r = HUD_ROWS + 2; r < RT_ROWS - 1; r++) {
        for (int c = 1; c < RT_COLS - 1; c++) {
            int open_here  = !tile_solid[ROOM_NT[g_room_idx][r][c]];
            int open_above = !tile_solid[ROOM_NT[g_room_idx][r - 1][c]];
            int floor_blw  =  tile_solid[ROOM_NT[g_room_idx][r + 1][c]];
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

    /* Los enemigos REALES ya están dibujados en la geometría (VRAM del ROM).
     * No spawneamos los de maqueta para no duplicar. El movimiento real es la
     * capa de comportamiento pendiente (port de enemies.c validado vs RAM). */
    g_enemy_n = 0;
    if (0) for (int c = 4; c < RT_COLS - 3 && g_enemy_n < MAX_ENEMIES; c += 7) {
        int ex = c * 8;
        /* buscar fila abierta con piso debajo */
        for (int r = HUD_ROWS; r < RT_ROWS - 2; r++) {
            int t  = ROOM_NT[g_room_idx][r][c];
            int tb = ROOM_NT[g_room_idx][r + 1][c];
            if (!tile_solid[t] && tile_solid[tb]) {
                g_enemy_px[g_enemy_n] = ex;
                g_enemy_py[g_enemy_n] = r * 8 + 2;
                g_enemy_dir[g_enemy_n] = (g_enemy_n & 1) ? 1 : -1;
                g_enemy_n++;
                break;
            }
        }
    }
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
        for (int i = 0; i < g_hspeed; i++) {
            int nx = g_player_px + dir;
            if (box_solid(nx, g_player_py)) break;
            g_player_px = nx; g_player_moving = 1;
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

    /* --- enemigos: patrullan horizontal, rebotan en pared o borde --- */
    for (int e = 0; e < g_enemy_n; e++) {
        int nx = g_enemy_px[e] + g_enemy_dir[e];
        int fy = g_enemy_py[e] + PH;            /* pies */
        if (nx < 4 || nx > ROOM_W - 10 ||
            solid_at(nx + (g_enemy_dir[e] > 0 ? 7 : 0), g_enemy_py[e] + 6) ||
            !solid_at(nx + 4, fy))              /* no caminar al vacío */
            g_enemy_dir[e] = -g_enemy_dir[e];
        else
            g_enemy_px[e] = nx;
    }
    return 0;
}
