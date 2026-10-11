![Frog Color screenshot: village](screen-1.png) ![Frog Color screenshot: overworld](screen-2.png)

# For the Frog the Bell Tolls — Game Boy Color colourisation patch

`frog-claude-color.ips` turns the monochrome DMG ROM into a Game Boy Color
(CGB-enhanced) game with full colour, while still booting on a plain DMG.

* Apply to **your own** copy of the original ROM
  (SHA-1 `90bddff5d48eedad6ec5157946f602ae4170e0c6`, 512 KB, English version).
* `python3 apply_patch.py apply original.gb frog-claude-color.ips frog-claude-color.gb`
  (or any IPS patcher, e.g. Flips).
* Run the result in a CGB-capable emulator (SameBoy, mGBA, BGB, Gambatte) or on a Game Boy Color.

## What the patch does
* Header flag $143 = $80 (CGB-compatible); header checksum fixed.
* Switches the CGB to double-speed mode at start-up (needed for the extra VRAM work),
  and compensates the sound timer reload so music tempo is unchanged.
* Keeps a palette class for every VRAM tile slot (looked up from a 32 KB table built
  into two previously empty ROM banks) and writes CGB BG attributes / OBJ palette bits
  whenever the game draws tiles, sprites or tilemaps.
* Uploads 8 BG + 8 OBJ palettes to CGB palette RAM, re-deriving them from the game's own
  BGP/OBP shadow registers so fades keep working.

## Rebuilding / tuning colours
`source/` holds my assembler, the hook code (`hooks.s`) and the palette/tile
classification (`palconf.py`). Edit `palconf.py` and run `build.py` with your own ROM
path; nothing from the original game is stored in this folder.

## Legal
This patch contains only my own bytes (hook code, palette data, tile-class table).
It contains no original game code, graphics or audio. Do not distribute the
original ROM or the patched ROM. Laws vary; this is not legal advice.

