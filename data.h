#ifndef DATA_H
#define DATA_H

#include "common.h"

/* ---------------------------------------------------------------- items */
enum {
    IT_COPPER_ORE, IT_TIN_ORE, IT_LOG, IT_IRON_ORE, IT_COAL, IT_STONE, IT_CLAY, IT_HERB, IT_AETHER_ORE,
    IT_BRONZE, IT_IRON_BAR, IT_STEEL, IT_AETHER_BAR, IT_PLANK, IT_ARROW, IT_BRICK, IT_POTION, IT_BOLT, IT_MANA,
    IT_COUNT
};
#define IT_NONE 255

typedef struct { const char *name; } ItemDef;
extern const ItemDef item_defs[IT_COUNT];
int item_icon(int item);

/* ---------------------------------------------------------------- resources on the map */
enum { RES_NONE, RES_COPPER, RES_TIN, RES_IRON, RES_COAL, RES_STONE, RES_CLAY, RES_AETHER, RES_HERB, RES_TREE, RES_COUNT };

typedef struct {
    const char *name;
    u8 item;        /* item produced by extracting it */
    u8 extractor;   /* building type that can be placed on it */
    int sprite;     /* -1 = tree (themed) */
} ResDef;
extern const ResDef res_defs[RES_COUNT];

/* ---------------------------------------------------------------- buildings */
enum {
    B_KEEP, B_TRACK, B_JUNCTION, B_ROUTER, B_MINE, B_WOODCUTTER, B_HERBGARDEN,
    B_SMELTER, B_FLETCHER, B_SAWMILL, B_KILN, B_ALCHEMY, B_FORGE, B_ENCHANTER,
    B_ARCHER, B_BALLISTA, B_WARD, B_WALL, B_BRICKWALL,
    B_COUNT
};

enum { K_KEEP, K_TRACK, K_JUNCTION, K_ROUTER, K_EXTRACT, K_CRAFT, K_TOWER, K_WALL };

typedef struct { u8 item, n; } Stack;

typedef struct {
    const char *name;
    const char *desc;
    int sprite, sprite_on;   /* sprite_on: active animation frame or -1 */
    int unlock;              /* first region (0-based) where it is available */
    Stack cost[2];
    int hp;
    u8 kind;
    float rate;              /* extractor seconds per item */
    /* towers */
    u8 ammo;
    float range;             /* tiles */
    float cooldown;
    float dmg;
    u8 aoe;
    u8 ammo_cap;
} BldDef;
extern const BldDef bld_defs[B_COUNT];

typedef struct {
    u8 bld;
    Stack in[2];
    Stack out;
    float time;
} Recipe;
extern const Recipe recipes[];
extern const int recipe_count;

/* ---------------------------------------------------------------- enemies */
enum { EN_WOLF, EN_GOBLIN, EN_BRUTE, EN_SKELETON, EN_TROLL, EN_COUNT };

typedef struct {
    const char *name;
    int sprite;     /* frame 0; frame 1 = sprite+1 */
    float hp, speed, dmg;
} EnemyDef;
extern const EnemyDef enemy_defs[EN_COUNT];

/* ---------------------------------------------------------------- regions */
#define REGION_COUNT 5
#define MAX_WAVES 6

typedef struct { u8 type, count; } WaveEntry;
typedef struct { WaveEntry e[3]; } Wave;

typedef struct {
    const char *name;
    const char *blurb[3];
    int theme;
    u32 seed;
    u8 deposits[RES_COUNT];   /* number of deposit clusters of each resource */
    float water;              /* 0..1 amount of water */
    Stack start[7];
    Stack goals[2];
    int num_waves;
    Wave waves[MAX_WAVES];
    float first_wave, wave_interval;
    int mx, my;               /* position on world map */
} RegionDef;
extern const RegionDef region_defs[REGION_COUNT];

#endif
