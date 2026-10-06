#include "ui.h"
#include "gfx.h"
#include <math.h>
#include <stdio.h>

static int theme(void) { return region_defs[W.region].theme; }

static int tree_sprite(int th, int var)
{
    switch (th) {
    case 0: return (var & 2) ? SPR_TREE_OAK2 : SPR_TREE_OAK;
    case 1: return SPR_TREE_OAK2;
    case 2: return (var % 3 == 0) ? SPR_TREE_OAK2 : SPR_TREE_WILLOW;
    case 3: return SPR_TREE_PINE;
    default: return SPR_TREE_DEAD;
    }
}

static u32 water_col(int th)
{
    static const u32 c[5] = {RGB(44, 84, 130), RGB(42, 80, 124), RGB(40, 72, 82), RGB(84, 120, 150), RGB(26, 20, 40)};
    return c[th];
}

static int is_water(int x, int y)
{
    return tile_ok(x, y) && W.tiles[y][x].ground == G_WATER;
}

/* ---------------------------------------------------------------- ground */
static void draw_ground(int cam_x, int cam_y)
{
    int th = theme();
    int tx0 = cam_x / TILE, ty0 = cam_y / TILE;
    int tx1 = (cam_x + SCREEN_W) / TILE + 1, ty1 = (cam_y + SCREEN_H) / TILE + 1;
    for (int ty = ty0; ty < ty1 && ty < MAP_H; ty++)
        for (int tx = tx0; tx < tx1 && tx < MAP_W; tx++) {
            Tile *t = &W.tiles[ty][tx];
            int sx = tx * TILE - cam_x, sy = ty * TILE - cam_y;
            if (t->ground == G_WATER) {
                int f = ((W.anim / 40) + tx + ty) & 1;
                gfx_sprite(SPR_GROUND_0_0 + th * 5 + 3 + f, sx, sy, 0);
                u32 bank = water_col(th);
                if (!is_water(tx, ty - 1)) gfx_fill(sx, sy, TILE, 2, bank);
                if (!is_water(tx - 1, ty)) gfx_fill(sx, sy, 1, TILE, bank);
                if (!is_water(tx + 1, ty)) gfx_fill(sx + TILE - 1, sy, 1, TILE, bank);
                if (!is_water(tx, ty + 1)) gfx_fill(sx, sy + TILE - 1, TILE, 1, RGB(200, 220, 230));
            } else {
                gfx_sprite(SPR_GROUND_0_0 + th * 5 + (t->var % 3), sx, sy, 0);
            }
        }
}

void render_ground_only(int cam_x, int cam_y) { draw_ground(cam_x, cam_y); }

/* ---------------------------------------------------------------- objects */
static void item_at(int item, int x, int y)
{
    gfx_sprite(item_icon(item), x - 4, y - 5, 0);
}

static void draw_flat(int cam_x, int cam_y)
{
    int frame = ((W.anim * 38) / 60) % 5;
    int tx0 = cam_x / TILE, ty0 = cam_y / TILE;
    int tx1 = (cam_x + SCREEN_W) / TILE + 1, ty1 = (cam_y + SCREEN_H) / TILE + 1;
    for (int ty = ty0; ty < ty1 && ty < MAP_H; ty++)
        for (int tx = tx0; tx < tx1 && tx < MAP_W; tx++) {
            int bi = W.tiles[ty][tx].bld;
            if (bi < 0) continue;
            Building *b = &W.blds[bi];
            int sx = tx * TILE - cam_x, sy = ty * TILE - cam_y;
            if (b->type == B_TRACK) {
                gfx_sprite(SPR_TRACK_0_0 + b->dir * 5 + frame, sx, sy, b->flash > 0 ? SF_FLASH : 0);
                for (int i = 0; i < b->cn; i++) {
                    float o = (b->cpos[i] - 0.5f) * TILE;
                    item_at(b->citem[i], sx + 10 + (int)(DX[b->dir] * o), sy + 10 + (int)(DY[b->dir] * o));
                }
            } else if (b->type == B_JUNCTION) {
                gfx_sprite(SPR_B_JUNCTION, sx, sy, b->flash > 0 ? SF_FLASH : 0);
                for (int k = 0; k < 4; k++)
                    if (b->jitem[k] != IT_NONE) item_at(b->jitem[k], sx + 10 + DX[k] * 3, sy + 10 + DY[k] * 3);
            }
        }
}

static void hp_bar(int x, int y, int w, float frac)
{
    if (frac < 0) frac = 0;
    gfx_fill(x, y, w, 3, RGB(0, 0, 0));
    gfx_fill(x + 1, y + 1, w - 2, 1, RGB(170, 20, 20));
    gfx_fill(x + 1, y + 1, (int)((w - 2) * frac), 1, RGB(40, 220, 40));
}

static void draw_building(Building *b, int cam_x, int cam_y)
{
    const BldDef *d = &bld_defs[b->type];
    int size = b->type == B_KEEP ? 3 : 1;
    int spr = (d->sprite_on >= 0 && b->active) ? d->sprite_on : d->sprite;
    int sx = b->x * TILE - cam_x;
    int sy = (b->y + size) * TILE - spr_h(spr) - cam_y;
    gfx_sprite(spr, sx, sy, b->flash > 0 ? SF_FLASH : 0);
    if (b->type == B_ROUTER && b->ritem != IT_NONE) item_at(b->ritem, sx + 10, sy + 8);
    if (d->kind == K_TOWER && !b->inv[d->ammo] && (W.anim / 20) % 2) {
        gfx_sprite(item_icon(d->ammo), sx + 6, sy - 10, 0);
        gfx_text_shadow(sx + 13, sy - 12, "!", C_RED);
    }
    if (b->hp < d->hp && b->type != B_KEEP) hp_bar(sx + 2, sy - 4, 16, b->hp / d->hp);
}

static void draw_enemy(int i, int cam_x, int cam_y)
{
    Enemy *e = &W.en[i];
    const EnemyDef *d = &enemy_defs[e->type];
    int spr = d->sprite + (e->attacking ? ((W.anim / 8) & 1) : e->anim);
    int w = spr_w(spr), h = spr_h(spr);
    int sx = (int)e->x - w / 2 - cam_x, sy = (int)e->y - h - cam_y;
    if (e->attacking) sx += ((W.anim / 6) & 1) ? 1 : -1;
    gfx_sprite(spr, sx, sy, (e->facing ? SF_FLIP : 0) | (e->flash > 0 ? SF_FLASH : 0));
    if (e->hp < d->hp) hp_bar(sx + w / 2 - 7, sy - 4, 14, e->hp / d->hp);
}

static void draw_objects(int cam_x, int cam_y)
{
    int th = theme();
    int tx0 = cam_x / TILE - 1, ty0 = cam_y / TILE;
    int tx1 = (cam_x + SCREEN_W) / TILE + 2, ty1 = (cam_y + SCREEN_H) / TILE + 3;
    if (tx0 < 0) tx0 = 0;
    for (int ty = ty0; ty < ty1 && ty < MAP_H; ty++) {
        for (int tx = tx0; tx < tx1 && tx < MAP_W; tx++) {
            Tile *t = &W.tiles[ty][tx];
            int sx = tx * TILE - cam_x, sy = ty * TILE - cam_y;
            if (t->ground == G_CAMP) gfx_sprite(SPR_CAMP, sx - 10, sy + 30 - spr_h(SPR_CAMP), 0);
            if (t->bld >= 0) {
                Building *b = &W.blds[t->bld];
                int size = b->type == B_KEEP ? 3 : 1;
                if (b->type != B_TRACK && b->type != B_JUNCTION && b->y + size - 1 == ty && b->x == tx)
                    draw_building(b, cam_x, cam_y);
                continue;
            }
            if (t->res == RES_TREE) {
                int s = tree_sprite(th, t->var);
                gfx_sprite(s, sx, sy + TILE - spr_h(s), (t->var & 1) ? SF_FLIP : 0);
            } else if (t->res != RES_NONE) {
                int s = res_defs[t->res].sprite;
                gfx_sprite(s, sx, sy + TILE - spr_h(s), (t->var & 1) ? SF_FLIP : 0);
            }
        }
        for (int i = 0; i < MAX_EN; i++)
            if (W.en[i].alive && (int)(W.en[i].y / TILE) == ty) draw_enemy(i, cam_x, cam_y);
    }
}

static void draw_projectiles(int cam_x, int cam_y)
{
    for (int i = 0; i < MAX_PROJ; i++) {
        Proj *p = &W.pr[i];
        if (!p->alive) continue;
        float dx = p->lx - p->x, dy = p->ly - p->y, l = sqrtf(dx * dx + dy * dy);
        if (l < 0.01f) l = 1;
        dx /= l; dy /= l;
        int x0 = (int)p->x - cam_x, y0 = (int)p->y - cam_y;
        int len = p->kind ? 7 : 5;
        u32 shaft = p->kind ? RGB(70, 70, 76) : RGB(216, 176, 112);
        gfx_line(x0, y0, x0 - (int)(dx * len), y0 - (int)(dy * len), shaft);
        if (p->kind) gfx_line(x0 + 1, y0, x0 + 1 - (int)(dx * len), y0 - (int)(dy * len), shaft);
        gfx_pixel(x0, y0, RGB(220, 224, 230));
    }
}

static void draw_fx(int cam_x, int cam_y)
{
    for (int i = 0; i < MAX_FX; i++) {
        Fx *f = &W.fx[i];
        if (!f->alive) continue;
        float k = f->t / f->life;
        int x = (int)f->x - cam_x, y = (int)f->y - cam_y;
        switch (f->kind) {
        case FX_PUFF: {
            int r = 2 + (int)(k * 8);
            for (int a = 0; a < 6; a++) {
                float ang = a * 1.047f;
                gfx_fill(x + (int)(cosf(ang) * r) - 1, y + (int)(sinf(ang) * r * 0.7f) - 1, 3, 3,
                         k < 0.5f ? RGB(220, 210, 190) : RGB(140, 132, 120));
            }
            break;
        }
        case FX_RING:
            gfx_circle(x, y, 4 + (int)(k * 60), RGB(150, 120, 255));
            gfx_circle(x, y, 3 + (int)(k * 58), RGB(122, 224, 240));
            break;
        case FX_SMOKE: {
            int s = 2 + (int)(k * 3);
            gfx_blend(x, y, s, s, RGB(120, 116, 110), (int)(160 * (1 - k)));
            break;
        }
        case FX_SPARK:
            gfx_pixel(x, y, C_YELLOW);
            gfx_pixel(x + 2, y - 1, C_ORANGE);
            gfx_pixel(x - 1, y + 2, C_WHITE);
            break;
        }
    }
}

/* ---------------------------------------------------------------- cursor + ghost */
static void draw_cursor(void)
{
    int sx = U.cur_x * TILE - U.cam_x, sy = U.cur_y * TILE - U.cam_y;
    int type = U.tools[U.sel];
    int hover = bld_at(U.cur_x, U.cur_y);
    const BldDef *d = &bld_defs[type];

    if (hover < 0) {
        int why;
        int ok = can_place(type, U.cur_x, U.cur_y, &why);
        int spr = type == B_TRACK ? SPR_TRACK_0_0 + U.rot * 5 : d->sprite;
        int gx = sx, gy = sy + TILE - spr_h(spr);
        gfx_sprite(spr, gx, gy, ok ? SF_GHOST_OK : SF_GHOST_BAD);
        if (d->kind == K_TOWER) gfx_circle(sx + 10, sy + 10, (int)(d->range * TILE), ok ? C_GREEN : C_RED);
    } else {
        Building *b = &W.blds[hover];
        const BldDef *hd = &bld_defs[b->type];
        if (hd->kind == K_TOWER) gfx_circle(b->x * TILE + 10 - U.cam_x, b->y * TILE + 10 - U.cam_y, (int)(hd->range * TILE), C_YELLOW);
    }
    if (type == B_TRACK) {      /* direction arrow for tracks */
        int cx = sx + 10 + DX[U.rot] * 7, cy = sy + 10 + DY[U.rot] * 7;
        gfx_fill(cx - 1, cy - 1, 3, 3, C_YELLOW);
        gfx_line(sx + 10, sy + 10, cx, cy, C_YELLOW);
    }
    u32 c = (W.anim / 15) % 2 ? C_YELLOW : C_WHITE;
    int L = 5;
    gfx_fill(sx - 1, sy - 1, L, 1, c); gfx_fill(sx - 1, sy - 1, 1, L, c);
    gfx_fill(sx + TILE - L + 1, sy - 1, L, 1, c); gfx_fill(sx + TILE, sy - 1, 1, L, c);
    gfx_fill(sx - 1, sy + TILE, L, 1, c); gfx_fill(sx - 1, sy + TILE - L + 1, 1, L, c);
    gfx_fill(sx + TILE - L + 1, sy + TILE, L, 1, c); gfx_fill(sx + TILE, sy + TILE - L + 1, 1, L, c);
}

void render_scene(int cam_x, int cam_y)
{
    draw_ground(cam_x, cam_y);
    draw_flat(cam_x, cam_y);
    draw_objects(cam_x, cam_y);
    draw_projectiles(cam_x, cam_y);
    draw_fx(cam_x, cam_y);
}

void render_world(void)
{
    render_scene(U.cam_x, U.cam_y);
    draw_cursor();
}

/* ---------------------------------------------------------------- HUD */
const char *tip_text(int step)
{
    static const char *tips[] = {
        "Press L/R to pick the Mine, then X on copper rocks.",
        "Lay Track to the Keep: hold X and move. [] rotates.",
        "Mine some tin too (grey rocks), then build a Smelter.",
        "Track copper + tin into the Smelter. Bronze to Keep!",
        "Raiders come from the camps! Build an Archer tower.",
        "Woodcutter on trees > Fletcher > arrows to the tower.",
    };
    return (step >= 0 && step < 6) ? tips[step] : 0;
}

static void fmt_time(char *buf, float t)
{
    int s = (int)(t + 0.99f);
    sprintf(buf, "%d:%02d", s / 60, s % 60);
}

static int draw_cost(int x, int y, int type)
{
    const BldDef *d = &bld_defs[type];
    char buf[16];
    for (int k = 0; k < 2; k++) {
        if (!d->cost[k].n) continue;
        gfx_sprite(item_icon(d->cost[k].item), x, y + 1, 0);
        sprintf(buf, "%d", d->cost[k].n);
        x += 10;
        x += gfx_text_shadow(x, y, buf, W.stock[d->cost[k].item] >= d->cost[k].n ? C_WHITE : C_RED) + 6;
    }
    return x;
}

static void hover_text(void)
{
    char buf[64];
    int bi = bld_at(U.cur_x, U.cur_y);
    Tile *t = tile_at(U.cur_x, U.cur_y);
    buf[0] = 0;
    if (bi >= 0) sprintf(buf, "%s", bld_defs[W.blds[bi].type].name);
    else if (t->ground == G_CAMP) sprintf(buf, "Raider camp");
    else if (t->ground == G_WATER) sprintf(buf, "Water");
    else if (t->res != RES_NONE) sprintf(buf, "%s", res_defs[t->res].name);
    if (buf[0]) gfx_text_shadow(4, 18, buf, C_CYAN);
}

static void info_panel(void)
{
    char buf[64];
    int x = 300, y = 64, w = 176, h = 140;
    gfx_panel_alpha(x, y, w, h, 225);
    int bi = bld_at(U.cur_x, U.cur_y);
    int ly = y + 5;
    if (bi >= 0) {
        Building *b = &W.blds[bi];
        const BldDef *d = &bld_defs[b->type];
        gfx_text_shadow(x + 6, ly, d->name, C_ORANGE); ly += 13;
        sprintf(buf, "HP %d/%d", (int)b->hp, d->hp);
        gfx_text_shadow(x + 6, ly, buf, C_WHITE); ly += 13;
        if (d->kind == K_TOWER) {
            gfx_sprite(item_icon(d->ammo), x + 6, ly + 1, 0);
            sprintf(buf, "%s: %d/%d", item_defs[d->ammo].name, b->inv[d->ammo], d->ammo_cap);
            gfx_text_shadow(x + 16, ly, buf, b->inv[d->ammo] ? C_WHITE : C_RED); ly += 13;
            sprintf(buf, "Range %.1f  Dmg %d", d->range, (int)d->dmg);
            gfx_text_shadow(x + 6, ly, buf, C_GREY); ly += 13;
        } else if (d->kind == K_CRAFT) {
            if (b->recipe >= 0) {
                const Recipe *R = &recipes[(int)b->recipe];
                sprintf(buf, "Making %s %d%%", item_defs[R->out.item].name, (int)(100 * b->timer / R->time));
                gfx_text_shadow(x + 6, ly, buf, C_GREEN);
            } else
                gfx_text_shadow(x + 6, ly, "Idle - waiting for input", C_GREY);
            ly += 13;
            int any = 0;
            for (int i = 0; i < IT_COUNT; i++)
                if (b->inv[i]) {
                    gfx_sprite(item_icon(i), x + 6 + any * 34, ly + 1, 0);
                    sprintf(buf, "%d", b->inv[i]);
                    gfx_text_shadow(x + 16 + any * 34, ly, buf, C_WHITE);
                    any++;
                }
            if (any) ly += 13;
        } else if (d->kind == K_EXTRACT) {
            Tile *t = tile_at(b->x, b->y);
            sprintf(buf, "Producing %s", item_defs[res_defs[t->res].item].name);
            gfx_text_shadow(x + 6, ly, buf, C_WHITE); ly += 13;
            if (b->outn >= 3) { gfx_text_shadow(x + 6, ly, "Blocked: no output!", C_RED); ly += 13; }
        } else if (d->kind == K_KEEP) {
            sprintf(buf, "Raids repelled %d/%d", W.waves_cleared, region_defs[W.region].num_waves);
            gfx_text_shadow(x + 6, ly, buf, C_WHITE); ly += 13;
        }
        gfx_text_shadow(x + 6, y + h - 28, "O: remove (full refund)", C_GREY);
    } else {
        int type = U.tools[U.sel];
        const BldDef *d = &bld_defs[type];
        gfx_text_shadow(x + 6, ly, d->name, C_ORANGE); ly += 13;
        /* word-wrap description */
        const char *s = d->desc;
        while (*s) {
            int n = (int)strlen(s);
            if (n > 27) {
                n = 27;
                while (n > 0 && s[n] != ' ') n--;
                if (!n) n = 27;
            }
            memcpy(buf, s, n); buf[n] = 0;
            gfx_text_shadow(x + 6, ly, buf, C_WHITE);
            ly += 12;
            s += n;
            while (*s == ' ') s++;
        }
        ly += 2;
        for (int r = 0; r < recipe_count; r++) {
            const Recipe *R = &recipes[r];
            if (R->bld != type) continue;
            int xx = x + 6;
            for (int k = 0; k < 2; k++) {
                if (!R->in[k].n) continue;
                sprintf(buf, "%d", R->in[k].n);
                xx += gfx_text_shadow(xx, ly, buf, C_WHITE);
                gfx_sprite(item_icon(R->in[k].item), xx + 1, ly + 1, 0);
                xx += 11;
                if (k == 0 && R->in[1].n) xx += gfx_text_shadow(xx, ly, "+", C_GREY);
            }
            xx += gfx_text_shadow(xx, ly, " = ", C_GREY);
            sprintf(buf, "%d", R->out.n);
            xx += gfx_text_shadow(xx, ly, buf, C_YELLOW);
            gfx_sprite(item_icon(R->out.item), xx + 1, ly + 1, 0);
            gfx_text_shadow(xx + 12, ly, item_defs[R->out.item].name, C_YELLOW);
            ly += 12;
        }
    }
    gfx_text_shadow(x + 6, y + h - 15, "/\\: close", C_GREY);
}

void render_hud(void)
{
    char buf[64];
    const RegionDef *R = &region_defs[W.region];

    /* ---- top bar: stock ---- */
    gfx_panel(0, 0, SCREEN_W, 16);
    int x = 4;
    for (int i = 0; i < IT_COUNT && x < 318; i++) {
        if (!W.stock[i]) continue;
        gfx_sprite(item_icon(i), x, 4, 0);
        sprintf(buf, "%d", W.stock[i]);
        x += 10;
        x += gfx_text_shadow(x, 3, buf, C_YELLOW) + 6;
    }
    /* keep hp + raid timer */
    Building *k = &W.blds[W.keep];
    gfx_sprite(SPR_UI_HEART, 324, 4, 0);
    sprintf(buf, "%d", (int)(k->hp + 0.5f));
    gfx_text_shadow(335, 3, buf, k->hp < 250 ? C_RED : C_WHITE);
    gfx_sprite(SPR_UI_SWORDS, 368, 3, 0);
    if (W.wave_active) sprintf(buf, "RAID! %d", enemies_alive() + W.npending);
    else if (W.wave_next < R->num_waves) { char t[16]; fmt_time(t, W.wave_timer); sprintf(buf, "Raid %s", t); }
    else sprintf(buf, "All clear");
    gfx_text_shadow(380, 3, buf, W.wave_active ? C_RED : C_WHITE);
    if (U.speed > 1) gfx_text_shadow(460, 3, "2x", C_GREEN);

    /* ---- goals ---- */
    int gy = 19, gx = SCREEN_W - 104;
    gfx_panel_alpha(gx, gy, 102, 52, 240);
    gfx_text_shadow(gx + 6, gy + 3, "Goals", C_ORANGE);
    int ly = gy + 15;
    for (int g = 0; g < 2; g++) {
        if (!R->goals[g].n) continue;
        int it = R->goals[g].item, have = W.delivered[it], need = R->goals[g].n;
        if (have > need) have = need;
        gfx_sprite(item_icon(it), gx + 6, ly + 1, 0);
        sprintf(buf, "%d/%d", have, need);
        gfx_text_shadow(gx + 18, ly, buf, have >= need ? C_GREEN : C_YELLOW);
        ly += 11;
    }
    gfx_sprite(SPR_UI_SWORDS, gx + 5, ly + 1, 0);
    sprintf(buf, "%d/%d", W.waves_cleared, R->num_waves);
    gfx_text_shadow(gx + 18, ly, buf, W.waves_cleared >= R->num_waves ? C_GREEN : C_YELLOW);

    hover_text();

    /* ---- toolbar ---- */
    int slot = 22, tb_w = U.ntools * slot + 8, tb_x = (SCREEN_W - tb_w) / 2, tb_y = SCREEN_H - 36;
    gfx_panel_alpha(tb_x, tb_y, tb_w, 36, 230);
    for (int i = 0; i < U.ntools; i++) {
        int type = U.tools[i];
        int sx = tb_x + 4 + i * slot, sy = tb_y + 3;
        int spr = bld_defs[type].sprite;
        gfx_clip(sx, sy, slot - 1, 30);
        gfx_sprite(spr, sx + 1, sy + 30 - spr_h(spr) + (spr_h(spr) > 30 ? (spr_h(spr) - 30) / 2 : 0) - (type == B_TRACK || type == B_JUNCTION ? 5 : 0),
                   can_afford(type) ? 0 : SF_DARK);
        gfx_noclip();
        if (i == U.sel) {
            gfx_rect(sx - 1, sy - 1, slot + 1, 32, C_YELLOW);
            gfx_rect(sx, sy, slot - 1, 30, RGB(140, 110, 20));
        }
    }
    /* selected tool name + cost */
    int type = U.tools[U.sel];
    int nw = text_w(bld_defs[type].name) + 90;
    int nx = (SCREEN_W - nw) / 2, ny = tb_y - 16;
    gfx_panel_alpha(nx, ny, nw, 16, 220);
    int cx = nx + 6 + gfx_text_shadow(nx + 6, ny + 3, bld_defs[type].name, C_ORANGE) + 8;
    draw_cost(cx, ny + 3, type);

    /* ---- tutorial tip ---- */
    const char *tip = tip_text(U.tip);
    if (tip && !W.won && !W.lost) {
        int tw = text_w(tip) + 12;
        int tx = (SCREEN_W - tw) / 2, ty = ny - 20;
        gfx_panel_alpha(tx, ty, tw, 17, 235);
        gfx_text_shadow(tx + 6, ty + 3, tip, C_YELLOW);
    }

    if (U.info) info_panel();

    /* ---- banner + toast ---- */
    if (U.banner_t > 0 && !W.won) {
        const char *s = "Raid incoming!";
        int bw = text_w(s) * 2;
        gfx_text_big((SCREEN_W - bw) / 2, 34, s, C_RED, 2);
    }
    if (U.msg_t > 0) {
        int mw = text_w(U.msg) + 12;
        gfx_panel_alpha((SCREEN_W - mw) / 2, 100, mw, 17, 235);
        gfx_text_shadow((SCREEN_W - mw) / 2 + 6, 103, U.msg, C_WHITE);
    }
}
