/* Strong overrides for the second screen's developer actions: in the companion they drive RetroArch
 * save states (a dedicated slot, so the player's own states are never overwritten). Engine-side TU. */
#include <stdbool.h>
#include <stdint.h>

#include "port_dump_state.h"
#include "port_dump_state_3ds.h"
#include "ra_client.h"

#define TMC_STATE_SLOT 9 /* RetroArch slot used by SAVE STATE / LOAD STATE on the second screen */

static int sLastResult = PORT_DUMP_STATE_NO_DUMP;

/* SAVE STATE (the "mem dump" row) */
void Port_PPU_3DS_WriteQuickDump(void) {
    RaClient_Command("SAVE_STATE_SLOT " "9");
}

/* LOAD STATE */
PortDumpStateResult Port_DumpState3DS_LoadLatest(void) {
    sLastResult = RaClient_Command("LOAD_STATE_SLOT " "9") ? PORT_DUMP_STATE_OK : PORT_DUMP_STATE_IO_ERROR;
    return (PortDumpStateResult)sLastResult;
}

const char* Port_DumpState_ResultLabel(PortDumpStateResult result) {
    return result == PORT_DUMP_STATE_OK ? "DONE" : "FAILED";
}
