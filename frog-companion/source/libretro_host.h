#pragma once
/* Minimal libretro frontend: hosts a Game Boy core (gambatte) inside this process so the game can render its own
 * pause menu from a save record that is copied in from the live game. No audio, no window: frames go to a buffer. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { LR_B = 0, LR_Y = 1, LR_SELECT = 2, LR_START = 3, LR_UP = 4, LR_DOWN = 5, LR_LEFT = 6, LR_RIGHT = 7, LR_A = 8 };

bool Lr_Open(const char* corePath, const char* romPath);
void Lr_Close(void);
void Lr_Run(int frames);                       /* run N frames with the current buttons */
void Lr_Press(int button, int hold, int after); /* hold for `hold` frames, then run `after` more */
const uint32_t* Lr_Frame(int* w, int* h);       /* last frame, ABGR8888 (R in the low byte) */
uint8_t* Lr_Wram(size_t* size);                 /* 0xC000.. work RAM */
uint8_t* Lr_Sram(size_t* size);                 /* cartridge save RAM */
/* Serialises the hosted core's state (QA: lets RetroArch's LOAD_STATE put the live game into the same situation). */
bool Lr_SaveState(const char* path);
unsigned long Lr_FramesRun(void);
unsigned long Lr_VideoCalls(void);
