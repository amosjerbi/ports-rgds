<p align="center">
  <img src="screens/Dual%20screen.png" alt="For the Frog the Bell Tolls on the RG DS: the game on the top panel, its own pause menu on the bottom panel" width="760">
</p>

# For the Frog the Bell Tolls — pause-menu companion for the RG DS

The game on the top panel and **its own pause menu** (STATUS, HOLD, SELECT items with counts, KEY) on the bottom
panel, with live LIFE and COIN and **tap to use an item** (drink the heart bottle, eat a tomato, equip). Works with the
[colourised build](../frog) and RetroArch's gambatte or SameBoy core on the Anystream RG DS (ROCKNIX).

| ![Heart bottle: tap, health goes up, bottle is used](screens/Heart%20bottle.png) |
|:--:|
| Heart bottle: health 16/56 → 28/56, the bottle becomes empty, and the top screen's hearts follow |

![Live updates: counts and coins change on the bottom screen as the game changes](screens/Live%20updates.png)

## How it works
The pause menu is drawn by the game from a save record in work RAM. Instead of re-implementing it, `frog-bottom`
hosts a **second, hidden copy of the game** (the same libretro core, loaded with `dlopen`) parked in the menu.
When the live game's record changes (read over RetroArch's network command interface), the record is copied into the
hidden copy, which redraws the menu with its own code; the frame is scaled onto the bottom panel. ARMOR, JUMP, the
HOLD slots and the keys are exactly what the game computes. Details, the record layout and the pitfalls are in
[`source/README.md`](source/README.md) and [`SKILL-frog-companion.md`](SKILL-frog-companion.md).

Tapping a cell moves the hidden copy's cursor there and presses A. The changes the game makes to the save record are
written back to the live game, and the live HUD hearts are brought in line.

## What is in this folder
| Path | Contents |
| --- | --- |
| `source/` | the companion (`frog_bottom.c`, a small libretro host, a RetroArch network client), the launch wrapper `gb-launch`, the QA scripts, a Docker build |
| `SKILL-frog-companion.md` | a Claude Code skill: approach, memory layout, pitfalls, QA workflow |
| `screens/` | screenshots |

**Not included, on purpose:** the ROM, saves and save states, any extracted game data, and prebuilt binaries.

## Build
You need Docker. `./build.sh` (in `source/`) builds `build/frog-bottom` for aarch64 Linux in a Debian trixie container
(glibc 2.41, the same as ROCKNIX) and links the device's own SDL2.

## Install on the RG DS
Everything goes in one folder, `/storage/roms/ports/frog/`. It is **not** a Ports-menu entry (no `.sh` files; the
wrapper has no extension on purpose).

1. Copy `frog-bottom` and `source/gb-launch` there (`chmod +x`). Your ROM stays in `/storage/roms/gb/`.
2. Back up `/storage/.config/emulationstation/es_systems.cfg`, then change the `gb` system's command to
   `/storage/roms/ports/frog/gb-launch %ROM% -P%SYSTEM% --core=%CORE% --emulator=%EMULATOR% --controllers="%CONTROLLERSCONFIG%"`
   (it was `/usr/bin/runemu.sh %ROM% ...`). Restart EmulationStation.
3. Launch the game from the Game Boy list with the gambatte or SameBoy core and load a diary. The bottom panel shows
   "WAITING FOR THE GAME" until a diary is loaded.

The wrapper starts the companion only for this game (header title `KAERUNOTAMENI`), opens RetroArch's network port only
while it runs, and puts the lower panel back to off afterwards. Every other Game Boy game and core goes to the stock
launcher unchanged. An empty `no-companion` file next to the wrapper disables it.

Tapping the **diary** (bottom-right icon) opens the game's save screen in the live game: the companion refocuses RetroArch and
presses Start, the arrows and A through RetroArch's network remote, which the wrapper also enables only for the session.

## Status
Tested on a real RG DS (gambatte and SameBoy): the menu matches the game's own, live updates, tap-to-use with
write-back, recovery when an action leaves a sub-screen, CPU about 4% of a core, 37 MB. Not tested: a real fingertip
(a virtual multitouch screen was used), other ROM versions.

## Credits and legal
Built on the [colourisation patch](../frog) project, RetroArch, gambatte and SameBoy. The code here is GPL-3.0-or-later.

*For the Frog the Bell Tolls* belongs to Nintendo. This is an unofficial fan project, not affiliated with or endorsed
by Nintendo. No ROM or game data is distributed here; use a copy you legally own.
