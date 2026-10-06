#include "ui.h"
#include "gfx.h"
#include <stdio.h>
#include <math.h>

Ui U;

enum { ST_TITLE, ST_MAP, ST_PLAY, ST_PAUSE, ST_CONTROLS, ST_END };

#define PROGRESS_MAGIC 0x46575031u

typedef struct {
    u32 magic;
    s32 unlocked;          /* highest region index unlocked */
    u8 done[REGION_COUNT];
    s8 saved_region;       /* region with an in-progress save, -1 none */
    u8 tutorial_done;
    u8 pad[2];
} Progress;

static Progress P;
static int state = ST_TITLE;
static u32 prev_buttons, pressed, held;
static int rep_t[4];
static float ana_x, ana_y;
static int map_sel;
static int pause_sel;
static int frame;
static int end_new_region;          /* region unlocked by this win, -1 none */

/* ================================================================ save / load */
static void save_progress(void) { plat_save("progress.sav", &P, sizeof(P)); }

static void load_progress(void)
{
    if (!plat_load("progress.sav", &P, sizeof(P)) || P.magic != PROGRESS_MAGIC) {
        memset(&P, 0, sizeof(P));
        P.magic = PROGRESS_MAGIC;
        P.saved_region = -1;
    }
}

static int save_world(void)
{
    if (!plat_save("world.sav", &W, sizeof(W))) return 0;
    P.saved_region = (s8)W.region;
    save_progress();
    return 1;
}

static int load_world(int region)
{
    if (P.saved_region != region) return 0;
    if (!plat_load("world.sav", &W, sizeof(W))) return 0;
    if (W.magic != WORLD_MAGIC || W.region != region) return 0;
    return 1;
}

static void clear_world_save(void)
{
    plat_delete("world.sav");
    P.saved_region = -1;
    save_progress();
}

/* ================================================================ helpers */
static void toast(const char *m)
{
    snprintf(U.msg, sizeof(U.msg), "%s", m);
    U.msg_t = 2.0f;
}

static int has_bld(int type, int need_ammo)
{
    for (int i = 0; i < W.nbld; i++) {
        Building *b = &W.blds[i];
        if (!b->alive || b->type != type) continue;
        if (!need_ammo || b->inv[bld_defs[type].ammo]) return 1;
    }
    return 0;
}

static void setup_ui_for_world(int fresh)
{
    U.ntools = 0;
    for (int t = 1; t < B_COUNT; t++)
        if (bld_defs[t].unlock <= W.region) U.tools[U.ntools++] = t;
    U.sel = 0;
    U.rot = 0;
    U.info = 0;
    U.speed = 1;
    U.msg_t = 0;
    U.banner_t = 0;
    U.last_wave = W.wave_next;
    U.cur_x = W.keep_x + 1;
    U.cur_y = W.keep_y + 4;
    U.cam_x = U.cur_x * TILE - SCREEN_W / 2;
    U.cam_y = U.cur_y * TILE - SCREEN_H / 2;
    U.tip = (W.region == 0 && !P.tutorial_done) ? 0 : -1;
    (void)fresh;
}

static void start_region(int region, int fresh)
{
    if (fresh || !load_world(region)) {
        world_new(region);
        if (P.saved_region == region) clear_world_save();
        fresh = 1;
    }
    setup_ui_for_world(fresh);
    state = ST_PLAY;
}

static void clamp_cam(int *cx, int *cy)
{
    int mx = MAP_W * TILE - SCREEN_W, my = MAP_H * TILE - SCREEN_H + 40;
    if (*cx < 0) *cx = 0;
    if (*cy < -18) *cy = -18;
    if (*cx > mx) *cx = mx;
    if (*cy > my) *cy = my;
}

/* d-pad with key repeat; returns direction bits that fire this frame */
static u32 dpad_fire(void)
{
    static const u32 bits[4] = {BTN_RIGHT, BTN_DOWN, BTN_LEFT, BTN_UP};
    u32 out = 0;
    for (int k = 0; k < 4; k++) {
        if (held & bits[k]) {
            rep_t[k]++;
            if (rep_t[k] == 1 || (rep_t[k] > 12 && (rep_t[k] - 12) % 3 == 0)) out |= bits[k];
        } else
            rep_t[k] = 0;
    }
    return out;
}

/* ================================================================ title */
static void update_title(void)
{
    W.anim++;
    if (pressed & (BTN_CROSS | BTN_START)) {
        state = ST_MAP;
        map_sel = P.unlocked < REGION_COUNT ? P.unlocked : REGION_COUNT - 1;
        if (P.saved_region >= 0) map_sel = P.saved_region;
    }
}

static void draw_title(void)
{
    int cx = (int)(380 + sinf(frame * 0.004f) * 140);
    int cy = 250 + (int)(cosf(frame * 0.003f) * 60);
    render_scene(cx, cy);
    gfx_blend(0, 0, SCREEN_W, SCREEN_H, RGB(10, 8, 4), 110);
    const char *t = "FORGEWORKS";
    int w = text_w(t) * 4;
    gfx_text_big((SCREEN_W - w) / 2, 46, t, RGB(255, 176, 40), 4);
    const char *sub = "Build. Automate. Defend the realm.";
    gfx_text_shadow((SCREEN_W - text_w(sub)) / 2, 106, sub, C_YELLOW);
    if ((frame / 30) % 2) {
        const char *p = "Press X to start";
        gfx_text_shadow((SCREEN_W - text_w(p)) / 2, 170, p, C_WHITE);
    }
    gfx_text_shadow(6, SCREEN_H - 14, "v1.0", C_GREY);
    const char *h = "HOME to quit";
    gfx_text_shadow(SCREEN_W - text_w(h) - 6, SCREEN_H - 14, h, C_GREY);
}

/* ================================================================ world map */
static const u32 theme_land[5] = {RGB(110, 150, 70), RGB(140, 146, 90), RGB(80, 110, 76), RGB(226, 234, 240), RGB(104, 90, 128)};

static void update_map(void)
{
    if (pressed & (BTN_RIGHT | BTN_DOWN)) map_sel = (map_sel + 1) % REGION_COUNT;
    if (pressed & (BTN_LEFT | BTN_UP)) map_sel = (map_sel + REGION_COUNT - 1) % REGION_COUNT;
    if (pressed & BTN_CIRCLE) { world_new(0); state = ST_TITLE; }
    if (map_sel <= P.unlocked) {
        if (pressed & BTN_CROSS) start_region(map_sel, 0);
        if ((pressed & BTN_TRIANGLE) && P.saved_region == map_sel) start_region(map_sel, 1);
    }
}

static void draw_map(void)
{
    char buf[64];
    gfx_clear(RGB(40, 30, 20));
    gfx_fill(8, 8, SCREEN_W - 16, 190, RGB(214, 192, 142));
    gfx_rect(8, 8, SCREEN_W - 16, 190, RGB(120, 90, 50));
    gfx_rect(10, 10, SCREEN_W - 20, 186, RGB(160, 130, 80));
    /* sea texture */
    for (int y = 14; y < 194; y += 9)
        for (int x = 16 + (y % 18); x < SCREEN_W - 16; x += 18) gfx_fill(x, y, 4, 1, RGB(196, 172, 122));
    /* land blobs */
    for (int r = 0; r < REGION_COUNT; r++) {
        const RegionDef *R = &region_defs[r];
        u32 c = theme_land[R->theme];
        for (int k = 0; k < 7; k++) {
            int ox = (int)(cosf(k * 0.9f + r) * 22), oy = (int)(sinf(k * 1.3f + r * 2) * 14);
            gfx_disc(R->mx + ox, R->my + oy, 20 - k, RGB(150, 128, 88));
        }
        for (int k = 0; k < 7; k++) {
            int ox = (int)(cosf(k * 0.9f + r) * 22), oy = (int)(sinf(k * 1.3f + r * 2) * 14);
            gfx_disc(R->mx + ox, R->my + oy, 18 - k, c);
        }
    }
    /* dotted roads */
    for (int r = 0; r + 1 < REGION_COUNT; r++) {
        int x0 = region_defs[r].mx, y0 = region_defs[r].my, x1 = region_defs[r + 1].mx, y1 = region_defs[r + 1].my;
        for (int s = 0; s <= 20; s++) {
            if (s % 2) continue;
            int x = x0 + (x1 - x0) * s / 20, y = y0 + (y1 - y0) * s / 20;
            gfx_fill(x - 1, y - 1, 3, 3, r < P.unlocked ? RGB(120, 60, 30) : RGB(120, 110, 90));
        }
    }
    /* nodes */
    for (int r = 0; r < REGION_COUNT; r++) {
        const RegionDef *R = &region_defs[r];
        u32 c = P.done[r] ? RGB(232, 192, 64) : (r <= P.unlocked ? RGB(200, 50, 40) : RGB(120, 116, 108));
        gfx_disc(R->mx, R->my, 9, RGB(30, 20, 10));
        gfx_disc(R->mx, R->my, 7, c);
        if (P.done[r]) {
            gfx_line(R->mx - 3, R->my, R->mx - 1, R->my + 3, C_WHITE);
            gfx_line(R->mx - 1, R->my + 3, R->mx + 4, R->my - 3, C_WHITE);
        } else if (r > P.unlocked) {
            gfx_fill(R->mx - 2, R->my - 1, 5, 4, RGB(60, 56, 50));
            gfx_rect(R->mx - 2, R->my - 4, 5, 4, RGB(60, 56, 50));
        } else {
            sprintf(buf, "%d", r + 1);
            gfx_text(R->mx - 2, R->my - 5, buf, C_WHITE);
        }
        if (P.saved_region == r) gfx_text_shadow(R->mx + 10, R->my - 16, "*", C_YELLOW);
        if (r == map_sel) {
            int pr = 12 + (frame / 8) % 3;
            gfx_circle(R->mx, R->my, pr, C_YELLOW);
            gfx_circle(R->mx, R->my, pr + 1, RGB(120, 90, 0));
        }
    }
    gfx_text_big(24, 18, "The Realm", RGB(120, 60, 20), 2);

    /* info panel */
    const RegionDef *R = &region_defs[map_sel];
    gfx_panel(8, 202, SCREEN_W - 16, 64);
    sprintf(buf, "%d. %s", map_sel + 1, R->name);
    gfx_text_shadow(16, 207, buf, C_ORANGE);
    if (map_sel > P.unlocked) {
        gfx_text_shadow(16, 222, "Locked - secure the previous region first.", C_GREY);
    } else {
        for (int i = 0; i < 3; i++)
            if (R->blurb[i][0]) gfx_text_shadow(16, 220 + i * 11, R->blurb[i], C_WHITE);
        int gx = 300;
        gfx_text_shadow(gx, 207, "Deliver:", C_ORANGE);
        int ly = 220;
        for (int g = 0; g < 2; g++) {
            if (!R->goals[g].n) continue;
            gfx_sprite(item_icon(R->goals[g].item), gx, ly + 1, 0);
            sprintf(buf, "%d %s", R->goals[g].n, item_defs[R->goals[g].item].name);
            gfx_text_shadow(gx + 11, ly, buf, C_YELLOW);
            ly += 11;
        }
        sprintf(buf, "Survive %d raids", R->num_waves);
        gfx_sprite(SPR_UI_SWORDS, gx, ly + 1, 0);
        gfx_text_shadow(gx + 11, ly, buf, C_YELLOW);
        const char *act = P.saved_region == map_sel ? "X: Continue   /\\: Start over" : (P.done[map_sel] ? "X: Replay" : "X: Begin");
        gfx_text(SCREEN_W - 20 - text_w(act), 16, act, RGB(90, 50, 15));
    }
    gfx_text(SCREEN_W - 20 - text_w("O: Title"), 180, "O: Title", RGB(90, 50, 15));
}

/* ================================================================ gameplay */
static const char *why_text(int why, int type)
{
    static char buf[48];
    switch (why) {
    case WHY_BLOCKED: return "Something is already there.";
    case WHY_WATER: return "Can't build on water.";
    case WHY_CAMP: return "That's the raiders' camp!";
    case WHY_TREE: return "A tree is in the way. Use a Woodcutter.";
    case WHY_NEED_RES:
        if (type == B_MINE) return "Mines go on rocks, ore or clay.";
        if (type == B_WOODCUTTER) return "Woodcutters go on trees.";
        return "Herb gardens go on herb patches.";
    case WHY_COST: {
        const BldDef *d = &bld_defs[type];
        for (int k = 0; k < 2; k++)
            if (d->cost[k].n && W.stock[d->cost[k].item] < d->cost[k].n) {
                snprintf(buf, sizeof(buf), "Not enough %s.", item_defs[d->cost[k].item].name);
                return buf;
            }
        return "Can't afford that.";
    }
    }
    return "";
}

static void try_place(int x, int y, int dir, int quiet)
{
    int type = U.tools[U.sel], why;
    if (!can_place(type, x, y, &why)) {
        if (!quiet && why != WHY_BLOCKED) toast(why_text(why, type));
        return;
    }
    place_building(type, x, y, dir);
}

static int tutorial_cond(int step)
{
    switch (step) {
    case 0: return has_bld(B_MINE, 0);
    case 1: return W.delivered[IT_COPPER_ORE] + W.delivered[IT_TIN_ORE] > 0 || has_bld(B_SMELTER, 0);
    case 2: return has_bld(B_SMELTER, 0);
    case 3: return W.delivered[IT_BRONZE] > 0;
    case 4: return has_bld(B_ARCHER, 0);
    case 5: return has_bld(B_ARCHER, 1);
    }
    return 0;
}

static void update_tutorial(void)
{
    /* advance past every step whose goal (or a later step's goal) is already met */
    int reached = -1;
    for (int j = U.tip; j < 6; j++)
        if (tutorial_cond(j)) reached = j;
    if (reached < 0) return;
    U.tip = reached + 1;
    if (!tip_text(U.tip)) {
        U.tip = -1;
        P.tutorial_done = 1;
        save_progress();
        toast("Tutorial complete. Good luck!");
    }
}

static void update_play(void)
{
    if (pressed & BTN_START) { state = ST_PAUSE; pause_sel = 0; return; }
    if (pressed & BTN_SELECT) { U.speed = U.speed == 1 ? 2 : 1; toast(U.speed == 2 ? "Speed 2x" : "Speed 1x"); }
    if (pressed & BTN_L) U.sel = (U.sel + U.ntools - 1) % U.ntools;
    if (pressed & BTN_R) U.sel = (U.sel + 1) % U.ntools;
    if (pressed & BTN_TRIANGLE) U.info = !U.info;

    int ox = U.cur_x, oy = U.cur_y;
    u32 fire = dpad_fire();
    int mdir = -1;
    if (fire & BTN_RIGHT) { U.cur_x++; mdir = 0; }
    else if (fire & BTN_LEFT) { U.cur_x--; mdir = 2; }
    else if (fire & BTN_DOWN) { U.cur_y++; mdir = 1; }
    else if (fire & BTN_UP) { U.cur_y--; mdir = 3; }
    /* analog nub: fast cursor */
    if (mdir < 0) {
        if (ana_x > 1.0f) { U.cur_x++; ana_x -= 1.0f; mdir = 0; }
        else if (ana_x < -1.0f) { U.cur_x--; ana_x += 1.0f; mdir = 2; }
        else if (ana_y > 1.0f) { U.cur_y++; ana_y -= 1.0f; mdir = 1; }
        else if (ana_y < -1.0f) { U.cur_y--; ana_y += 1.0f; mdir = 3; }
    }
    if (U.cur_x < 0) U.cur_x = 0;
    if (U.cur_y < 0) U.cur_y = 0;
    if (U.cur_x >= MAP_W) U.cur_x = MAP_W - 1;
    if (U.cur_y >= MAP_H) U.cur_y = MAP_H - 1;
    int moved = (U.cur_x != ox || U.cur_y != oy);

    if (pressed & BTN_SQUARE) {
        U.rot = (U.rot + 1) & 3;
        int b = bld_at(U.cur_x, U.cur_y);
        if (b >= 0 && W.blds[b].type == B_TRACK) W.blds[b].dir = (u8)U.rot;
    }

    int type = U.tools[U.sel];
    if (pressed & BTN_CROSS) try_place(U.cur_x, U.cur_y, U.rot, 0);
    else if ((held & BTN_CROSS) && moved) {
        if (type == B_TRACK && mdir >= 0) {
            U.rot = mdir;
            int pb = bld_at(ox, oy);
            if (pb >= 0 && W.blds[pb].type == B_TRACK) W.blds[pb].dir = (u8)mdir;
        }
        try_place(U.cur_x, U.cur_y, U.rot, 1);
    }
    if ((pressed & BTN_CIRCLE) || ((held & BTN_CIRCLE) && moved)) {
        int b = bld_at(U.cur_x, U.cur_y);
        if (b >= 0 && W.blds[b].type != B_KEEP) remove_building(b, 1);
        else if (b >= 0 && (pressed & BTN_CIRCLE)) toast("The Keep can't be removed.");
    }

    /* camera follows cursor */
    int px = U.cur_x * TILE + 10, py = U.cur_y * TILE + 10;
    int tx = U.cam_x, ty = U.cam_y;
    if (px - tx < 70) tx = px - 70;
    if (px - tx > SCREEN_W - 70) tx = px - (SCREEN_W - 70);
    if (py - ty < 60) ty = py - 60;
    if (py - ty > SCREEN_H - 90) ty = py - (SCREEN_H - 90);
    clamp_cam(&tx, &ty);
    U.cam_x += (tx - U.cam_x + (tx > U.cam_x ? 3 : 0)) / 4;
    U.cam_y += (ty - U.cam_y + (ty > U.cam_y ? 3 : 0)) / 4;

    for (int s = 0; s < U.speed; s++) world_update(DT);
    if (U.msg_t > 0) U.msg_t -= DT;
    if (U.banner_t > 0) U.banner_t -= DT;
    if (W.wave_next != U.last_wave) { U.last_wave = W.wave_next; U.banner_t = 2.5f; }
    if (U.tip >= 0) update_tutorial();

    if (W.won || W.lost) {
        end_new_region = -1;
        if (W.won) {
            P.done[W.region] = 1;
            if (W.region + 1 < REGION_COUNT && P.unlocked < W.region + 1) {
                P.unlocked = W.region + 1;
                end_new_region = W.region + 1;
            }
            if (P.saved_region == W.region) P.saved_region = -1;
            plat_delete("world.sav");
            save_progress();
        }
        state = ST_END;
    }
}

static void draw_play(void)
{
    render_world();
    render_hud();
}

/* ================================================================ pause */
static const char *pause_items[] = {"Resume", "Controls", "Save & exit to map", "Restart region", "Exit without saving"};

static void update_pause(void)
{
    if (pressed & BTN_DOWN) pause_sel = (pause_sel + 1) % 5;
    if (pressed & BTN_UP) pause_sel = (pause_sel + 4) % 5;
    if (pressed & (BTN_START | BTN_CIRCLE)) { state = ST_PLAY; return; }
    if (pressed & BTN_CROSS) {
        switch (pause_sel) {
        case 0: state = ST_PLAY; break;
        case 1: state = ST_CONTROLS; break;
        case 2:
            if (!save_world()) toast("Save failed!");
            map_sel = W.region;
            state = ST_MAP;
            break;
        case 3: start_region(W.region, 1); break;
        case 4: map_sel = W.region; state = ST_MAP; break;
        }
    }
}

static void draw_pause(void)
{
    render_world();
    render_hud();
    gfx_blend(0, 0, SCREEN_W, SCREEN_H, RGB(0, 0, 0), 120);
    int w = 200, h = 108, x = (SCREEN_W - w) / 2, y = 70;
    gfx_panel(x, y, w, h);
    gfx_text_shadow(x + (w - text_w("Paused")) / 2, y + 6, "Paused", C_ORANGE);
    for (int i = 0; i < 5; i++) {
        u32 c = i == pause_sel ? C_YELLOW : C_WHITE;
        if (i == pause_sel) gfx_text_shadow(x + 14, y + 24 + i * 15, ">", C_YELLOW);
        gfx_text_shadow(x + 26, y + 24 + i * 15, pause_items[i], c);
    }
}

static void update_controls(void)
{
    if (pressed & (BTN_CROSS | BTN_CIRCLE | BTN_START)) state = ST_PAUSE;
}

static void draw_controls(void)
{
    static const char *lines[][2] = {
        {"D-pad / nub", "Move the cursor"},
        {"L / R", "Choose a building"},
        {"X", "Build (hold + move to paint)"},
        {"O", "Remove (hold + move), full refund"},
        {"[]  Square", "Rotate track direction"},
        {"/\\ Triangle", "Info: building status / recipes"},
        {"SELECT", "Toggle 2x speed"},
        {"START", "Pause menu"},
    };
    render_world();
    gfx_blend(0, 0, SCREEN_W, SCREEN_H, RGB(0, 0, 0), 140);
    int w = 340, h = 160, x = (SCREEN_W - w) / 2, y = 50;
    gfx_panel(x, y, w, h);
    gfx_text_shadow(x + 10, y + 6, "Controls", C_ORANGE);
    for (int i = 0; i < 8; i++) {
        gfx_text_shadow(x + 14, y + 24 + i * 14, lines[i][0], C_YELLOW);
        gfx_text_shadow(x + 110, y + 24 + i * 14, lines[i][1], C_WHITE);
    }
    gfx_text_shadow(x + 10, y + h - 16, "Items flow out of buildings into touching tracks.", C_GREY);
}

/* ================================================================ victory / defeat */
static void update_end(void)
{
    if (W.won) {
        world_update(DT);
        if (pressed & BTN_CROSS) {
            map_sel = end_new_region >= 0 ? end_new_region : W.region;
            state = ST_MAP;
        }
    } else {
        if (pressed & BTN_CROSS) start_region(W.region, 1);
        if (pressed & BTN_CIRCLE) { map_sel = W.region; state = ST_MAP; }
    }
}

static void draw_end(void)
{
    char buf[64];
    render_world();
    render_hud();
    gfx_blend(0, 0, SCREEN_W, SCREEN_H, RGB(0, 0, 0), 120);
    int w = 330, h = 120, x = (SCREEN_W - w) / 2, y = 60;
    gfx_panel(x, y, w, h);
    if (W.won) {
        int final = W.region == REGION_COUNT - 1;
        const char *t = final ? "The realm is safe!" : "Region secured!";
        gfx_text_big(x + (w - text_w(t) * 2) / 2, y + 10, t, C_YELLOW, 2);
        int ly = y + 42;
        if (final) {
            gfx_text_shadow(x + 14, ly, "You conquered every region. Thanks for playing!", C_WHITE);
            ly += 14;
        }
        if (end_new_region >= 0) {
            sprintf(buf, "Unlocked: %s", region_defs[end_new_region].name);
            gfx_text_shadow(x + 14, ly, buf, C_GREEN);
            ly += 14;
            int bx = x + 14;
            gfx_text_shadow(bx, ly, "New:", C_ORANGE);
            bx += 30;
            for (int t2 = 0; t2 < B_COUNT; t2++)
                if (bld_defs[t2].unlock == end_new_region) {
                    if (bx + text_w(bld_defs[t2].name) > x + w - 10) { ly += 12; bx = x + 44; }
                    bx += gfx_text_shadow(bx, ly, bld_defs[t2].name, C_WHITE) + 8;
                }
        }
        gfx_text_shadow(x + 14, y + h - 18, "X: Back to the world map", C_YELLOW);
    } else {
        const char *t = "The Keep has fallen";
        gfx_text_big(x + (w - text_w(t) * 2) / 2, y + 10, t, C_RED, 2);
        gfx_text_shadow(x + 14, y + 46, "Tip: walls slow raiders, towers need a steady", C_WHITE);
        gfx_text_shadow(x + 14, y + 58, "supply of ammo from your factory.", C_WHITE);
        gfx_text_shadow(x + 14, y + h - 18, "X: Try again    O: World map", C_YELLOW);
    }
}

/* used by the desktop test harness only */
void debug_unlock_all(void)
{
    P.unlocked = REGION_COUNT - 1;
}

/* ================================================================ entry points */
void game_init(void)
{
    load_progress();
    world_new(0);
    U.cam_x = 0;
    U.cam_y = 0;
    state = ST_TITLE;
}

void game_frame(u32 buttons, int ax, int ay, u32 *pixels)
{
    gfx_set_target(pixels);
    held = buttons;
    pressed = buttons & ~prev_buttons;
    prev_buttons = buttons;
    frame++;

    /* analog: deadzone, then accumulate tiles */
    float fx = 0, fy = 0;
    if (ax > 35 || ax < -35) fx = (ax - (ax > 0 ? 35 : -35)) / 93.0f;
    if (ay > 35 || ay < -35) fy = (ay - (ay > 0 ? 35 : -35)) / 93.0f;
    if (fx == 0) ana_x = 0; else ana_x += fx * 0.3f;
    if (fy == 0) ana_y = 0; else ana_y += fy * 0.3f;

    switch (state) {
    case ST_TITLE: update_title(); break;
    case ST_MAP: update_map(); break;
    case ST_PLAY: update_play(); break;
    case ST_PAUSE: update_pause(); break;
    case ST_CONTROLS: update_controls(); break;
    case ST_END: update_end(); break;
    }
    switch (state) {
    case ST_TITLE: draw_title(); break;
    case ST_MAP: draw_map(); break;
    case ST_PLAY: draw_play(); break;
    case ST_PAUSE: draw_pause(); break;
    case ST_CONTROLS: draw_controls(); break;
    case ST_END: draw_end(); break;
    }
}
