#!/usr/bin/env python3
"""Builds ICON0.PNG (144x80) and PIC1.PNG (480x272) for the PSP menu from
screenshots taken with the headless harness (see README, 'Developer notes')."""
import sys, os
from PIL import Image, ImageDraw, ImageFont, ImageEnhance

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
title_ppm, scene_ppm = sys.argv[1], sys.argv[2]

pic1 = Image.open(title_ppm).convert("RGB")
pic1.save(os.path.join(ROOT, "PIC1.PNG"))

scene = Image.open(scene_ppm).convert("RGB")
# crop around the keep (centre of the screen)
cx, cy = 250, 66
icon = scene.crop((cx - 72, cy - 48, cx + 72, cy + 32))
icon = ImageEnhance.Brightness(icon).enhance(0.8)
font = ImageFont.load_default_imagefont() if hasattr(ImageFont, "load_default_imagefont") else ImageFont.load_default()
txt = Image.new("RGBA", (64, 12), (0, 0, 0, 0))
ImageDraw.Draw(txt).text((2, 0), "FORGEWORKS", font=font, fill=(255, 176, 40, 255))
txt = txt.resize((128, 24), Image.NEAREST)
outline = Image.new("RGBA", txt.size, (0, 0, 0, 0))
a = txt.split()[3]
dark = Image.new("RGBA", txt.size, (30, 18, 8, 255))
base = icon.convert("RGBA")
for dx in (-2, -1, 0, 1, 2):
    for dy in (-2, -1, 0, 1, 2):
        base.paste(dark, (8 + dx, 54 + dy), a)
base.paste(txt, (8, 54), txt)
base.convert("RGB").save(os.path.join(ROOT, "ICON0.PNG"))
print("wrote ICON0.PNG and PIC1.PNG")
