#include "game.h"
#include <math.h>
#include <stdlib.h>

World W;
const int DX[4] = {1, 0, -1, 0};
const int DY[4] = {0, 1, 0, -1};

#define TRACK_SPEED 1.9f      /* tiles per second */
#define TRACK_SPACING 0.28f
#define INF16 0xFFFF

/* ================================================================ helpers */
u32 rnd(void)
{
    u32 x = W.rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    W.rng = x ? x : 0x9E3779B9u;
    return W.rng;
}
float rndf(void) { return (float)(rnd() & 0xFFFFFF) / 16777216.0f; }
static int rndi(int n) { return n > 0 ? (int)(rnd() % (u32)n) : 0; }

int tile_ok(int x, int y) { return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H; }
Tile *tile_at(int x, int y) { return &W.tiles[y][x]; }
int bld_at(int x, int y) { return tile_ok(x, y) ? W.tiles[y][x].bld : -1; }

void fx_add(int kind, float x, float y, float life)
{
    for (int i = 0; i < MAX_FX; i++)
        if (!W.fx[i].alive) {
            W.fx[i].alive = 1; W.fx[i].kind = (u8)kind;
            W.fx[i].x = x; W.fx[i].y = y; W.fx[i].t = 0; W.fx[i].life = life;
            return;
        }
}

int enemies_alive(void)
{
    int n = 0;
    for (int i = 0; i < MAX_EN; i++) n += W.en[i].alive;
    return n;
}

/* ================================================================ flow field (Dijkstra from the keep) */
static u32 heap[MAP_W * MAP_H * 4];
static int hn;

static void hpush(u32 v)
{
    if (hn >= (int)(sizeof(heap) / sizeof(heap[0]))) return;
    int i = hn++;
    heap[i] = v;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (heap[p] <= heap[i]) break;
        u32 t = heap[p]; heap[p] = heap[i]; heap[i] = t; i = p;
    }
}

static u32 hpop(void)
{
    u32 top = heap[0];
    heap[0] = heap[--hn];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < hn && heap[l] < heap[m]) m = l;
        if (r < hn && heap[r] < heap[m]) m = r;
        if (m == i) break;
        u32 t = heap[m]; heap[m] = heap[i]; heap[i] = t; i = m;
    }
    return top;
}

static int enter_cost(int x, int y)
{
    Tile *t = &W.tiles[y][x];
    if (t->ground == G_WATER) return INF16;
    if (t->bld >= 0) {
        Building *b = &W.blds[t->bld];
        if (b->type == B_KEEP) return 1;
        if (bld_defs[b->type].kind == K_WALL) return 40;
        return 12;
    }
    if (t->res == RES_TREE) return 3;
    return 1;
}

static void flow_compute(void)
{
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) W.dist[y][x] = INF16;
    hn = 0;
    for (int y = 0; y < 3; y++)
        for (int x = 0; x < 3; x++) {
            W.dist[W.keep_y + y][W.keep_x + x] = 0;
            hpush((u32)((W.keep_y + y) * MAP_W + W.keep_x + x));
        }
    while (hn > 0) {
        u32 v = hpop();
        int d = (int)(v >> 11), i = (int)(v & 2047);
        int x = i % MAP_W, y = i / MAP_W;
        if (d > W.dist[y][x]) continue;
        int c = enter_cost(x, y);
        if (c >= INF16) continue;
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!tile_ok(nx, ny) || W.tiles[ny][nx].ground == G_WATER) continue;
            int nd = d + c;
            if (nd < W.dist[ny][nx]) {
                W.dist[ny][nx] = (u16)nd;
                hpush(((u32)nd << 11) | (u32)(ny * MAP_W + nx));
            }
        }
    }
    W.flow_dirty = 0;
}

/* ================================================================ map generation */
static float lattice(int x, int y, u32 s)
{
    u32 h = (u32)x * 374761393u + (u32)y * 668265263u + s * 2654435761u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFF) / 65535.0f;
}

static float vnoise(float x, float y, u32 s)
{
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    float a = lattice(ix, iy, s), b = lattice(ix + 1, iy, s);
    float c = lattice(ix, iy + 1, s), d = lattice(ix + 1, iy + 1, s);
    return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
}

static int cmpf(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}

static float keep_dist(int x, int y)
{
    float dx = x - (W.keep_x + 1), dy = y - (W.keep_y + 1);
    return sqrtf(dx * dx + dy * dy);
}

static int near_camp(int x, int y, int r)
{
    for (int i = 0; i < W.nspawn; i++)
        if (abs(W.spawn_x[i] - x) <= r && abs(W.spawn_y[i] - y) <= r) return 1;
    return 0;
}

static int free_land(int x, int y)
{
    if (!tile_ok(x, y)) return 0;
    Tile *t = &W.tiles[y][x];
    return t->ground == G_GRASS && t->res == RES_NONE && t->bld < 0 && W.dist[y][x] != INF16 &&
           keep_dist(x, y) > 4.5f && !near_camp(x, y, 2);
}

static void carve_to_keep(int x, int y)
{
    int kx = W.keep_x + 1, ky = W.keep_y + 1;
    while (x != kx || y != ky) {
        W.tiles[y][x].ground = G_GRASS;
        if (abs(kx - x) > abs(ky - y)) x += kx > x ? 1 : -1;
        else y += ky > y ? 1 : -1;
    }
}

static void place_spawns(int count)
{
    int edges[4] = {0, 1, 2, 3};
    for (int i = 3; i > 0; i--) {
        int j = rndi(i + 1), t = edges[i]; edges[i] = edges[j]; edges[j] = t;
    }
    W.nspawn = 0;
    for (int e = 0; e < 4 && W.nspawn < count; e++) {
        int best_x = -1, best_y = -1;
        for (int tries = 0; tries < 60; tries++) {
            int x, y;
            switch (edges[e]) {
            case 0: x = 1; y = 3 + rndi(MAP_H - 6); break;
            case 1: x = MAP_W - 2; y = 3 + rndi(MAP_H - 6); break;
            case 2: x = 3 + rndi(MAP_W - 6); y = 1; break;
            default: x = 3 + rndi(MAP_W - 6); y = MAP_H - 2; break;
            }
            if (W.tiles[y][x].ground == G_GRASS && W.dist[y][x] != INF16) { best_x = x; best_y = y; break; }
        }
        if (best_x < 0) {           /* no dry land reachable: carve a path */
            switch (edges[e]) {
            case 0: best_x = 1; best_y = MAP_H / 2; break;
            case 1: best_x = MAP_W - 2; best_y = MAP_H / 2; break;
            case 2: best_x = MAP_W / 2; best_y = 1; break;
            default: best_x = MAP_W / 2; best_y = MAP_H - 2; break;
            }
            carve_to_keep(best_x, best_y);
            flow_compute();
        }
        W.spawn_x[W.nspawn] = best_x;
        W.spawn_y[W.nspawn] = best_y;
        W.nspawn++;
        for (int yy = -1; yy <= 1; yy++)
            for (int xx = -1; xx <= 1; xx++)
                if (tile_ok(best_x + xx, best_y + yy)) {
                    W.tiles[best_y + yy][best_x + xx].ground = G_GRASS;
                    W.tiles[best_y + yy][best_x + xx].res = RES_NONE;
                }
        W.tiles[best_y][best_x].ground = G_CAMP;
    }
}

static void blob(int res, int cx, int cy, int size)
{
    int x = cx, y = cy;
    for (int i = 0; i < size * 3 && size > 0; i++) {
        if (free_land(x, y)) {
            W.tiles[y][x].res = (u8)res;
            size--;
        }
        int d = rndi(4);
        int nx = x + DX[d], ny = y + DY[d];
        if (tile_ok(nx, ny) && W.tiles[ny][nx].ground == G_GRASS) { x = nx; y = ny; }
    }
}

static void gen_map(void)
{
    const RegionDef *R = &region_defs[W.region];
    u32 seed = R->seed;
    static float vals[MAP_W * MAP_H], sorted[MAP_W * MAP_H];

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            Tile *t = &W.tiles[y][x];
            t->ground = G_GRASS; t->res = RES_NONE; t->bld = -1; t->var = (u8)rndi(6);
            vals[y * MAP_W + x] = vnoise(x / 7.0f, y / 7.0f, seed) * 0.65f + vnoise(x / 3.2f, y / 3.2f, seed + 17) * 0.35f;
        }
    memcpy(sorted, vals, sizeof(vals));
    qsort(sorted, MAP_W * MAP_H, sizeof(float), cmpf);
    float thr = sorted[(int)((MAP_W * MAP_H - 1) * (1.0f - R->water))];
    W.keep_x = MAP_W / 2 - 1;
    W.keep_y = MAP_H / 2 - 1;
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (vals[y * MAP_W + x] > thr && keep_dist(x, y) > 5.5f) W.tiles[y][x].ground = G_WATER;

    flow_compute();
    place_spawns(W.region == 0 ? 2 : 3);
    flow_compute();

    /* resource deposits: first cluster of each kind close to the keep */
    for (int r = RES_COPPER; r <= RES_HERB; r++) {
        for (int k = 0; k < R->deposits[r]; k++) {
            float lo = k == 0 ? 5.5f : 8.0f, hi = k == 0 ? 9.0f : 26.0f;
            for (int tries = 0; tries < 300; tries++) {
                int x = 2 + rndi(MAP_W - 4), y = 2 + rndi(MAP_H - 4);
                float kd = keep_dist(x, y);
                if (kd < lo || kd > hi || !free_land(x, y)) continue;
                blob(r, x, y, 5 + rndi(4));
                break;
            }
        }
    }
    /* forests */
    for (int k = 0; k < R->deposits[RES_TREE]; k++) {
        for (int tries = 0; tries < 200; tries++) {
            int x = 2 + rndi(MAP_W - 4), y = 2 + rndi(MAP_H - 4);
            if (!free_land(x, y) || keep_dist(x, y) < (k == 0 ? 6 : 7)) continue;
            int n = 8 + rndi(10);
            for (int i = 0; i < n * 2 && n > 0; i++) {
                int tx = x + rndi(7) - 3, ty = y + rndi(5) - 2;
                if (free_land(tx, ty)) { W.tiles[ty][tx].res = RES_TREE; n--; }
            }
            break;
        }
    }
    for (int i = 0; i < 25; i++) {          /* lone trees */
        int x = rndi(MAP_W), y = rndi(MAP_H);
        if (free_land(x, y) && keep_dist(x, y) > 7) W.tiles[y][x].res = RES_TREE;
    }
}

/* ================================================================ buildings */
static int alloc_bld(void)
{
    for (int i = 0; i < W.nbld; i++)
        if (!W.blds[i].alive) return i;
    if (W.nbld < MAX_BLD) return W.nbld++;
    return -1;
}

static int place_raw(int type, int x, int y, int dir)
{
    int i = alloc_bld();
    if (i < 0) return -1;
    Building *b = &W.blds[i];
    memset(b, 0, sizeof(*b));
    b->alive = 1;
    b->type = (u8)type;
    b->dir = (u8)dir;
    b->x = (s16)x;
    b->y = (s16)y;
    b->hp = (float)bld_defs[type].hp;
    b->recipe = -1;
    b->ritem = IT_NONE;
    for (int k = 0; k < 4; k++) b->jitem[k] = IT_NONE;
    int sz = type == B_KEEP ? 3 : 1;
    for (int yy = 0; yy < sz; yy++)
        for (int xx = 0; xx < sz; xx++) W.tiles[y + yy][x + xx].bld = (s16)i;
    W.flow_dirty = 1;
    return i;
}

int can_afford(int type)
{
    const BldDef *d = &bld_defs[type];
    for (int k = 0; k < 2; k++)
        if (d->cost[k].n && W.stock[d->cost[k].item] < d->cost[k].n) return 0;
    return 1;
}

int can_place(int type, int x, int y, int *why)
{
    int w = WHY_OK;
    if (!tile_ok(x, y) || type == B_KEEP) w = WHY_BLOCKED;
    else {
        Tile *t = &W.tiles[y][x];
        if (t->bld >= 0) w = WHY_BLOCKED;
        else if (t->ground == G_WATER) w = WHY_WATER;
        else if (t->ground == G_CAMP) w = WHY_CAMP;
        else if (bld_defs[type].kind == K_EXTRACT) {
            if (t->res == RES_NONE || res_defs[t->res].extractor != type) w = WHY_NEED_RES;
        } else if (t->res == RES_TREE) w = WHY_TREE;
        if (w == WHY_OK && !can_afford(type)) w = WHY_COST;
    }
    if (why) *why = w;
    return w == WHY_OK;
}

int place_building(int type, int x, int y, int dir)
{
    if (!can_place(type, x, y, 0)) return -1;
    const BldDef *d = &bld_defs[type];
    for (int k = 0; k < 2; k++)
        if (d->cost[k].n) W.stock[d->cost[k].item] -= d->cost[k].n;
    return place_raw(type, x, y, dir);
}

void remove_building(int idx, int refund)
{
    Building *b = &W.blds[idx];
    if (!b->alive || b->type == B_KEEP) return;
    if (refund) {
        const BldDef *d = &bld_defs[b->type];
        for (int k = 0; k < 2; k++)
            if (d->cost[k].n) W.stock[d->cost[k].item] += d->cost[k].n;
    }
    W.tiles[b->y][b->x].bld = -1;
    b->alive = 0;
    W.flow_dirty = 1;
}

static void damage_bld(int idx, float dmg)
{
    Building *b = &W.blds[idx];
    if (!b->alive) return;
    b->hp -= dmg;
    if (b->hp <= 0) {
        if (b->type == B_KEEP) {
            b->hp = 0;
            W.lost = 1;
            return;
        }
        fx_add(FX_PUFF, b->x * TILE + 10.0f, b->y * TILE + 12.0f, 0.5f);
        remove_building(idx, 0);
    }
}

/* ================================================================ logistics */
static int crafter_accepts(int type, int item)
{
    for (int r = 0; r < recipe_count; r++)
        if (recipes[r].bld == type)
            for (int k = 0; k < 2; k++)
                if (recipes[r].in[k].n && recipes[r].in[k].item == item) return 1;
    return 0;
}

/* offer an item travelling in direction d into building ti */
static int offer(int ti, int item, int d)
{
    Building *t = &W.blds[ti];
    const BldDef *def = &bld_defs[t->type];
    switch (def->kind) {
    case K_KEEP:
        W.stock[item]++;
        W.delivered[item]++;
        return 1;
    case K_TRACK: {
        if (t->dir == ((d + 2) & 3)) return 0;
        if (t->cn >= TRACK_CAP) return 0;
        float p = (t->dir == d) ? 0.0f : 0.5f;
        for (int i = 0; i < t->cn; i++)
            if (fabsf(t->cpos[i] - p) < TRACK_SPACING) return 0;
        int j = 0;
        while (j < t->cn && t->cpos[j] > p) j++;
        for (int i = t->cn; i > j; i--) { t->cpos[i] = t->cpos[i - 1]; t->citem[i] = t->citem[i - 1]; }
        t->cpos[j] = p;
        t->citem[j] = (u8)item;
        t->cn++;
        return 1;
    }
    case K_JUNCTION:
        if (t->jitem[d] != IT_NONE) return 0;
        t->jitem[d] = (u8)item;
        t->jt[d] = 0;
        return 1;
    case K_ROUTER:
        if (t->ritem != IT_NONE) return 0;
        t->ritem = (u8)item;
        t->rsrc = (u8)((d + 2) & 3);
        t->rt = 0;
        return 1;
    case K_CRAFT:
        if (!crafter_accepts(t->type, item) || t->inv[item] >= INV_CAP) return 0;
        t->inv[item]++;
        return 1;
    case K_TOWER:
        if (item != def->ammo || t->inv[item] >= def->ammo_cap) return 0;
        t->inv[item]++;
        return 1;
    }
    return 0;
}

static int push_dir(int bi, int d, int item)
{
    Building *b = &W.blds[bi];
    int ti = bld_at(b->x + DX[d], b->y + DY[d]);
    if (ti < 0 || ti == bi) return 0;
    return offer(ti, item, d);
}

static void push_out(int bi)
{
    Building *b = &W.blds[bi];
    if (!b->outn) return;
    for (int k = 0; k < 4; k++) {
        int d = (b->rr + k) & 3;
        if (push_dir(bi, d, b->outq[0])) {
            for (int i = 1; i < b->outn; i++) b->outq[i - 1] = b->outq[i];
            b->outn--;
            b->rr = (u8)((d + 1) & 3);
            return;
        }
    }
}

static void update_track(int bi, float dt)
{
    Building *b = &W.blds[bi];
    if (!b->cn) return;
    float v = TRACK_SPEED * dt;
    b->cpos[0] += v;
    if (b->cpos[0] >= 1.0f) {
        b->cpos[0] = 1.0f;
        if (push_dir(bi, b->dir, b->citem[0])) {
            for (int i = 1; i < b->cn; i++) { b->cpos[i - 1] = b->cpos[i]; b->citem[i - 1] = b->citem[i]; }
            b->cn--;
        }
    }
    for (int i = 1; i < b->cn; i++) {
        float lim = b->cpos[i - 1] - TRACK_SPACING;
        float np = b->cpos[i] + v;
        if (np > lim) np = lim;
        if (np > b->cpos[i]) b->cpos[i] = np;
    }
}

static void update_crafter(int bi, float dt)
{
    Building *b = &W.blds[bi];
    if (b->recipe < 0) {
        for (int r = 0; r < recipe_count; r++) {
            const Recipe *R = &recipes[r];
            if (R->bld != b->type) continue;
            int ok = 1;
            for (int k = 0; k < 2; k++)
                if (R->in[k].n && b->inv[R->in[k].item] < R->in[k].n) ok = 0;
            if (!ok || b->outn + R->out.n > OUTQ_CAP) continue;
            for (int k = 0; k < 2; k++)
                if (R->in[k].n) b->inv[R->in[k].item] -= R->in[k].n;
            b->recipe = (s8)r;
            b->craft_out = R->out.item;
            b->timer = 0;
            break;
        }
    }
    if (b->recipe >= 0) {
        const Recipe *R = &recipes[(int)b->recipe];
        b->active = 1;
        b->timer += dt;
        if (b->timer >= R->time) {
            for (int i = 0; i < R->out.n && b->outn < OUTQ_CAP; i++) b->outq[b->outn++] = R->out.item;
            b->recipe = -1;
        }
        /* chimney smoke */
        if ((b->type == B_SMELTER || b->type == B_KILN || b->type == B_FORGE) && ((W.anim + bi * 7) % 40) == 0) {
            float sx = b->x * TILE + (b->type == B_FORGE ? 4.0f : 14.0f);
            float sy = b->y * TILE - (b->type == B_SMELTER ? 12.0f : 6.0f);
            fx_add(FX_SMOKE, sx, sy, 1.6f);
        }
    } else
        b->active = 0;
    push_out(bi);
}

static int find_target(float x, float y, float range)
{
    int best = -1;
    float bd = range * range;
    for (int i = 0; i < MAX_EN; i++) {
        Enemy *e = &W.en[i];
        if (!e->alive) continue;
        float dx = e->x - x, dy = e->y - y, d = dx * dx + dy * dy;
        if (d <= bd) { bd = d; best = i; }
    }
    return best;
}

static void kill_enemy(int i)
{
    Enemy *e = &W.en[i];
    e->alive = 0;
    fx_add(FX_PUFF, e->x, e->y - 6, 0.4f);
}

static void hurt_enemy(int i, float dmg)
{
    Enemy *e = &W.en[i];
    if (!e->alive) return;
    e->hp -= dmg;
    e->flash = 0.12f;
    if (e->hp <= 0) kill_enemy(i);
}

static void update_tower(int bi, float dt)
{
    Building *b = &W.blds[bi];
    const BldDef *d = &bld_defs[b->type];
    if (b->cd > 0) b->cd -= dt;
    if (!b->inv[d->ammo] || b->cd > 0) return;
    float cx = b->x * TILE + 10.0f, cy = b->y * TILE + 10.0f;
    float range = d->range * TILE;
    if (d->aoe) {
        int hit = 0;
        for (int i = 0; i < MAX_EN; i++) {
            Enemy *e = &W.en[i];
            if (!e->alive) continue;
            float dx = e->x - cx, dy = e->y - cy;
            if (dx * dx + dy * dy <= range * range) { hurt_enemy(i, d->dmg); hit = 1; }
        }
        if (hit) {
            b->inv[d->ammo]--;
            b->cd = d->cooldown;
            fx_add(FX_RING, cx, cy, 0.45f);
        }
        return;
    }
    int t = find_target(cx, cy, range);
    if (t < 0) return;
    for (int i = 0; i < MAX_PROJ; i++) {
        Proj *p = &W.pr[i];
        if (p->alive) continue;
        p->alive = 1;
        p->kind = b->type == B_BALLISTA;
        p->x = cx;
        p->y = cy - (b->type == B_ARCHER ? 20.0f : 6.0f);
        p->target = t;
        p->dmg = d->dmg;
        p->lx = W.en[t].x;
        p->ly = W.en[t].y - 6;
        b->inv[d->ammo]--;
        b->cd = d->cooldown;
        b->aim = atan2f(W.en[t].y - cy, W.en[t].x - cx);
        break;
    }
}

static void update_buildings(float dt)
{
    for (int i = 0; i < W.nbld; i++) {
        Building *b = &W.blds[i];
        if (!b->alive) continue;
        if (b->flash > 0) b->flash -= dt;
        const BldDef *d = &bld_defs[b->type];
        switch (d->kind) {
        case K_TRACK:
            update_track(i, dt);
            break;
        case K_JUNCTION:
            for (int k = 0; k < 4; k++)
                if (b->jitem[k] != IT_NONE) {
                    b->jt[k] += dt;
                    if (b->jt[k] >= 0.12f && push_dir(i, k, b->jitem[k])) b->jitem[k] = IT_NONE;
                }
            break;
        case K_ROUTER:
            if (b->ritem != IT_NONE) {
                b->rt += dt;
                if (b->rt >= 0.1f)
                    for (int k = 0; k < 4; k++) {
                        int dd = (b->rr + k) & 3;
                        if (dd == b->rsrc) continue;
                        if (push_dir(i, dd, b->ritem)) {
                            b->ritem = IT_NONE;
                            b->rr = (u8)((dd + 1) & 3);
                            break;
                        }
                    }
            }
            break;
        case K_EXTRACT: {
            Tile *t = &W.tiles[b->y][b->x];
            int item = res_defs[t->res].item;
            if (item == IT_NONE) break;
            if (b->outn < 3) {
                b->timer += dt;
                b->active = 1;
                if (b->timer >= d->rate) {
                    b->outq[b->outn++] = (u8)item;
                    b->timer = 0;
                }
            } else
                b->active = 0;
            push_out(i);
            break;
        }
        case K_CRAFT:
            update_crafter(i, dt);
            break;
        case K_TOWER:
            update_tower(i, dt);
            break;
        case K_KEEP:
            if (!W.wave_active && b->hp < d->hp) {
                b->hp += 3.0f * dt;
                if (b->hp > d->hp) b->hp = (float)d->hp;
            }
            break;
        }
    }
}

/* ================================================================ enemies */
static void spawn_enemy(int type)
{
    for (int i = 0; i < MAX_EN; i++) {
        Enemy *e = &W.en[i];
        if (e->alive) continue;
        memset(e, 0, sizeof(*e));
        int s = rndi(W.nspawn);
        e->alive = 1;
        e->type = (u8)type;
        e->hp = enemy_defs[type].hp;
        e->ox = (rndf() - 0.5f) * 8.0f;
        e->oy = (rndf() - 0.5f) * 6.0f;
        e->tx = W.spawn_x[s];
        e->ty = W.spawn_y[s];
        e->x = e->tx * TILE + 10.0f + e->ox;
        e->y = e->ty * TILE + 12.0f + e->oy;
        return;
    }
}

static void next_step(Enemy *e)
{
    int best = -1, bestv = INF16 + 100;
    for (int k = 0; k < 4; k++) {
        int nx = e->tx + DX[k], ny = e->ty + DY[k];
        if (!tile_ok(nx, ny) || W.tiles[ny][nx].ground == G_WATER) continue;
        if (W.dist[ny][nx] == INF16) continue;
        int v = W.dist[ny][nx] + enter_cost(nx, ny);
        if (v < bestv) { bestv = v; best = k; }
    }
    if (best >= 0) {
        e->tx += DX[best];
        e->ty += DY[best];
    }
}

static void update_enemies(float dt)
{
    for (int i = 0; i < MAX_EN; i++) {
        Enemy *e = &W.en[i];
        if (!e->alive) continue;
        const EnemyDef *d = &enemy_defs[e->type];
        if (e->flash > 0) e->flash -= dt;
        int cx = (int)(e->x / TILE), cy = (int)(e->y / TILE);
        int target = bld_at(cx, cy);
        if (target < 0) target = bld_at(e->tx, e->ty);
        if (target >= 0) {
            e->attacking = 1;
            e->atk_t += dt;
            damage_bld(target, d->dmg * dt);
            if (e->atk_t >= 0.5f) {
                e->atk_t = 0;
                if (W.blds[target].alive) W.blds[target].flash = 0.12f;
                fx_add(FX_SPARK, e->x + (e->facing ? -5.0f : 5.0f), e->y - 6, 0.25f);
            }
            continue;
        }
        e->attacking = 0;
        float gx = e->tx * TILE + 10.0f + e->ox, gy = e->ty * TILE + 12.0f + e->oy;
        float dx = gx - e->x, dy = gy - e->y;
        float dist = sqrtf(dx * dx + dy * dy);
        float step = d->speed * TILE * dt;
        if (dist <= step) {
            e->x = gx;
            e->y = gy;
            next_step(e);
        } else {
            e->x += dx / dist * step;
            e->y += dy / dist * step;
            if (dx < -0.1f) e->facing = 1;
            else if (dx > 0.1f) e->facing = 0;
        }
        if ((W.anim + i * 5) % 12 == 0) e->anim ^= 1;
    }
}

static void update_projectiles(float dt)
{
    for (int i = 0; i < MAX_PROJ; i++) {
        Proj *p = &W.pr[i];
        if (!p->alive) continue;
        if (p->target >= 0 && W.en[p->target].alive) {
            p->lx = W.en[p->target].x;
            p->ly = W.en[p->target].y - 6;
        } else
            p->target = -1;
        float dx = p->lx - p->x, dy = p->ly - p->y, dist = sqrtf(dx * dx + dy * dy);
        float step = (p->kind ? 240.0f : 300.0f) * dt;
        if (dist <= step) {
            if (p->target >= 0) hurt_enemy(p->target, p->dmg);
            p->alive = 0;
        } else {
            p->x += dx / dist * step;
            p->y += dy / dist * step;
        }
    }
}

static void update_fx(float dt)
{
    for (int i = 0; i < MAX_FX; i++) {
        Fx *f = &W.fx[i];
        if (!f->alive) continue;
        f->t += dt;
        if (f->kind == FX_SMOKE) { f->y -= 9.0f * dt; f->x += 3.0f * dt; }
        if (f->t >= f->life) f->alive = 0;
    }
}

/* ================================================================ raids */
static void launch_wave(void)
{
    const Wave *wv = &region_defs[W.region].waves[W.wave_next];
    W.npending = 0;
    for (int k = 0; k < 3; k++)
        for (int n = 0; n < wv->e[k].count && W.npending < MAX_PENDING; n++) W.pending[W.npending++] = wv->e[k].type;
    for (int i = W.npending - 1; i > 0; i--) {
        int j = rndi(i + 1);
        u8 t = W.pending[i]; W.pending[i] = W.pending[j]; W.pending[j] = t;
    }
    W.wave_active = 1;
    W.wave_next++;
    W.spawn_cd = 0;
}

static void update_waves(float dt)
{
    const RegionDef *R = &region_defs[W.region];
    if (!W.wave_active) {
        if (W.wave_next < R->num_waves) {
            W.wave_timer -= dt;
            if (W.wave_timer <= 0) launch_wave();
        }
    } else {
        if (W.npending > 0) {
            W.spawn_cd -= dt;
            if (W.spawn_cd <= 0) {
                spawn_enemy(W.pending[--W.npending]);
                W.spawn_cd = 0.8f;
            }
        } else if (!enemies_alive()) {
            W.wave_active = 0;
            W.waves_cleared++;
            W.wave_timer = R->wave_interval;
        }
    }
}

int goals_done(void)
{
    const RegionDef *R = &region_defs[W.region];
    for (int k = 0; k < 2; k++)
        if (R->goals[k].n && W.delivered[R->goals[k].item] < R->goals[k].n) return 0;
    return 1;
}

/* ================================================================ public */
void world_new(int region)
{
    memset(&W, 0, sizeof(W));
    W.magic = WORLD_MAGIC;
    W.region = region;
    W.rng = region_defs[region].seed * 2654435761u + 12345u;
    gen_map();
    W.keep = place_raw(B_KEEP, W.keep_x, W.keep_y, 0);
    const RegionDef *R = &region_defs[region];
    for (int k = 0; k < 7; k++)
        if (R->start[k].n) W.stock[R->start[k].item] += R->start[k].n;
    W.wave_timer = R->first_wave;
    flow_compute();
}

void world_update(float dt)
{
    W.anim++;
    if (W.won || W.lost) {
        update_fx(dt);
        return;
    }
    W.time += dt;
    if (W.flow_dirty) flow_compute();
    update_buildings(dt);
    update_enemies(dt);
    update_projectiles(dt);
    update_fx(dt);
    update_waves(dt);
    W.trickle += dt;
    if (W.trickle >= 3.0f) {        /* the keep's peasants gather a few logs */
        W.trickle -= 3.0f;
        W.stock[IT_LOG]++;
    }
    if (goals_done() && W.waves_cleared >= region_defs[W.region].num_waves) W.won = 1;
}
