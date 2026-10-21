# Frog companion: the pause menu on the RG DS bottom screen

For "For the Frog the Bell Tolls" (`frog-color.gb`) running in RetroArch (gambatte) on the Anystream RG DS (ROCKNIX).
The top panel is the game. The bottom panel shows the game's **own pause menu** (STATUS, HOLD, SELECT items with
counts, KEY) plus a live LIFE / COIN header, so you never have to pause to check them.

## How it works
The game draws its pause menu from a save record in work RAM (`0xCBC2`, ~126 bytes). Instead of re-implementing the
menu, `frog-bottom` hosts a **second copy of the game** (the same gambatte core, loaded with `dlopen`) parked in
that menu. When the live game's record changes:

1. the record is read from RetroArch over its network command interface (`READ_CORE_MEMORY`, UDP 55355),
2. the hidden copy closes its menu, takes the record, reopens the menu and lets the game redraw it,
3. the finished frame (colourised exactly like the real game) is scaled onto the bottom panel.

So ARMOR, JUMP, the HOLD slots and the keys are whatever the game's own code computes. The LIFE/COIN row is drawn
from the record directly (the game only redraws its own on events).

Record layout (offsets from `0xCBC2`): `+10` life, `+11` max life (quarter hearts), `+0x19..0x1B` coin (24-bit
big-endian), `+0x21..` item ids, counts follow; POWER = `0xCBC9 + 9`, SPEED = `0xCBCA`.

The hidden copy boots by replaying the title -> Load -> diary -> dialogs key sequence on the save file next to the
ROM (`frog-color.srm`), then the live record overwrites its data, so it does not matter which diary it picked.

## Touch
Tap an item in the SELECT grid and the game does what pressing A on it does in the real pause menu (drink the heart
bottle, eat a tomato, equip). It runs in the hidden copy: the cursor is moved to the tapped cell, A is pressed, and
the changes to the save record are written back to the live game over `WRITE_CORE_MEMORY`, only for bytes the live
game has not changed since. If life changed, the live HUD heart row (window tilemap, tiles 4B full ... 4F empty) is
brought in line, so the top screen follows too. If an action leaves the hidden game in a message or sub-screen, it
backs out with B/A and, failing that, restarts the hidden copy (about two seconds).

The bottom panel's touchscreen is read straight from evdev (`fe5e0000.i2c`; `FROG_TOUCH_DEV` overrides). The kernel
only reports an axis when it changed, so the reader keeps the last position: a tap at the same height as the previous
one carries no Y. `FROG_TEST_TAP="ms,x,y"` injects one tap for QA.

## Install (device)
Everything is in one folder, `/storage/roms/ports/frog/` (not a Ports-menu entry: no `.sh` files):
`frog-bottom`, `gb-launch`. `es_systems.cfg`'s `gb` command points at `gb-launch`, which starts the companion only
for this game (header title `KAERUNOTAMENI`) with the gambatte core and sends every other Game Boy game to the stock
launcher. It opens RetroArch's network port only for the session and powers the lower panel off afterwards.
`no-companion` next to the wrapper disables it. It works with the gambatte and SameBoy cores (both serve the same
memory map). Other cores (SameBoy etc.) get plain RetroArch.

## Build / QA
`./build.sh` builds `build/frog-bottom` in an arm64 Debian container (SDL2 + glibc 2.41, like the device).
`qa/frog_qa.sh` runs on the device: launches through the wrapper with ES's environment, can load a state, poke the
live game's memory (`PKS="t:addr:bytes;..."`), capture both panels and sample CPU. `FROG_DUMP_STATE=path` makes
`frog-bottom` write a state of the hidden game in play (used to put the real game in a known situation).

## Gotchas
* gambatte needs `GET_CAN_DUPE` answered true and accepts RGB565; without them `retro_load_game` fails silently.
* RetroArch rewrites `<rom>.state.auto` on every exit and auto-resumes from it: tests must delete theirs.
* Loading a state into RetroArch changes the game's save RAM and RetroArch writes it back on exit: back up
  `frog-color.srm` around tests.
* Detect the menu by its bottom rows (panels vs the black status bar), never by counting Start presses, and let the
  game's fade-in finish before capturing.
