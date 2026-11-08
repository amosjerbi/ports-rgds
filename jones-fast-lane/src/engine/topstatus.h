/* Life-goals panel for the DS top screen. */
#ifndef SCI_TOPSTATUS_H
#define SCI_TOPSTATUS_H
#include <stdint.h>

#define TOPSTATUS_W 256
#define TOPSTATUS_H 192
#define TOPSTATUS_COLORS 13

extern const uint8_t topstatus_palette[TOPSTATUS_COLORS][3];

/* Call from the engine thread (e.g. on present). Returns 0 when no game is
 * in progress (show something else), 1 when the panel is unchanged, 2 when
 * `out` (256x192, palette indices 0..TOPSTATUS_COLORS-1) was redrawn.
 * While a location window is open the panel shows that location instead of
 * the life goals (locpanel.h). */
int topstatus_update(uint8_t *out, uint32_t now);

#endif
