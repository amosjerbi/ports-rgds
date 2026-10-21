/* Mirrors the parts of the running game's state that the second screen needs from a live mGBA (via
 * RetroArch's memory interface) into this program's own engine structures, then runs the project's
 * unchanged snapshot publisher. Engine-side TU: compiled with the engine flags. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "area.h"
#include "main.h"
#include "player.h"
#include "port_gba_mem.h"
#include "port_second_screen_state.h"
#include "room.h"
#include "save.h"
#include "structures.h"
#include "ui.h"

#include "gba_layout.h"
#include "ra_client.h"

/* The save block holds no pointers, so its native layout is the GBA one and can be copied verbatim. */
_Static_assert(sizeof(SaveFile) == GBA_SaveFile_size, "SaveFile layout differs from the GBA layout");

static uint16_t Le16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static bool Rd(uint32_t addr, void* out, size_t n) {
    return RaClient_Read(addr, out, n);
}

/* Returns true when a full pass succeeded. On failure the previous state stays in place. */
bool RamMirror_Update(void) {
    uint8_t buf[GBA_SaveFile_size];

    /* gMain.task / substate */
    if (!Rd(GBA_ADDR_gMain, buf, GBA_Main_size))
        return false;
    gMain.task = buf[GBA_Main_task];
    gMain.substate = buf[GBA_Main_substate];

    /* gUI: the intro-cinema / subtask-context fields */
    if (!Rd(GBA_ADDR_gUI, buf, 8))
        return false;
    gUI.nextToLoad = buf[GBA_UI_nextToLoad];
    gUI.lastState = buf[GBA_UI_lastState];
    gUI.field_0x3 = buf[GBA_UI_field_0x3];
    gUI.state = buf[GBA_UI_state];
    gUI.field_0x5 = buf[GBA_UI_field_0x5];

    /* gSaveHeader (start of EWRAM): language selects the HUD label block */
    if (!Rd(GBA_ADDR_gSaveHeader, buf, GBA_SaveHeader_language + 1))
        return false;
    gSaveHeader->language = buf[GBA_SaveHeader_language];

    /* gSave: verbatim */
    if (!Rd(GBA_ADDR_gSave, buf, GBA_SaveFile_size))
        return false;
    memcpy(&gSave, buf, GBA_SaveFile_size);
    uint8_t equippedBefore[2] = { gSave.stats.equipped[SLOT_A], gSave.stats.equipped[SLOT_B] };

    /* gRoomControls */
    if (!Rd(GBA_ADDR_gRoomControls, buf, GBA_RoomControls_size))
        return false;
    gRoomControls.area = buf[GBA_RoomControls_area];
    gRoomControls.room = buf[GBA_RoomControls_room];

    /* gPlayerEntity position: x/y are 16.16 fixed point, the snapshot uses the integer half */
    if (!Rd(GBA_ADDR_gPlayerEntity + GBA_PlayerEntity_x, buf, 8))
        return false;
    gPlayerEntity.base.x.HALF.HI = Le16(buf + 2);
    gPlayerEntity.base.y.HALF.HI = Le16(buf + GBA_PlayerEntity_y - GBA_PlayerEntity_x + 2);

    /* gPlayerState.chargeState */
    if (!Rd(GBA_ADDR_gPlayerState + GBA_PlayerState_chargeAction, buf, 6))
        return false;
    gPlayerState.chargeState.action = buf[0];
    gPlayerState.chargeState.chargeTimer = (s16)Le16(buf + (GBA_PlayerState_chargeTimer - GBA_PlayerState_chargeAction));

    /* gArea: identity flags, then the room table the automap uses */
    if (!Rd(GBA_ADDR_gArea, buf, GBA_Area_roomResInfos))
        return false;
    gArea.areaMetadata = buf[GBA_Area_areaMetadata];
    gArea.dungeon_idx = buf[GBA_Area_dungeon_idx];
    gArea.portal_mode = buf[GBA_Area_portal_mode];
    {
        static uint8_t rooms[MAX_ROOMS * GBA_RoomResInfo_size];
        if (!Rd(GBA_ADDR_gArea + GBA_Area_roomResInfos, rooms, sizeof rooms))
            return false;
        for (int i = 0; i < MAX_ROOMS; i++) {
            const uint8_t* r = rooms + i * GBA_RoomResInfo_size;
            gArea.roomResInfos[i].map_x = Le16(r + GBA_RoomResInfo_map_x);
            gArea.roomResInfos[i].map_y = Le16(r + GBA_RoomResInfo_map_y);
            gArea.roomResInfos[i].pixel_width = Le16(r + GBA_RoomResInfo_pixel_width);
            gArea.roomResInfos[i].pixel_height = Le16(r + GBA_RoomResInfo_pixel_height);
        }
    }

    /* gHUD: contextual R-button prompt */
    if (!Rd(GBA_ADDR_gHUD + GBA_HUD_rActionInteractObject, buf, 4))
        return false;
    gHUD.rActionInteractObject = buf[0];
    gHUD.rActionPlayerState = buf[GBA_HUD_rActionPlayerState - GBA_HUD_rActionInteractObject];

    Port_SecondScreenState_Publish();
    {   /* QA trace: one line every ~3 s so a wrong address or a stalled link shows up in tmc-bottom.log */
        static unsigned n;
        if ((++n % 90) == 1) {
            SecondScreenSnapshot snap;
            Port_SecondScreenState_Read(&snap);
            fprintf(stderr, "[bottom] task=%u sub=%u area=%u room=%u hp=%u/%u rupees=%u A=%02x B=%02x inGame=%u\n",
                    gMain.task, gMain.substate, gRoomControls.area, gRoomControls.room, gSave.stats.health,
                    gSave.stats.maxHealth, gSave.stats.rupees, gSave.stats.equipped[SLOT_A],
                    gSave.stats.equipped[SLOT_B], snap.inGame);
        }
    }

    /* Tap-to-equip: the publisher applies ForceEquipItem to our mirrored gSave. Push any change back into
     * the emulator exactly as ForceEquipItem writes it (both equipped bytes, plus the two HUD refresh
     * flags that make the real HUD redraw), then the next pass reads the result back. */
    {
        const uint8_t before[2] = { equippedBefore[0], equippedBefore[1] };
        const uint8_t after[2] = { gSave.stats.equipped[SLOT_A], gSave.stats.equipped[SLOT_B] };
        if (memcmp(before, after, 2) != 0) {
            const uint8_t hud[2] = { 0x7f, 0x7f };
            bool ok = RaClient_Write(GBA_ADDR_gSave + GBA_SaveFile_equipped, after, 2) &&
                      RaClient_Write(GBA_ADDR_gHUD + GBA_HUD_unk_13, hud, 2);
            fprintf(stderr, "[bottom] equip %02x/%02x -> %02x/%02x %s\n", before[0], before[1], after[0], after[1],
                    ok ? "written" : "FAILED");
            if (!ok) { /* keep the mirror honest: next pass re-reads the real values */
                gSave.stats.equipped[SLOT_A] = before[0];
                gSave.stats.equipped[SLOT_B] = before[1];
            }
        }
    }
    return true;
}
