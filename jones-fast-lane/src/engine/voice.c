/* CD speech: audio001.map indexes raw 8-bit unsigned 11025 Hz clips in the
 * audio volume (audio001.002). A clip is read whole into RAM when played and
 * mixed into the music stream by voice_mix(). */
#include <stdlib.h>
#include <string.h>
#include "voice.h"
#include "platform.h"

typedef struct { uint16_t num; uint32_t off, size; } VoiceEnt;

static VoiceEnt *ents;
static int nents;

static uint8_t *clip;            /* current clip (raw u8 PCM) */
static uint32_t clip_len;
static uint32_t pos_fx;          /* position in clip, 16.16 */
static int state;                /* 0 idle, 1 loaded (not started), 2 playing, 3 paused */
static int volume = 127;
static int cur_num = -1;

int voice_init(const uint8_t *map, uint32_t size)
{
    free(ents);
    ents = NULL;
    nents = 0;
    if (!map || size < 10) return 0;
    ents = malloc(size / 10 * sizeof *ents);
    if (!ents) return 0;
    for (uint32_t i = 0; i + 10 <= size; i += 10) {
        const uint8_t *p = map + i;
        uint16_t n = (uint16_t)(p[0] | p[1] << 8);
        uint32_t o = p[2] | p[3] << 8 | p[4] << 16 | (uint32_t)p[5] << 24;
        uint32_t s = p[6] | p[7] << 8 | p[8] << 16 | (uint32_t)p[9] << 24;
        if (n == 0xffff) break;
        ents[nents].num = n;
        ents[nents].off = o & 0x0fffffff;
        ents[nents].size = s;
        nents++;
    }
    return nents;
}

int voice_available(void) { return nents > 0; }

static const VoiceEnt *find(int num)
{
    for (int i = 0; i < nents; i++) if (ents[i].num == num) return &ents[i];
    return NULL;
}

static uint32_t ticks_of(uint32_t samples) { return (uint32_t)((uint64_t)samples * 60 / VOICE_RATE); }

/* Read clip `num` into a new buffer (no lock needed). Returns 1 with *buf
 * set when it must be installed, 0 when it is already current, -1 if absent. */
int voice_fetch(int num, uint8_t **buf, uint32_t *len)
{
    *buf = NULL;
    const VoiceEnt *e = find(num);
    if (!e) return -1;
    if (cur_num == num && clip) return 0;
    extern void *res_alloc(uint32_t n);
    uint8_t *b = res_alloc(e->size);
    if (!b) return -1;
    if (!plat_voice_read(e->off, b, e->size)) { free(b); return -1; }
    *buf = b;
    *len = e->size;
    return 1;
}

/* Make `buf` (from voice_fetch, may be NULL = keep current) the clip and
 * rewind; start it when `start`. Returns the length in ticks; *old is the
 * replaced buffer, to free after unlocking. */
int voice_install(int num, uint8_t *buf, uint32_t len, int start, uint8_t **old)
{
    *old = NULL;
    if (buf) { *old = clip; clip = buf; clip_len = len; cur_num = num; }
    if (!clip) return 0;
    pos_fx = 0;
    state = start ? 2 : 1;
    return (int)ticks_of(clip_len);
}

void voice_start(void) { if (clip && state == 1) state = 2; }
void voice_stop(void) { if (state) state = 0; pos_fx = 0; }
void voice_pause(int on) { if (on && state == 2) state = 3; else if (!on && state == 3) state = 2; }
void voice_set_volume(int v) { volume = v < 0 ? 0 : v > 127 ? 127 : v; }

int voice_position(void)
{
    if (state == 1) return 0;
    if (state != 2 && state != 3) return -1;
    return (int)ticks_of(pos_fx >> 16);
}

void voice_mix(int16_t *out, int n, int rate)
{
    if (state != 2 || !clip) return;
    uint32_t step = (uint32_t)(((uint64_t)VOICE_RATE << 16) / (uint32_t)rate);
    int vol = volume;       /* 0..127, applied as /128: u8 PCM << 8 is full scale */
    for (int i = 0; i < n; i++) {
        uint32_t ip = pos_fx >> 16;
        if (ip >= clip_len) { state = 0; break; }
        int a = clip[ip] - 128;
        int b = ip + 1 < clip_len ? clip[ip + 1] - 128 : a;
        int f = (pos_fx >> 8) & 0xff;
        int s = ((a << 8) + (b - a) * f) * vol >> 7;    /* ~ s16 */
        int m = out[i] + s;
        out[i] = (int16_t)(m > 32767 ? 32767 : m < -32768 ? -32768 : m);
        pos_fx += step;
    }
}
