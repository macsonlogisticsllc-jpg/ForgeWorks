#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;

#define SCREEN_W 480
#define SCREEN_H 272
#define FB_STRIDE 512          /* PSP framebuffer line width in pixels */

#define TILE 20
#define MAP_W 48
#define MAP_H 32

#define DT (1.0f / 60.0f)

/* colours are stored 0xAABBGGRR, the PSP's native 8888 layout */
#define RGB(r, g, b) (0xFF000000u | ((u32)(b) << 16) | ((u32)(g) << 8) | (u32)(r))

/* ---- buttons (platform maps native buttons onto these) ---- */
enum {
    BTN_UP = 1 << 0,
    BTN_DOWN = 1 << 1,
    BTN_LEFT = 1 << 2,
    BTN_RIGHT = 1 << 3,
    BTN_CROSS = 1 << 4,
    BTN_CIRCLE = 1 << 5,
    BTN_SQUARE = 1 << 6,
    BTN_TRIANGLE = 1 << 7,
    BTN_L = 1 << 8,
    BTN_R = 1 << 9,
    BTN_START = 1 << 10,
    BTN_SELECT = 1 << 11,
};

/* ---- platform services (implemented in main_psp.c / tools/headless.c) ---- */
int plat_save(const char *name, const void *data, int size);   /* 1 on success */
int plat_load(const char *name, void *data, int size);         /* 1 on success */
void plat_delete(const char *name);

/* ---- game entry points (implemented in screens.c) ---- */
void game_init(void);
/* advance one 60Hz frame and draw into pixels (stride FB_STRIDE) */
void game_frame(u32 buttons, int analog_x, int analog_y, u32 *pixels);

#endif
