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
#include "room_loader.h"
#include "player.h"

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
    rl_reset();                /* estado de partida (sub_4D52): vidas, persistencia */
    geom_decode_room(room);
    rl_load_room(room);        /* sala desde el ROM: VRAM + tablas (sub_64DD) */
    player_sync_pixel();       /* sub_6F45 + sub_6F27 (cierre real de sub_64DD) */
    enemies_room_init(room);
    keys_room_init(room);
    items_room_init(room);
    g_actors_on = 1;   /* overlay de enemigos sobre el render del VDP */

    /* El JUGADOR es el real (player.c = sub_40BB + sub_6F5C, validado contra
     * las trazas): colisión por el colmap del loader, puertas por
     * rl_door_press (sub_4325/758C), transición por rl_room_exit (sub_5053 +
     * commit de persistencia sub_6134), sprite por el VDP (planos 8-10).
     * Maqueta restante: enemigos path-replay (Fase 4), pickup AABB (los
     * efectos reales son Fase 5), sin daño ni empuje de bloques todavía
     * (sub_4406 / sub_4273 — Fases 4-5). */
    while (hal_poll_events()) {
        player_frame(hal_joystick_read(0), hal_key_pressed() ? 1u : 0u);
        {
            uint8_t edge = player_take_exit();
            if (edge) {
                rl_room_exit(edge);
                room = rl_ram_rb(0xE320u);
                geom_decode_room(room);
                rl_load_room(room);
                player_sync_pixel();
                enemies_room_init(room);
                keys_room_init(room);
                items_room_init(room);
            }
        }
        enemies_step();
        keys_update(g_plr_px, g_plr_py, 16, 16);
        items_update(g_plr_px, g_plr_py, 16, 16);
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

    /* --- Modo dump de estado (harness, sin SDL): el ROOM LOADER PORTADO
     * (room_loader.c = sub_64DD fiel) decodifica las 100 salas desde el ROM
     * y vuelca las tablas RAW — comparables byte a byte contra los dumps de
     * openMSX en tests/fixtures/:
     *   colmap_XX.bin (900B: 0xE496 colmap + 0xE6EE índice)
     *   e346_XX.bin (64B puertas)  e3d6_XX.bin (64B coleccionables)
     *   e43e_XX.bin (80B estructurales)  objs_XX.bin (288B: 0xE380-0xE49F)
     *   ont_XX.bin (768B name table desde la VRAM emulada)
     * Mismo orden de carga que capture_vram.tcl (00..99 BCD, sin limpiar
     * VRAM entre salas) para reproducir también el estado visual. */
    {
        const char *dd = getenv("CASTLE_DUMP");
        if (dd) {
            char p[512];
            g_rom = rom_buf; g_rom_size = rom_size;
            rl_reset();
            rl_boot_vram();
            rl_load_room(0x70u);   /* la partida real cargó 0x70 antes de los force-calls */
            for (int idx = 0; idx < 100; idx++) {
                int xx = ((idx / 10) << 4) | (idx % 10);   /* sala BCD */
                FILE *f;
                uint8_t buf[900];
                rl_load_room((uint8_t)xx);

                snprintf(p, sizeof p, "%s/colmap_%02X.bin", dd, xx);
                f = fopen(p, "wb");
                if (!f) {
                    fprintf(stderr, "CASTLE_DUMP: no puedo escribir %s\n", p);
                    free(rom_buf);
                    return 1;
                }
                for (int i = 0; i < 900; i++) buf[i] = rl_ram_rb((uint16_t)(0xE496u + i));
                fwrite(buf, 1, 900, f); fclose(f);

                snprintf(p, sizeof p, "%s/e346_%02X.bin", dd, xx);
                f = fopen(p, "wb");
                for (int i = 0; i < 64; i++) buf[i] = rl_ram_rb((uint16_t)(0xE346u + i));
                if (f) { fwrite(buf, 1, 64, f); fclose(f); }

                snprintf(p, sizeof p, "%s/e3d6_%02X.bin", dd, xx);
                f = fopen(p, "wb");
                for (int i = 0; i < 64; i++) buf[i] = rl_ram_rb((uint16_t)(0xE3D6u + i));
                if (f) { fwrite(buf, 1, 64, f); fclose(f); }

                snprintf(p, sizeof p, "%s/e43e_%02X.bin", dd, xx);
                f = fopen(p, "wb");
                for (int i = 0; i < 80; i++) buf[i] = rl_ram_rb((uint16_t)(0xE43Eu + i));
                if (f) { fwrite(buf, 1, 80, f); fclose(f); }

                snprintf(p, sizeof p, "%s/objs_%02X.bin", dd, xx);
                f = fopen(p, "wb");
                for (int i = 0; i < 288; i++) buf[i] = rl_ram_rb((uint16_t)(0xE380u + i));
                if (f) { fwrite(buf, 1, 288, f); fclose(f); }

                snprintf(p, sizeof p, "%s/ont_%02X.bin", dd, xx);
                f = fopen(p, "wb");
                for (int i = 0; i < 768; i++) buf[i] = hal_vdp_read_vram((uint16_t)(0x1800u + i));
                if (f) { fwrite(buf, 1, 768, f); fclose(f); }

                snprintf(p, sizeof p, "%s/vram_%02X.bin", dd, xx);
                f = fopen(p, "wb");
                if (f) {
                    for (int i = 0; i < 0x4000; i++)
                        fputc(hal_vdp_read_vram((uint16_t)i), f);
                    fclose(f);
                }
            }
            printf("CASTLE_DUMP: 100 salas (decoder portado) -> %s\n", dd);
            free(rom_buf);
            return 0;
        }
    }

    /* --- Modo traza del jugador (Fase 3, sin SDL): CASTLE_PTRACE=out.txt +
     * CASTLE_MOVES=guion. Corre el jugador portado (player.c) sobre la sala
     * 0x70 cargada por el room loader y emite el estado por frame en el MISMO
     * punto que el bp del oráculo (0x4070: tras limpiar input en frames
     * pares, antes del poll) — comparable línea a línea con
     * tests/fixtures/traces/trace_*.txt. */
    {
        const char *pt = getenv("CASTLE_PTRACE");
        if (pt) {
            const char *mv = getenv("CASTLE_MOVES");
            FILE *f = fopen(pt, "w");
            int n;
            if (!mv || !f) {
                fprintf(stderr, "CASTLE_PTRACE necesita CASTLE_MOVES\n");
                if (f) fclose(f);
                free(rom_buf);
                return 1;
            }
            g_rom = rom_buf; g_rom_size = rom_size;
            rl_reset();
            rl_load_room(0x70u);
            /* alineación con el oráculo: el EAC9 real al iniciar las trazas
             * cumple EAC9 ≡ 2 (mod 4) — afecta paridad Y bit1 (ciclo anim) */
            rl_ram_wb(0xEAC9u, 2u);
            g_plr_px = ((int)rl_ram_rb(0xE334u) + 1) * 8;
            g_plr_py = ((int)rl_ram_rb(0xE335u) + 4) * 8 - 1;
            g_plr_frame = 0;
            n = (int)strlen(mv);
            for (int i = 0; i <= n; i++) {
                uint8_t fc = rl_ram_rb(0xEAC9u);
                uint8_t cb = rl_ram_rb(0xEACBu), cc = rl_ram_rb(0xEACCu);
                uint8_t stick = 0, trig = 0;
                char m;
                if ((fc & 1u) == 0u) { cb = 0u; cc = 0u; }   /* vista post-clear */
                fprintf(f, "%d %d %d %d %d %d %d %d %d\n", i, g_plr_py, g_plr_px,
                        g_plr_frame * 12, rl_ram_rb(0xEAD6u), cb, cc,
                        rl_ram_rb(0xE334u), rl_ram_rb(0xE335u));
                if (i == n) break;
                m = mv[i];
                switch (m) {
                    case 'R': stick = 3u; break;
                    case 'L': stick = 7u; break;
                    case 'U': trig = 1u; break;
                    case 'D': stick = 3u; trig = 1u; break;
                    case 'A': stick = 7u; trig = 1u; break;
                    case 'W': stick = 1u; break;
                    case 'S': stick = 5u; break;
                    default: break;
                }
                player_frame(stick, trig);
            }
            fclose(f);
            printf("CASTLE_PTRACE: %d lineas -> %s\n", n + 1, pt);
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
                /* captura animada con el JUGADOR REAL (player.c) + enemigos:
                 * CASTLE_MOVES (R/L/U/D/A/W/S/.) un char por frame,
                 * CASTLE_GIVEKEYS=1 da 9 llaves de cada color (en la RAM real:
                 * abre puertas vía rl_door_press), CASTLE_FRAMES=N. */
                int nframes = 40;
                const char *nf = getenv("CASTLE_FRAMES");
                const char *mv = getenv("CASTLE_MOVES");
                if (nf) nframes = atoi(nf);
                rl_reset();
                rl_load_room(room);
                player_sync_pixel();
                enemies_room_init(room);
                keys_room_init(room);
                items_room_init(room);
                if (getenv("CASTLE_GIVEKEYS")) {
                    for (int c = 0; c < KEY_COLORS; c++)
                        rl_ram_wb((uint16_t)(0xE337u + c), 9u);
                    rl_keys_hud_redraw();
                }
                g_actors_on = 1;
                for (int f = 0; f < nframes; f++) {
                    uint8_t stick = 0u, trig = 0u;
                    char m = (mv && f < (int)strlen(mv)) ? mv[f] : '.';
                    switch (m) {
                        case 'R': stick = 3u; break;
                        case 'L': stick = 7u; break;
                        case 'U': trig = 1u; break;
                        case 'D': stick = 3u; trig = 1u; break;
                        case 'A': stick = 7u; trig = 1u; break;
                        case 'W': stick = 1u; break;
                        case 'S': stick = 5u; break;
                        default: break;
                    }
                    player_frame(stick, trig);
                    {
                        uint8_t edge = player_take_exit();
                        if (edge) {
                            rl_room_exit(edge);
                            room = rl_ram_rb(0xE320u);
                            geom_decode_room(room);
                            rl_load_room(room);
                            player_sync_pixel();
                            enemies_room_init(room);
                            keys_room_init(room);
                            items_room_init(room);
                            printf(">>> f%02d transicion a sala 0x%02X\n", f, room);
                        }
                    }
                    enemies_step();
                    keys_update(g_plr_px, g_plr_py, 16, 16);
                    items_update(g_plr_px, g_plr_py, 16, 16);
                    hal_vdp_present();
                    {
                        char p[256];
                        snprintf(p, sizeof(p), "%s_%02d.bmp", shot, f);
                        hal_screenshot(p);
                    }
                    if (getenv("CASTLE_ENEMYDBG"))
                        printf("f%02d px=%d py=%d fase=%d col=%d fila=%d inv=%d,%d,%d,%d,%d,%d\n",
                               f, g_plr_px, g_plr_py, rl_ram_rb(0xEAD6u),
                               rl_ram_rb(0xE334u), rl_ram_rb(0xE335u),
                               rl_ram_rb(0xE337u), rl_ram_rb(0xE338u), rl_ram_rb(0xE339u),
                               rl_ram_rb(0xE33Au), rl_ram_rb(0xE33Bu), rl_ram_rb(0xE33Cu));
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
