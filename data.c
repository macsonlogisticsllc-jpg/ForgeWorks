#include "data.h"
#include "assets.h"

const ItemDef item_defs[IT_COUNT] = {
    {"Copper ore"}, {"Tin ore"}, {"Logs"}, {"Iron ore"}, {"Coal"}, {"Stone"}, {"Clay"}, {"Herbs"}, {"Aether ore"},
    {"Bronze bar"}, {"Iron bar"}, {"Steel bar"}, {"Aether bar"}, {"Planks"}, {"Arrows"}, {"Bricks"}, {"Potion"},
    {"Bolts"}, {"Mana crystal"},
};

int item_icon(int item) { return SPR_ICON_COPPER_ORE + item; }

const ResDef res_defs[RES_COUNT] = {
    {"Grass", IT_NONE, 255, -1},
    {"Copper rocks", IT_COPPER_ORE, B_MINE, SPR_ORE_COPPER},
    {"Tin rocks", IT_TIN_ORE, B_MINE, SPR_ORE_TIN},
    {"Iron rocks", IT_IRON_ORE, B_MINE, SPR_ORE_IRON},
    {"Coal rocks", IT_COAL, B_MINE, SPR_ORE_COAL},
    {"Stone outcrop", IT_STONE, B_MINE, SPR_ORE_STONE},
    {"Clay pit", IT_CLAY, B_MINE, SPR_ORE_CLAY},
    {"Aether crystals", IT_AETHER_ORE, B_MINE, SPR_ORE_AETHER},
    {"Herb patch", IT_HERB, B_HERBGARDEN, SPR_ORE_HERB},
    {"Tree", IT_LOG, B_WOODCUTTER, -1},
};

#define S(i, n) {i, n}
#define NOCOST {IT_NONE, 0}

const BldDef bld_defs[B_COUNT] = {
    /* name, desc, sprite, sprite_on, unlock, cost, hp, kind, rate, ammo, range, cd, dmg, aoe, cap */
    {"Keep", "Stores everything delivered. Protect it!", SPR_KEEP, -1, 0, {NOCOST, NOCOST}, 800, K_KEEP},
    {"Track", "Carries items. Hold X and move to lay a line.", SPR_TRACK_0_0, -1, 0, {S(IT_LOG, 1), NOCOST}, 30, K_TRACK},
    {"Junction", "Lets two track lines cross.", SPR_B_JUNCTION, -1, 0, {S(IT_LOG, 3), NOCOST}, 40, K_JUNCTION},
    {"Router", "Splits items out to all other sides.", SPR_B_ROUTER, -1, 1, {S(IT_LOG, 4), S(IT_STONE, 2)}, 50, K_ROUTER},
    {"Mine", "Place on rocks, ore or clay. Outputs to neighbours.", SPR_B_MINE, -1, 0, {S(IT_LOG, 8), NOCOST}, 60, K_EXTRACT, 1.8f},
    {"Woodcutter", "Place on a tree. Produces logs.", SPR_B_WOODCUTTER, -1, 0, {S(IT_LOG, 6), NOCOST}, 60, K_EXTRACT, 2.2f},
    {"Herb garden", "Place on a herb patch. Grows herbs.", SPR_B_HERBGARDEN, -1, 2, {S(IT_LOG, 10), NOCOST}, 60, K_EXTRACT, 2.6f},
    {"Smelter", "Copper+Tin=Bronze. Iron+Coal=Iron bar.", SPR_B_SMELTER, SPR_B_SMELTER_ON, 0, {S(IT_LOG, 12), S(IT_COPPER_ORE, 8)}, 80, K_CRAFT},
    {"Fletcher", "Log = 3 Arrows (ammo for archers).", SPR_B_FLETCHER, -1, 0, {S(IT_LOG, 10), NOCOST}, 60, K_CRAFT},
    {"Sawmill", "Log = 2 Planks.", SPR_B_SAWMILL, SPR_B_SAWMILL_ON, 3, {S(IT_LOG, 12), S(IT_IRON_BAR, 6)}, 70, K_CRAFT},
    {"Kiln", "Clay+Coal = 2 Bricks.", SPR_B_KILN, SPR_B_KILN_ON, 2, {S(IT_STONE, 10), S(IT_LOG, 6)}, 80, K_CRAFT},
    {"Alchemy lab", "2 Herbs+Clay = Potion.", SPR_B_ALCHEMY, SPR_B_ALCHEMY_ON, 2, {S(IT_BRICK, 10), S(IT_LOG, 8)}, 70, K_CRAFT},
    {"Forge", "Iron bar+Coal=Steel. Steel+Plank=3 Bolts.", SPR_B_FORGE, SPR_B_FORGE_ON, 3, {S(IT_STONE, 15), S(IT_IRON_BAR, 10)}, 90, K_CRAFT},
    {"Enchanter", "Aether bar+Potion = Mana crystal.", SPR_B_ENCHANTER, SPR_B_ENCHANTER_ON, 4, {S(IT_BRICK, 15), S(IT_STEEL, 10)}, 90, K_CRAFT},
    {"Archer tower", "Shoots raiders. Feed it Arrows.", SPR_B_ARCHER, -1, 0, {S(IT_LOG, 15), S(IT_BRONZE, 5)}, 120, K_TOWER, 0, IT_ARROW, 4.5f, 0.7f, 7, 0, 24},
    {"Ballista", "Long range, heavy hits. Feed it Bolts.", SPR_B_BALLISTA, -1, 3, {S(IT_PLANK, 10), S(IT_STEEL, 8)}, 160, K_TOWER, 0, IT_BOLT, 6.5f, 1.5f, 30, 0, 12},
    {"Arcane ward", "Blasts every raider nearby. Feed it Mana.", SPR_B_WARD, -1, 4, {S(IT_BRICK, 12), S(IT_AETHER_BAR, 6)}, 160, K_TOWER, 0, IT_MANA, 3.2f, 2.2f, 22, 1, 6},
    {"Stone wall", "Blocks raiders. They must break it.", SPR_B_WALL, -1, 1, {S(IT_STONE, 4), NOCOST}, 150, K_WALL},
    {"Brick wall", "A much tougher wall.", SPR_B_BRICKWALL, -1, 2, {S(IT_BRICK, 4), NOCOST}, 400, K_WALL},
};

const Recipe recipes[] = {
    {B_SMELTER, {S(IT_COPPER_ORE, 1), S(IT_TIN_ORE, 1)}, S(IT_BRONZE, 1), 2.0f},
    {B_SMELTER, {S(IT_IRON_ORE, 1), S(IT_COAL, 1)}, S(IT_IRON_BAR, 1), 2.4f},
    {B_SMELTER, {S(IT_AETHER_ORE, 1), S(IT_COAL, 1)}, S(IT_AETHER_BAR, 1), 3.0f},
    {B_FLETCHER, {S(IT_LOG, 1), NOCOST}, S(IT_ARROW, 3), 1.6f},
    {B_SAWMILL, {S(IT_LOG, 1), NOCOST}, S(IT_PLANK, 2), 1.5f},
    {B_KILN, {S(IT_CLAY, 1), S(IT_COAL, 1)}, S(IT_BRICK, 2), 2.5f},
    {B_ALCHEMY, {S(IT_HERB, 2), S(IT_CLAY, 1)}, S(IT_POTION, 1), 3.0f},
    {B_FORGE, {S(IT_IRON_BAR, 1), S(IT_COAL, 1)}, S(IT_STEEL, 1), 3.0f},
    {B_FORGE, {S(IT_STEEL, 1), S(IT_PLANK, 1)}, S(IT_BOLT, 3), 2.0f},
    {B_ENCHANTER, {S(IT_AETHER_BAR, 1), S(IT_POTION, 1)}, S(IT_MANA, 1), 3.0f},
};
const int recipe_count = sizeof(recipes) / sizeof(recipes[0]);

const EnemyDef enemy_defs[EN_COUNT] = {
    {"Wolf", SPR_EN_WOLF_0, 14, 1.7f, 5},
    {"Goblin", SPR_EN_GOBLIN_0, 28, 1.0f, 9},
    {"Goblin brute", SPR_EN_BRUTE_0, 70, 0.8f, 18},
    {"Skeleton", SPR_EN_SKELETON_0, 55, 0.95f, 14},
    {"Troll", SPR_EN_TROLL_0, 600, 0.5f, 45},
};

#define W(t, n) {t, n}
#define WN {0, 0}

const RegionDef region_defs[REGION_COUNT] = {
    {
        "Meadowfall Valley",
        {"Green fields rich in copper and tin.", "Smelt bronze and fend off wolves.", ""},
        0, 1234,
        /* none cu tin iron coal stone clay aether herb tree */
        {0, 4, 4, 0, 0, 0, 0, 0, 0, 9}, 0.22f,
        {S(IT_LOG, 60), S(IT_COPPER_ORE, 20), S(IT_TIN_ORE, 10), S(IT_BRONZE, 8), NOCOST, NOCOST, NOCOST},
        {S(IT_BRONZE, 40), NOCOST},
        3,
        {{{W(EN_WOLF, 3), WN, WN}}, {{W(EN_WOLF, 5), WN, WN}}, {{W(EN_WOLF, 8), WN, WN}}},
        150, 110, 70, 150,
    },
    {
        "Greystone Hills",
        {"Rolling hills over iron and coal seams.", "Goblins raid from the high camps.", "New: Router, Stone wall."},
        1, 9876,
        {0, 2, 2, 4, 4, 3, 0, 0, 0, 7}, 0.18f,
        {S(IT_LOG, 80), S(IT_BRONZE, 20), S(IT_STONE, 10), S(IT_COPPER_ORE, 10), NOCOST, NOCOST, NOCOST},
        {S(IT_IRON_BAR, 40), S(IT_STONE, 60)},
        4,
        {{{W(EN_WOLF, 4), WN, WN}}, {{W(EN_GOBLIN, 3), W(EN_WOLF, 2), WN}}, {{W(EN_GOBLIN, 5), W(EN_WOLF, 3), WN}},
         {{W(EN_GOBLIN, 7), W(EN_WOLF, 4), WN}}},
        160, 110, 150, 90,
    },
    {
        "Marshwood",
        {"A damp, tangled marsh of clay and herbs.", "Brew potions and fire bricks.", "New: Herb garden, Kiln, Alchemy lab."},
        2, 4242,
        {0, 1, 1, 1, 4, 2, 4, 0, 4, 8}, 0.42f,
        {S(IT_LOG, 80), S(IT_BRONZE, 20), S(IT_STONE, 30), S(IT_BRICK, 12), S(IT_IRON_BAR, 10), S(IT_COPPER_ORE, 10), NOCOST},
        {S(IT_BRICK, 50), S(IT_POTION, 20)},
        4,
        {{{W(EN_GOBLIN, 5), WN, WN}}, {{W(EN_GOBLIN, 5), W(EN_BRUTE, 1), WN}}, {{W(EN_GOBLIN, 6), W(EN_BRUTE, 2), WN}},
         {{W(EN_GOBLIN, 8), W(EN_BRUTE, 3), WN}}},
        170, 120, 245, 150,
    },
    {
        "Frostpeak Pass",
        {"A frozen pass guarding deep iron veins.", "Forge steel and arm the ballistas.", "New: Sawmill, Forge, Ballista."},
        3, 777,
        {0, 1, 1, 4, 5, 3, 1, 0, 0, 8}, 0.22f,
        {S(IT_LOG, 100), S(IT_BRONZE, 20), S(IT_STONE, 40), S(IT_IRON_BAR, 20), S(IT_BRICK, 20), S(IT_COPPER_ORE, 10), NOCOST},
        {S(IT_STEEL, 30), S(IT_BOLT, 40)},
        5,
        {{{W(EN_GOBLIN, 6), W(EN_SKELETON, 1), WN}}, {{W(EN_SKELETON, 3), W(EN_GOBLIN, 4), WN}},
         {{W(EN_SKELETON, 4), W(EN_BRUTE, 2), WN}}, {{W(EN_SKELETON, 6), W(EN_GOBLIN, 6), WN}},
         {{W(EN_SKELETON, 8), W(EN_BRUTE, 3), WN}}},
        180, 120, 330, 75,
    },
    {
        "The Sunken Citadel",
        {"Ruins humming with strange aether.", "Craft mana and survive the final siege.", "New: Enchanter, Arcane ward."},
        4, 31337,
        {0, 1, 1, 2, 4, 2, 2, 4, 2, 5}, 0.30f,
        {S(IT_LOG, 100), S(IT_BRONZE, 20), S(IT_STONE, 40), S(IT_IRON_BAR, 20), S(IT_BRICK, 30), S(IT_STEEL, 15), S(IT_PLANK, 10)},
        {S(IT_MANA, 25), S(IT_AETHER_BAR, 20)},
        6,
        {{{W(EN_SKELETON, 4), W(EN_GOBLIN, 6), WN}}, {{W(EN_SKELETON, 6), W(EN_BRUTE, 3), WN}},
         {{W(EN_SKELETON, 8), W(EN_WOLF, 10), WN}}, {{W(EN_BRUTE, 6), W(EN_SKELETON, 6), WN}},
         {{W(EN_SKELETON, 10), W(EN_BRUTE, 5), WN}}, {{W(EN_TROLL, 1), W(EN_SKELETON, 8), W(EN_BRUTE, 4)}}},
        180, 120, 410, 135,
    },
};
