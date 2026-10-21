---
name: rgds-minishcap-dualscreen
description: Build, deploy and QA the dual-screen Minish Cap for the RG DS (ROCKNIX): real mGBA on the top panel plus a RAM-reading companion that draws the project's second screen on the bottom panel. Use when asked to change the companion, the launch wrapper, the second-screen UI in companion mode, or to test it on the device.
---

# Dual-screen Minish Cap on the RG DS (mGBA + companion)

Distilled from this folder's project. Reuse the approach, not the exact numbers.

## Ground rules
- Publish **only own work**: the platform glue, the companion, docs, and a patch of the few upstream files that were changed. Never publish the ROM, extracted assets, binaries built from the ROM-derived tables, or the decompiled source tree. Remind the user of the legal note before any publishing.
- The user's ROM is read-only. Verify it by SHA-1 (USA `b4bd50e4131b027c334547b4524e2dbbd4227130`).
- Do not kill or restart the user's running game or EmulationStation without need. Back up any ROCKNIX config before editing it (`es_systems.cfg`, `system.cfg`, `retroarch.cfg`).
- Test on the device, with screenshots of **both** panels, before reporting anything as working.

## Device facts (RG DS, ROCKNIX)
- aarch64, glibc 2.41, **SDL2 only**, no compiler. Build in an arm64 Debian trixie container (Docker/colima) so glibc matches.
- Two 640x480 panels: `DSI-2` (top, x=0) and `DSI-1` (bottom, x=640, touch). EmulationStation keeps DSI-1 powered off; a window whose title contains "Bottom Screen" is moved to DSI-1 and powers it on (sway rule).
- ES launches with `SDL_VIDEO_DISPLAY_PRIORITY=DSI-2,DSI-1`; SDL display 0 = top, 1 = bottom.
- Two Goodix touch controllers, one per panel, each 0..639 x 0..479. `fe5e0000.i2c` = bottom. ES attaches them to another Wayland seat than SDL binds, so **read the bottom node from evdev** instead of SDL touch events.
- `/usr/bin` is read-only (squashfs). Customise via `/storage/.config/emulationstation/es_systems.cfg` (change the `gba` command to a wrapper, add a core) and `/storage/.config/system/configs/system.cfg` (per-game core).
- Install everything in **one folder under `/storage/roms/ports/tmc-rgds/`** (binaries, wrapper, saves/settings: the programs use the folder they run from). Name the wrapper without `.sh` so ES does not list it as a Ports entry (ports extensions are `.sh .appimage`).

## Architecture
1. RetroArch runs the unmodified ROM with the mGBA core on the top panel.
2. `tmc-bottom` reads game RAM over RetroArch's **UDP network commands** (`READ_CORE_MEMORY <hexaddr> <n>`, port 55355, `network_cmd_enable = true`). mGBA serves the GBA address space (EWRAM `0x02..`, IWRAM `0x03..`, ROM `0x08..`).
3. RAM addresses: the decomp's `linker.ld` pins globals with `. = 0xOFF; sym = .;` (USA). Struct field offsets: compile the engine's own headers for **ARM32** (`arm-none-eabi-gcc`, the original layout) and read `offsetof` values out of the assembly (`tools/gen_layout.py`; zero offsets appear as `.space`, not `.word`).
4. The companion copies those regions into the engine's own structures and runs the unchanged snapshot publisher and painter, then draws 640x480 on the bottom panel.
5. Write-back: tap-to-equip runs the engine's `ForceEquipItem` on the mirrored copy; diff the equipped bytes afterwards and push them with `WRITE_CORE_MEMORY`, plus the two HUD refresh flags (`gHUD.unk_13/14 = 0x7f`).
6. Settings > EMULATOR drives RetroArch `SAVE_STATE_SLOT 9` / `LOAD_STATE_SLOT 9` (slot 9 so the player's own states are never touched). Settings rows that do nothing in this mode are hidden (`Port_SecondScreen_SetCompanionMode`).
7. `gba-launch` wraps the GBA launch: Minish Cap (`BZME`) + mGBA core focuses the top output (new windows open on the focused output), starts the companion beside stock `runemu.sh`, opens the network port only for that session, then restores `network_cmd_enable=false` and powers `DSI-1` off.

## Pitfalls (each one cost real debugging time)
- **`pkill -x` does not work on BusyBox**; use `pkill -9 retroarch`. Stale RetroArch instances **all bind UDP 55355** and answer each other's reads, giving nonsense that looks like a code bug. Always assert `pidof retroarch | wc -w` is 1.
- `pkill -f runemu.sh` kills your own SSH shell (its command line contains the string). Use `pkill -f "[r]unemu.sh"`.
- BusyBox `ps`/`pidof` truncate `emulationstation` to `emulationstatio`.
- The device **sleeps mid-test** and its clock can jump. Wrap tests in `systemd-inhibit --what=idle:sleep`.
- The second screen **ignores taps until the painted frame is promoted** (`Port_SecondScreen_3DS_PromoteSubmitted` after `MarkSubmitted`). If taps do nothing, check this first.
- When editing with scripted string replaces, assert the anchor exists and check the call you meant to keep survived (a replace once swallowed `Port_SecondScreenState_Publish()` and the bottom screen froze while everything else looked healthy).
- Mac and Docker-VM clocks can differ, so `make` may think objects are current. Check the binary for a new string after a change.
- `rgds` actions that change the user's setup (ES config, killing the game) need a yes; test launches should leave nothing running.

## QA harness (`companion/qa/`)
`run_qa.sh` launches the game through the wrapper with **ES's own environment** (copied from `/proc/<es pid>/environ`) and supports: `LS=<s>` send `LOAD_STATE`, `TAPS="ms,x,y;.."` software taps, `TOUCHSIM="s,x,y;.."` real multitouch events from a `uinput` virtual screen (`touchsim.py`, then `RGDS_TOUCH_DEV=<node>`), `SHOTS="s.."` capture both panels with `grim -o DSI-2/DSI-1`, `QUIT=<s>` terminate RetroArch, `CPUAT=<s>` 10 s CPU sample. Good checks: process count after exit, `network_cmd_enable` restored, `DSI-1` power false, companion RSS over a 4-minute soak, equip swap visible on the real top HUD.

## Fix patterns
- Wrong values on the bottom screen: print the mirrored `task/area/room/hp/equipped` every few seconds (`[bottom] task=...`), compare with the top panel, then check addresses/offsets.
- RetroArch has no input-injection command; use `LOAD_STATE` to get in-game quickly and a `uinput` device for touch.
- New second-screen behaviour should be gated at **runtime** (companion mode flag), not by `#ifdef`, so the native build and the companion share the same objects.
