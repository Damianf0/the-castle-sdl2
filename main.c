/*
 * THE CASTLE (ASCII, 1986) — main.c
 * ==================================
 * Punto de entrada del port. Responsabilidades:
 *
 *   1. Cargar la ROM desde disco
 *   2. Inicializar la HAL SDL2
 *   3. Cargar tiles desde la ROM a la VRAM emulada
 *   4. Inicializar todos los subsistemas del juego
 *   5. Correr el loop principal (title → game → title ...)
 *   6. Manejar cierre limpio
 *
 * Uso:
 *   ./the_castle [ruta/a/the_castle.rom]
 *   (por defecto busca "the_castle.rom" en el directorio actual)
 *
 * Compile (Linux):
 *   gcc -std=c99 -Wall -O2 \
 *       main.c the_castle.c tiles.c enemies.c particles.c doors.c hal_sdl2.c \
 *       $(sdl2-config --cflags --libs) -lm -o the_castle
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

#include "hal.h"
#include "game.h"
#include "geom.h"
#include "actors.h"
#include "enemies_port.h"
#include "keys_port.h"
#include "items_port.h"
#include "doors_port.h"
#include "blocks_port.h"
#include "tiledata.h"
#include "colmap_data.h"
#include "doors_data.h"
#include "keys_data.h"
#include "items_data.h"

/* ==========================================================================
 * CONSTANTES
 * ========================================================================== */

#define ROM_SIZE_EXPECTED  32768u          /* 32 KB — ROM estándar MSX       */
#define ROM_DEFAULT_PATH   "the_castle.rom"

/* ==========================================================================
 * VARIABLES GLOBALES DEL JUEGO
 * (Definiciones — las declaraciones extern están en game.h)
 * ========================================================================== */

uint8_t g_state_flags    = 0;
uint8_t g_anim_frame     = 0;
uint8_t g_facing         = 0;
uint8_t g_player_speed   = 0;
uint8_t g_transition     = 0;
uint8_t g_game_over      = 0;
uint8_t g_room_exit      = 0;
uint8_t g_restart_flag   = 0;
uint8_t g_intro_active   = 0;
uint8_t g_enemies_active = 0;
uint8_t g_player_col     = 0;
uint8_t g_player_row     = 0;
uint8_t g_player_x       = 0;
uint8_t g_player_y       = 0;
uint8_t g_lives          = 0;
uint8_t g_room_number    = 0;
uint8_t g_score[3]       = {0, 0, 0};
uint8_t g_hiscore[3]     = {0, 0, 0};
uint8_t g_map[0x400]     = {0};

const uint8_t *g_rom      = NULL;
uint32_t       g_rom_size = 0;

/* ==========================================================================
 * STUBS de doors.c (update_roller_by_pos, update_bat_by_slot)
 *
 * doors.c necesita llamar a funciones de enemies.c para los rollers
 * y murciélagos que aparecen en la tabla de coleccionables.
 * Aquí las implementamos como wrappers que delegan a enemies.c.
 *
 * En una versión completa se refactorizaría enemies.c para exponer
 * estas funciones directamente.
 * ========================================================================== */
void update_roller_by_pos(uint8_t col, uint8_t row, uint8_t move_flags)
{
    /*
     * Equivale a llamar sub_710B con un slot sintético.
     * Por ahora: solo dibujar el tile en la posición dada.
     * TODO: instanciar un EnemySlot temporal y llamar a la lógica real.
     */
    (void)move_flags;
    /* Tile del roller = 0x34 en la name table */
    uint16_t addr = (uint16_t)(0x1800u + (uint16_t)row * 32u + col + 1u);
    hal_vdp_write_vram(addr, 0x34u);
}

void update_bat_by_slot(uint8_t col, uint8_t row, uint8_t move_flags)
{
    /*
     * Equivale a llamar sub_719D con un slot sintético.
     * TODO: instanciar un EnemySlot temporal y llamar a la lógica real.
     */
    (void)move_flags;
    uint16_t addr = (uint16_t)(0x1800u + (uint16_t)row * 32u + col + 1u);
    hal_vdp_write_vram(addr, 0x36u);
}

/* ==========================================================================
 * CARGA DE ROM
 * ========================================================================== */

static uint8_t *load_rom(const char *path, uint32_t *size_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "Error: no se puede abrir la ROM '%s'\n", path);
        fprintf(stderr, "  Copia the_castle.rom al directorio de trabajo\n"
                        "  o pasa la ruta como primer argumento.\n");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz != (long)ROM_SIZE_EXPECTED) {
        fprintf(stderr, "Advertencia: la ROM tiene %ld bytes (se esperaban %u)\n",
                sz, ROM_SIZE_EXPECTED);
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fprintf(stderr, "Error: sin memoria para cargar la ROM (%ld bytes)\n", sz);
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)sz, f);
    fclose(f);

    if (read != (size_t)sz) {
        fprintf(stderr, "Error: lectura incompleta de la ROM\n");
        free(buf);
        return NULL;
    }

    /* Verificar magic MSX: primeros 2 bytes deben ser 'A' 'B' (0x41 0x42) */
    if (buf[0] != 0x41 || buf[1] != 0x42) {
        fprintf(stderr, "Advertencia: magic bytes incorrectos (0x%02X 0x%02X, se esperaba 0x41 0x42)\n",
                buf[0], buf[1]);
        fprintf(stderr, "  El archivo puede no ser una ROM MSX válida.\n");
    }

    *size_out = (uint32_t)sz;
    printf("ROM cargada: '%s' (%u bytes)\n", path, *size_out);
    printf("  Entry point INIT: 0x%04X\n",
           (unsigned)(buf[2] | (buf[3] << 8)));

    return buf;
}

/* ==========================================================================
 * GAME FRAME (sub_4064) — Una iteración del bucle de juego por frame
 *
 * Estructura fiel al código original en 0x4064:
 *
 *   sub_4064:
 *     CALL sub_5D5D         → check title_mode flag (g_state_flags bit 0)
 *     [if not title mode: reset anim_frame, facing]
 *     CALL sub_6383         → reset keyframe queue
 *     LD A,1 → (0xEAE8)    → enemies_active = 1
 *     CALL sub_5128         → music tick + VSync
 *     XOR A → (0xEAE8)     → enemies_active = 0
 *     CALL 0x62D8           → render background + triggers
 *     CALL sub_5B96         → scroll update
 *     check g_restart_flag  → if set, return
 *     CALL sub_442D         → update doors
 *     CALL sub_434A         → update collectibles
 *     CALL sub_40BB         → update player
 *     CALL sub_6F5C         → update enemies
 *     CALL sub_4406         → update traps
 *     CALL sub_438D         → check key pickup
 *     CALL sub_4499         → check door exit
 *     CALL sub_5A2D         → update HUD
 *     check g_game_over     → if set, return
 *     check g_room_exit     → CALL sub_5053 → if NZ, return
 *     g_state_flags++
 *     CALL 0x623C           → camera/particles update
 *     JR sub_4064
 * ========================================================================== */
void game_frame(void)
{
    /* sub_5D5D: check title/demo mode bit */
    bool title_mode = (g_state_flags & 0x01u) != 0;
    if (!title_mode) {
        g_anim_frame = 0;
        g_facing     = 0;
    }

    /* sub_6383: reset keyframe queue */
    memset(g_keyframe_queue, 0xFFu, sizeof(g_keyframe_queue));

    /* enemies_active toggle around poll/music (sub_5128) */
    g_enemies_active = 1;
    if (!hal_poll_events()) { g_game_over = 1; return; }
    g_enemies_active = 0;

    /* sub_62D8: render background with trigger processing */
    render_background();

    /* sub_5B96: scroll/trigger update */
    scroll_update();

    /* check restart flag (g_restart_flag) */
    if (g_restart_flag) return;

    /* update_doors (sub_442D) */
    update_doors();

    /* update_collectibles (sub_434A) */
    update_collectibles();

    /* game_loop (sub_40BB: player movement + camera) */
    game_loop();

    /* update_enemies (sub_6F5C + sub_6F27) */
    update_enemies();

    /* update_traps (sub_4406) */
    update_traps();

    /* check_key_pickup (sub_438D) */
    check_key_pickup();

    /* check_door_exit (sub_4499) */
    check_door_exit();

    /* update HUD (sub_5A2D — solo la parte dinámica) */
    draw_hud_dynamic();

    /* check game over */
    if (g_game_over) return;

    /* check room exit → sub_5053 */
    if (g_room_exit) {
        room_transition();
        if (g_room_exit) return;
    }

    /* increment frame counter */
    g_state_flags++;

    /* camera + particles update (sub_623C) */
    camera_update();
    update_particles();
}

/* ==========================================================================
 * LOOP PRINCIPAL (sub_401C)
 *
 * Estructura fiel al código original en 0x4016:
 *
 *   restart_title:
 *     reset_keyframe_queue()    ; sub_6383
 *     game_reset_level()        ; sub_4D52
 *   inner_loop:
 *     run_title_or_game()       ; sub_4A4A  — maneja title + demo + juego
 *     game_reset_level()        ; sub_4D52
 *     if aborted → restart_title
 *     clear_aux_state()         ; sub_4029
 *     goto inner_loop
 *
 * sub_4A4A (title_screen) NO retorna hasta que el jugador muere (game over).
 * Internamente maneja: 3 ciclos de título → demo mode → juego real.
 * ========================================================================== */
static void main_loop(void)
{
    while (true) {
        /* sub_4A4A: title_screen maneja título + demo + juego completo */
        title_screen();

        /* Si el usuario cerró la ventana durante title_screen */
        if (!hal_poll_events()) break;

        /* sub_4D52: reset de nivel tras game over */
        game_reset_level();

        /* sub_4029: clear aux state */
        music_set_tempo(0u, 0u);
        music_set_transpose(0u, 0u);
    }
}

/* ==========================================================================
 * JUEGO FIEL — render de la VRAM real + jugador/enemigos/llaves/puertas/bloques.
 * Es el gameplay que antes sólo se alcanzaba con CASTLE_VIEW=1. Corre hasta que
 * se cierra la ventana. Salir por los bordes cambia de sala.
 * ========================================================================== */
void faithful_play(uint8_t start_room)
{
    uint8_t room = start_room;
    geom_decode_room(room);
    actors_init_room(room, 0);
    enemies_room_init(room);
    keys_room_init(room);
    items_room_init(room);
    doors_room_init(room);
    blocks_room_init(room);
    g_actors_on = 1;   /* activa el render de geometría real + actores */

    while (hal_poll_events()) {
        uint8_t dir = hal_joystick_read(0);
        int left  = (dir == 6 || dir == 7 || dir == 8);
        int right = (dir == 2 || dir == 3 || dir == 4);
        int up    = (dir == 1 || dir == 2 || dir == 8) || hal_key_pressed();

        int edge = actors_update(left, right, up);
        enemies_step();
        keys_update(g_player_px, g_player_py, 16, 16);
        items_update(g_player_px, g_player_py, 16, 16);
        doors_update(g_player_px, g_player_py, 16, 16);
        blocks_step();
        if (edge) {
            uint8_t hi = room >> 4, lo = room & 0x0Fu;
            if      (edge == 1) hi = (hi == 0) ? 9 : hi - 1;
            else if (edge == 5) hi = (hi == 9) ? 0 : hi + 1;
            else if (edge == 3) lo = (lo == 9) ? 0 : lo + 1;
            else if (edge == 7) lo = (lo == 0) ? 9 : lo - 1;
            room = (uint8_t)((hi << 4) | lo);
            geom_decode_room(room);
            actors_init_room(room, edge);
            enemies_room_init(room);
            keys_room_init(room);
            items_room_init(room);
            doors_room_init(room);
            blocks_room_init(room);
        }
        hal_wait_vsync();
    }
    g_actors_on = 0;
}

/* ==========================================================================
 * ENTRY POINT
 * ========================================================================== */
int main(int argc, char *argv[])
{
    const char *rom_path = (argc > 1) ? argv[1] : ROM_DEFAULT_PATH;

    /* --- 1. Cargar ROM --- */
    uint8_t  *rom_buf  = NULL;
    uint32_t  rom_size = 0;

    rom_buf = load_rom(rom_path, &rom_size);
    if (!rom_buf) return 1;

    /* --- Modo dump de estado (harness de tests, sin SDL): vuelca por sala las
     * tablas que el juego usa, en formato canónico, y sale.
     *   CASTLE_DUMP=dir → dir/colmap_XX.bin (600 bytes crudos del campo 20x30)
     *                   + dir/doors_XX.txt  (dcol drow dw dh color count)
     *                   + dir/keys_XX.txt   (scol srow sw sh color)
     *                   + dir/items_XX.txt  (scol srow val)
     * Hoy sale de las tablas horneadas de los fixtures (verde trivial). Cuando
     * el room loader portado (sub_64DD) las reemplace, este dump debe seguir
     * byte-idéntico — esa es la red de regresión del port. */
    {
        const char *dd = getenv("CASTLE_DUMP");
        if (dd) {
            char p[512];
            for (int idx = 0; idx < 100; idx++) {
                int xx = ((idx / 10) << 4) | (idx % 10);   /* sala BCD */
                FILE *f;
                snprintf(p, sizeof p, "%s/colmap_%02X.bin", dd, xx);
                f = fopen(p, "wb");
                if (!f) {
                    fprintf(stderr, "CASTLE_DUMP: no puedo escribir %s\n", p);
                    free(rom_buf);
                    return 1;
                }
                fwrite(COLMAP[idx], 1, CM_ROWS * CM_COLS, f);
                fclose(f);

                snprintf(p, sizeof p, "%s/doors_%02X.txt", dd, xx);
                f = fopen(p, "w");
                for (int i = 0; f && i < DOOR_COUNT[idx]; i++) {
                    const DoorDef *d = &DOOR_DATA[idx][i];
                    fprintf(f, "%u %u %u %u %u %u\n",
                            d->dcol, d->drow, d->dw, d->dh, d->color, d->count);
                }
                if (f) fclose(f);

                snprintf(p, sizeof p, "%s/keys_%02X.txt", dd, xx);
                f = fopen(p, "w");
                for (int i = 0; f && i < KEY_COUNT[idx]; i++) {
                    const KeySpawn *k = &KEY_DATA[idx][i];
                    fprintf(f, "%u %u %u %u %u\n",
                            k->scol, k->srow, k->sw, k->sh, k->color);
                }
                if (f) fclose(f);

                snprintf(p, sizeof p, "%s/items_%02X.txt", dd, xx);
                f = fopen(p, "w");
                for (int i = 0; f && i < ITEM_COUNT[idx]; i++) {
                    const ItemSpawn *it = &ITEM_DATA[idx][i];
                    fprintf(f, "%u %u 0x%02X\n", it->scol, it->srow, it->val);
                }
                if (f) fclose(f);
            }
            printf("CASTLE_DUMP: 100 salas -> %s\n", dd);
            free(rom_buf);
            return 0;
        }
    }

    /* --- 2. Inicializar HAL SDL2 --- */
#ifdef CASTLE_PAL_TIMING
    bool pal = true;
#else
    bool pal = false;
#endif
    if (!hal_init(pal)) {
        fprintf(stderr, "Error: no se pudo inicializar SDL2\n");
        free(rom_buf);
        return 1;
    }

    /* --- Modo render de fixture: CASTLE_VRAMIN=dump.bin + CASTLE_SHOT=out.bmp
     * carga un volcado de VRAM de 16KB (oráculo openMSX) en la VRAM emulada,
     * renderiza UN frame con el VDP fiel y guarda el BMP. Los sprites del dump
     * se desactivan (la referencia compara solo el fondo SCREEN 2). */
    {
        const char *vin = getenv("CASTLE_VRAMIN");
        const char *shot = getenv("CASTLE_SHOT");
        if (vin && shot) {
            FILE *f = fopen(vin, "rb");
            uint8_t vbuf[0x4000];
            if (!f || fread(vbuf, 1, sizeof vbuf, f) != sizeof vbuf) {
                fprintf(stderr, "CASTLE_VRAMIN: no puedo leer 16KB de %s\n", vin);
                if (f) fclose(f);
                hal_quit(); free(rom_buf);
                return 1;
            }
            fclose(f);
            hal_vdp_init_screen2();
            hal_vdp_copy_to_vram(0x0000u, vbuf, 0x4000u);
            hal_vdp_clear_sprites();
            hal_vdp_present();
            hal_screenshot(shot);
            printf("CASTLE_VRAMIN: %s -> %s\n", vin, shot);
            hal_quit(); free(rom_buf);
            return 0;
        }
    }

    /* --- 3. Cargar tiles ROM → VRAM emulada --- */
    tiles_load_from_rom(rom_buf, rom_size);

    /* --- 6. Inicializar subsistemas --- */
    game_init();
    enemies_init();
    particles_init();
    doors_init();
    music_init();
    camera_init();

    /* --- Modo viewer interactivo: recorré las 100 salas con las flechas.
     *   CASTLE_VIEW=1  (CASTLE_GEOMDBG=1 para el render de ladrillo) */
    if (getenv("CASTLE_VIEW")) {
        uint8_t room = 0x70u;
        const char *rs = getenv("CASTLE_ROOM");
        if (rs) room = (uint8_t)strtol(rs, NULL, 16);
        faithful_play(room);
        hal_quit();
        free(rom_buf);
        return 0;
    }

    /* --- Modo screenshot: decodifica una sala, renderiza un frame y sale.
     *   CASTLE_SHOT=ruta.bmp  CASTLE_ROOM=70 (hex)  CASTLE_GEOMDBG=1 (overlay) */
    {
        const char *shot = getenv("CASTLE_SHOT");
        if (shot) {
            uint8_t room = 0x70u;
            const char *rs = getenv("CASTLE_ROOM");
            if (rs) room = (uint8_t)strtol(rs, NULL, 16);
            geom_decode_room(room);
            if (getenv("CASTLE_ACTORS")) {
                /* captura animada: simula frames con jugador+enemigos y guarda secuencia */
                actors_init_room(room, 0);
                enemies_room_init(room);
                keys_room_init(room);
                items_room_init(room);
                doors_room_init(room);
                blocks_room_init(room);
                g_actors_on = 1;
                int nframes = 40;
                const char *nf = getenv("CASTLE_FRAMES");
                if (nf) nframes = atoi(nf);
                int jhold = 0; const char *jt = getenv("CASTLE_JUMPHOLD");
                if (jt) jhold = atoi(jt);  /* test de salto: mantener SPACE jhold frames desde f=5 */
                const char *opx = getenv("CASTLE_PX"), *opy = getenv("CASTLE_PY");
                if (opx) g_player_px = atoi(opx);
                if (opy) g_player_py = atoi(opy);
                if (getenv("CASTLE_GIVEKEYS"))
                    for (int c = 0; c < KEY_COLORS; c++) g_key_inv[c] = 9;
                /* CASTLE_MOVES: guion de movimientos, 1 char por frame:
                 *   R=derecha L=izquierda U=salto D=der+salto A=izq+salto .=nada
                 * (si termina el guion, no se mueve más). Pisa la coreografía default. */
                const char *mv = getenv("CASTLE_MOVES");
                for (int f = 0; f < nframes; f++) {
                    int left, right, up;
                    if (mv) {
                        char m = (f < (int)strlen(mv)) ? mv[f] : '.';
                        left  = (m == 'L' || m == 'A');
                        right = (m == 'R' || m == 'D');
                        up    = (m == 'U' || m == 'D' || m == 'A');
                    } else {
                        left  = 0;
                        right = jhold ? 0 : (f > 8 && f < 28);
                        up    = jhold ? (f >= 5 && f < 5 + jhold) : (f == 14 || f == 22);
                    }
                    actors_update(left, right, up);
                    enemies_step();
                    keys_update(g_player_px, g_player_py, 16, 16);
                    items_update(g_player_px, g_player_py, 16, 16);
                    doors_update(g_player_px, g_player_py, 16, 16);
                    blocks_step();
                    if (getenv("CASTLE_DOORTEST") && f == 25) {
                        doors_room_init(room);   /* simula salir y volver a la sala */
                        printf(">>> re-init sala (simula volver)\n");
                    }
                    /* CASTLE_DOORROOM=XX (hex): a f25 carga las puertas de OTRA
                     * sala (verifica que la gemela quedó abierta por pairing) */
                    const char *drm = getenv("CASTLE_DOORROOM");
                    if (drm && f == 25) {
                        doors_room_init((unsigned char)strtol(drm, NULL, 16));
                        printf(">>> puertas de la sala 0x%s:\n", drm);
                    }
                    hal_vdp_present();
                    char p[256];
                    snprintf(p, sizeof(p), "%s_%02d.bmp", shot, f);
                    hal_screenshot(p);
                    if (jt) printf("f%02d py=%d air=%d\n", f, g_player_py, g_player_air);
                    if (getenv("CASTLE_ENEMYDBG")) {
                        printf("f%02d lives=%d inv[%d,%d,%d,%d,%d,%d] px=%d py=%d doors:", f, g_player_lives,
                               g_key_inv[0],g_key_inv[1],g_key_inv[2],g_key_inv[3],g_key_inv[4],g_key_inv[5], g_player_px, g_player_py);
                        for (int dd = 0; dd < g_door_n; dd++) printf("[c%d@%d,%d cnt%d %s]", g_door[dd].color, g_door[dd].dcol, g_door[dd].drow, g_door[dd].count, g_door[dd].open?"OPEN":"shut");
                        printf(" blk:"); for (int bb = 0; bb < g_block_n; bb++) printf("(%d,%d)", g_block[bb].scol, g_block[bb].srow);
                        for (int e = 0; e < g_pen_n; e++)
                            printf(" | t%02X r%d c%d", g_pen[e].type, g_pen[e].row, g_pen[e].col);
                        printf("\n");
                    }
                }
                printf("captura animada -> %s_NN.bmp (sala 0x%02x, %d frames)\n",
                       shot, room, nframes);
            } else {
                hal_vdp_present();
                hal_screenshot(shot);
                printf("screenshot -> %s (sala 0x%02x, %d objetos)\n",
                       shot, room, g_geom_object_count);
            }
            hal_quit();
            free(rom_buf);
            return 0;
        }
    }

    printf("Iniciando The Castle...\n");
    printf("Controles: Cursores/WASD = moverse, Z/Space = acción, Esc = salir\n");

    /* --- 5. Loop principal --- */
    main_loop();

    /* --- 6. Cierre limpio --- */
    hal_quit();
    free(rom_buf);

    printf("Hasta la próxima.\n");
    return 0;
}
