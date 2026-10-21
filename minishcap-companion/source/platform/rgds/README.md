# The Minish Cap RG DS

Dual-screen Minish Cap for the Anystream RG DS (ROCKNIX, aarch64): the game on the top panel (DSI-2) and the
project's second screen - map, quest status, items and touch equip - on the bottom panel (DSI-1, touch).
It reads your own USA `.gba`; nothing from the ROM is shipped.

There are two ways to run it. Both use the same second-screen code (`src/port/port_second_screen*.c`).

| Mode | Game runs in | Launch |
| --- | --- | --- |
| **mGBA + companion** (default) | RetroArch's real mGBA core | the normal GBA-list entry |
| **Native** | this project's engine (no emulator) | GBA list > Core > `minishcap-rgds` |

## mGBA + companion (`tmc-bottom`)
RetroArch runs the unmodified ROM on the top panel. `tmc-bottom` runs beside it and:

1. reads the game's RAM over RetroArch's network command interface (`READ_CORE_MEMORY`, UDP 55355),
2. mirrors the structures the second screen needs (`gMain`, `gSave`, `gRoomControls`, the player, `gArea`,
   `gHUD`, `gUI`) into the engine's own structures, using the real GBA addresses (decomp `linker.ld`) and
   32-bit field offsets (compiled from the engine's headers for ARM32), then runs the unchanged snapshot
   publisher and painter,
3. draws the result on the bottom panel at 640x480,
4. writes equips made by touch back into the emulator (`WRITE_CORE_MEMORY`: the two equipped bytes and the
   two HUD refresh flags, exactly what `ForceEquipItem` changes),
5. maps the Settings > EMULATOR rows to RetroArch `SAVE_STATE_SLOT 9` / `LOAD_STATE_SLOT 9` (slot 9, so your own
   states are never overwritten).

It does not touch the emulator's video or input. Cost measured on the device: under 1% of one core.
**USA ROM only** (`BZME`): the RAM map comes from the USA decomp; other regions get plain mGBA.

Touch is read straight from the bottom panel's evdev node (`fe5e0000.i2c`), because EmulationStation attaches
the touchscreens to a different Wayland seat than SDL uses. `RGDS_TOUCH_DEV` overrides the node.

## Build
`./platform/rgds/build.sh` builds `build/tmc-rgds` and `build/tmc-bottom` in an arm64 Debian trixie container
(same glibc as ROCKNIX) and links the device's own SDL2. Needs Docker (colima on Apple Silicon).

Regenerating `companion/gba_layout.h` (only needed if the engine headers change):
```sh
arm-none-eabi-gcc -std=gnu11 -S -O0 -w -include region.h -DTMC_3DS -DPC_PORT -DNON_MATCHING -DUSE_HDMA \
  -DMULTI_REGION -DUSA -DENGLISH -DREVISION=0 -I. -Isrc/engine/include -Isrc/port -Isrc/port/ppu/include \
  -Isrc/offsets/USA -Iplatform/3ds/source platform/rgds/companion/tools/layout_probe.c -o /tmp/probe.s
python3 platform/rgds/companion/tools/gen_layout.py platform/rgds/companion/tools/tmc_linker.ld /tmp/probe.s \
  platform/rgds/companion/gba_layout.h
```
`tools/tmc_linker.ld` is zeldaret/tmc's `linker.ld` (GPL-3.0), the source of the RAM addresses.

## Install on the device
Everything lives in one folder, `/storage/roms/ports/tmc-rgds/` (the ports folder, but **not** a Ports-menu entry:
EmulationStation only lists `.sh`/`.appimage` files there, and the wrapper has no extension on purpose):

* `tmc-rgds`, `tmc-bottom` - the binaries; `gba-launch` - the launch wrapper (`companion/gba-launch`).
* Saves and settings (`tmc.sav`, `tmc.ini`, `tmc-bottom.ini`) are written in that same folder: the programs use
  the folder they run from (`RGDS_DATA_DIR` overrides it), so the whole port can be moved or backed up as one.
* `es_systems.cfg` (`/storage/.config/emulationstation/`, back it up first): in the `gba` system, change the
  command to `/storage/roms/ports/tmc-rgds/gba-launch %ROM% -P%SYSTEM% ...` (was `/usr/bin/runemu.sh %ROM% ...`)
  and add `<core>minishcap-rgds</core>` under the `retroarch` emulator's cores.
* The wrapper starts `tmc-bottom` for Minish Cap with the mGBA core, opens RetroArch's network command port only
  for that session (`network_cmd_enable` in `retroarch.cfg`), closes it afterwards, focuses the top panel before
  launching (new windows open on the focused output) and puts the lower panel back to off (EmulationStation keeps
  it off). Create an empty `no-companion` file next to the wrapper to get plain mGBA.

## Settings on the second screen
In mGBA mode only options that act on this screen are shown: SCREEN (panel backdrop), GAMEPLAY (windcrest
pins, floor auto-return) and EMULATOR (save/load state). The 3DS engine options (aspect ratio, turbo speed,
volume, autosave, memory dump, updater) are hidden because they do nothing here. The native build keeps them.

## QA harness
`companion/qa/run_qa.sh` launches the game through the wrapper with EmulationStation's own environment, can
send RetroArch `LOAD_STATE`, inject taps (software, or real multitouch events through a `uinput` virtual
screen via `touchsim.py`), capture both panels and measure CPU. Examples are in the script header.

## Not done
Audio in the native build (the mGBA mode has RetroArch's audio), widescreen, the EU ROM in companion mode,
turbo/volume controls for the emulator.
