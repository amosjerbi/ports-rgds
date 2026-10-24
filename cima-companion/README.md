# CIMA: The Enemy companion (RG DS)

Touch companion for **CIMA: The Enemy (USA, `BCME`)** on the Anystream RG DS (ROCKNIX). RetroArch's mGBA core runs the
game on the top panel; `cima-bottom` runs on the bottom panel and reads the game's RAM over RetroArch's UDP command
interface (port 55355). It is read-only.

## What it does
- **ROSTER** – 16 characters with a live HP bar (current/max) and their four stats; tap one for a detail line, scroll with the
  arrows or by dragging.

## Memory map (matched against the party screen: Ark J 18 HP, Ivy F 16 HP, stats 5 5 5 5)
| What | Address |
|---|---|
| ROM code | `0x080000AC` (`BCME`) |
| Character records | `0x02020224`, 16 x `0xDC` bytes (a second copy sits at `0x02012A28`) |
| HP current / max | record `+0x30` / `+0x31` (u8) |
| Four stats | record `+0x01..+0x04` (u8) |

Limits: names are assigned in the game's roster order (Ark, Ivy, Vanrose, Shelley, Jean, Eberle, Halley, Emmy, Philis, Telmia,
Doug, Ileyda, Rick, Diana, Yurald, Claude); only the first two were confirmed on screen. Writing HP did not change the HUD
bar, so the real live HP, party membership, items and map are not mapped yet.

## Build / deploy
```
./build.sh                  # arm64 Docker build -> build/cima-bottom
WRAPPER=1 ./deploy.sh       # installs to /storage/roms/ports/cima-companion and adds the CIMA branch to gba-launch
```
`gba-launch` is the shared wrapper in `/storage/roms/ports/tmc-rgds/` (backup `gba-launch.bak-cima`; `bak-dd` is the version
before the CIMA branch). The ROM is not included.
