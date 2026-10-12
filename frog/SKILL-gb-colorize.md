---
name: gb-colorize-patch
description: Colourise a monochrome Game Boy (DMG) ROM into a CGB-enhanced game as an own-work IPS patch, then iterate on user colour requests from emulator save states ("make X red", "check s1"). Use when asked to colour/recolour a GB ROM, fix a tile/sprite/portrait colour from a SameBoy save state, or rebuild the IPS.
---

# Colourising a DMG Game Boy ROM (IPS patch workflow)

Distilled from the "For the Frog the Bell Tolls" project (`frog-claude/work`). Reuse the approach, not the exact addresses.

## Ground rules
- Deliver **only an IPS patch of own work** (hooks, palette data, tile-class table). Never distribute the ROM, extracted assets or decompiled source. Remind the user of the legal note before any publishing.
- Back up before modifying; keep the original read-only (`backup/*.original.gb`). Verify the user's ROM by SHA-1.
- Do not install or change anything system-level (or on the user's device) without a yes. Don't kill/pause the user's running game without need.
- Every edit: save a `/tmp/vN.beforeNN.gb` backup, rebuild, verify, rebuild IPS, report the backup path.

## Architecture (CGB colourisation of a DMG game)
- Header `$143=0x80`, double-speed switch at init, fix header + global checksums.
- **Per-ROM-tile palette-class table** (32 KB in two free banks, 1 byte per tile of each graphics bank): class bits 0-2, bit3 fill, bits4-5 role, bit6 plane, bit7 frog-style pixel drop.
- Runtime hooks at the tile loaders / VBlank queue consumers / OAM flag writers keep a WRAM tile->class table and write BG attributes (VRAM bank 1) and OBJ palette bits in OAM flags.
- 8 BG + 8 OBJ palettes live in bank-0 free space and are re-derived each VBlank from BGP/OBP shadow regs (so fades work).
- Own SM83 assembler/disassembler (`asm.py`, `sm83.py`); `build.py` patches the original + assembles `hooks.s` + writes palettes/table; assert patched bytes before overwriting; assert segments don't overlap.
- Scene-specific palettes (portraits): detect the scene by a 4-6 byte VRAM tile signature in the palette hook, upload an alternate palette group (`ALTPAL`, `ALTPAL2`, `ALTOBJ`). Re-detect on change so the cycle restarts.

## Diagnosing a colour request from a save state
1. Parse the SameBoy BESS state (`bess.py`, `bessrender.py`) and render it; run `refresh_state.py <s1> <new.gb> <out> [old.gb]` to re-derive tile classes/attributes/palettes for the new ROM (pass the old ROM so duplicate-content tiles resolve by their old class).
2. List the on-screen OAM sprites / BG cells with their ROM source: `romtiles.lookup_all(raw16)` -> `(bank, index)` candidates, plus the class in the table.
3. Identical-looking tiles have many source candidates: change **every** candidate the game could load, or verify with a copy of the ROM.
4. Render a zoomed crop (grid + tile ids) before deciding which tiles belong to the object (hair vs cloak vs hands...).

## Fix patterns (cheapest first)
1. Re-class the tiles (`table[bank*1024+idx] = class`).
2. Edit a palette colour (only if every user of the class should change).
3. Pixel-level ROM tile edit when one pixel value must split into two colours (e.g. eyes vs outline): remap values in the tile data, then pick a palette that gives the new values the wanted colours. A CGB sprite tile has only **3 opaque colours** - decide trade-offs and say so (white eyes vs pink cheeks).
4. Scene-only palette override (portrait scenes) when all palettes are in use.
5. Free a palette by merging two near-identical ones (frog faces -> frog green) or by two-toning a tiny user (the grey well).
- Colour is per 8x8 tile; leaks at tile edges are inherent. Window/HUD tiles share classes with the scene - check hearts/HUD after a palette change.
- A blank tile shared by backdrop and face interior cannot be recoloured separately without patching the map data.

## Verification loop (always)
- Rebuild from a script (`mk_v9b.py`: v9 table + palettes + current hooks) so the result is reproducible; a revert = restore backup + drop the block from the script and check `cmp`.
- Scripted PyBoy boot/walk regression: old vs new ROM screens must be identical unless the change should show there (`diffwalk` bbox).
- Test scene overrides in the emulator by writing the signature bytes into VRAM and reading palette RAM.
- Rebuild IPS with `mkips.py make orig new out.ips`, `apply` it to the original and `cmp` with the new ROM.
- Show the user a rendered crop; state limits honestly.

## Second-screen (dual-screen handheld) companion
- RetroArch network command UDP 127.0.0.1:55355 (`READ_CORE_MEMORY`, `WRITE_CORE_MEMORY`); pause detection via game WY shadow; virtual joypad byte ORed into the game's button read (`$DAD4`).
- App window app_id `lowerdeck` (sway rules put it on the bottom output without focus; RetroArch pauses on focus loss). Live item table read from WRAM; icon atlas rendered from the emulator; tap macros drive the real pause menu.

## Reporting style
Short outcome, what changed (tiles/palettes), limits, checks run, IPS bytes, backup path.
