/* The Minish Cap NDS - scaffold entry point.
 * Boots both screens, finds a .gba on the SD card, checks it is a supported
 * Minish Cap ROM and exercises the page cache. The engine is not linked yet. */
#include <dirent.h>
#include <fat.h>
#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "nds_rom_pager.h"
#include "nds_slot2.h"

/* Engine entry points (declared here: engine headers redefine libnds' u32/s32). */
extern uint8_t* gRomData;
extern unsigned int gRomSize;
void Port_LoadRom(const char* path);
void AgbMain(void);
void Port_PPU_Init(void* window);
void Port_SecondScreenTheme_Ready(void);

#define ROM_DIR "/nds/The Minish Cap DS"

static int FindRom(char* out, size_t cap) {
    DIR* d = opendir(ROM_DIR);
    struct dirent* e;
    if (!d)
        return -1;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        if (n > 4 && !strcasecmp(e->d_name + n - 4, ".gba")) {
            snprintf(out, cap, ROM_DIR "/%s", e->d_name);
            closedir(d);
            return 0;
        }
    }
    closedir(d);
    return -2;
}

int main(void) {
    char path[300];
    NdsRomInfo info;

    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    consoleDemoInit();                       /* sub (touch) screen console */


    iprintf("The Minish Cap NDS (scaffold)\n\n");
    {
        const uint8_t* rom;
        uint32_t size;
        char code[5];
        int rc = Nds_AttachSlot2Rom(&rom, &size, code);
        if (!rc) {
            /* Preferred path: flat ROM at 0x08000000; engine sets gRomData = rom. */
            iprintf("Slot-2 ROM %s (%luMB)\nflat pointers OK\n", code, (unsigned long)(size >> 20));
            iprintf("hdr title: %.12s\n", (const char*)rom + 0xA0);
            gRomData = (uint8_t*)rom; /* flat Slot-2 window; see port_rom.c (TMC_NDS) */
            gRomSize = size;
            cpuStartTiming(0);
            Port_LoadRom(NULL);
            Port_SecondScreenTheme_Ready();
            Port_PPU_Init(NULL);
            AgbMain(); /* does not return */
            goto loop;
        }
        iprintf("No Slot-2 ROM (%d)\nTrying SD (probe only)...\n\n", rc);
    }
    if (!fatInitDefault()) {
        iprintf("FAT init failed (DLDI?)\n");
    } else if (FindRom(path, sizeof path)) {
        iprintf("Put a .gba in:\n" ROM_DIR "\n");
    } else if (NdsRom_Open(path, &info)) {
        iprintf("Cannot read ROM\n");
    } else {
        uint8_t probe[4] = { 0 };
        int ok = !strcmp(info.game_code, "BZME") || !strcmp(info.game_code, "BZMP");
        iprintf("ROM %s v%u %luKB\n%s\n", info.game_code, info.version, (unsigned long)(info.size >> 10),
                ok ? "Supported region" : "Unsupported ROM");
        NdsRom_Read(0x08, probe, 4);
        iprintf("page cache ok: %02X%02X%02X%02X\n", probe[0], probe[1], probe[2], probe[3]);
        iprintf("\nEngine not ported yet.\n");
    }
loop:
    for (;;) {
        swiWaitForVBlank();
        scanKeys();
        if (keysDown() & KEY_START)
            break;
    }
    NdsRom_Close();
    return 0;
}
