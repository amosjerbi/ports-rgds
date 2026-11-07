<p align="center">
  <img src="screens/Big-City-1-1.png" alt="Donkey Kong (GB), Big-City 1-1 in Game Boy Color colour" width="480">
</p>

# Donkey Kong (Game Boy, SGB Enhanced) — Game Boy Color colour patch

`dk-color.ips` turns the Super Game Boy–enhanced DMG ROM into a Game Boy Color (CGB-compatible) game with real colour
that works on emulators and handhelds **without Super Game Boy support** (for example the RG DS SameBoy core). On a
plain DMG or a real SGB the game's original code path still runs.

### Screens

| ![Big-City 1-1](screens/Big-City-1-1.png) | ![Big-City 1-2](screens/Big-City-1-2.png) | ![Forest 2-1](screens/Forest-2-1.png) |
|:--:|:--:|:--:|
| Big-City 1-1 | Big-City 1-2 (scrolls vertically) | Forest 2-1 |

![Classic stages with a brown Donkey Kong](screens/Classic-stages.png)

* Apply to **your own** copy of the original ROM
  (SHA-1 `6ed661bd1d6d8cdd48e1c10f8ca4e8dcba49128e`, 512 KB, *Donkey Kong (Japan, USA) (En) (SGB Enhanced)*).
* `python3 apply_patch.py original.gb dk-color.ips dk-color.gb` (or any IPS patcher, e.g. Flips).
  The patched ROM has SHA-1 `0ee193e89a27109eb3335177cc22f894d37747e8`.
* Run it in a CGB-capable emulator (SameBoy, mGBA, BGB, Gambatte) or on a Game Boy Color.
* **No ROM is included in this repository.** Only the patch (my own code and colour data) and the build sources.

## What the patch does
* Header flag `$143 = $80` (CGB-compatible), header and global checksums fixed.
* The game already sends SGB colour packets. On a CGB the patch pretends an SGB is present, swallows the packets and
  replays them as CGB BG palettes, a 20x18 attribute map (uploaded to VRAM bank 1 with GDMA) and per-sprite OBJ palettes.
  Palettes are re-derived from the game's own BGP/OBP every frame, so fades keep working.
* The game redraws with a chain of scan-line interrupts; the interrupt vectors are wrapped (WRAM bank switch) and the
  VBlank is held back while a scan-line handler runs, which fixes a once-per-few-seconds blank-frame flicker.
* **Hand-made stage looks** (override on top of the SGB colours, selected by the level counter `$C85C`):
  * Big-City 1-1 — sky, lilac tower, tan building, brown-red girders, green blocks, gold key and lock, dark HUD.
  * Big-City 1-2 (the vertically scrolling stage) — same look, with a map indexed by background row so colours follow the scroll.
  * Forest 2-1 — the pink trunk shade is blue; Mario, turtles, spiky enemies and the boar each have their own palette.
* **World themes** (`data/themes.json`, `tools/themes.py`): all other play stages get a per-world palette instead of the game's
  purple/beige SGB colours — Big-City blue/brick, Forest green/blue, Ship pale blue, Jungle green, Desert sand, Airplane sky blue,
  Iceberg icy blue, Rocky-Valley brown, Tower grey-blue. Characters reuse the 1-1 look.
* Donkey Kong is brown and beige in the classic stages 2, 3 and 4 (his tiles differ per stage).

## All worlds
Every stage of worlds 1–9 (plus the four classic stages) as the patched ROM renders it today. Big-City 1-1, 1-2 and
Forest 2-1 have the hand-made look; every other stage uses its world's colour theme (one palette per world).

| World | Stages | Screenshots |
|:--|:--:|:--|
| Classic | 4 | [classic](screens/worlds/classic.png) |
| 1 Big-City | 8 | [world 1](screens/worlds/world-1-big-city.png) |
| 2 Forest | 12 | [world 2](screens/worlds/world-2-forest.png) |
| 3 Ship | 8 | [world 3](screens/worlds/world-3-ship.png) |
| 4 Jungle | 12 | [world 4](screens/worlds/world-4-jungle.png) |
| 5 Desert | 12 | [world 5](screens/worlds/world-5-desert.png) |
| 6 Airplane | 8 | [world 6](screens/worlds/world-6-airplane.png) |
| 7 Iceberg | 12 | [world 7](screens/worlds/world-7-iceberg.png) |
| 8 Rocky-Valley | 16 | [world 8](screens/worlds/world-8-rocky-valley.png) |
| 9 Tower | 9 | [world 9](screens/worlds/world-9-tower.png) |

![World 1, Big-City](screens/worlds/world-1-big-city.png)

## Known limits
* Only three stages have the hand-made look; the rest use one flat theme palette per world (no per-tile colouring).
* Donkey Kong is still green in the very first classic stage: recolouring him there needs a sprite pass inside VBlank,
  which overruns VBlank on accurate emulators (SameBoy, the RG DS core) and garbles the tile uploads. The build is
  available as `DMA=1 ./make.sh` for experiments, but it is **not** what `dk-color.ips` contains.
* Colour is per 8x8 cell, like on the SGB.

## Build from source
```
pip install pyboy
./make.sh "Donkey Kong (Japan, USA) (En) (SGB Enhanced).gb"      # -> out/dk-color.gb, out/dk-color.ips
```
* `color/hooks.s` — the SM83 hooks and the stage overrides (assembled by `tools/gbasm.py`).
* `tools/dkdata.py` — turns the game's SGB packets (bank `$1E`) into compact colour commands.
* `tools/dkbuild.py` — patches the original ROM, writes the IPS and verifies it applies.
* `tools/sgbdump.py` — dumps the SGB palette/attribute data from your own ROM (kept local in `out/sgb/`).
* `data/ovr*.json` — palettes, attribute maps and sprite-class tables of the stage looks.

## Legal
Apply the patch only to a ROM you own. Do not share the ROM, the dumps in `out/`, or any extracted game assets.
