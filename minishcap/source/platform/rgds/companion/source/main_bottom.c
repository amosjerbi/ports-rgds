/* tmc-bottom: bottom-panel companion for a Minish Cap running in RetroArch's mGBA core.
 * Reads the game's RAM over RetroArch's network command interface and draws the project's second
 * screen (map, quest, items) on the bottom panel. It never touches the emulator's video or input. */
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "display_rgds.h"
#include "ra_client.h"

void Port_Config_Load(const char* path);
void Port_LoadRom(const char* path);
int Port_SecondScreenTheme_Ready(void);
void Port_PPU_Init(void* window);
void Port_PPU_RGDS_BottomOnly(int on);
void Port_PPU_PresentFrame(void);
bool RamMirror_Update(void);
void Port_SecondScreen_SetCompanionMode(int on);
void Port_SecondScreen_3DS_OnTap(int x, int y, int longPress);

#define DATA_DIR "/storage/.config/tmc-rgds"
#define MAX_SILENT_SECONDS 20 /* once connected: exit if RetroArch stays unreachable this long (it closed) */
#define STARTUP_GRACE_SECONDS 120 /* before the first connection: RetroArch is still booting */

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: tmc-bottom <rom.gba>\n");
        return 2;
    }
    if (chdir(DATA_DIR) != 0)
        fprintf(stderr, "[bottom] warning: cannot enter " DATA_DIR "\n");
    if (!RaClient_Open("127.0.0.1", 55355)) {
        fprintf(stderr, "[bottom] cannot open the RetroArch command socket\n");
        return 1;
    }
    if (!RgdsDisplay_InitBottomOnly())
        return 1;
    Port_Config_Load("tmc-bottom.ini");
    Port_LoadRom(argv[1]); /* the compositor reads ROM-resident art and tables */
    Port_SecondScreenTheme_Ready();
    Port_SecondScreen_SetCompanionMode(1);
    Port_PPU_RGDS_BottomOnly(1);
    Port_PPU_Init(NULL);

    /* RGDS_TEST_TAPS="ms,x,y;ms,x,y": inject taps through the real tap handler at given times (QA). */
    struct { unsigned ms; int x, y; bool done; } taps[16];
    int tapCount = 0;
    if (getenv("RGDS_TEST_TAPS")) {
        char* spec = strdup(getenv("RGDS_TEST_TAPS"));
        for (char* tok = strtok(spec, ";"); tok && tapCount < 16; tok = strtok(NULL, ";")) {
            unsigned ms;
            int x, y;
            if (sscanf(tok, "%u,%d,%d", &ms, &x, &y) == 3)
                taps[tapCount].ms = ms, taps[tapCount].x = x, taps[tapCount].y = y, taps[tapCount++].done = false;
        }
        free(spec);
    }
    const Uint32 startMs = SDL_GetTicks();
    unsigned silentFrames = 0, tick = 0;
    bool connected = false;
    while (RgdsDisplay_PumpEvents()) {
        if (RamMirror_Update()) {
            if (!connected)
                fprintf(stderr, "[bottom] connected to RetroArch\n");
            connected = true;
            silentFrames = 0;
        } else if (++silentFrames > 30u * (connected ? MAX_SILENT_SECONDS : STARTUP_GRACE_SECONDS)) {
            fprintf(stderr, "[bottom] RetroArch is gone, exiting\n");
            break;
        }
        for (int i = 0; i < tapCount; i++)
            if (!taps[i].done && SDL_GetTicks() - startMs >= taps[i].ms) {
                taps[i].done = true;
                fprintf(stderr, "[bottom] test tap (%d,%d)\n", taps[i].x, taps[i].y);
                Port_SecondScreen_3DS_OnTap(taps[i].x, taps[i].y, 0);
            }
        Port_PPU_PresentFrame();
        if ((++tick % 600) == 0 && connected)
            fprintf(stderr, "[bottom] running (%u ticks)\n", tick);
        SDL_Delay(33); /* ~30 Hz: the second screen does not need more, and reads are a few UDP round trips */
    }
    RaClient_Close();
    RgdsDisplay_Shutdown();
    return 0;
}
