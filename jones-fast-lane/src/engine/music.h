/* SCI1 AdLib-track music synthesizer (portable, software mixed). */
#ifndef SCI_MUSIC_H
#define SCI_MUSIC_H
#include <stdint.h>

int  music_init(int sample_rate, const uint8_t *patch003, uint32_t size);  /* 1 if instruments loaded */
int  music_play(int id, const uint8_t *res, uint32_t size, int loop, int vol);  /* copies res */
void music_stop(int id);
void music_set_volume(int id, int vol);
void music_set_master(int vol);                  /* 0..15 */
void music_render(int16_t *out, int n);          /* mono s16 */

#endif
