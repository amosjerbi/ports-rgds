# Shining Soul II companion (RG DS)

Touch companion for **Shining Soul II (USA)** on the ROCKNIX RG DS. RetroArch's mGBA core runs the game on the top
screen; `ss2-bottom` (SDL2, 640x480) runs on the bottom panel.

## What it shows

STATUS: name, gold, live HP and SP bars, ATK / DEF / VIT / STR / DEX / INT. Two buttons press the game's own buttons
through RetroArch's network gamepad: **USE ITEM** presses R (the item equipped in the ITEM slot) and **SWAP WEAPON**
presses L (the ARM slot).

The inventory grid is not shown: its RAM layout was not found.

## Memory map (AU2E)

| What | Address |
| --- | --- |
| Name, 7 x u16, `A` = 0x21 | `0x02003872` |
| STR DEX INT VIT, u16 | `0x02003884` |
| Gold, u16 | `0x0200388C` |
| ATK, DEF, u16 | `0x020038A4` |
| Current HP, u32 (authoritative copy) | `0x03003ED0` |
| HP, SP x10, 0, max HP, max SP x10 (u32, mirror) | `0x0200B260` |

The EWRAM copy of HP is rewritten from the IWRAM one every frame; max HP = VIT x 3 and max SP = INT x 2.

## Build and install

`./build.sh` (Docker, arm64) then `./deploy.sh`; `WRAPPER=1 ./deploy.sh` also installs this folder's `gba-launch`
(the master wrapper with every companion's branch, old one kept as `gba-launch.bak-ss2`). RetroArch's network
command and gamepad ports are enabled only while the game runs.

## Two handhelds over wifi

`netplay/` has an IPS patch (my own bytes only, no ROM data), a bridge and a launch wrapper that let two handhelds on the
same wifi play Shining Soul II's **Multiplayer Mode** as if linked by cable. The patch replaces the link library's poll
so the game's own lobby, character exchange and co-op code run unchanged; `ss2-link.py` carries the 24-byte link records
(about 60 per second each way) over UDP through RetroArch's network command port. The handheld with the lower IP is
player 1. Setup, play and rebuild steps are in [`netplay/README.md`](netplay/README.md).
