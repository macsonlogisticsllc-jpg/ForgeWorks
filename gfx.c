#include "gfx.h"

u32 *g_fb;
static int cx0 = 0, cy0 = 0, cx1 = SCREEN_W, cy1 = SCREEN_H;

void gfx_set_target(u32 *pixels) { g_fb = pixels; }

void gfx_clip(int x, int y, int w, int h)
{
    cx0 = x < 0 ? 0 : x;
    cy0 = y < 0 ? 0 : y;
    cx1 = x + w > SCREEN_W ? SCREEN_W : x + w;
    cy1 = y + h > SCREEN_H ? SCREEN_H : y + h;
}

void gfx_noclip(void)
{
    cx0 = 0; cy0 = 0; cx1 = SCREEN_W; cy1 = SCREEN_H;
}

static inline u32 mix(u32 a, u32 b, int t)   /* t=0 -> a, 255 -> b */
{
    u32 r = ((a & 0xFF) * (255 - t) + (b & 0xFF) * t) / 255;
    u32 g = (((a >> 8) & 0xFF) * (255 - t) + ((b >> 8) & 0xFF) * t) / 255;
    u32 bl = (((a >> 16) & 0xFF) * (255 - t) + ((b >> 16) & 0xFF) * t) / 255;
    return 0xFF000000u | (bl << 16) | (g << 8) | r;
}

static inline u32 half(u32 a, u32 b)
{
    return (((a & 0xFEFEFEu) >> 1) + ((b & 0xFEFEFEu) >> 1)) | 0xFF000000u;
}

void gfx_clear(u32 c)
{
    for (int y = 0; y < SCREEN_H; y++) {
        u32 *p = g_fb + y * FB_STRIDE;
        for (int x = 0; x < SCREEN_W; x++) p[x] = c;
    }
}

void gfx_fill(int x, int y, int w, int h, u32 c)
{
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    for (int yy = y0; yy < y1; yy++) {
        u32 *p = g_fb + yy * FB_STRIDE;
        for (int xx = x0; xx < x1; xx++) p[xx] = c;
    }
}

void gfx_blend(int x, int y, int w, int h, u32 c, int alpha)
{
    int x0 = x < cx0 ? cx0 : x, y0 = y < cy0 ? cy0 : y;
    int x1 = x + w > cx1 ? cx1 : x + w, y1 = y + h > cy1 ? cy1 : y + h;
    for (int yy = y0; yy < y1; yy++) {
        u32 *p = g_fb + yy * FB_STRIDE;
        for (int xx = x0; xx < x1; xx++) p[xx] = mix(p[xx], c, alpha);
    }
}

void gfx_pixel(int x, int y, u32 c)
{
    if (x >= cx0 && x < cx1 && y >= cy0 && y < cy1) g_fb[y * FB_STRIDE + x] = c;
}

void gfx_rect(int x, int y, int w, int h, u32 c)
{
    gfx_fill(x, y, w, 1, c);
    gfx_fill(x, y + h - 1, w, 1, c);
    gfx_fill(x, y, 1, h, c);
    gfx_fill(x + w - 1, y, 1, h, c);
}

void gfx_line(int x0, int y0, int x1, int y1, u32 c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int guard = 0; guard < 2000; guard++) {
        gfx_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_circle(int cx, int cy, int r, u32 c)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        gfx_pixel(cx + x, cy + y, c); gfx_pixel(cx + y, cy + x, c);
        gfx_pixel(cx - y, cy + x, c); gfx_pixel(cx - x, cy + y, c);
        gfx_pixel(cx - x, cy - y, c); gfx_pixel(cx - y, cy - x, c);
        gfx_pixel(cx + y, cy - x, c); gfx_pixel(cx + x, cy - y, c);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

void gfx_disc(int cx, int cy, int r, u32 c)
{
    for (int y = -r; y <= r; y++) {
        int w = 0;
        while ((w + 1) * (w + 1) + y * y <= r * r) w++;
        gfx_fill(cx - w, cy + y, 2 * w + 1, 1, c);
    }
}

int spr_w(int id) { return asset_sprites[id].w; }
int spr_h(int id) { return asset_sprites[id].h; }

void gfx_sprite(int id, int x, int y, int flags)
{
    const SpriteDef *s = &asset_sprites[id];
    const u8 *src = asset_pixels + s->off;
    int w = s->w, h = s->h;
    int sx0 = 0, sy0 = 0, sx1 = w, sy1 = h;
    if (x < cx0) sx0 = cx0 - x;
    if (y < cy0) sy0 = cy0 - y;
    if (x + w > cx1) sx1 = cx1 - x;
    if (y + h > cy1) sy1 = cy1 - y;
    if (sx0 >= sx1 || sy0 >= sy1) return;
    u32 tint = (flags & SF_GHOST_BAD) ? RGB(255, 40, 30) : RGB(60, 255, 60);
    for (int yy = sy0; yy < sy1; yy++) {
        u32 *dst = g_fb + (y + yy) * FB_STRIDE + x;
        const u8 *row = src + yy * w;
        for (int xx = sx0; xx < sx1; xx++) {
            int ix = (flags & SF_FLIP) ? (w - 1 - xx) : xx;
            u8 i = row[ix];
            if (!i) continue;
            u32 c = asset_palette[i];
            if (flags & SF_FLASH) c = half(c, 0xFFFFFFFFu);
            if (flags & SF_DARK) c = mix(c, RGB(20, 18, 14), 170);
            if (flags & (SF_GHOST_OK | SF_GHOST_BAD)) c = half(half(c, tint), dst[xx]);
            else if (flags & SF_HALF) c = half(c, dst[xx]);
            dst[xx] = c;
        }
    }
}

/* ---------------------------------------------------------------- text */
int text_w(const char *s) { return (int)strlen(s) * FONT_W; }

static void glyph(int x, int y, int ch, u32 c, int scale)
{
    if (ch < 32 || ch > 126) ch = '?';
    const u8 *g = asset_font + (ch - 32) * FONT_H;
    for (int row = 0; row < FONT_H; row++) {
        u8 bits = g[row];
        if (!bits) continue;
        for (int col = 0; col < FONT_W; col++)
            if (bits & (1 << col)) {
                if (scale == 1) gfx_pixel(x + col, y + row, c);
                else gfx_fill(x + col * scale, y + row * scale, scale, scale, c);
            }
    }
}

int gfx_text(int x, int y, const char *s, u32 c)
{
    int x0 = x;
    for (; *s; s++, x += FONT_W) glyph(x, y, (unsigned char)*s, c, 1);
    return x - x0;
}

int gfx_text_shadow(int x, int y, const char *s, u32 c)
{
    gfx_text(x + 1, y + 1, s, C_BLACK);
    return gfx_text(x, y, s, c);
}

void gfx_text_big(int x, int y, const char *s, u32 c, int scale)
{
    for (const char *p = s; *p; p++) {
        int gx = x + (int)(p - s) * FONT_W * scale;
        /* thick dark outline */
        for (int oy = -1; oy <= 2; oy++)
            for (int ox = -1; ox <= 2; ox++)
                if (ox || oy) glyph(gx + ox * (scale / 2 + 1), y + oy * (scale / 2 + 1), (unsigned char)*p, RGB(30, 18, 8), scale);
    }
    for (const char *p = s; *p; p++) glyph(x + (int)(p - s) * FONT_W * scale, y, (unsigned char)*p, c, scale);
}

/* ---------------------------------------------------------------- panels */
void gfx_panel_alpha(int x, int y, int w, int h, int alpha)
{
    gfx_blend(x + 2, y + 2, w - 4, h - 4, RGB(62, 53, 41), alpha);
    gfx_rect(x, y, w, h, RGB(24, 20, 14));
    gfx_rect(x + 1, y + 1, w - 2, h - 2, RGB(110, 96, 70));
    gfx_fill(x + 2, y + h - 3, w - 4, 1, RGB(40, 34, 25));
}

void gfx_panel(int x, int y, int w, int h) { gfx_panel_alpha(x, y, w, h, 255); }
