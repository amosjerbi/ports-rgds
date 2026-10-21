/* 3DS-only services the shared second-screen/settings code references. The DS has no
 * updater, turbo, dump/checkpoint or master-volume UI yet, so these are inert. */
#include <stdbool.h>
#include <stdint.h>

#include "port_dump_state.h"
#include "port_dump_state_3ds.h"
#include "updater.h"

unsigned Platform3DS_Core1TimeLimit(void) { return 0; }
bool Platform3DS_IsNew3DS(void) { return false; }
void Platform3DS_SetTurboMultiplier(unsigned multiplier) { (void)multiplier; }
uint64_t Platform3DS_SystemTick(void) {
    extern uint64_t Platform3DS_Milliseconds(void);
    return Platform3DS_Milliseconds();
}

float Port_Audio_GetMasterVolume(void) { return 1.0f; }
void Port_Audio_SetMasterVolume(float volume) { (void)volume; }

PortDumpStateResult Port_DumpState3DS_LoadLatest(void) { return PORT_DUMP_STATE_NO_DUMP; }
const char* Port_DumpState_ResultLabel(PortDumpStateResult result) {
    (void)result;
    return "unavailable";
}

void Port_PPU_SetColorCorrection(bool enabled) { (void)enabled; }
double Port_PPU_3DS_CurrentFps(void) { return 0.0; }
double Port_PPU_3DS_AverageFps(void) { return 0.0; }
void Port_PPU_3DS_WriteQuickDump(void) {}

bool Updater_Busy(void) { return false; }
void Updater_Cancel(void) {}
void Updater_Check(void) {}
void Updater_Download(void) {}
void Updater_SetChannel(bool prerelease) { (void)prerelease; }
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
