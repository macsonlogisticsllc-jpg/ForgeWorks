#!/bin/sh
# Builds the desktop test harness (not the PSP game).
cd "$(dirname "$0")/.." && gcc -O2 -Wall -Wno-missing-field-initializers -o fw_headless \
  tools/headless.c src/screens.c src/render.c src/world.c src/data.c src/gfx.c src/assets.c -lm && echo "built ./fw_headless"
