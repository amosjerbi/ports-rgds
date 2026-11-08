/* CD speech clips (audio001.map / audio001.002). */
#ifndef SCI_VOICE_H
#define SCI_VOICE_H
#include <stdint.h>

#define VOICE_RATE 11025

int  voice_init(const uint8_t *map, uint32_t size);   /* entries parsed */
int  voice_available(void);

/* The functions below share state with voice_mix(); the platform calls them
 * with its audio lock held (see plat_voice). */
int  voice_fetch(int num, uint8_t **buf, uint32_t *len);   /* unlocked: 1 new, 0 current, -1 none */
int  voice_install(int num, uint8_t *buf, uint32_t len, int start, uint8_t **old);  /* ticks */
void voice_start(void);
void voice_stop(void);
void voice_pause(int on);
void voice_set_volume(int v);
int  voice_position(void);                            /* ticks, -1 when not playing */
void voice_mix(int16_t *out, int n, int rate);       /* adds into out */

/* Platform: read `size` bytes at `offset` of the audio volume. */
int  plat_voice_read(uint32_t offset, void *buf, uint32_t size);

/* Platform: thread-safe wrapper. cmd: 1 load(num) 2 play(num) 3 stop 4 pause
 * 5 resume 6 position 8 volume(arg). Returns as DoAudio. */
int  plat_voice(int cmd, int arg);

#endif
