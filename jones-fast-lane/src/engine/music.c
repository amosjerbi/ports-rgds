/* SCI1 music synthesizer: plays the AdLib track of SCI1 sound resources.
 *
 * Each of the 48 FM instruments in patch.003 is rendered once into a
 * single-cycle wavetable (2-operator FM, modulator envelope held at its
 * level), then voices play those tables with an OPL-style ADSR envelope in
 * the dB domain. MIDI events are sequenced in 60 Hz ticks like the original
 * driver. Output: mono signed 16-bit PCM at the rate given to music_init.
 *
 * Thread model: music_play/music_stop may be called from another thread
 * than music_render; the host serializes them with music_lock/unlock. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "music.h"

#define NINSTR   48
#define WLEN     256          /* one period of (note freq / 2) */
#define NVOICES  16
#define NSONGS   4
#define NCHAN    16
#define MAXTRK   16
#define ENV_BLOCK 32          /* samples per envelope update */

typedef struct {
    int16_t wave[WLEN];
    uint8_t ar, dr, sl, rr, sustain;
} Instr;

typedef struct {
    const uint8_t *p, *end, *start;
    uint32_t wait;            /* ticks until next event */
    uint8_t status, done;
} Track;

typedef struct {
    int      id;              /* resource number, 0 = free */
    uint8_t *data;
    uint32_t size;
    int      loop, vol;       /* loop: -1 forever; vol 0..127 */
    int      ntrk;
    Track    trk[MAXTRK];
    uint8_t  prog[NCHAN], cvol[NCHAN], pan[NCHAN];
    int16_t  bend[NCHAN];
} Song;

typedef struct {
    int      song, chan, note;    /* song index, -1 if free */
    const Instr *ins;
    uint32_t phase, inc;          /* 16.16 table index */
    int      stage;               /* 0 attack 1 decay 2 sustain 3 release 4 off */
    int32_t  att;                 /* attenuation, 1/256 dB (0 = loud) */
    int32_t  rate;                /* per-block change in 1/256 dB */
    int32_t  amp;                 /* 0..4096, from att * velocity * volumes */
    int      vel;
    uint32_t age;
} Voice;

static Instr   instr[NINSTR];
static int     have_instr;
static Song    songs[NSONGS];
static Voice   voices[NVOICES];
static int     rate = 22050;
static uint32_t samples_per_tick_fx;   /* 16.16 */
static uint32_t tick_accum;
static uint32_t age_counter;
static int     master = 15;
static int     env_count;
static uint16_t db_to_amp[97 * 4];     /* 0.25 dB steps -> 0..4096 */

static const float mult_tab[16] = { 0.5f, 1, 2, 3, 4, 5, 6, 7, 8, 8, 10, 10, 12, 12, 15, 15 };

/* ---------------------------------------------------------- instruments */

static float opl_wave(int wf, float ph)
{
    float s = sinf(ph);
    switch (wf) {
    case 1: return s > 0 ? s : 0;                 /* half sine */
    case 2: return fabsf(s);                      /* abs sine */
    case 3: {                                     /* quarter sine pulses */
        float x = fmodf(ph, (float)M_PI);
        if (x < 0) x += (float)M_PI;
        return x < (float)M_PI / 2 ? fabsf(s) : 0;
    }
    }
    return s;
}

static float tl_amp(int tl) { return powf(10.0f, -0.75f * tl / 20.0f); }

static void render_instr(Instr *in, const uint8_t *d)
{
    const uint8_t *m = d, *c = d + 13;
    float mm = mult_tab[m[1] & 15], mc = mult_tab[c[1] & 15];
    float am = tl_amp(m[8] & 63), ac = tl_amp(c[8] & 63);
    int fb = d[2] & 7, additive = !d[12] ? 1 : 0;
    int wm = d[26] & 3, wc = d[27] & 3;
    float fbk = fb ? (float)M_PI * (float)(1 << fb) / 64.0f : 0;
    float buf[WLEN], prev1 = 0, prev2 = 0, peak = 0;
    for (int pass = 0; pass < 2; pass++) {        /* first pass settles feedback */
        for (int i = 0; i < WLEN; i++) {
            float t = (float)i / WLEN * 2.0f * (float)M_PI;   /* one period of f/2 */
            float mo = opl_wave(wm, 2 * mm * t + fbk * (prev1 + prev2)) * am;
            prev2 = prev1; prev1 = mo;
            float co = opl_wave(wc, 2 * mc * t + 4.0f * (float)M_PI * mo) * ac;
            buf[i] = additive ? (mo + co) * 0.5f : co;
        }
    }
    /* remove DC (half/abs sine waves) */
    float dc = 0;
    for (int i = 0; i < WLEN; i++) dc += buf[i];
    dc /= WLEN;
    for (int i = 0; i < WLEN; i++) { buf[i] -= dc; if (fabsf(buf[i]) > peak) peak = fabsf(buf[i]); }
    (void)peak;
    for (int i = 0; i < WLEN; i++) {
        float v = buf[i] * 12000.0f;
        in->wave[i] = (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    in->ar = c[3] & 15;
    in->sl = c[4] & 15;
    in->sustain = c[5] != 0;
    in->dr = c[6] & 15;
    in->rr = c[7] & 15;
}

int music_init(int sample_rate, const uint8_t *patch, uint32_t size)
{
    rate = sample_rate;
    samples_per_tick_fx = (uint32_t)(((uint64_t)rate << 16) / 60);
    for (int i = 0; i < (int)(sizeof db_to_amp / sizeof *db_to_amp); i++)
        db_to_amp[i] = (uint16_t)(4096.0f * powf(10.0f, -(i / 4.0f) / 20.0f));
    memset(songs, 0, sizeof songs);
    for (int i = 0; i < NVOICES; i++) voices[i].song = -1;
    have_instr = 0;
    if (patch && size >= NINSTR * 28) {
        for (int i = 0; i < NINSTR; i++) render_instr(&instr[i], patch + i * 28);
        have_instr = 1;
    }
    return have_instr;
}

void music_set_master(int vol) { master = vol < 0 ? 0 : vol > 15 ? 15 : vol; }

/* ---------------------------------------------------------- envelopes */

/* OPL timings (ms): attack 0 -> full, decay/release over 96 dB */
static int32_t rate_per_block(int r, int attack)
{
    static const float att_ms[16] = { 0, 2826, 1413, 706, 353, 176, 88, 44, 22, 11, 5.5f, 2.8f, 1.4f, 0.7f, 0.35f, 0 };
    static const float dec_ms[16] = { 0, 39280, 19640, 9820, 4910, 2455, 1227, 614, 307, 153, 77, 38, 19, 9.6f, 4.8f, 2.4f };
    float ms = attack ? att_ms[r] : dec_ms[r];
    if (r == 0) return 0;                      /* rate 0: never changes */
    if (ms <= 0) return 96 * 256;              /* instant */
    float blocks = ms * rate / 1000.0f / ENV_BLOCK;
    int32_t v = (int32_t)(96 * 256 / (blocks < 1 ? 1 : blocks));
    return v < 1 ? 1 : v;
}

static void voice_amp(Voice *v)
{
    Song *s = &songs[v->song];
    int a = v->att / 64;                       /* 1/256 dB -> 0.25 dB */
    if (a >= (int)(sizeof db_to_amp / sizeof *db_to_amp)) { v->amp = 0; return; }
    int32_t amp = db_to_amp[a];
    amp = amp * v->vel / 127;
    amp = amp * s->cvol[v->chan] / 127;
    amp = amp * s->vol / 127;
    amp = amp * master / 15;
    v->amp = amp;
}

static void voice_env(Voice *v)
{
    const Instr *in = v->ins;
    switch (v->stage) {
    case 0:
        if (v->rate >= 96 * 256 || (v->att -= v->rate) <= 0) {
            v->att = 0;
            v->stage = 1;
            v->rate = rate_per_block(in->dr, 0);
        }
        break;
    case 1: {
        int32_t sl = (in->sl == 15 ? 93 : in->sl * 3) * 256;
        v->att += v->rate;
        if (v->att >= sl) {
            v->att = sl;
            if (in->sustain) v->stage = 2;
            else { v->stage = 3; v->rate = rate_per_block(in->rr, 0); }
        }
        break;
    }
    case 2: break;
    case 3:
        v->att += v->rate;
        break;
    }
    if (v->att >= 96 * 256 || (v->stage == 3 && v->rate == 0 && v->att > 90 * 256)) {
        v->stage = 4;
        v->song = -1;
        return;
    }
    voice_amp(v);
}

/* ---------------------------------------------------------- voices */

static uint32_t note_inc(int note, int bend)
{
    float semis = note - 69 + bend / 4096.0f;             /* bend +-8192 = +-2 semitones */
    float f = 440.0f * powf(2.0f, semis / 12.0f);
    return (uint32_t)(f * 0.5f * WLEN / rate * 65536.0f);  /* table = one period of f/2 */
}

static void note_off(int si, int ch, int note)
{
    for (int i = 0; i < NVOICES; i++) {
        Voice *v = &voices[i];
        if (v->song == si && v->chan == ch && v->note == note && v->stage < 3) {
            v->stage = 3;
            v->rate = rate_per_block(v->ins->rr, 0);
        }
    }
}

static void note_on(int si, int ch, int note, int vel)
{
    if (!have_instr) return;
    Song *s = &songs[si];
    Voice *best = NULL;
    for (int i = 0; i < NVOICES; i++) {
        Voice *v = &voices[i];
        if (v->song == si && v->chan == ch && v->note == note) { best = v; break; }   /* retrigger */
        if (v->song < 0) { if (!best || best->song >= 0) best = v; }
    }
    if (!best) {   /* steal: prefer releasing voices, then the oldest */
        for (int i = 0; i < NVOICES; i++) {
            Voice *v = &voices[i];
            if (!best || (v->stage == 3) > (best->stage == 3) ||
                ((v->stage == 3) == (best->stage == 3) && v->age < best->age))
                best = v;
        }
    }
    Voice *v = best;
    v->song = si;
    v->chan = ch;
    v->note = note;
    v->ins = &instr[s->prog[ch] % NINSTR];
    v->phase = 0;
    v->inc = note_inc(note, s->bend[ch]);
    v->vel = vel;
    v->age = ++age_counter;
    v->att = 96 * 256;
    v->stage = 0;
    v->rate = rate_per_block(v->ins->ar, 1);
    if (v->ins->ar == 0) { v->att = 96 * 256; }       /* never attacks */
    voice_env(v);
}

static void song_free_voices(int si)
{
    for (int i = 0; i < NVOICES; i++) if (voices[i].song == si) voices[i].song = -1;
}

/* ---------------------------------------------------------- sequencer */

static uint32_t read_delta(Track *t)
{
    uint32_t d = 0;
    while (t->p < t->end && *t->p == 0xf8) { d += 240; t->p++; }
    if (t->p < t->end) d += *t->p++;
    return d;
}

static void track_rewind(Track *t)
{
    t->p = t->start;
    t->status = 0;
    t->done = 0;
    t->wait = read_delta(t);
}

static void song_reset_channels(Song *s)
{
    for (int c = 0; c < NCHAN; c++) { s->cvol[c] = 127; s->pan[c] = 64; s->bend[c] = 0; s->prog[c] = 0; }
}

static void track_event(int si, Track *t)
{
    Song *s = &songs[si];
    if (t->p >= t->end) { t->done = 1; return; }
    uint8_t b = *t->p;
    if (b & 0x80) { t->status = b; t->p++; }
    uint8_t st = t->status, ch = st & 15;
    if (st == 0xfc || !st) { t->done = 1; return; }
    if (st == 0xf0) {                                     /* sysex */
        while (t->p < t->end && *t->p != 0xf7) t->p++;
        if (t->p < t->end) t->p++;
        return;
    }
    uint8_t d1 = t->p < t->end ? t->p[0] : 0;
    switch (st & 0xf0) {
    case 0xc0: s->prog[ch] = d1; t->p += 1; return;
    case 0xd0: t->p += 1; return;
    }
    uint8_t d2 = t->p + 1 < t->end ? t->p[1] : 0;
    t->p += 2;
    switch (st & 0xf0) {
    case 0x80: note_off(si, ch, d1); break;
    case 0x90: if (d2) note_on(si, ch, d1, d2); else note_off(si, ch, d1); break;
    case 0xb0:
        if (d1 == 7) s->cvol[ch] = d2;
        else if (d1 == 10) s->pan[ch] = d2;
        else if (d1 == 123 || d1 == 120) for (int i = 0; i < NVOICES; i++)
            if (voices[i].song == si && voices[i].chan == ch) voices[i].song = -1;
        break;
    case 0xe0: {
        s->bend[ch] = (int16_t)(((d2 << 7) | d1) - 0x2000);
        for (int i = 0; i < NVOICES; i++) {
            Voice *v = &voices[i];
            if (v->song == si && v->chan == ch) v->inc = note_inc(v->note, s->bend[ch]);
        }
        break;
    }
    }
}

static void song_tick(int si)
{
    Song *s = &songs[si];
    int alive = 0;
    for (int k = 0; k < s->ntrk; k++) {
        Track *t = &s->trk[k];
        if (t->done) continue;
        while (!t->done && t->wait == 0) {
            track_event(si, t);
            if (!t->done) t->wait = read_delta(t);
        }
        if (!t->done) { t->wait--; alive = 1; }
    }
    if (!alive) {
        if (s->loop == -1 || s->loop > 1) {
            if (s->loop > 1) s->loop--;
            for (int k = 0; k < s->ntrk; k++) track_rewind(&s->trk[k]);
        } else {
            for (int i = 0; i < NVOICES; i++)
                if (voices[i].song == si && voices[i].stage < 3) note_off(si, voices[i].chan, voices[i].note);
            free(s->data);
            s->data = NULL;
            s->id = 0;
        }
    }
}

/* ---------------------------------------------------------- public */

int music_play(int id, const uint8_t *res, uint32_t size, int loop, int vol)
{
    music_stop(id);
    int si = -1;
    for (int i = 0; i < NSONGS; i++) if (!songs[i].id) { si = i; break; }
    if (si < 0) si = 0, music_stop(songs[0].id);
    Song *s = &songs[si];
    memset(s, 0, sizeof *s);
    s->data = malloc(size);
    if (!s->data) return 0;
    memcpy(s->data, res, size);
    s->size = size;
    s->loop = loop;
    s->vol = vol < 0 ? 127 : vol > 127 ? 127 : vol;
    song_reset_channels(s);
    /* find the AdLib (device 0) track list */
    const uint8_t *d = s->data;
    uint32_t p = 0;
    while (p < size && d[p] != 0xff) {
        int dev = d[p++];
        while (p + 6 <= size && d[p] != 0xff) {
            uint32_t off = (uint32_t)(d[p + 2] | d[p + 3] << 8), len = (uint32_t)(d[p + 4] | d[p + 5] << 8);
            p += 6;
            if (dev != 0 || off + len > size || len < 2 || s->ntrk >= MAXTRK) continue;
            Track *t = &s->trk[s->ntrk++];
            t->start = d + off + 2;           /* skip channel/voices header */
            t->end = d + off + len;
            track_rewind(t);
        }
        p++;
    }
    if (!s->ntrk) { free(s->data); s->data = NULL; return 0; }
    s->id = id;
    return 1;
}

void music_stop(int id)
{
    if (!id) return;
    for (int i = 0; i < NSONGS; i++) {
        if (songs[i].id != id) continue;
        song_free_voices(i);
        free(songs[i].data);
        songs[i].data = NULL;
        songs[i].id = 0;
    }
}

void music_set_volume(int id, int vol)
{
    for (int i = 0; i < NSONGS; i++)
        if (songs[i].id == id) songs[i].vol = vol < 0 ? 0 : vol > 127 ? 127 : vol;
}

void music_render(int16_t *out, int n)
{
    while (n > 0) {
        /* advance the sequencer at 60 Hz */
        while (tick_accum >= samples_per_tick_fx) {
            tick_accum -= samples_per_tick_fx;
            for (int i = 0; i < NSONGS; i++) if (songs[i].id) song_tick(i);
        }
        int chunk = ENV_BLOCK - env_count;
        if (chunk > n) chunk = n;
        uint32_t ticks_left = (samples_per_tick_fx - tick_accum + 0xffff) >> 16;
        if ((uint32_t)chunk > ticks_left) chunk = (int)ticks_left;
        if (chunk < 1) chunk = 1;
        int32_t mix[ENV_BLOCK];
        memset(mix, 0, sizeof(int32_t) * chunk);
        for (int i = 0; i < NVOICES; i++) {
            Voice *v = &voices[i];
            if (v->song < 0 || !v->amp) continue;
            const int16_t *w = v->ins->wave;
            uint32_t ph = v->phase, inc = v->inc;
            int32_t amp = v->amp;
            for (int k = 0; k < chunk; k++) {
                mix[k] += (w[(ph >> 16) & (WLEN - 1)] * amp) >> 12;
                ph += inc;
            }
            v->phase = ph;
        }
        for (int k = 0; k < chunk; k++) {
            int32_t x = mix[k] >> 1;
            out[k] = (int16_t)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
        }
        out += chunk;
        n -= chunk;
        tick_accum += (uint32_t)chunk << 16;
        env_count += chunk;
        if (env_count >= ENV_BLOCK) {
            env_count = 0;
            for (int i = 0; i < NVOICES; i++) if (voices[i].song >= 0) voice_env(&voices[i]);
        }
    }
}
