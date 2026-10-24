# Sword of Mana companion (RG DS)

Bottom-panel companion for **Sword of Mana (USA/Australia, `AVSE`)** on the Anystream RG DS (ROCKNIX). RetroArch's mGBA core
runs the game on the top panel; `som-bottom` reads the game's RAM over RetroArch's UDP command interface (port 55355).
Read-only.

## Tabs
- **STATUS** – hero and partner HP/MP, plus the eight recovery items. **Tap an item to use it on the hero:** the companion presses Start > Items > Recovery > the item > A on the hero through RetroArch's network gamepad (UDP 55400), then closes the menu. It follows the menu state at `0x02013F40` (0 field, 7 top ring, 6 category, 11 list, 3 target) and the cursor at `0x02013EC4`. It only targets the hero, and it opens the pause menu on the top screen for about 3 seconds.
- **ITEMS** – Heal, Mystery, Coin/Icon, Meat, Seed/Fruit and Veggie groups; owned items are bright with their count, the rest dimmed. Drag or use the arrows.
- **ACCESSORIES** – the 28 accessories the game's list shows, with owned counts.

## STATUS tab
Hero and partner side by side: HP, MP and Pow / Def / Int / Mnd / Agi, plus lucre and play time in the header (and the hero's level).
All values were matched to the in-game Status screen (Alex Lv 49, 276/276 HP, 143/143 MP, stats 50 60 25 35 53; partner 195/195, 224/224, 22 20 99 51 21; 9551 lucre).

## Memory map
| What | Address |
|---|---|
| ROM code | `0x080000AC` (`AVSE`) |
| HP, max HP, MP, max MP (u16 ×4) | hero `0x02002E02`, partner `0x02002E2A` |
| Pow Def Int Mnd Agi (u16 ×5) | hero `0x02025A10`, partner `0x02025A1C` |
| Lucre (u32) | `0x02025A70` (also mirrored at `0x02021BA0`) |
| Play time (frames, 60/s) | `0x02021B9C` |
| Hero level (u16) | `0x02021BD2` (in the loaded save buffer) |

Item counts are u16 per slot from `0x02020D58` (the array the menus read; `0x020030B8` is a copy). Slots 0-87 are Heal 0-7, Mystery 8-15, Coins 16-23, Spirit Icons 24-31, Meat 32-47, Seeds 48-55, Fruit 56-71, Veggies 72-87; accessories sit at slots 100-123, 134, 135, 156, 157. Names for slots 0-87 follow the game's own item-name order (heal, meat and the first mystery slot were checked in the menus); slot 12 is shown as Magic Rope (an assumption).

Not mapped: partner level, character names, weapon and spirit levels, key items, slots 88-99 / 124-133 / 136-155 (equipment that the Accessories list does not show). The game rewrites lucre every frame, so values are not editable.

## Build / deploy
```
./build.sh                  # arm64 Docker build -> build/som-bottom
WRAPPER=1 ./deploy.sh       # installs to /storage/roms/ports/som-companion and adds the Sword of Mana branch to gba-launch
```
`gba-launch` is the shared wrapper in `/storage/roms/ports/tmc-rgds/` (backup `gba-launch.bak-som`). ROM and save are not included.
