# Forgeworks

A medieval factory-building and light tower-defense game for the PSP, inspired by Mindustry
with an Old School RuneScape look. Mine ore, lay tracks, smelt bars, and keep your towers
stocked with ammo while raiders march on your Keep. Five regions, each unlocking new buildings.

---

## 1. Get the playable file (EBOOT.PBP)

The PSP needs the source code compiled into a file called `EBOOT.PBP`. Pick one route.

### Route A: GitHub builds it for you (no installs)

1. Make a free account at https://github.com and click **New repository**. Name it
   `forgeworks`, keep the defaults, and click **Create repository**.
2. On the new repo page, click **uploading an existing file**. Drag in **everything inside**
   the `forgeworks` folder (the `src` and `tools` folders, `Makefile`, `ICON0.PNG`,
   `PIC1.PNG`, this README). Click **Commit changes**.
3. Add the build recipe. Folders starting with a dot are often hidden or skipped when you
   drag files, so create it by hand:
   - Click **Add file → Create new file**.
   - Type the name exactly: `.github/workflows/build.yml`
   - Open `.github/workflows/build.yml` from this project in a text editor, copy all of it,
     and paste it in. Click **Commit changes**.
4. Click the **Actions** tab. A build named "Build PSP EBOOT" starts by itself and takes
   about a minute. A green check means success.
5. Click the finished build, scroll to **Artifacts**, and download **Forgeworks-PSP**.
   Unzip it to get `EBOOT.PBP`.

If the build shows a red X, click into it, copy the error text, and paste it to Claude.

### Route B: Docker (if you already use it)

From inside the project folder:

```
docker run --rm -v "$PWD":/src -w /src pspdev/pspdev make
```

### Route C: Native PSP toolchain

Install the toolchain from https://pspdev.github.io, then run `make` in the project folder.

---

## 2. Play it in PPSSPP

1. Install PPSSPP (free) from https://www.ppsspp.org. It runs on Windows, Mac, Linux,
   Android and iOS.
2. Easiest: in PPSSPP choose **Load...** and open `EBOOT.PBP`.
3. To have it show up in the game list, place it here instead:
   `<PPSSPP memstick>/PSP/GAME/Forgeworks/EBOOT.PBP`
   (In PPSSPP, **Settings → System** shows where the memstick folder is.)

It also runs on a real PSP with custom firmware: copy the same folder to
`ms0:/PSP/GAME/Forgeworks/` on the memory stick.

Saves are written to `PSP/SAVEDATA/FORGEWORKS/`.

---

## 3. How to play

| Button | Action |
|---|---|
| D-pad / analog nub | Move the cursor |
| L / R | Choose a building |
| X | Build (hold X and move to paint tracks or walls) |
| O | Remove a building, full refund (hold and move to remove many) |
| Square | Rotate the track direction |
| Triangle | Info panel: building status, or recipes for the selected building |
| SELECT | Toggle 2x speed |
| START | Pause menu (controls, save and exit, restart) |

The basics:

- Extractors go on resource tiles: **Mine** on rocks, ore and clay, **Woodcutter** on trees,
  **Herb garden** on herb patches.
- Buildings push their output into any touching track, crafter, tower or the Keep.
- Tracks only accept items from behind or the sides, so point them where you want items to go.
- The **Keep** stores everything delivered. Building costs come out of the Keep's stock,
  and delivered items count toward the region's goals (top right).
- Raiders spawn at the dark camps and walk to the Keep, smashing anything in the way.
  Towers only fire while they have ammo, so feed them from your factory.
  Towers near the Keep cover every path.
- A region is secured when its delivery goals are met and every raid is repelled.
  That unlocks the next region and its new buildings.

| Region | New buildings | Goal |
|---|---|---|
| 1. Meadowfall Valley | Track, Junction, Mine, Woodcutter, Smelter, Fletcher, Archer tower | 40 bronze, 3 raids |
| 2. Greystone Hills | Router, Stone wall | 40 iron bars, 60 stone, 4 raids |
| 3. Marshwood | Herb garden, Kiln, Alchemy lab, Brick wall | 50 bricks, 20 potions, 4 raids |
| 4. Frostpeak Pass | Sawmill, Forge, Ballista | 30 steel, 40 bolts, 5 raids |
| 5. The Sunken Citadel | Enchanter, Arcane ward | 25 mana crystals, 20 aether bars, 6 raids |

---

## 4. Developer notes

```
src/main_psp.c   PSP layer: screen, buttons, HOME-exit, save files
src/screens.c    title, world map, gameplay input, pause, win/lose, saving
src/world.c      map generation, conveyors, crafting, enemies, towers, raids
src/render.c     world drawing and HUD
src/gfx.c        software renderer (sprites, text, shapes)
src/data.c       ALL balance numbers: items, buildings, recipes, enemies, regions
src/assets.c/.h  generated art (do not edit by hand)
tools/gen_assets.py   draws every sprite; rerun after changing art
tools/headless.c      desktop test harness that runs scripts and saves screenshots
tools/make_icons.py   builds ICON0.PNG / PIC1.PNG
```

- Tune the game in `src/data.c`: costs, recipe times, tower damage, wave sizes, goals.
- Change art in `tools/gen_assets.py`, then run `python3 tools/gen_assets.py` (needs Pillow).
- Test on a desktop: `sh tools/build_headless.sh`, then `./fw_headless script.txt`. See the top of
  `tools/headless.c` for script commands.
- If you change the `World` struct layout, bump `WORLD_MAGIC` in `src/game.h` so old
  mid-region saves are ignored instead of loaded wrongly.
