# Pokémon Emerald companion (RG DS)

Touch companion for **Pokémon Emerald (USA/Europe, `BPEE`)** on the Anystream RG DS (ROCKNIX). RetroArch's mGBA core runs
the game on the top panel; `pe-bottom` runs on the bottom panel and reads the game's RAM over RetroArch's UDP command
interface (port 55355). Read-only.

## Tabs
- **PARTY** – six slots with nickname, species, level and HP bar; tap one for its four moves, stats, held item and EXP.
- **BAG** – Items, Key, Balls, TM/HM and Berries pockets with quantities (names read from the ROM); drag or use the arrows.
- **TRAINER** – name, trainer ID, play time, money, coins, Pokédex caught/seen and the eight badges. The header always shows money, badges, dex and time.

## Memory map (layouts from the pokeemerald decomp, verified on the running game)
| What | Address |
|---|---|
| ROM code | `0x080000AC` (`BPEE`) |
| SaveBlock1 / SaveBlock2 pointers | `0x03005D8C` / `0x03005D90` – the blocks move on every load, so they are re-read each poll |
| Party count / party | `0x020244E9` / `0x020244EC` (6 × 100 bytes; the 48-byte substructure block is XOR-encrypted with PID ^ OTID and shuffled by PID % 24) |
| Money, coins, bag quantities | SaveBlock1 `+0x490`, `+0x494`, `+0x560…` – XOR'd with the key at SaveBlock2 `+0xAC` |
| Badges | flags `0x867..0x86E` (SaveBlock1 `+0x1270` flag array) |
| Pokédex | SaveBlock2 `+0x28` (owned) / `+0x5C` (seen) bitfields |
| Species / move / item names | ROM `0x083185C8` (11 B), `0x0831977C` (13 B), `0x085839A0` (44-B records) |

## Build / deploy
```
./build.sh                  # arm64 Docker build -> build/pe-bottom
WRAPPER=1 ./deploy.sh       # installs to /storage/roms/ports/pe-companion and adds the Emerald branch to gba-launch
```
`gba-launch` is the shared wrapper in `/storage/roms/ports/tmc-rgds/` (backup `gba-launch.bak-pe`). The ROM and save are not included.
