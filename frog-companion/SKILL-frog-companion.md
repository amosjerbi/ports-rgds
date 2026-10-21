---
name: frog-companion-rgds
description: Build, deploy and QA the bottom-screen companion for "For the Frog the Bell Tolls" on the RG DS (ROCKNIX): the game's own pause menu, rendered by a hidden second copy of the game, with tap-to-use items. Use when asked to change the companion, the launch wrapper, touch handling, or to test it on the device.
---

# Frog pause-menu companion on the RG DS

Distilled from this folder's project. Reuse the approach, not the exact numbers.

## Ground rules
- Publish **only own work**: the companion source, wrapper, docs. Never the ROM, saves, save states, extracted assets or modified game binaries. Remind the user in one line before any publishing.
- The user's save (`<rom>.srm`) and slot-0 state are theirs. Back them up around any test that loads a state (RetroArch writes SRAM back on exit and autosave rewrites `<rom>.state` / `.state.auto`) and restore them afterwards, verifying md5.
- Do not kill or restart the user's running game without need; a normal quit (SIGTERM) lets RetroArch save properly.

## Idea: let the game draw its own menu
The pause menu is drawn from a save record in work RAM (`0xCBC2`, ~126 bytes). Do not re-implement it. Host a second copy of the game in the companion (`dlopen` the same libretro core, `retro_run` per frame), parked in the menu. When the live record changes, copy it in, reopen the menu, let the game redraw, show the frame. Everything the game computes (ARMOR, JUMP, HOLD slots, keys) comes for free, and a colour patch shows too.
- Live game access: RetroArch network commands (`network_cmd_enable`, UDP 55355): `READ_CORE_MEMORY <hexaddr> <n>`, `WRITE_CORE_MEMORY <addr> <bytes>`. Both gambatte and SameBoy serve the GB address space (ROM 0x0000, VRAM 0x8000, SRAM 0xA000, WRAM 0xC000, HRAM 0xFF80). Open the port only for the session.
- Record layout (offsets from 0xCBC2): +10 life, +11 max life (quarter hearts), +0x19..0x1B coin (24-bit big-endian), +0x21.. item ids (first four = HOLD, then 3x4 SELECT grid), counts after. POWER = 0xCBC9 + 9, SPEED = 0xCBCA.
- Finding such layouts: PyBoy headless + poke-and-compare (change a byte, open the menu, diff the frame) maps bytes to on-screen elements in seconds; saved states from other emulators give known values to search for.

## Libretro host pitfalls
- gambatte refuses `retro_load_game` unless `GET_CAN_DUPE` is answered true; it also needs a pixel format it accepts (it asks for RGB565). Implement `GET_LOG_INTERFACE` to see why a load fails.
- The core needs a valid save in SRAM before booting a diary; seed it from `<rom>.srm` (fallback `.sav`) and replay the key sequence title -> Load -> diary -> dialogs. Validate with the record (max life multiple of 4, life <= max); try other diary slots; restart on failure.
- Detect "menu open" from the frame (the menu fills the bottom rows; gameplay has the black status bar), never by counting Start presses, and let the game's fade-in finish (~70 frames) before capturing.

## Touch
- Read the bottom panel's touchscreen from evdev (`fe5e0000.i2c`); ES attaches touchscreens to another Wayland seat so SDL touch never arrives. **The kernel only reports an axis that changed**: never reset the stored X/Y on a new touch.
- Tap on a SELECT cell: refresh the hidden copy from the live record, press RIGHT/DOWN to the cell (the cursor starts on the first cell), press A, diff the record, write changed bytes back **only where the live byte still equals the snapshot**. If life changed, also rewrite the live HUD hearts (window tilemap 0x9C00+2: tiles 4B full, 4C..4E partial, 4F empty) because the game redraws them only on its own events.
- Some actions leave a message or sub-screen: back out with B/A, then restart the hidden game.

## Device and QA
- aarch64 ROCKNIX, SDL2 only, two 640x480 panels (`DSI-2` top, `DSI-1` bottom with touch). The wrapper (`gb-launch`, no `.sh` so ES does not list it) starts the companion only for this game (header title) with gambatte or SameBoy, focuses the top output first, restores the network port and powers `DSI-1` off after.
- `qa/frog_qa.sh` (run on the device): launch through the wrapper with ES's environment, load a state, poke live memory (`PKS`), inject touches from a `uinput` virtual screen (`TOUCHSIM`), capture both panels, sample CPU, restore the user's files. `qa/touch_only.sh` isolates the touch reader.
- Gotchas: BusyBox `pkill -x` is broken and `pkill -f` matches your own ssh command line (use script files); stale RetroArch instances share UDP 55355; a launch with the SameBoy core silently skipped the companion until the wrapper matched it.
