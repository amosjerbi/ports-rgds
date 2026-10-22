# Aria of Sorrow companion (RG DS)

Touch companion for *Castlevania: Aria of Sorrow (USA)* on the Anystream RG DS (ROCKNIX). The game runs in
RetroArch's mGBA core on the top panel; `aos-bottom` runs on the bottom panel (DSI-1) and shows five tabs:

| Tab | What it does |
| --- | --- |
| SOUL SET | Bullet / Guardian / Enchant: owned souls with counts, tap to equip (or clear) |
| EQUIP | Weapon / Armor / Accessory: owned gear, tap to equip; the game recalculates ATT/DEF itself |
| ITEM | Consumables with counts and descriptions; USE for Potion, High/Super Potion, Meat Strip, Tasty Meat, Mind Up, High Mind Up, Mana Prism |
| ABILITY | The six ability souls with ON/OFF switches |
| ENEMY | All 113 enemies (HP, EXP, weakness, resistance, drops, soul, description); dim = soul not obtained |

Lists scroll by dragging or with the arrow buttons. The header shows level, HP/MP, ATT/DEF, stats, EXP, gold and play time.
Nothing from the ROM is shipped: names, descriptions and enemy data are read from the running game's ROM over RetroArch.

## How it works
Like the Minish Cap companion in `platform/rgds/`: it talks to RetroArch's network command interface (UDP 55355,
`READ_CORE_MEMORY` / `WRITE_CORE_MEMORY`), reads touch straight from the bottom panel's evdev node, and the GBA wrapper
(`gba-launch`) starts it with the game, opens the command port only for that session and closes it afterwards.

Memory map (EWRAM; the save slot at `0x0201324C` mirrors the SRAM slot byte for byte): see `src/aos_data.h`.
Highlights: items = 136 bytes of counts at `0x02013294`; souls = four nibble-packed count arrays (bullet 56, guardian 24,
enchant 35, ability 6); equipped soul bytes `0x02013269..6B` (index+1), weapon `0x02013268` (item-32), armor `0x0201326C`
and accessory `0x0201326D` (item-91); ability switches are a bit mask at `0x02013396`. Text is the ROM string table at
`0x08506B38`, enemies are 36-byte records at `0x080E964C`.

## Build / install
```sh
./build.sh                 # arm64 Debian trixie container (needs Docker/colima) -> build/aos-bottom
DEVICE=192.168.0.105 ./deploy.sh   # copies aos-bottom + the device's DejaVuSans.ttf to /storage/roms/ports/aos-companion/
```
Then install `gba-launch` as `/storage/roms/ports/tmc-rgds/gba-launch` (it is the existing Minish Cap wrapper plus an Aria
branch; `es_systems.cfg` must already point the `gba` system at it, see `platform/rgds/README.md`). Create an empty
`/storage/roms/ports/aos-companion/no-companion` file to get plain mGBA.

## QA on the device
`qa/launch_like_es.sh` launches the game through the wrapper with EmulationStation's environment; `qa/touchd.py` is a virtual
multitouch screen (`qa/start_touchd.sh`, then `echo "tap:300,450" > /tmp/touch.fifo`) and `qa/start_app.sh` runs the app against it.
Set `AOS_TEST_TAPS="ms,x,y;..."` to inject taps through the real tap handler.

## Caveats
* USA ROM only (`A2CE`). Writes go to the running game's RAM; nothing is saved until the game itself saves.
* Weakness/resistance names (Slash, Fire, Ice, Thunder, Poison, Holy, Curse, Dark) were matched against the in-game icons for
  the common ones; the Thunder/Curse labels are unverified.
* Healing items are applied by the companion (HP/MP written directly), they do not run the game's item code.
