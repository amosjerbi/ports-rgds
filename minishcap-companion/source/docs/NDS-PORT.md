# NDS port plan

Status: scaffold only (`platform/nds/`). Nothing beyond ROM detection runs.

## Constraints vs the 3DS port
| Area | 3DS port | NDS |
| --- | --- | --- |
| RAM | ROM fully in heap (`port_rom.c`, ~16 MB) | 4 MB total |
| Video | software PPU 400x240 + PICA200 | 256x192 x2, hardware 2D engine, no 3D-GPU path |
| Audio | agbplay software mixer + DSP | ARM7 only, no DSP |
| CPU | 268/804 MHz ARM11 | 67 MHz ARM9 |

## Steps
1. **ROM access** - a paged cache (`nds_rom_pager`) cannot back `gRomData`: ~90 sites in
   ~40 files (engine included) hold raw native pointers into it, and eviction would dangle them.
   Instead, mirror the N64 path: on DS/DS Lite the GBA slot maps the ROM at `0x08000000`, so
   `Nds_AttachSlot2Rom()` (`platform/nds/source/nds_slot2.c`) hands back a flat pointer and
   `Port_LoadRom` (`-DTMC_NDS`) skips file I/O. Requires the original cart or a GBA flashcart in
   Slot-2; **not available on DSi/3DS**. The pager remains only for SD-side tooling/probing.
   Open: Slot-2 reads are slow (16-bit bus, ~6-cycle waits) - copy hot tables into RAM later.
2. **Link engine** - DONE (links, unrun). `platform/nds/Makefile` builds 656 engine/actor/port
   sources (list: `platform/nds/engine_sources.mk`, derived from the 3DS CMake list) with
   `-DTMC_NDS -DTMC_3DS`, so the DS reuses the 3DS 32-bit/no-SDL paths and its SDK-free
   `Platform3DS_*` API (`platform_nds.c` implements it on libnds; `stubs_nds.c` holds video/audio/
   second-screen placeholders). `main_nds.c` sets `gRomData` to the Slot-2 window, calls
   `Port_LoadRom` and `AgbMain`. Changes to shared code: `port_types.h`, `gba/types.h`
   (32-bit types) and `port_rom.c` (skip file I/O) accept `TMC_NDS`.
   Excluded: `ppu/src/mode1.c` (software PPU), `port_second_screen*`, `port_dump_state`, agbplay/C++.
   Memory (ARM9, 4 MB, `size`): text 2.10 MB (of which ~1.2 MB is `data_const_stubs`/`port_rom_tables`
   ROM copies), data 10 KB, bss 1.18 MB => 3.29 MB, ~880 KB left for heap + stack.
   Next saving: drop the ROM-copy rodata and resolve it from the flat Slot-2 ROM instead.
   **Runtime-verified in melonDS** (standalone, GBA cart in Slot-2, DirectBoot): `Port_LoadRom`
   detects USA, resolves area/room tables, inits data stubs, and the engine main loop runs
   (on-screen heartbeat reached frame 1024 in ~20 s). DraStic cannot test this (no Slot-2).
   Save init reports "legacy profile preserved; writing disabled" (no FAT yet).
   Test recipe: `melonDS -f tmc-nds.nds tmc.gba` with `QT_QPA_PLATFORM=wayland`.
3. **Video** - top screen DONE (first pass). The shared software PPU (`mode1.c`, single-threaded:
   `__thread` -> plain globals and the 3DS worker block compiled out under `TMC_NDS`) renders
   240x160 ABGR into `virtuappu_frame_buffer`; `platform/nds/engine/port_ppu_nds.c` drives it per
   frame and `source/nds_video.c` converts to RGB555 on main-engine BG3 (MODE_5_2D, centred in
   256x192). Verified in melonDS: the title screen renders with correct colours.
   Speed is the open problem: ~5-6 fps in melonDS (336 frames/60 s) -- the 67 MHz ARM9 cannot
   run this renderer at 60 fps. Next: map BG/OBJ onto the DS 2D engine (hardware tiles/sprites)
   instead of software compositing; keep `mode1.c` as the reference/fallback.
   Memory now: text 2.11 MB + bss 1.53 MB = 3.65 MB (~350 KB free).
4. **Audio** - new m4a path on ARM7 (no agbplay).
5. **Second screen** - DONE (first pass, bottom screen). The 3DS compositor
   (`port_second_screen*.c`, `bottom_*_3ds.c`) paints at any size, so it runs unchanged at 256x192
   into the shared 192 KB scratch buffer (`engine/port_ppu_nds.c`), blitted to the sub engine by
   `nds_video.c`. A simplified version of the 3DS scheduler repaints on snapshot change/animation
   (every 4th frame is checked). DS touch -> `Port_SecondScreen_3DS_OnTap` (`platform_nds.c`).
   Verified in melonDS: the title screen shows the gold-framed Triforce idle screen.
   Not yet verified: in-game map/dungeon/quest tabs and touch (needs input injection or hardware).
   LightLock is shimmed to no-ops (`compat/3ds/synchronization.h`); updater/dump/turbo are inert
   stubs (`engine/nds_stubs_engine.c`).
6. **Saves/input** - EEPROM to SD (`port_save`), DS keys mapped to GBA keys.

## Memory (ARM9, program RAM is 3.5 MB: libnds reserves the top 512 KB for the ARM7 loader)
Current: text 1.18 MB, bss 2.45 MB; only ~34 KB left for the heap, which is too tight (libfat,
printf, any malloc). Savings already taken: ROM-copy stubs replaced by linker symbols into the
Slot-2 ROM (`platform/nds/tools/gen_rom_symbols.py`, 951 KB; the 12 stubs at unaligned cart
offsets stay in RAM), unused PPU default buffers (~340 KB), the 195 KB `kFrameObjListsData` fallback.
**USA only**: the stubs are USA-region bytes; an EU cart needs the region remap path.
Largest remaining: gEwram 256 KB, gFrameObjLists 195 KB, scratch 192 KB, second-screen theme
(menu backgrounds 300 KB + arena 156 KB), world map 300 KB, dungeon canvas 100 KB.
