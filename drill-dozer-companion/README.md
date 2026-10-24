# Drill Dozer companion (RG DS)

Touch companion for **Drill Dozer (USA, `V49E`)** on the Anystream RG DS (ROCKNIX). RetroArch's mGBA core runs the
game on the top panel; `dd-bottom` runs on the bottom panel and talks to RetroArch over its UDP command interface
(port 55355).

## What it does
- **CHIPS** – shows the chip counter (the currency on the Equipment screen) and edits it: -1000 … +1000, ZERO, MAX 99999.
- **GUIDE** – short in-game reference (hub menu, shop items, 31 treasures).

## Memory map (found by writing values and reading the Equipment screen)
| What | Address | Notes |
|---|---|---|
| ROM code | `0x080000AC` | `V49E` |
| Chips | `0x02034670` | u32, confirmed live (changing it updates the Equipment screen) |

Not found yet: gear, current/max energy (writes to the save-struct candidates at `0x02034664` / `0x020346BC` and the
IWRAM copy at `0x030012B0` did not change the Equipment screen), drill bit/type, treasures and stage progress.

## Build / deploy
```
./build.sh                       # arm64 Docker build -> build/dd-bottom
WRAPPER=1 ./deploy.sh            # installs to /storage/roms/ports/dd-companion and adds the Drill Dozer branch to gba-launch
```
`gba-launch` is the shared wrapper in `/storage/roms/ports/tmc-rgds/` (backup kept as `gba-launch.bak-dd`). The ROM and
save files are not included.
