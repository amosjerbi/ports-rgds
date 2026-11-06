<p align="center">
  <img src="screens/Gear%20screen.png" alt="A Link Between Worlds on the RG DS: the game on the top panel, the gear screen on the bottom panel" width="320">
</p>

# Zelda: A Link Between Worlds on the RG DS (rgds-core)

The 3DS game runs in Azahar inside RG DS Play, set up the same way as Ocarina of Time 3D. The game is on the top panel.
The 3DS touch screen is on the bottom panel: gear, items, and the map, all touch-based.

Everything this needs (the core, the player, `run3ds.sh`, and the install steps) is in [`../azahar-3ds`](../azahar-3ds).
This folder holds the game's own launcher. No ROM or save is included.

## Install
1. Set up [`azahar-3ds`](../azahar-3ds) first. You need the core from this repo **with the geometry-shader patch**:
   older builds fail to link shaders in this game and run out of memory.
2. Put your decrypted dump at
   `/data/roms/3ds/Legend of Zelda, The - A Link Between Worlds (USA) (En,Fr,Es).3ds`, or change the `ROM=` line.
3. Copy `Zelda A Link Between Worlds.sh` to `/data/portmaster/ports/` and make it executable. It appears under **Ports**,
   which needs *Settings → Library → Show all systems* on.

A Citra/Azahar save from a computer works. Copy its `title/00040000/000ec300` folder (title ID `00040000000EC300`, USA)
into `/data/rgds/azahar/data/Azahar/sdmc/Nintendo 3DS/<zeros>/<zeros>/title/00040000/`.

## Status
* Boots, loads saves, and draws correctly on both screens, with touch on the bottom panel.
* **Slow: about 20–40% speed.** The emulator's main thread uses a whole Cortex-A55 core, so the game is CPU-bound. That
  makes it playable to look around, but not at full speed.
* The first run compiles shaders and stutters more. Later runs load them from Azahar's disk cache.
