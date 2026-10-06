#ifndef GFX_H
#define GFX_H

#include "common.h"
#include "assets.h"

/* sprite draw flags */
enum {
    SF_FLIP = 1,        /* mirror horizontally */
    SF_GHOST_OK = 2,    /* translucent green placement preview */
    SF_GHOST_BAD = 4,   /* translucent red placement preview */
    SF_FLASH = 8,       /* white hit flash */
    SF_DARK = 16,       /* darkened (locked / disabled) */
    SF_HALF = 32,       /* 50% translucent */
};

extern u32 *g_fb;

void gfx_set_target(u32 *pixels);
void gfx_clip(int x, int y, int w, int h);
void gfx_noclip(void);

void gfx_clear(u32 c);
void gfx_fill(int x, int y, int w, int h, u32 c);
void gfx_blend(int x, int y, int w, int h, u32 c, int alpha);   /* alpha 0..255 */
void gfx_rect(int x, int y, int w, int h, u32 c);
void gfx_pixel(int x, int y, u32 c);
void gfx_line(int x0, int y0, int x1, int y1, u32 c);
void gfx_circle(int cx, int cy, int r, u32 c);
void gfx_disc(int cx, int cy, int r, u32 c);

void gfx_sprite(int id, int x, int y, int flags);   /* x,y = top-left of the sprite */
int spr_w(int id);
int spr_h(int id);

/* text: 6x11 bitmap font */
int gfx_text(int x, int y, const char *s, u32 c);             /* returns width */
int gfx_text_shadow(int x, int y, const char *s, u32 c);
void gfx_text_big(int x, int y, const char *s, u32 c, int scale);
int text_w(const char *s);

/* RuneScape-style panel */
void gfx_panel(int x, int y, int w, int h);
void gfx_panel_alpha(int x, int y, int w, int h, int alpha);

/* common UI colours */
#define C_YELLOW RGB(255, 255, 0)
#define C_ORANGE RGB(255, 152, 31)
#define C_WHITE RGB(255, 255, 255)
#define C_RED RGB(255, 60, 40)
#define C_GREEN RGB(60, 230, 60)
#define C_CYAN RGB(0, 255, 255)
#define C_GREY RGB(160, 160, 150)
#define C_BLACK RGB(0, 0, 0)

#endif
