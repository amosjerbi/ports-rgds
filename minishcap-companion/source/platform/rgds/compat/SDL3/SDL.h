#pragma once
/* The shared port headers were written against SDL3; the RG DS image ships SDL2. Only the
 * types used in their declarations are needed, and those exist in SDL2 under the same names. */
#include <SDL2/SDL.h>

/* SDL3 names the shared port code uses that SDL2 spells differently. */
#define SDL_EVENT_QUIT SDL_QUIT
#define SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE 8
