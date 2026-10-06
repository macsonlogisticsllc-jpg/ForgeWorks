/*
 * Headless test harness (desktop only, not part of the PSP build).
 * Runs the real game code, driven by a script, and writes screenshots.
 *
 *   frames N              run N frames with no input
 *   press A B ..          press buttons for one frame (then release one frame)
 *   hold A B .. N         hold buttons for N frames
 *   shot file.ppm         save the current screen
 *   cursor X Y            move the build cursor
 *   tool NAME             select a building in the toolbar by type index
 *   place TYPE X Y DIR    place a building directly (pays cost)
 *   stock ITEM N          set keep stock
 *   print                 print state summary
 */
#include "../src/ui.h"
#include "../src/gfx.h"
#include <stdio.h>
#include <stdlib.h>

static u32 fb[FB_STRIDE * SCREEN_H];

int plat_save(const char *name, const void *data, int size)
{
    char p[256];
    snprintf(p, sizeof(p), "/tmp/fw_%s", name);
    FILE *f = fopen(p, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, size, f) == (size_t)size;
    fclose(f);
    return ok;
}

int plat_load(const char *name, void *data, int size)
{
    char p[256];
    snprintf(p, sizeof(p), "/tmp/fw_%s", name);
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    int ok = fread(data, 1, size, f) == (size_t)size;
    fclose(f);
    return ok;
}

void plat_delete(const char *name)
{
    char p[256];
    snprintf(p, sizeof(p), "/tmp/fw_%s", name);
    remove(p);
}

static void shot(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
    for (int y = 0; y < SCREEN_H; y++)
        for (int x = 0; x < SCREEN_W; x++) {
            u32 c = fb[y * FB_STRIDE + x];
            unsigned char rgb[3] = {c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF};
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
}

static u32 btn(const char *s)
{
    static const char *n[] = {"UP", "DOWN", "LEFT", "RIGHT", "X", "O", "SQ", "TRI", "L", "R", "START", "SELECT"};
    for (int i = 0; i < 12; i++)
        if (!strcmp(s, n[i])) return 1u << i;
    return 0;
}

static void print_state(void)
{
    printf("t=%.1f region=%d keep_hp=%.0f waves_cleared=%d next=%d active=%d enemies=%d won=%d lost=%d\n", W.time, W.region,
           W.blds[W.keep].hp, W.waves_cleared, W.wave_next, W.wave_active, enemies_alive(), W.won, W.lost);
    printf("  stock:");
    for (int i = 0; i < IT_COUNT; i++)
        if (W.stock[i]) printf(" %s=%d", item_defs[i].name, W.stock[i]);
    printf("\n  delivered:");
    for (int i = 0; i < IT_COUNT; i++)
        if (W.delivered[i]) printf(" %s=%d", item_defs[i].name, W.delivered[i]);
    int nb = 0;
    for (int i = 0; i < W.nbld; i++) nb += W.blds[i].alive;
    printf("\n  buildings=%d cursor=%d,%d tip=%d\n", nb, U.cur_x, U.cur_y, U.tip);
    for (int i = 0; i < MAX_EN; i++)
        if (W.en[i].alive)
            printf("  enemy %d type=%d at tile %d,%d hp=%.0f attacking=%d\n", i, W.en[i].type, (int)(W.en[i].x / TILE),
                   (int)(W.en[i].y / TILE), W.en[i].hp, W.en[i].attacking);
    for (int i = 0; i < W.nbld; i++)
        if (W.blds[i].alive && bld_defs[W.blds[i].type].kind == K_TOWER)
            printf("  tower at %d,%d ammo=%d\n", W.blds[i].x, W.blds[i].y, W.blds[i].inv[bld_defs[W.blds[i].type].ammo]);
}

int main(int argc, char **argv)
{
    FILE *in = argc > 1 ? fopen(argv[1], "r") : stdin;
    if (!in) { perror("script"); return 1; }
    game_init();
    game_frame(0, 0, 0, fb);
    char line[256];
    while (fgets(line, sizeof(line), in)) {
        char *tok[16];
        int n = 0;
        for (char *t = strtok(line, " \t\r\n"); t && n < 16; t = strtok(0, " \t\r\n")) tok[n++] = t;
        if (!n || tok[0][0] == '#') continue;
        if (!strcmp(tok[0], "frames")) {
            int k = atoi(tok[1]);
            for (int i = 0; i < k; i++) game_frame(0, 0, 0, fb);
        } else if (!strcmp(tok[0], "press")) {
            u32 b = 0;
            for (int i = 1; i < n; i++) b |= btn(tok[i]);
            game_frame(b, 0, 0, fb);
            game_frame(0, 0, 0, fb);
        } else if (!strcmp(tok[0], "hold")) {
            u32 b = 0;
            for (int i = 1; i < n - 1; i++) b |= btn(tok[i]);
            int k = atoi(tok[n - 1]);
            for (int i = 0; i < k; i++) game_frame(b, 0, 0, fb);
        } else if (!strcmp(tok[0], "shot")) {
            shot(tok[1]);
        } else if (!strcmp(tok[0], "cursor")) {
            U.cur_x = atoi(tok[1]);
            U.cur_y = atoi(tok[2]);
        } else if (!strcmp(tok[0], "tool")) {
            int t = atoi(tok[1]);
            for (int i = 0; i < U.ntools; i++)
                if (U.tools[i] == t) U.sel = i;
        } else if (!strcmp(tok[0], "place")) {
            int r = place_building(atoi(tok[1]), atoi(tok[2]), atoi(tok[3]), atoi(tok[4]));
            if (r < 0) {
                int why;
                can_place(atoi(tok[1]), atoi(tok[2]), atoi(tok[3]), &why);
                printf("place failed: type %s at %s,%s why=%d\n", tok[1], tok[2], tok[3], why);
            }
        } else if (!strcmp(tok[0], "stock")) {
            W.stock[atoi(tok[1])] = atoi(tok[2]);
        } else if (!strcmp(tok[0], "print")) {
            print_state();
        } else if (!strcmp(tok[0], "findres")) {
            int r = atoi(tok[1]), bx = -1, by = -1, bd = 1 << 30;
            for (int y = 0; y < MAP_H; y++)
                for (int x = 0; x < MAP_W; x++)
                    if (W.tiles[y][x].res == r && W.tiles[y][x].bld < 0) {
                        int d = (x - W.keep_x - 1) * (x - W.keep_x - 1) + (y - W.keep_y - 1) * (y - W.keep_y - 1);
                        if (d < bd) { bd = d; bx = x; by = y; }
                    }
            U.cur_x = bx; U.cur_y = by;
            printf("res %d nearest at %d,%d\n", r, bx, by);
        } else if (!strcmp(tok[0], "deliver")) {
            W.delivered[atoi(tok[1])] = atoi(tok[2]);
        } else if (!strcmp(tok[0], "wavesdone")) {
            W.waves_cleared = W.wave_next = region_defs[W.region].num_waves;
        } else if (!strcmp(tok[0], "unlockall")) {
            debug_unlock_all();
        } else if (!strcmp(tok[0], "dumpmap")) {
            for (int y = 0; y < MAP_H; y++) {
                for (int x = 0; x < MAP_W; x++) {
                    Tile *t = &W.tiles[y][x];
                    char c = '.';
                    if (t->ground == G_WATER) c = '~';
                    else if (t->ground == G_CAMP) c = 'C';
                    else if (t->bld >= 0) c = W.blds[t->bld].type == B_KEEP ? 'K' : '#';
                    else if (t->res) c = "-ctikslahT"[t->res];
                    putchar(c);
                }
                putchar('\n');
            }
        }
    }
    return 0;
}
