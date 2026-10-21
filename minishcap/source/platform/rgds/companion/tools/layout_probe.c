/* Emits the GBA (32-bit) field offsets of the engine structures the second screen reads, by compiling the
 * engine's own headers for ARM32 (the original layout). Run through gen_layout.py; never linked. */
#include <stddef.h>
#include "area.h"
#include "game.h"
#include "item.h"
#include "itemMetaData.h"
#include "main.h"
#include "player.h"
#include "room.h"
#include "save.h"
#include "subtask.h"
#include "ui.h"

#define P(name, expr) const unsigned long probe_##name = (unsigned long)(expr);

P(Main_task, offsetof(Main, task))
P(Main_substate, offsetof(Main, substate))
P(Main_size, sizeof(Main))
P(UI_nextToLoad, offsetof(UI, nextToLoad))
P(UI_lastState, offsetof(UI, lastState))
P(UI_state, offsetof(UI, state))
P(UI_field_0x3, offsetof(UI, field_0x3))
P(UI_field_0x5, offsetof(UI, field_0x5))
P(UI_size, sizeof(UI))
P(RoomControls_area, offsetof(RoomControls, area))
P(RoomControls_room, offsetof(RoomControls, room))
P(RoomControls_size, sizeof(RoomControls))
P(PlayerEntity_x, offsetof(PlayerEntity, base.x))
P(PlayerEntity_y, offsetof(PlayerEntity, base.y))
P(PlayerState_chargeAction, offsetof(PlayerState, chargeState.action))
P(PlayerState_chargeTimer, offsetof(PlayerState, chargeState.chargeTimer))
P(Area_areaMetadata, offsetof(Area, areaMetadata))
P(Area_dungeon_idx, offsetof(Area, dungeon_idx))
P(Area_portal_mode, offsetof(Area, portal_mode))
P(Area_roomResInfos, offsetof(Area, roomResInfos))
P(Area_size, sizeof(Area))
P(RoomResInfo_size, sizeof(RoomResInfo))
P(RoomResInfo_map_x, offsetof(RoomResInfo, map_x))
P(RoomResInfo_map_y, offsetof(RoomResInfo, map_y))
P(RoomResInfo_pixel_width, offsetof(RoomResInfo, pixel_width))
P(RoomResInfo_pixel_height, offsetof(RoomResInfo, pixel_height))
P(HUD_rActionPlayerState, offsetof(HUD, rActionPlayerState))
P(HUD_rActionInteractObject, offsetof(HUD, rActionInteractObject))
P(HUD_size, sizeof(HUD))
P(SaveFile_size, sizeof(SaveFile))
P(SaveHeader_language, offsetof(SaveHeader, language))
P(SaveFile_equipped, offsetof(SaveFile, stats.equipped))
P(HUD_unk_13, offsetof(HUD, unk_13))
P(HUD_unk_14, offsetof(HUD, unk_14))
