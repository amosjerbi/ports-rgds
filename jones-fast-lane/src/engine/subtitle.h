/* Subtitles for the CD release's speech (built by tools/mksubs.py). */
#ifndef SCI_SUBTITLE_H
#define SCI_SUBTITLE_H
#include <stdint.h>

void subtitle_init(const uint8_t *data, uint32_t size);   /* data must stay valid */
int  subtitle_available(void);
void subtitle_show(int clip, int length_ticks);          /* called when a clip plays */
void subtitle_hide(void);
const char *subtitle_current(uint32_t now);              /* NULL when nothing to show */

#endif
