/* Placeholders for subsystems not yet implemented on the NDS: audio (m4a on ARM7), 3DS second screen. */
#include <stdbool.h>
#include <stdint.h>

/* --- Audio (TODO: ARM7 m4a) --- */
void Port_Audio_3DSSetPaused(bool paused) { (void)paused; }
void Port_Audio_Reset(void) {}
void Port_M4A_Backend_SoundInit(uint32_t mode) { (void)mode; }
void Port_M4A_Backend_SetVSyncEnabled(bool e) { (void)e; }
bool Port_M4A_Backend_StartSongById(uint8_t p, uint16_t id) { (void)p; (void)id; return false; }
void Port_M4A_Backend_StopPlayer(uint8_t p) { (void)p; }
void Port_M4A_Backend_ContinuePlayer(uint8_t p) { (void)p; }
bool Port_M4A_Backend_IsPlayerActive(uint8_t p) { (void)p; return false; }
void Port_M4A_Backend_SetTrackVolume(uint8_t p, uint16_t t, uint16_t v) { (void)p; (void)t; (void)v; }

/* --- Atomics: ARMv5 has no LDREX/STREX and the ARM9 build is single-threaded --- */
unsigned int __atomic_fetch_add_4(volatile void* p, unsigned int v, int order) {
    (void)order;
    unsigned int old = *(volatile unsigned int*)p;
    *(volatile unsigned int*)p = old + v;
    return old;
}
