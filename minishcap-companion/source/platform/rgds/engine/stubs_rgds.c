/* 3DS-only services the shared second-screen/settings code references, and audio placeholders.
 * The RG DS has no updater, turbo or checkpoint UI yet, and audio is not wired (silent). */
#include <stdbool.h>
#include <stdint.h>

#include "port_dump_state.h"
#include "port_dump_state_3ds.h"
#include "updater.h"

unsigned Platform3DS_Core1TimeLimit(void) { return 0; }
bool Platform3DS_IsNew3DS(void) { return true; }
void Platform3DS_SetTurboMultiplier(unsigned multiplier) { (void)multiplier; }

float Port_Audio_GetMasterVolume(void) { return 1.0f; }
void Port_Audio_SetMasterVolume(float volume) { (void)volume; }
void Port_Audio_3DSSetPaused(bool paused) { (void)paused; }
void Port_Audio_Reset(void) {}
void Port_M4A_Backend_SoundInit(uint32_t mode) { (void)mode; }
void Port_M4A_Backend_SetSoundMode(uint32_t mode) { (void)mode; }
void Port_M4A_Backend_SetVSyncEnabled(bool e) { (void)e; }
bool Port_M4A_Backend_StartSongById(uint8_t p, uint16_t id) { (void)p; (void)id; return false; }
void Port_M4A_Backend_StopPlayer(uint8_t p) { (void)p; }
void Port_M4A_Backend_ContinuePlayer(uint8_t p) { (void)p; }
bool Port_M4A_Backend_IsPlayerActive(uint8_t p) { (void)p; return false; }
void Port_M4A_Backend_SetTrackVolume(uint8_t p, uint16_t t, uint16_t v) { (void)p; (void)t; (void)v; }
void Port_M4A_Backend_SetTrackPan(uint8_t p, uint16_t t, int8_t pan) { (void)p; (void)t; (void)pan; }

__attribute__((weak)) PortDumpStateResult Port_DumpState3DS_LoadLatest(void) { return PORT_DUMP_STATE_NO_DUMP; }
__attribute__((weak)) const char* Port_DumpState_ResultLabel(PortDumpStateResult result) {
    (void)result;
    return "unavailable";
}

void Port_PPU_SetColorCorrection(bool enabled) { (void)enabled; }
double Port_PPU_3DS_CurrentFps(void) { return 0.0; }
double Port_PPU_3DS_AverageFps(void) { return 0.0; }
__attribute__((weak)) void Port_PPU_3DS_WriteQuickDump(void) {}

bool Updater_Busy(void) { return false; }
void Updater_Cancel(void) {}
void Updater_Check(void) {}
void Updater_Download(void) {}
void Updater_SetChannel(bool prerelease) { (void)prerelease; }
unsigned Updater_GetNotes(char* out, unsigned capacity) {
    if (out && capacity)
        out[0] = 0;
    return 0;
}
unsigned Update_FormatNotes(const char* markdown, char lines[][43], unsigned capacity) {
    (void)markdown; (void)lines; (void)capacity;
    return 0;
}
void Updater_GetStatus(UpdateStatus* out) {
    if (out) {
        out->state = UPDATE_IDLE;
        out->prerelease = false;
        out->progress = 0;
        out->revision = 0;
        out->version[0] = 0;
        out->message[0] = 0;
    }
}
