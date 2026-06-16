/*
 * THE CASTLE — Cheats de QA (sólo en el juego interactivo, faithful_play)
 * =======================================================================
 * Controles (teclas de función, no son parte del MSX):
 *   F5  god mode ON/OFF — invulnerable (power-up rojo permanente) + vidas
 *       siempre al máximo (nunca game over; caer a un pozo te respawnea).
 *   F6  dar 9 llaves de cada color (abre cualquier puerta) + redibuja HUD.
 *   F7  dar el mapa (dibuja el minimapa completo).
 *   F8  sala ANTERIOR / F9 sala SIGUIENTE en el barrido 00..99 — teletransporta
 *       ignorando paredes y conexiones (para recorrer las 100 salas). El
 *       jugador aparece en una celda libre con piso.
 *
 * Todo opera sobre el espejo de RAM real (mismas celdas que el juego), así
 * que las llaves, vidas y mapa son indistinguibles de los reales.
 */
#include <stdint.h>
#include <stdio.h>
#include "hal.h"
#include "game.h"
#include "geom.h"
#include "room_loader.h"
#include "player.h"

static int     g_god = 0;        /* god mode activo */
static uint8_t g_prev_keys = 0;  /* máscara del frame anterior (edge detect) */

static uint8_t cm(uint8_t b, uint8_t c)
{
    return rl_ram_rb((uint16_t)(0xE496u + (uint16_t)c * 30u + b));
}

/* ¿el 2x2 en (b,c) está libre de sólidos (& 0x30)? */
static int cell_open(uint8_t b, uint8_t c)
{
    for (int db = 0; db < 2; db++)
        for (int dc = 0; dc < 2; dc++)
            if (cm((uint8_t)(b + db), (uint8_t)(c + dc)) & 0x30u) return 0;
    return 1;
}

/* celda de spawn: 1ª opción = 2x2 libre con PISO debajo, centrada (igual
 * criterio que los fixtures pick_cell); fallback = cualquier 2x2 libre
 * (god mode te deja caer/flotar sin morir). Devuelve 1 y (*b,*c). */
static int find_free_cell(uint8_t *bo, uint8_t *co)
{
    for (int c = 17; c >= 1; c--) {
        int best = 999, found = 0;
        for (int b = 3; b < 26; b++) {
            if (!cell_open((uint8_t)b, (uint8_t)c)) continue;
            if (!(cm((uint8_t)b, (uint8_t)(c + 2)) & 0x40u)) continue;
            if (!(cm((uint8_t)(b + 1), (uint8_t)(c + 2)) & 0x40u)) continue;
            { int d = (b > 14) ? (b - 14) : (14 - b);
              if (d < best) { best = d; *bo = (uint8_t)b; *co = (uint8_t)c; found = 1; } }
        }
        if (found) return 1;
    }
    for (int c = 1; c < 18; c++)                   /* fallback: sin piso */
        for (int b = 3; b < 26; b++)
            if (cell_open((uint8_t)b, (uint8_t)c)) { *bo = (uint8_t)b; *co = (uint8_t)c; return 1; }
    return 0;
}

/* idx 0..99 (fila*10+col) -> sala BCD (hi=fila, lo=col) */
static uint8_t idx_to_bcd(int idx)
{
    idx = ((idx % 100) + 100) % 100;
    return (uint8_t)(((idx / 10) << 4) | (idx % 10));
}
static int bcd_to_idx(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0Fu);
}

/* Teletransporta a la sala `room` y reposiciona al jugador en celda libre.
 * Reusa la misma secuencia de carga que faithful_play. */
static void teleport(uint8_t room)
{
    uint8_t b, c;
    geom_decode_room(room);
    rl_ram_wb(0xE320u, room);
    rl_load_room(room);
    if (find_free_cell(&b, &c)) {
        rl_ram_wb(0xE334u, b);
        rl_ram_wb(0xE335u, c);
    }
    player_sync_pixel();
    player_room_enter();
    music_room_start();
    printf("[cheat] sala 0x%02X%s\n", room, g_god ? "  (god ON)" : "");
}

/* Llamar al principio de cada frame de faithful_play. Actualiza *room si
 * hubo teletransporte. Devuelve 1 si teletransportó (el caller debe saltar
 * el resto del frame, como una transición). */
int cheats_frame(uint8_t *room)
{
    uint8_t k = hal_cheat_keys();
    uint8_t edge = (uint8_t)(k & ~g_prev_keys);   /* recién apretadas */
    int teleported = 0;
    g_prev_keys = k;

    if (edge & 0x01u) {                            /* F5: god toggle */
        g_god = !g_god;
        if (!g_god) rl_ram_wb(0xE343u, 0u);        /* quitar tinte invuln */
        printf("[cheat] god mode %s\n", g_god ? "ON" : "OFF");
    }
    if (g_god) {                                   /* cada frame */
        rl_ram_wb(0xE324u, 9u);                    /* vidas activas */
        rl_ram_wb(0xE336u, 9u);                    /* vidas comprometidas */
        if (rl_ram_rb(0xE343u) < 3u) rl_ram_wb(0xE343u, 0x0Au);  /* invuln */
    }
    if (edge & 0x02u) {                            /* F6: todas las llaves */
        for (int i = 0; i < 6; i++) rl_ram_wb((uint16_t)(0xE337u + i), 9u);
        rl_keys_hud_redraw();
        printf("[cheat] 9 llaves de cada color\n");
    }
    if (edge & 0x04u) {                            /* F7: dar el mapa */
        rl_ram_wb(0xE321u, (uint8_t)(rl_ram_rb(0xE321u) | 0x08u));
        minimap_draw_full();
        printf("[cheat] mapa dado\n");
    }
    if (edge & 0x08u) {                            /* F8: sala anterior */
        teleport(idx_to_bcd(bcd_to_idx(*room) - 1));
        *room = rl_ram_rb(0xE320u);
        teleported = 1;
    } else if (edge & 0x10u) {                     /* F9: sala siguiente */
        teleport(idx_to_bcd(bcd_to_idx(*room) + 1));
        *room = rl_ram_rb(0xE320u);
        teleported = 1;
    }
    return teleported;
}
