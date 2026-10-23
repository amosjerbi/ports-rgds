# Harmony of Dissonance companion (RG DS)

Touch companion for *Castlevania: Harmony of Dissonance (USA)* on the Anystream RG DS (ROCKNIX). The game runs in
RetroArch's mGBA core on the top panel; `hod-bottom` runs on the bottom panel (DSI-1) and shows six tabs:

<p>
<img src="screenshots/spellbook.png" width="320"> <img src="screenshots/equip.png" width="320">
<img src="screenshots/item.png" width="320"> <img src="screenshots/relics.png" width="320">
<img src="screenshots/enemy-list.png" width="320"> <img src="screenshots/enemy-detail.png" width="320">
<img src="screenshots/map.png" width="320"> <img src="screenshots/map-castle-a.png" width="320">
</p>

| Tab | What it does |
| --- | --- |
| SPELLBOOK | Owned spell books (Fire / Ice / Bolt / Wind / Summoning Tome); tap to make one the active book |
| EQUIP | Whip attachment, body armor and the three accessory slots; owned gear with counts, tap to equip or clear. STR / DEF / INT / LCK and the stock counts are updated like the game does |
| ITEM | Consumables, keys, maps and jewels with counts and descriptions |
| RELICS | The twelve relics (magic items) with ON/OFF switches |
| ENEMY | The encyclopedia: all 100 enemies (level, HP, EXP, weakness, resistance, drops); dim = not yet defeated |
| MAP | The castle map, drawn live from the explored-room flags (full screen, no header), so it follows your exploration without opening the game's map. The button in the corner switches between A+B, castle A and castle B. SELECT on the handheld toggles this tab. The layout is learned from the game once, automatically (see Map tab notes) |

Lists scroll by dragging or with the arrow buttons. The header shows level, HP/MP, STR/DEF/INT/LCK, hearts, EXP, gold and play time.
Nothing from the ROM is shipped: names, descriptions and enemy data are read from the running game's ROM over RetroArch.

## How it works
It talks to RetroArch's network command interface (UDP 55355, `READ_CORE_MEMORY` / `WRITE_CORE_MEMORY`), reads touch straight
from the bottom panel's evdev node, and the GBA wrapper (`gba-launch`) starts it with the game, opens the command port only
for that session and closes it afterwards.

Memory map (EWRAM) and ROM tables: see `src/hod_data.h`. Highlights: player block at `0x02018770` (level, HP/MP, EXP, gold),
equipment bytes `0x02018780..84` (name index - 97 for body / three accessory slots, whip = name index - 88, `0xFF` = none),
consumable counts at `0x020187A0`, equipment counts at `0x020187BE` (a worn item is not in the stock count), spell book bits at
`0x0201877F`, relic switches at `0x02018841`. Text is 16-bit codes in the ROM string table at `0x08495498`; enemies are 36-byte
records at `0x080C7E44`.

## Build / install
```sh
./build.sh                         # arm64 Debian trixie container (needs Docker/colima) -> build/hod-bottom
DEVICE=192.168.0.105 ./deploy.sh   # copies hod-bottom, gba-launch and the device's DejaVuSans.ttf to /storage/roms/ports/hod-companion/
```
Then point the `gba` system's `<command>` in `/storage/.config/emulationstation/es_systems.cfg` (back it up first) at
`/storage/roms/ports/hod-companion/gba-launch %ROM% -P%SYSTEM% --core=%CORE% --emulator=%EMULATOR% --controllers="%CONTROLLERSCONFIG%"`.
Other games still go through the stock launcher. Create an empty `/storage/roms/ports/hod-companion/no-companion` file to get plain mGBA.

## QA on the device
`qa/launch_like_es.sh` launches the game through the wrapper with EmulationStation's environment; `qa/touchd.py` is a virtual
multitouch screen (`qa/start_touchd.sh`, then `echo "tap:300,450" > /tmp/touch.fifo`) and `qa/start_app.sh` runs the app against it.
Set `HOD_TEST_TAPS="ms,x,y;..."` to inject taps through the real tap handler.

## Map tab notes
* The layout (which flag draws which room) is learned from your own running game and stored locally; nothing from the ROM is shipped.
* It builds itself once, automatically, the first time the game is in normal play (about 20 seconds; the game's map flashes on the top screen), and is kept in `map-layout.cache`. `BUILD LIVE MAP` on the tab repeats it by hand.
* It needs RetroArch's `network_remote_enable` + `network_remote_enable_user_p1` (`gba-launch` turns them on for the session and off again afterwards). The test patterns written into the explored-room flags are always put back.
* Both castles share one room grid. Every 8-byte row of the explored-room arrays at `0x020000C4` and `0x020001FC` holds castle A's 32 flags, then castle B's; the A+B view colours a room blue (only A), green (only B) or cyan (both), as the game does.
* The game's own map screen also draws a title, the percentage and the L/R arrows into the same bitmap; those are masked out. There is no position marker yet.

## Caveats
* USA ROM only (`ACHE`). Writes go to the running game's RAM; nothing is saved until the game itself saves.
* Not verified: whip attachments are always listed (no ownership flags found); relic ownership and spell book ownership bits were
  inferred from this save; weakness/resistance bits other than Fire (1) and Thunder (4) are labelled by guess (Ice, Wind, Holy, ...).
* Relic switches and whip attachments do not recompute stats (the game does that itself); equipment changes do.
