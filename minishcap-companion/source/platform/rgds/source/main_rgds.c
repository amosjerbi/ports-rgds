/* The Minish Cap RG DS: native dual-screen build for the Anystream RG DS (ROCKNIX, aarch64).
 * Top panel = gameplay, bottom panel = the project's second screen (map, quest, touch items). */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "display_rgds.h"

/* Engine entry points (declared here: engine headers pull in a lot). */
void Port_Config_Load(const char* path);
void Port_LoadRom(const char* path);
int Port_SecondScreenTheme_Ready(void);
void Port_PPU_Init(void* window);
void Port_PPU_Shutdown(void);
void AgbMain(void);

#define DEFAULT_ROM_DIR "/storage/roms/gba"

static int IsMinishCap(const char* path) {
    FILE* f = fopen(path, "rb");
    unsigned char hdr[0xB0];
    int ok = 0;
    if (f) {
        ok = fread(hdr, 1, sizeof hdr, f) == sizeof hdr && (!memcmp(hdr + 0xAC, "BZME", 4) || !memcmp(hdr + 0xAC, "BZMP", 4));
        fclose(f);
    }
    return ok;
}

static int FindRom(char* out, size_t cap) {
    DIR* d = opendir(DEFAULT_ROM_DIR);
    struct dirent* e;
    if (!d)
        return -1;
    while ((e = readdir(d))) {
        size_t n = strlen(e->d_name);
        char path[1024];
        if (n < 5 || strcasecmp(e->d_name + n - 4, ".gba"))
            continue;
        snprintf(path, sizeof path, DEFAULT_ROM_DIR "/%s", e->d_name);
        if (IsMinishCap(path)) {
            snprintf(out, cap, "%s", path);
            closedir(d);
            return 0;
        }
    }
    closedir(d);
    return -2;
}

int main(int argc, char** argv) {
    char rom[1024];
    if (argc > 1) {
        snprintf(rom, sizeof rom, "%s", argv[1]);
    } else if (FindRom(rom, sizeof rom)) {
        fprintf(stderr, "No Minish Cap .gba (BZME/BZMP) found in " DEFAULT_ROM_DIR " - pass the ROM path as an argument.\n");
        return 1;
    }
    if (!IsMinishCap(rom)) {
        fprintf(stderr, "%s is not a supported Minish Cap ROM.\n", rom);
        return 1;
    }
    /* Saves and settings are written relative to the working directory. */
    mkdir(Rgds_DataDir(), 0755);
    if (chdir(Rgds_DataDir()) != 0)
        fprintf(stderr, "warning: cannot enter %s; saves go to the current directory\n", Rgds_DataDir());

    if (!RgdsDisplay_Init())
        return 1;
    fprintf(stderr, "[rgds] loading %s\n", rom);
    Port_Config_Load("tmc.ini");
    Port_LoadRom(rom);
    Port_SecondScreenTheme_Ready();
    Port_PPU_Init(NULL);
    AgbMain(); /* returns only when Platform3DS_IsRunning() turns false */
    Port_PPU_Shutdown();
    RgdsDisplay_Shutdown();
    return 0;
}
