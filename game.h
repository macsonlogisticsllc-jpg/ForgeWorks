#ifndef GAME_H
#define GAME_H

#include "common.h"
#include "data.h"

#define MAX_BLD 1200
#define MAX_EN 96
#define MAX_PROJ 96
#define MAX_FX 96
#define MAX_PENDING 48
#define MAX_SPAWNS 3
#define TRACK_CAP 4
#define OUTQ_CAP 6
#define INV_CAP 6

enum { G_GRASS, G_WATER, G_CAMP };

typedef struct {
    u8 ground;
    u8 res;
    u8 var;
    u8 pad;
    s16 bld;     /* building index or -1 */
} Tile;

typedef struct {
    u8 alive, type, dir, active;
    s16 x, y;
    float hp;
    float timer;
    float flash;
    u8 inv[IT_COUNT];         /* crafter inputs / tower ammo */
    u8 outq[OUTQ_CAP];
    u8 outn;
    u8 rr;
    s8 recipe;                /* recipe in progress (-1 none) */
    u8 craft_out;             /* output item of recipe in progress */
    /* track */
    u8 cn;
    u8 citem[TRACK_CAP];
    float cpos[TRACK_CAP];
    /* junction: one slot per travel direction */
    u8 jitem[4];
    float jt[4];
    /* router */
    u8 ritem, rsrc;
    float rt;
    /* tower */
    float cd;
    float aim;               /* for drawing */
} Building;

typedef struct {
    u8 alive, type, attacking, anim;
    float x, y;              /* pixel position (feet) */
    float ox, oy;            /* personal offset inside tile */
    float hp;
    float flash;
    int tx, ty;              /* tile we are walking to */
    int facing;              /* 0 right, 1 left */
    float atk_t;
} Enemy;

typedef struct {
    u8 alive, kind;          /* kind: 0 arrow, 1 bolt */
    float x, y;
    int target;
    float dmg;
    float lx, ly;            /* last known target pos */
} Proj;

enum { FX_PUFF, FX_RING, FX_SMOKE, FX_SPARK };
typedef struct {
    u8 alive, kind;
    float x, y, t, life;
} Fx;

typedef struct {
    u32 magic;
    int region;
    u32 rng;
    Tile tiles[MAP_H][MAP_W];
    Building blds[MAX_BLD];
    int nbld;                         /* high-water mark */
    Enemy en[MAX_EN];
    Proj pr[MAX_PROJ];
    Fx fx[MAX_FX];
    int stock[IT_COUNT];
    int delivered[IT_COUNT];
    int keep;                         /* keep building index */
    int keep_x, keep_y;
    u16 dist[MAP_H][MAP_W];
    u8 flow_dirty;
    int spawn_x[MAX_SPAWNS], spawn_y[MAX_SPAWNS], nspawn;
    float wave_timer;
    int wave_next;                    /* index of next wave to launch */
    int wave_active;
    int waves_cleared;
    u8 pending[MAX_PENDING];
    int npending;
    float spawn_cd;
    float trickle;
    float time;
    int won, lost;
    int anim;                         /* frame counter for animations */
} World;

#define WORLD_MAGIC 0x46574B33u      /* bump when World layout changes */

extern World W;
extern const int DX[4], DY[4];

/* world.c */
u32 rnd(void);
float rndf(void);
void world_new(int region);
void world_update(float dt);
int tile_ok(int x, int y);
Tile *tile_at(int x, int y);
int bld_at(int x, int y);
int can_place(int type, int x, int y, int *why);
int can_afford(int type);
int place_building(int type, int x, int y, int dir);
void remove_building(int idx, int refund);
int goals_done(void);
int enemies_alive(void);
void fx_add(int kind, float x, float y, float life);

enum { WHY_OK, WHY_BLOCKED, WHY_WATER, WHY_NEED_RES, WHY_TREE, WHY_COST, WHY_CAMP };

#endif
