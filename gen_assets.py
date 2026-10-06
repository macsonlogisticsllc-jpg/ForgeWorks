#!/usr/bin/env python3
"""
Forgeworks asset generator.

Draws every sprite in the game procedurally (chunky, flat-shaded "low-poly"
style), packs them into a 255-colour palette and writes:
    src/assets.h   - sprite ids + declarations
    src/assets.c   - palette, sprite table, pixel data, bitmap font
    tools/atlas_preview.png - contact sheet for checking the art

Run:  python3 tools/gen_assets.py
"""
import os, random, sys
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")

sprites = []          # (name, image, oy)   oy = pixels the sprite rises above its tile's top edge


def add(name, img, oy=None):
    if oy is None:
        oy = max(0, img.height - 20)
    sprites.append((name, img, oy))


def C(h):
    return tuple(int(h[i:i + 2], 16) for i in (1, 3, 5)) + (255,)


def sh(c, f):
    return tuple(max(0, min(255, int(round(v * f)))) for v in c[:3]) + (255,)


def new(w, h):
    return Image.new("RGBA", (w, h), (0, 0, 0, 0))


def shadow(d, cx, cy, rx, ry):
    d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=(20, 24, 14, 255))


# ------------------------------------------------------------------ palette
WOOD, WOOD_D, WOOD_L = C("#7a5230"), C("#553820"), C("#9c6c40")
STONE, STONE_D, STONE_L = C("#8a8680"), C("#605d58"), C("#b0aca4")
THATCH, THATCH_D, THATCH_L = C("#b08a48"), C("#86662e"), C("#cca660")
RED, RED_D = C("#9a3e2e"), C("#6a2820")
GLOW, GLOW_HOT, GLOW_DIM = C("#ff9a28"), C("#fff27a"), C("#a8401c")
IRONC, IRONC_L = C("#55585f"), C("#8e939c")
BLACK = C("#16120e")
GOLD = C("#e8c040")

# ------------------------------------------------------------------ ground
THEMES = [
    # name,      ground,     water,      tree
    ("meadow", "#5a8f35", "#3d6ea8", "oak"),
    ("hills", "#77874a", "#3b68a0", "oak2"),
    ("marsh", "#4a6a3c", "#36606e", "willow"),
    ("frost", "#dde6ec", "#6a98c0", "pine"),
    ("citadel", "#4c4658", "#26203a", "dead"),
]


def ground_tile(base, seed, theme):
    r = random.Random(seed)
    b = C(base)
    im = Image.new("RGBA", (20, 20), b)
    px = im.load()
    for y in range(20):
        for x in range(20):
            v = r.random()
            if v < 0.10:
                px[x, y] = sh(b, 0.88)
            elif v < 0.16:
                px[x, y] = sh(b, 1.10)
    if theme == "citadel":
        # flagstone cracks
        d = ImageDraw.Draw(im)
        dk = sh(b, 0.72)
        d.line([0, 9, 19, 9], fill=dk)
        x0 = 6 + r.randrange(6)
        d.line([x0, 0, x0, 8], fill=dk)
        x1 = 3 + r.randrange(12)
        d.line([x1, 10, x1, 19], fill=dk)
        d.line([0, 19, 19, 19], fill=sh(b, 0.8))
    elif theme == "frost":
        for _ in range(4):
            x, y = r.randrange(20), r.randrange(20)
            px[x, y] = C("#b8c8d6")
    else:
        for _ in range(4):           # grass tufts
            x, y = r.randrange(1, 19), r.randrange(2, 19)
            px[x, y] = sh(b, 1.28)
            px[x, y - 1] = sh(b, 1.18)
            if x + 1 < 20:
                px[x + 1, y] = sh(b, 0.80)
        if seed % 3 == 0 and theme == "meadow":
            x, y = r.randrange(2, 18), r.randrange(2, 18)
            px[x, y] = C("#f0e070")       # little flower
    return im


def water_tile(base, frame, seed):
    b = C(base)
    r = random.Random(seed)
    im = Image.new("RGBA", (20, 20), b)
    d = ImageDraw.Draw(im)
    for _ in range(4):
        x = r.randrange(0, 16)
        y = r.randrange(1, 19)
        x = (x + frame * 3) % 20
        d.line([x, y, min(19, x + 3), y], fill=sh(b, 1.22))
        if r.random() < 0.5:
            d.point([x + 1, y + 1], fill=sh(b, 0.86))
    return im


for ti, (tname, gcol, wcol, _) in enumerate(THEMES):
    for v in range(3):
        add(f"GROUND_{ti}_{v}", ground_tile(gcol, ti * 31 + v * 7 + 1, tname), 0)
    for f in range(2):
        add(f"WATER_{ti}_{f}", water_tile(wcol, f, ti * 13 + 5), 0)


# ------------------------------------------------------------------ rocks / deposits
def facet_rock(d, cx, by, w, h, col, light=1.22, dark=0.72):
    """Low-poly rock: a 5-point mound split into lit/shadow facets."""
    L, R, T = cx - w, cx + w, by - h
    p = [(L, by), (L + w * 0.35, by - h * 0.7), (cx - w * 0.1, T), (R - w * 0.3, by - h * 0.65), (R, by)]
    d.polygon(p, fill=col)
    d.polygon([p[0], p[1], p[2], (cx - w * 0.1, by - h * 0.25)], fill=sh(col, light))
    d.polygon([p[2], p[3], p[4], (cx + w * 0.1, by)], fill=sh(col, dark))
    d.line([p[0], p[4]], fill=sh(col, 0.55))


def deposit(ore_col, rock_col="#7c7872", seed=0, chunk=True):
    r = random.Random(seed)
    im = new(20, 22)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 19, 9, 2)
    facet_rock(d, 6, 19, 5, 7, C(rock_col))
    facet_rock(d, 13, 20, 6, 10, C(rock_col))
    facet_rock(d, 9, 21, 4, 5, sh(C(rock_col), 0.92))
    if chunk:
        oc = C(ore_col)
        for (x, y) in [(5, 15), (12, 13), (15, 17), (9, 18), (13, 18)]:
            x += r.randrange(-1, 2)
            d.polygon([(x, y), (x + 2, y - 1), (x + 3, y + 1), (x + 1, y + 2)], fill=oc)
            d.point([x + 1, y], fill=sh(oc, 1.35))
    return im


add("ORE_COPPER", deposit("#d07a38", seed=1))
add("ORE_TIN", deposit("#c4d0dc", seed=2))
add("ORE_IRON", deposit("#9a4a32", seed=3))
add("ORE_COAL", deposit("#1c1a1a", "#6a6660", seed=4))


def stone_outcrop():
    im = new(20, 24)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 21, 9, 2)
    facet_rock(d, 7, 21, 6, 13, C("#9a968e"))
    facet_rock(d, 14, 22, 5, 9, C("#8c8880"))
    facet_rock(d, 4, 22, 3, 4, C("#a4a098"))
    return im


add("ORE_STONE", stone_outcrop())


def clay_pit():
    im = new(20, 20)
    d = ImageDraw.Draw(im)
    d.ellipse([1, 5, 19, 18], fill=C("#7a4a2a"))
    d.ellipse([3, 6, 17, 16], fill=C("#a0603a"))
    for (x, y) in [(6, 9), (11, 8), (13, 12), (7, 13)]:
        d.ellipse([x, y, x + 3, y + 2], fill=C("#c07a4c"))
        d.point([x + 1, y], fill=C("#d89060"))
    return im


add("ORE_CLAY", clay_pit(), 0)


def aether():
    im = new(20, 24)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 21, 9, 2)
    facet_rock(d, 10, 22, 9, 7, C("#3a3446"))
    for (x, h, c) in [(6, 12, "#7ae0f0"), (11, 16, "#b07af0"), (15, 10, "#7ae0f0")]:
        cc = C(c)
        d.polygon([(x - 2, 19), (x, 19 - h), (x + 2, 19)], fill=cc)
        d.polygon([(x, 19 - h), (x + 2, 19), (x, 19)], fill=sh(cc, 0.7))
        d.point([x - 1, 18 - h // 2], fill=(255, 255, 255, 255))
    return im


add("ORE_AETHER", aether())


def herbs():
    im = new(20, 20)
    d = ImageDraw.Draw(im)
    d.ellipse([1, 6, 19, 18], fill=C("#4a3420"))
    r = random.Random(7)
    for (x, y) in [(5, 9), (10, 8), (14, 10), (7, 13), (12, 14), (16, 15), (3, 14)]:
        g = C("#3fae3a")
        d.polygon([(x, y + 3), (x - 2, y), (x, y + 1)], fill=g)
        d.polygon([(x, y + 3), (x + 2, y - 1), (x + 1, y + 2)], fill=sh(g, 0.75))
        d.line([x, y + 3, x, y - 1], fill=sh(g, 1.2))
        if r.random() < 0.6:
            d.point([x, y - 2], fill=C("#e070d0"))
    return im


add("ORE_HERB", herbs(), 0)


# ------------------------------------------------------------------ trees
def tree_oak(dark=1.0, seed=0):
    im = new(20, 34)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 31, 8, 2)
    d.rectangle([8, 22, 11, 31], fill=WOOD)
    d.rectangle([10, 22, 11, 31], fill=WOOD_D)
    g = sh(C("#3f8a2c"), dark)
    # faceted canopy built from triangles
    cx, cy = 10, 14
    pts = [(cx, 1), (cx + 8, 6), (cx + 9, 15), (cx + 5, 23), (cx - 5, 23), (cx - 9, 15), (cx - 8, 6)]
    d.polygon(pts, fill=g)
    d.polygon([(cx, 1), (cx - 8, 6), (cx - 9, 15), (cx - 1, 12)], fill=sh(g, 1.25))
    d.polygon([(cx, 1), (cx + 8, 6), (cx - 1, 12)], fill=sh(g, 1.10))
    d.polygon([(cx + 8, 6), (cx + 9, 15), (cx + 5, 23), (cx - 1, 12)], fill=sh(g, 0.78))
    d.polygon([(cx - 9, 15), (cx - 5, 23), (cx + 5, 23), (cx - 1, 12)], fill=sh(g, 0.90))
    d.polygon([(cx + 5, 23), (cx + 9, 15), (cx + 2, 20)], fill=sh(g, 0.65))
    return im


def tree_willow():
    im = new(20, 34)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 31, 8, 2)
    d.rectangle([8, 20, 11, 31], fill=C("#5a4632"))
    g = C("#556b2a")
    d.polygon([(10, 2), (18, 9), (19, 24), (1, 24), (2, 9)], fill=g)
    d.polygon([(10, 2), (2, 9), (1, 24), (6, 24), (8, 10)], fill=sh(g, 1.2))
    d.polygon([(10, 2), (18, 9), (19, 24), (14, 24), (12, 10)], fill=sh(g, 0.75))
    for x in range(2, 19, 3):          # hanging strands
        d.line([x, 16, x, 26], fill=sh(g, 0.9 if x % 2 else 1.1))
    return im


def tree_pine():
    im = new(20, 36)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 33, 7, 2)
    d.rectangle([9, 27, 11, 33], fill=WOOD_D)
    g = C("#2e5e3a")
    for (top, bot, w) in [(1, 13, 5), (7, 20, 7), (13, 28, 9)]:
        d.polygon([(10, top), (10 + w, bot), (10 - w, bot)], fill=g)
        d.polygon([(10, top), (10 - w, bot), (9, bot)], fill=sh(g, 1.25))
        d.polygon([(10, top), (10 - w // 2 - 1, top + (bot - top) // 2), (10 + w // 2 + 1, top + (bot - top) // 2)],
                  fill=C("#eef4f8"))
    return im


def tree_dead():
    im = new(20, 32)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 29, 6, 2)
    c = C("#5e5466")
    d.polygon([(8, 29), (12, 29), (11, 12), (9, 12)], fill=c)
    d.line([10, 14, 4, 6], fill=c, width=2)
    d.line([4, 6, 2, 7], fill=c)
    d.line([10, 12, 15, 4], fill=c, width=2)
    d.line([15, 4, 18, 5], fill=c)
    d.line([11, 18, 16, 13], fill=c)
    d.line([9, 20, 5, 16], fill=sh(c, 0.8))
    d.polygon([(10, 29), (12, 29), (11, 12)], fill=sh(c, 0.7))
    return im


add("TREE_OAK", tree_oak())
add("TREE_OAK2", tree_oak(0.85))
add("TREE_WILLOW", tree_willow())
add("TREE_PINE", tree_pine())
add("TREE_DEAD", tree_dead())


# ------------------------------------------------------------------ building helpers
def house_roof(d, x0, x1, y_eave, h, col):
    """Front-facing pitched roof slope (ridge along x)."""
    d.polygon([(x0 - 1, y_eave), (x1 + 1, y_eave), (x1 - 1, y_eave - h), (x0 + 1, y_eave - h)], fill=col)
    d.line([x0 + 1, y_eave - h, x1 - 1, y_eave - h], fill=sh(col, 1.3))
    d.line([x0 - 1, y_eave, x1 + 1, y_eave], fill=sh(col, 0.65))
    for yy in range(y_eave - h + 2, y_eave, 2):
        d.line([x0 + 1, yy, x1 - 1, yy], fill=sh(col, 0.88))


def stone_box(d, x0, y0, x1, y1, top_h, col=None):
    col = col or STONE
    d.rectangle([x0, y0 - top_h, x1, y0 - 1], fill=sh(col, 1.2))
    d.rectangle([x0, y0, x1, y1], fill=col)
    # masonry
    for yy in range(y0 + 2, y1, 3):
        d.line([x0, yy, x1, yy], fill=sh(col, 0.85))
        off = 0 if (yy // 3) % 2 else 2
        for xx in range(x0 + off, x1, 4):
            d.point([xx, yy + 1], fill=sh(col, 0.85))
    d.line([x1, y0, x1, y1], fill=sh(col, 0.75))


def flame(d, x, y, active):
    if active:
        d.polygon([(x - 2, y), (x, y - 4), (x + 2, y)], fill=GLOW)
        d.polygon([(x - 1, y), (x, y - 2), (x + 1, y)], fill=GLOW_HOT)
    else:
        d.rectangle([x - 2, y - 1, x + 2, y], fill=GLOW_DIM)


# ------------------------------------------------------------------ buildings
def b_keep():
    im = new(60, 86)
    d = ImageDraw.Draw(im)
    d.ellipse([2, 74, 58, 85], fill=(20, 24, 14, 255))
    # curtain wall
    stone_box(d, 4, 44, 55, 79, 8)
    for x in range(4, 56, 6):
        d.rectangle([x, 33, x + 3, 36], fill=STONE_L)
        d.rectangle([x, 36, x + 3, 37], fill=STONE)
    # central keep tower
    stone_box(d, 19, 22, 40, 50, 5, C("#948f88"))
    for x in range(19, 41, 5):
        d.rectangle([x, 13, x + 2, 17], fill=STONE_L)
    # roof spire
    d.polygon([(22, 13), (38, 13), (30, 0)], fill=RED)
    d.polygon([(30, 0), (38, 13), (30, 13)], fill=RED_D)
    d.line([30, 0, 30, -4], fill=WOOD_D)
    # banner
    d.rectangle([24, 26, 28, 38], fill=C("#b02828"))
    d.polygon([(24, 38), (26, 41), (28, 38)], fill=C("#b02828"))
    d.rectangle([25, 30, 27, 32], fill=GOLD)
    d.rectangle([32, 26, 36, 38], fill=C("#b02828"))
    d.polygon([(32, 38), (34, 41), (36, 38)], fill=C("#b02828"))
    d.rectangle([33, 30, 35, 32], fill=GOLD)
    # side towers
    for tx in (0, 46):
        stone_box(d, tx, 34, tx + 13, 80, 5, C("#86827b"))
        for x in range(tx, tx + 14, 4):
            d.rectangle([x, 26, x + 2, 29], fill=STONE_L)
        d.rectangle([tx + 5, 42, tx + 7, 47], fill=BLACK)
        d.rectangle([tx + 5, 56, tx + 7, 61], fill=BLACK)
    # gate
    d.rectangle([24, 62, 35, 79], fill=WOOD_D)
    d.ellipse([24, 57, 35, 68], fill=WOOD_D)
    for x in range(25, 35, 3):
        d.line([x, 60, x, 79], fill=WOOD)
    d.line([24, 70, 35, 70], fill=IRONC)
    return im


add("KEEP", b_keep(), 26)


def b_mine():
    im = new(20, 30)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 27, 9, 2)
    facet_rock(d, 10, 28, 10, 15, C("#7a766e"))
    d.ellipse([6, 16, 14, 28], fill=BLACK)
    d.rectangle([6, 22, 14, 28], fill=BLACK)
    d.rectangle([4, 15, 5, 28], fill=WOOD)
    d.rectangle([15, 15, 16, 28], fill=WOOD_D)
    d.rectangle([3, 13, 17, 15], fill=WOOD_L)
    d.point([10, 18], fill=GLOW_HOT)
    # headframe wheel
    d.line([7, 13, 10, 4], fill=WOOD_D)
    d.line([13, 13, 10, 4], fill=WOOD_D)
    d.ellipse([7, 1, 13, 7], outline=IRONC)
    d.point([10, 4], fill=IRONC_L)
    # little cart
    d.rectangle([13, 24, 19, 27], fill=IRONC)
    d.rectangle([14, 22, 18, 24], fill=C("#d07a38"))
    return im


def b_woodcutter():
    im = new(20, 30)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 27, 9, 2)
    d.rectangle([2, 16, 17, 27], fill=WOOD)
    for y in range(17, 28, 2):
        d.line([2, y, 17, y], fill=WOOD_D)
    d.rectangle([8, 20, 11, 27], fill=C("#3a2414"))
    d.point([10, 23], fill=GOLD)
    house_roof(d, 2, 17, 16, 9, THATCH)
    d.rectangle([13, 4, 15, 9], fill=STONE_D)
    # stump + axe
    d.rectangle([15, 25, 19, 28], fill=WOOD_L)
    d.rectangle([15, 25, 19, 25], fill=THATCH_L)
    d.line([17, 25, 19, 21], fill=WOOD_D)
    d.polygon([(18, 20), (20, 21), (19, 23)], fill=IRONC_L)
    return im


def b_herbgarden():
    im = new(20, 24)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 21, 9, 2)
    d.rectangle([1, 10, 18, 20], fill=WOOD)
    d.rectangle([1, 6, 18, 10], fill=C("#4a3420"))
    d.line([1, 6, 18, 6], fill=WOOD_L)
    d.line([1, 14, 18, 14], fill=WOOD_D)
    for x in range(3, 18, 3):
        g = C("#3fae3a")
        d.polygon([(x, 8), (x - 1, 3), (x + 1, 4)], fill=g)
        d.point([x + 1, 3], fill=sh(g, 1.3))
        if x % 2:
            d.point([x, 2], fill=C("#e070d0"))
    return im


def b_smelter(active):
    im = new(20, 32)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 29, 9, 2)
    d.rectangle([13, 2, 16, 14], fill=STONE_D)
    d.rectangle([13, 2, 16, 3], fill=STONE_L)
    stone_box(d, 1, 15, 18, 29, 5)
    d.ellipse([5, 18, 14, 27], fill=BLACK)
    d.rectangle([5, 23, 14, 28], fill=BLACK)
    if active:
        d.ellipse([6, 20, 13, 27], fill=GLOW)
        d.rectangle([6, 24, 13, 28], fill=GLOW)
        d.rectangle([8, 24, 11, 28], fill=GLOW_HOT)
    else:
        d.rectangle([6, 25, 13, 28], fill=GLOW_DIM)
    return im


def b_fletcher():
    im = new(20, 28)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 25, 9, 2)
    # awning on posts
    d.rectangle([2, 8, 3, 25], fill=WOOD_D)
    d.rectangle([16, 8, 17, 25], fill=WOOD_D)
    for i, x in enumerate(range(1, 19, 3)):
        c = C("#d8cfb4") if i % 2 == 0 else C("#a83a2a")
        d.polygon([(x, 4), (x + 3, 4), (x + 3, 9), (x, 9)], fill=c)
    d.line([1, 9, 18, 9], fill=sh(C("#a83a2a"), 0.7))
    # bench
    d.rectangle([3, 17, 16, 19], fill=WOOD_L)
    d.rectangle([4, 20, 5, 25], fill=WOOD)
    d.rectangle([14, 20, 15, 25], fill=WOOD)
    # arrows bundle + bow
    for x in range(6, 12, 2):
        d.line([x, 16, x + 3, 11], fill=C("#d8b070"))
        d.point([x + 3, 11], fill=IRONC_L)
        d.point([x, 16], fill=C("#f0f0f0"))
    d.arc([10, 10, 17, 24], 270, 90, fill=WOOD_D)
    d.line([14, 10, 14, 24], fill=C("#e8e0d0"))
    return im


def b_sawmill(active):
    im = new(20, 28)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 25, 9, 2)
    d.rectangle([1, 9, 2, 25], fill=WOOD_D)
    d.rectangle([17, 9, 18, 25], fill=WOOD_D)
    house_roof(d, 1, 18, 10, 7, WOOD_L)
    d.rectangle([2, 19, 17, 21], fill=WOOD)
    d.rectangle([3, 16, 9, 18], fill=C("#b48a58"))   # log
    d.ellipse([2, 15, 5, 19], fill=C("#d8b880"))
    # saw blade
    d.ellipse([10, 12, 17, 19], fill=IRONC_L)
    d.ellipse([12, 14, 15, 17], fill=IRONC)
    import math
    for k in range(8):
        a = k * math.pi / 4 + (math.pi / 8 if active else 0)
        x = 13.5 + math.cos(a) * 4.4
        y = 15.5 + math.sin(a) * 4.4
        d.point([round(x), round(y)], fill=C("#dfe4ea"))
    return im


def b_kiln(active):
    im = new(20, 30)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 27, 9, 2)
    br = C("#9a5236")
    d.rectangle([12, 3, 15, 12], fill=sh(br, 0.8))
    d.pieslice([1, 8, 18, 40], 180, 360, fill=br)
    d.rectangle([1, 24, 18, 27], fill=br)
    d.pieslice([1, 8, 18, 40], 180, 250, fill=sh(br, 1.2))
    for y in range(13, 27, 3):
        d.line([2, y, 17, y], fill=sh(br, 0.8))
    d.pieslice([6, 18, 13, 32], 180, 360, fill=BLACK)
    d.rectangle([6, 25, 13, 27], fill=BLACK)
    if active:
        d.pieslice([7, 20, 12, 32], 180, 360, fill=GLOW)
        d.rectangle([7, 25, 12, 27], fill=GLOW)
        d.rectangle([9, 24, 10, 27], fill=GLOW_HOT)
    else:
        d.rectangle([7, 26, 12, 27], fill=GLOW_DIM)
    return im


def b_alchemy(active):
    im = new(20, 32)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 29, 9, 2)
    stone_box(d, 2, 16, 17, 28, 2, C("#8e8a96"))
    d.polygon([(0, 16), (19, 16), (10, 1)], fill=C("#6a3a8a"))
    d.polygon([(10, 1), (19, 16), (10, 16)], fill=C("#4a2864"))
    d.point([10, 0], fill=GOLD)
    d.rectangle([4, 19, 7, 23], fill=C("#d8c070"))   # window
    # cauldron
    d.ellipse([9, 22, 18, 30], fill=C("#2a2a30"))
    gc = C("#7af04a") if active else C("#4a9a30")
    d.ellipse([10, 22, 17, 25], fill=gc)
    if active:
        d.point([12, 20], fill=gc)
        d.point([15, 18], fill=gc)
    return im


def b_forge(active):
    im = new(20, 30)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 27, 9, 2)
    d.rectangle([2, 3, 5, 14], fill=STONE_D)
    stone_box(d, 0, 14, 12, 27, 4)
    d.rectangle([3, 19, 9, 24], fill=BLACK)
    if active:
        d.rectangle([4, 20, 8, 24], fill=GLOW)
        d.rectangle([5, 22, 7, 24], fill=GLOW_HOT)
    else:
        d.rectangle([4, 23, 8, 24], fill=GLOW_DIM)
    # anvil
    d.rectangle([12, 20, 19, 22], fill=IRONC)
    d.rectangle([12, 20, 19, 20], fill=IRONC_L)
    d.polygon([(11, 20), (12, 20), (12, 22)], fill=IRONC)
    d.rectangle([14, 23, 17, 27], fill=sh(IRONC, 0.8))
    d.rectangle([13, 26, 18, 27], fill=IRONC)
    if active:
        d.rectangle([14, 19, 16, 19], fill=GLOW)
    return im


def b_enchanter(active):
    im = new(20, 34)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 31, 9, 2)
    stone_box(d, 1, 25, 18, 31, 3, C("#6a6478"))
    stone_box(d, 4, 20, 15, 24, 2, C("#7a748a"))
    c = C("#b07af0") if active else C("#7a4ab0")
    d.polygon([(10, 3), (14, 10), (10, 17), (6, 10)], fill=c)
    d.polygon([(10, 3), (14, 10), (10, 10)], fill=sh(c, 1.3))
    d.polygon([(10, 17), (14, 10), (10, 10)], fill=sh(c, 0.7))
    if active:
        for (x, y) in [(4, 6), (16, 8), (5, 14), (15, 15)]:
            d.point([x, y], fill=C("#e0c8ff"))
    d.point([8, 28], fill=C("#7ae0f0"))
    d.point([12, 28], fill=C("#7ae0f0"))
    return im


def b_archer():
    im = new(20, 42)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 39, 9, 2)
    # legs
    d.polygon([(3, 39), (5, 39), (7, 16), (5, 16)], fill=WOOD)
    d.polygon([(15, 39), (17, 39), (15, 16), (13, 16)], fill=WOOD_D)
    d.line([5, 36, 15, 24], fill=WOOD_D)
    d.line([15, 36, 5, 24], fill=WOOD)
    d.line([5, 30, 15, 30], fill=WOOD_D)
    # platform cabin
    d.rectangle([2, 13, 17, 20], fill=WOOD_L)
    for x in range(3, 17, 3):
        d.line([x, 13, x, 20], fill=WOOD)
    d.rectangle([2, 20, 17, 21], fill=WOOD_D)
    # archer head
    d.rectangle([9, 9, 11, 12], fill=C("#e0b090"))
    d.rectangle([8, 8, 12, 9], fill=C("#3a6a2a"))
    # roof
    d.polygon([(0, 11), (19, 11), (10, 1)], fill=RED)
    d.polygon([(10, 1), (19, 11), (10, 11)], fill=RED_D)
    return im


def b_ballista():
    im = new(20, 26)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 23, 9, 2)
    d.rectangle([3, 15, 16, 23], fill=WOOD)
    d.rectangle([3, 13, 16, 15], fill=WOOD_L)
    d.rectangle([4, 21, 6, 24], fill=WOOD_D)
    d.rectangle([13, 21, 15, 24], fill=WOOD_D)
    # bow arms
    d.arc([1, 4, 19, 16], 180, 360, fill=WOOD_D, width=2)
    d.line([2, 10, 10, 13], fill=C("#e8e0d0"))
    d.line([18, 10, 10, 13], fill=C("#e8e0d0"))
    # bolt
    d.line([10, 14, 10, 2], fill=IRONC_L, width=1)
    d.polygon([(8, 3), (12, 3), (10, 0)], fill=IRONC_L)
    return im


def b_ward(active=True):
    im = new(20, 40)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 37, 8, 2)
    stone_box(d, 3, 32, 16, 37, 2, C("#6a6478"))
    stone_box(d, 6, 14, 13, 31, 2, C("#7e788c"))
    d.point([9, 20], fill=C("#7ae0f0"))
    d.point([9, 25], fill=C("#b07af0"))
    c = C("#7ae0f0")
    d.polygon([(9, 1), (13, 6), (9, 11), (5, 6)], fill=c)
    d.polygon([(9, 1), (13, 6), (9, 6)], fill=sh(c, 1.25))
    d.polygon([(9, 11), (13, 6), (9, 6)], fill=sh(c, 0.7))
    return im


def b_wall(brick=False):
    im = new(20, 28)
    d = ImageDraw.Draw(im)
    if brick:
        col = C("#a0583a")
        d.rectangle([0, 8, 19, 27], fill=col)
        d.rectangle([0, 4, 19, 8], fill=sh(col, 1.2))
        for x in range(0, 20, 5):
            d.rectangle([x, 1, x + 2, 4], fill=sh(col, 1.2))
        for yy in range(10, 27, 3):
            d.line([0, yy, 19, yy], fill=sh(col, 0.75))
            off = 0 if (yy // 3) % 2 else 2
            for xx in range(off, 20, 5):
                d.point([xx, yy + 1], fill=sh(col, 0.75))
                d.point([xx, yy + 2], fill=sh(col, 0.75))
    else:
        stone_box(d, 0, 10, 19, 27, 6, C("#8c8880"))
    d.line([19, 4, 19, 27], fill=(40, 36, 30, 255))
    return im


def b_router():
    im = new(20, 24)
    d = ImageDraw.Draw(im)
    shadow(d, 10, 21, 9, 2)
    d.rectangle([2, 10, 17, 21], fill=WOOD)
    d.rectangle([2, 4, 17, 10], fill=WOOD_L)
    d.line([2, 10, 17, 21], fill=WOOD_D)
    d.line([2, 21, 17, 10], fill=WOOD_D)
    d.rectangle([2, 10, 17, 21], outline=WOOD_D)
    for (x, y) in [(2, 10), (16, 10), (2, 20), (16, 20)]:
        d.rectangle([x, y, x + 1, y + 1], fill=IRONC_L)
    a = C("#f0d060")
    d.polygon([(9, 5), (10, 5), (9, 4)], fill=a)
    d.polygon([(4, 7), (6, 6), (6, 8)], fill=a)
    d.polygon([(15, 7), (13, 6), (13, 8)], fill=a)
    d.polygon([(9, 9), (11, 9), (10, 10)], fill=a)
    return im


def b_junction():
    im = new(20, 20)
    d = ImageDraw.Draw(im)
    d.rectangle([1, 1, 18, 18], fill=WOOD_D)
    d.rectangle([2, 2, 17, 17], fill=WOOD)
    for k in (6, 13):
        d.line([0, k, 19, k], fill=IRONC_L)
        d.line([k, 0, k, 19], fill=IRONC_L)
    d.rectangle([8, 8, 11, 11], fill=IRONC)
    return im


add("B_MINE", b_mine())
add("B_WOODCUTTER", b_woodcutter())
add("B_HERBGARDEN", b_herbgarden())
add("B_SMELTER", b_smelter(False))
add("B_SMELTER_ON", b_smelter(True))
add("B_FLETCHER", b_fletcher())
add("B_SAWMILL", b_sawmill(False))
add("B_SAWMILL_ON", b_sawmill(True))
add("B_KILN", b_kiln(False))
add("B_KILN_ON", b_kiln(True))
add("B_ALCHEMY", b_alchemy(False))
add("B_ALCHEMY_ON", b_alchemy(True))
add("B_FORGE", b_forge(False))
add("B_FORGE_ON", b_forge(True))
add("B_ENCHANTER", b_enchanter(False))
add("B_ENCHANTER_ON", b_enchanter(True))
add("B_ARCHER", b_archer())
add("B_BALLISTA", b_ballista())
add("B_WARD", b_ward())
add("B_WALL", b_wall(False))
add("B_BRICKWALL", b_wall(True))
add("B_ROUTER", b_router())
add("B_JUNCTION", b_junction())


# ------------------------------------------------------------------ track (conveyor) 4 dirs x 5 frames
def track(frame):
    im = new(20, 20)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 3, 19, 16], fill=C("#6a5236"))
    d.line([0, 3, 19, 3], fill=C("#4e3c26"))
    d.line([0, 16, 19, 16], fill=C("#4e3c26"))
    for x in range(-5 + frame, 20, 5):
        if 0 <= x <= 19:
            d.rectangle([x, 4, min(19, x + 1), 15], fill=WOOD_L)
            d.line([x + 1, 4, x + 1, 15], fill=WOOD)
    for y in (6, 13):
        d.line([0, y, 19, y], fill=IRONC)
        d.line([0, y - 1, 19, y - 1], fill=IRONC_L)
    return im


for dr in range(4):
    for f in range(5):
        im = track(f)
        # dir 0=right 1=down 2=left 3=up ; PIL rotate is counter-clockwise
        rot = {0: 0, 1: 270, 2: 180, 3: 90}[dr]
        if rot:
            im = im.rotate(rot)
        add(f"TRACK_{dr}_{f}", im, 0)


# ------------------------------------------------------------------ camp (enemy spawn)
def camp():
    im = new(40, 38)
    d = ImageDraw.Draw(im)
    d.ellipse([1, 18, 39, 37], fill=C("#3a2c22"))
    d.ellipse([4, 20, 36, 35], fill=C("#4a3828"))
    # tents
    for (x, c) in [(4, "#5a6a2a"), (22, "#6a4a2a")]:
        cc = C(c)
        d.polygon([(x, 30), (x + 14, 30), (x + 7, 14)], fill=cc)
        d.polygon([(x + 7, 14), (x + 14, 30), (x + 7, 30)], fill=sh(cc, 0.7))
        d.polygon([(x + 5, 30), (x + 9, 30), (x + 7, 24)], fill=BLACK)
    # totem with skull
    d.rectangle([19, 6, 20, 30], fill=WOOD_D)
    d.rectangle([17, 4, 22, 9], fill=C("#e8e2d0"))
    d.point([18, 6], fill=BLACK)
    d.point([21, 6], fill=BLACK)
    # fire
    d.polygon([(17, 35), (23, 35), (20, 29)], fill=GLOW)
    d.polygon([(19, 35), (21, 35), (20, 32)], fill=GLOW_HOT)
    return im


add("CAMP", camp(), 18)


# ------------------------------------------------------------------ enemies (2 frames, face right)
def wolf(f):
    im = new(16, 12)
    d = ImageDraw.Draw(im)
    g = C("#7a7a80")
    d.ellipse([2, 10, 14, 12], fill=(20, 24, 14, 255))
    d.rectangle([3, 4, 11, 8], fill=g)
    d.rectangle([3, 4, 11, 5], fill=sh(g, 1.2))
    d.polygon([(10, 2), (15, 5), (14, 7), (10, 7)], fill=g)         # head
    d.polygon([(10, 2), (11, 0), (12, 3)], fill=sh(g, 0.8))         # ear
    d.point([13, 4], fill=C("#f0d040"))
    d.line([3, 5, 0, 3], fill=g)                                       # tail
    legs = [(4, 6), (9, 11)] if f == 0 else [(5, 7), (8, 10)]
    for x in legs[0] + legs[1]:
        d.line([x, 8, x, 10], fill=sh(g, 0.7))
    return im


def goblin(f, big=False):
    w, h = (16, 20) if big else (12, 16)
    im = new(w, h)
    d = ImageDraw.Draw(im)
    s = 1.3 if big else 1.0
    skin = C("#4e8a2e") if not big else C("#3c6a26")
    cx = w // 2
    d.ellipse([cx - 4, h - 2, cx + 4, h], fill=(20, 24, 14, 255))
    hy = 1
    d.rectangle([cx - 3, hy, cx + 2, hy + int(4 * s)], fill=skin)        # head
    d.polygon([(cx - 3, hy + 1), (cx - 6, hy), (cx - 3, hy + 3)], fill=skin)
    d.polygon([(cx + 2, hy + 1), (cx + 5, hy), (cx + 2, hy + 3)], fill=skin)
    d.point([cx + 1, hy + 2], fill=C("#ff3020"))
    by = hy + int(5 * s)
    d.rectangle([cx - 3, by, cx + 2, by + int(5 * s)], fill=C("#6a4a2a") if not big else IRONC)
    ly = by + int(5 * s) + 1
    if f == 0:
        d.line([cx - 2, ly, cx - 3, h - 1], fill=skin)
        d.line([cx + 1, ly, cx + 2, h - 1], fill=skin)
    else:
        d.line([cx - 2, ly, cx - 1, h - 1], fill=skin)
        d.line([cx + 1, ly, cx, h - 1], fill=skin)
    # club
    d.line([cx + 3, by + 2, cx + 5, by - 3], fill=WOOD_L, width=2 if big else 1)
    return im


def skeleton(f):
    im = new(12, 18)
    d = ImageDraw.Draw(im)
    b = C("#e8e2d0")
    d.ellipse([2, 16, 10, 18], fill=(20, 24, 14, 255))
    d.rectangle([3, 0, 8, 5], fill=b)
    d.point([4, 2], fill=BLACK)
    d.point([7, 2], fill=BLACK)
    d.line([5, 5, 5, 11], fill=b)
    for y in (7, 9):
        d.line([3, y, 8, y], fill=b)
    if f == 0:
        d.line([5, 11, 3, 16], fill=b)
        d.line([6, 11, 8, 16], fill=b)
    else:
        d.line([5, 11, 4, 16], fill=b)
        d.line([6, 11, 7, 16], fill=b)
    d.line([9, 9, 11, 2], fill=IRONC_L)
    d.line([8, 8, 10, 8], fill=WOOD_D)
    return im


def troll(f):
    im = new(26, 30)
    d = ImageDraw.Draw(im)
    s = C("#6e7a5e")
    d.ellipse([3, 26, 23, 30], fill=(20, 24, 14, 255))
    d.ellipse([4, 6, 21, 22], fill=s)                     # hunched body
    d.ellipse([4, 6, 14, 16], fill=sh(s, 1.15))
    d.ellipse([13, 2, 21, 10], fill=s)                    # head
    d.point([18, 5], fill=C("#ff3020"))
    d.point([19, 8], fill=C("#f0f0e0"))
    d.rectangle([7, 18, 18, 22], fill=C("#5a4028"))
    lx = (0, 2) if f == 0 else (2, 0)
    d.rectangle([7 + lx[0], 22, 10 + lx[0], 28], fill=sh(s, 0.8))
    d.rectangle([15 + lx[1], 22, 18 + lx[1], 28], fill=sh(s, 0.8))
    # log club
    d.line([21, 16, 25, 2], fill=WOOD, width=3)
    return im


add("EN_WOLF_0", wolf(0), 0)
add("EN_WOLF_1", wolf(1), 0)
add("EN_GOBLIN_0", goblin(0), 0)
add("EN_GOBLIN_1", goblin(1), 0)
add("EN_BRUTE_0", goblin(0, True), 0)
add("EN_BRUTE_1", goblin(1, True), 0)
add("EN_SKELETON_0", skeleton(0), 0)
add("EN_SKELETON_1", skeleton(1), 0)
add("EN_TROLL_0", troll(0), 0)
add("EN_TROLL_1", troll(1), 0)


# ------------------------------------------------------------------ items 8x8
def item_ore(col):
    im = new(8, 8)
    d = ImageDraw.Draw(im)
    c = C(col)
    d.polygon([(1, 6), (2, 2), (5, 1), (7, 4), (6, 7), (2, 7)], fill=c)
    d.polygon([(2, 2), (5, 1), (4, 4)], fill=sh(c, 1.3))
    d.polygon([(7, 4), (6, 7), (4, 4)], fill=sh(c, 0.7))
    return im


def item_bar(col):
    im = new(8, 8)
    d = ImageDraw.Draw(im)
    c = C(col)
    d.polygon([(0, 6), (2, 3), (7, 3), (7, 6)], fill=sh(c, 0.75))
    d.polygon([(1, 5), (2, 2), (6, 2), (6, 5)], fill=c)
    d.line([2, 2, 6, 2], fill=sh(c, 1.35))
    return im


def item_log():
    im = new(8, 8)
    d = ImageDraw.Draw(im)
    d.rectangle([1, 2, 6, 5], fill=WOOD)
    d.line([1, 2, 6, 2], fill=WOOD_L)
    d.ellipse([5, 2, 7, 5], fill=C("#d8b880"))
    return im


def item_plank():
    im = new(8, 8)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 3, 7, 5], fill=C("#c09058"))
    d.line([0, 3, 7, 3], fill=C("#dcb078"))
    d.line([0, 5, 7, 5], fill=WOOD)
    return im


def item_arrow(bolt=False):
    im = new(8, 8)
    d = ImageDraw.Draw(im)
    shaft = C("#4a4a50") if bolt else C("#d8b070")
    d.line([1, 6, 6, 1], fill=shaft)
    if bolt:
        d.line([1, 7, 6, 2], fill=shaft)
    d.polygon([(5, 0), (7, 0), (7, 2)], fill=IRONC_L)
    d.point([1, 7], fill=C("#f0f0f0") if not bolt else C("#b02828"))
    d.point([0, 6], fill=C("#f0f0f0") if not bolt else C("#b02828"))
    return im


def item_brick():
    im = new(8, 8)
    d = ImageDraw.Draw(im)
    d.rectangle([0, 3, 7, 6], fill=C("#a85a38"))
    d.rectangle([0, 2, 7, 3], fill=C("#c87a50"))
    return im


def item_potion():
    im = new(8, 8)
    d = ImageDraw.Draw(im)
    d.rectangle([3, 0, 4, 2], fill=C("#c8d8e0"))
    d.ellipse([1, 2, 6, 7], fill=C("#d03a8a"))
    d.point([2, 4], fill=C("#ffb0e0"))
    return im


def item_herb():
    im = new(8, 8)
    d = ImageDraw.Draw(im)
    g = C("#4abe3e")
    d.polygon([(1, 7), (3, 1), (5, 4)], fill=g)
    d.polygon([(3, 7), (7, 2), (6, 6)], fill=sh(g, 0.75))
    return im


def item_mana():
    im = new(8, 8)
    d = ImageDraw.Draw(im)
    c = C("#7ae0f0")
    d.polygon([(4, 0), (7, 4), (4, 7), (1, 4)], fill=c)
    d.polygon([(4, 0), (7, 4), (4, 4)], fill=sh(c, 1.25))
    d.polygon([(4, 7), (7, 4), (4, 4)], fill=sh(c, 0.65))
    return im


def item_clay():
    im = new(8, 8)
    d = ImageDraw.Draw(im)
    d.ellipse([0, 2, 7, 7], fill=C("#b06a40"))
    d.ellipse([1, 2, 5, 4], fill=C("#d08a58"))
    return im


ITEM_SPRITES = [
    ("IT_COPPER_ORE", item_ore("#d07a38")),
    ("IT_TIN_ORE", item_ore("#b8c8d4")),
    ("IT_LOG", item_log()),
    ("IT_IRON_ORE", item_ore("#9a4a32")),
    ("IT_COAL", item_ore("#2a2826")),
    ("IT_STONE", item_ore("#9a968e")),
    ("IT_CLAY", item_clay()),
    ("IT_HERB", item_herb()),
    ("IT_AETHER_ORE", item_ore("#9a6ae0")),
    ("IT_BRONZE", item_bar("#c88440")),
    ("IT_IRON_BAR", item_bar("#8a8f99")),
    ("IT_STEEL", item_bar("#c4d0dc")),
    ("IT_AETHER_BAR", item_bar("#a07af0")),
    ("IT_PLANK", item_plank()),
    ("IT_ARROW", item_arrow()),
    ("IT_BRICK", item_brick()),
    ("IT_POTION", item_potion()),
    ("IT_BOLT", item_arrow(True)),
    ("IT_MANA", item_mana()),
]
for n, im in ITEM_SPRITES:
    add("ICON_" + n[3:], im, 0)


# ------------------------------------------------------------------ ui bits
def heart():
    im = new(9, 8)
    d = ImageDraw.Draw(im)
    c = C("#d02020")
    d.polygon([(0, 2), (2, 0), (4, 2), (6, 0), (8, 2), (8, 3), (4, 7), (0, 3)], fill=c)
    d.point([2, 1], fill=C("#ff9090"))
    return im


def swords():
    im = new(9, 9)
    d = ImageDraw.Draw(im)
    d.line([0, 0, 7, 7], fill=IRONC_L)
    d.line([8, 0, 1, 7], fill=IRONC_L)
    d.line([1, 5, 3, 7], fill=GOLD)
    d.line([7, 5, 5, 7], fill=GOLD)
    return im


add("UI_HEART", heart(), 0)
add("UI_SWORDS", swords(), 0)


# ------------------------------------------------------------------ pack
def pack():
    # gather all opaque pixels to build a palette
    total_w = sum(im.width for _, im, _ in sprites)
    max_h = max(im.height for _, im, _ in sprites)
    strip = Image.new("RGB", (total_w, max_h), (0, 0, 0))
    mask = Image.new("L", (total_w, max_h), 0)
    x = 0
    for _, im, _ in sprites:
        strip.paste(im.convert("RGB"), (x, 0))
        mask.paste(im.split()[3], (x, 0))
        x += im.width
    colours = set()
    sp, mp = strip.load(), mask.load()
    for yy in range(max_h):
        for xx in range(total_w):
            if mp[xx, yy] >= 128:
                colours.add(sp[xx, yy])
    colours = sorted(colours)
    if len(colours) > 255:
        q = strip.quantize(255, method=Image.Quantize.MEDIANCUT)
        pal = q.getpalette()[:255 * 3]
        palette = [tuple(pal[i * 3:i * 3 + 3]) for i in range(255)]
        print("quantized", len(colours), "-> 255 colours")
    else:
        palette = colours
    lut = {}

    def idx(c):
        if c in lut:
            return lut[c]
        best, bd = 0, 1e9
        for i, p in enumerate(palette):
            dd = (p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 + (p[2] - c[2]) ** 2
            if dd < bd:
                best, bd = i, dd
        lut[c] = best + 1
        return best + 1

    data = []
    table = []
    for name, im, oy in sprites:
        off = len(data)
        px = im.load()
        for yy in range(im.height):
            for xx in range(im.width):
                r, g, b, a = px[xx, yy]
                data.append(idx((r, g, b)) if a >= 128 else 0)
        table.append((name, im.width, im.height, oy, off))
    return palette, data, table


def font_data():
    f = ImageFont.load_default_imagefont() if hasattr(ImageFont, "load_default_imagefont") else ImageFont.load_default()
    rows = []
    for ch in range(32, 127):
        im = Image.new("L", (6, 11), 0)
        ImageDraw.Draw(im).text((0, 0), chr(ch), font=f, fill=255)
        px = im.load()
        for y in range(11):
            bits = 0
            for x in range(6):
                if px[x, y] > 127:
                    bits |= 1 << x
            rows.append(bits)
    return rows


def write():
    palette, data, table = pack()
    font = font_data()
    with open(os.path.join(SRC, "assets.h"), "w") as fh:
        fh.write("/* Generated by tools/gen_assets.py - do not edit */\n#ifndef ASSETS_H\n#define ASSETS_H\n\n")
        fh.write("typedef struct { unsigned short w, h, oy; unsigned int off; } SpriteDef;\n\nenum {\n")
        for name, *_ in table:
            fh.write(f"    SPR_{name},\n")
        fh.write("    SPR_COUNT\n};\n\n")
        fh.write("extern const unsigned int asset_palette[256];\n")
        fh.write("extern const SpriteDef asset_sprites[SPR_COUNT];\n")
        fh.write("extern const unsigned char asset_pixels[];\n")
        fh.write("extern const unsigned char asset_font[95 * 11];\n")
        fh.write("#define FONT_W 6\n#define FONT_H 11\n\n#endif\n")
    with open(os.path.join(SRC, "assets.c"), "w") as fh:
        fh.write("/* Generated by tools/gen_assets.py - do not edit */\n#include \"assets.h\"\n\n")
        fh.write("/* colours are 0xAABBGGRR (PSP native 8888 order) */\n")
        fh.write("const unsigned int asset_palette[256] = {\n    0x00000000,")
        for i, (r, g, b) in enumerate(palette):
            if i % 6 == 0:
                fh.write("\n    ")
            fh.write(f"0xFF{b:02X}{g:02X}{r:02X}, ")
        fh.write("\n};\n\n")
        fh.write("const SpriteDef asset_sprites[SPR_COUNT] = {\n")
        for name, w, h, oy, off in table:
            fh.write(f"    {{{w}, {h}, {oy}, {off}}}, /* {name} */\n")
        fh.write("};\n\nconst unsigned char asset_pixels[] = {")
        for i, v in enumerate(data):
            if i % 32 == 0:
                fh.write("\n")
            fh.write(f"{v},")
        fh.write("\n};\n\nconst unsigned char asset_font[95 * 11] = {")
        for i, v in enumerate(font):
            if i % 22 == 0:
                fh.write("\n")
            fh.write(f"{v},")
        fh.write("\n};\n")
    print(f"{len(table)} sprites, {len(data)} px, {len(palette)} colours")

    # preview sheet
    cols = 12
    cell = 64
    rows_n = (len(sprites) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * cell, rows_n * cell), (50, 60, 40))
    for i, (name, im, oy) in enumerate(sprites):
        big = im.resize((im.width * 2, im.height * 2), Image.NEAREST)
        x = (i % cols) * cell + (cell - big.width) // 2
        y = (i // cols) * cell + (cell - big.height) // 2
        sheet.paste(big, (x, max(0, y)), big)
    sheet.save(os.path.join(ROOT, "tools", "atlas_preview.png"))


if __name__ == "__main__":
    write()
