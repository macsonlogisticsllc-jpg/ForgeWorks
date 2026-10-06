#ifndef UI_H
#define UI_H

#include "game.h"

typedef struct {
    int cam_x, cam_y;          /* camera, world pixels */
    int cur_x, cur_y;          /* cursor tile */
    int sel;                   /* index into tools[] */
    int rot;                   /* placement direction */
    int tools[B_COUNT];
    int ntools;
    int info;                  /* info panel visible */
    int speed;                 /* 1 or 2 */
    int tip;                   /* tutorial step, -1 = off */
    char msg[64];
    float msg_t;
    float banner_t;            /* "Raid incoming" banner */
    int last_wave;
} Ui;

extern Ui U;

/* render.c */
void render_world(void);
void render_hud(void);
void render_ground_only(int cam_x, int cam_y);
void render_scene(int cam_x, int cam_y);          /* world without cursor */
const char *tip_text(int step);
void debug_unlock_all(void);

#endif
