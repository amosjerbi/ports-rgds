/* Bottom screen: the game's 320x200 8-bit frame filtered down to the DS's 256x192 (16-bit bitmap). */
#ifndef SCALE_H
#define SCALE_H
#include <stdint.h>

#define SCALE_SRC_W 320
#define SCALE_SRC_H 200
#define SCALE_DST_W 256
#define SCALE_DST_H 192

/* palette as RGB15 (0bbbbbgggggrrrrr); forces a full redraw at the next scale_frame */
void scale_set_palette(const uint16_t *rgb15);
/* filter src (320x200 indices) into dst (RGB15 bitmap, stride 256 pixels); only rows whose source changed
 * since the last call are redrawn (all of them after scale_set_palette or scale_invalidate) */
void scale_frame(const uint8_t *src, uint16_t *dst);
void scale_invalidate(void);

#endif
