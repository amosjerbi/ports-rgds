/* RG DS presenter for the shared software PPU (src/port/ppu). Engine-side: compiled with the
 * engine flags; it hands finished frames to display_rgds.c, which owns SDL2. */
#include <stdbool.h>
#include <stdint.h>

#include "port_gba_mem.h"
#include "port_hdma.h"
#include "port_ppu.h"
#include "virtuappu.h"
#include "cpu/mode1.h"
#include "port_second_screen.h"
#include "port_second_screen_3ds.h"
#include "port_second_screen_state.h"
#include "display_rgds.h"

#define GBA_W 240
#define GBA_H 160

#define SCREEN_W 640 /* the bottom panel; the second screen is 4:3 like the 3DS bottom screen, at 2x */
#define SCREEN_H 480
#define BOTTOM_CADENCE 2u /* check the bottom screen every 2nd frame */

static uint32_t sTopFrame[GBA_W * GBA_H];
static uint32_t sBottomFrame[SCREEN_W * SCREEN_H];
static bool sInitialized;
static bool sBottomOnly; /* companion: the emulator draws the game, so skip rendering the top frame */

void Port_PPU_RGDS_BottomOnly(int on) { sBottomOnly = on != 0; }
static bool sBottomReady;
static bool sBottomValid;
static uint32_t sFrame, sBottomTick, sBottomPaintedTick;
static SecondScreenSnapshot sBottomSnapshot;

void Port_PPU_Init(SDL_Window* window) {
    (void)window;
    VirtuaPPUMode1GbaMemory memory = { gIoMem, gVram, gBgPltt, gObjPltt, gOamMem };
    virtuappu_mode1_bind_gba_memory(&memory);
    virtuappu_mode1_set_color_correction(false);
    virtuappu_registers.frame_width = GBA_W;
    virtuappu_registers.frame_height = GBA_H;
    virtuappu_registers.frame_pitch = GBA_W;
    virtuappu_registers.mode = 1;
    virtuappu_mode1_set_output_buffer(sTopFrame, GBA_W);
    Port_SecondScreen_Init();
    Port_SecondScreen_3DS_ResetFrameState();
    sInitialized = true;
}

void Port_PPU_Shutdown(void) {
    sInitialized = false;
}

/* Simplified 3DS bottom-screen scheduler (port_ppu_3ds.c): same refresh predicates, no worker thread. */
static void PresentBottom(void) {
    ++sFrame;
    const bool cadenceDue = (sFrame % BOTTOM_CADENCE) == 0;
    if (cadenceDue)
        ++sBottomTick;
    const bool forced = !sBottomReady || Port_SecondScreen_3DS_NeedsRefresh();
    if (!forced && !cadenceDue)
        return;
    SecondScreenSnapshot next;
    Port_SecondScreenState_Read(&next);
    const bool changed = Port_SecondScreen_3DS_SnapshotChangeNeedsRefresh(&sBottomSnapshot, &next, sBottomValid);
    const bool animated =
        Port_SecondScreen_3DS_NeedsPeriodicRefresh(&next, sBottomTick, sBottomPaintedTick, SCREEN_W, SCREEN_H);
    if (!(forced || changed || animated))
        return;
    if (!Port_SecondScreen_3DS_NeedsRefresh())
        Port_SecondScreen_3DS_RequestRefresh();
    sBottomSnapshot = next;
    sBottomValid = true;
    sBottomPaintedTick = sBottomTick;
    const uint32_t generation =
        Port_SecondScreen_3DS_PaintInto(sBottomFrame, SCREEN_W, SCREEN_H, SCREEN_W, &sBottomSnapshot, sBottomTick);
    RgdsDisplay_PresentBottom(sBottomFrame, SCREEN_W, SCREEN_H, SCREEN_W);
    Port_SecondScreen_3DS_MarkSubmitted(generation, sBottomSnapshot.inGame != 0);
    /* The 3DS promotes the submitted generation once its texture is on screen; touches are rejected
     * until then (BottomFrameState3DS_CanDispatchTouch). Presentation is synchronous here. */
    Port_SecondScreen_3DS_PromoteSubmitted();
    sBottomReady = true;
}

void Port_PPU_PresentFrame(void) {
    if (!sInitialized)
        return;
    const uint16_t dispcnt = (uint16_t)(gIoMem[0] | (gIoMem[1] << 8));
    const uint8_t mode = (uint8_t)(dispcnt & 7);
    virtuappu_registers.mode = (mode == 1 || mode == 2) ? 2 : 1;
    virtuappu_registers.frame_width = GBA_W;
    virtuappu_registers.frame_height = GBA_H;
    virtuappu_registers.frame_pitch = GBA_W;
    virtuappu_mode1_pre_line_callback = port_hdma_has_active_channels() ? port_hdma_step_line : NULL;
    virtuappu_mode1_bg2x_hdma_strobe = port_hdma_dest_overlaps(gIoMem + 0x28, gIoMem + 0x2c) != 0;
    virtuappu_mode1_bg2y_hdma_strobe = port_hdma_dest_overlaps(gIoMem + 0x2c, gIoMem + 0x30) != 0;
    if (!sBottomOnly) {
        virtuappu_render_frame();
        port_hdma_vblank_reset();
        RgdsDisplay_PresentTop(sTopFrame, GBA_W, GBA_H, GBA_W);
    }
    PresentBottom();
}

void Port_PPU_SetPresentIsFirstOfTick(bool first) { (void)first; }
void Port_PPU_CyclePresentationMode(int d) { (void)d; }
void Port_PPU_CycleWindowScale(int d) { (void)d; }
void Port_PPU_ToggleFullscreen(void) {}
bool Port_PPU_IsFullscreen(void) { return false; }
unsigned char Port_PPU_WindowScale(void) { return 1; }
const char* Port_PPU_PresentationModeName(void) { return "RGDS"; }
