/* SCI1-early DoSound. Tracks song state and timing (from the MIDI stream)
 * so scripts that wait for music behave correctly; audio output goes
 * through plat_sound(). */
#include <string.h>
#include "kernel.h"
#include "resource.h"

#define ARG(i)  hp16((uint16_t)(argv + 2 * (i)))

static int S_number, S_signal, S_loop, S_nodePtr, S_handle, S_priority, S_vol, S_dataInc, S_min, S_sec, S_frame;

typedef struct {
    reg_t obj;
    int number, playing, loop;
    uint32_t start, length;   /* ticks */
    int paused;
    uint32_t pause_at;
    int vol, fade_to, fade_step, fade_ticks, fade_stop, fading;
    uint32_t fade_last;
} Song;

#define MAX_SONGS 16
static Song songs[MAX_SONGS];
static int master_volume = 15, muted;
static int inited;

static void init_sel(void)
{
    S_number = sel_id("number"); S_signal = sel_id("signal"); S_loop = sel_id("loop");
    S_nodePtr = sel_id("nodePtr"); S_handle = sel_id("handle"); S_priority = sel_id("priority");
    S_vol = sel_id("vol"); S_dataInc = sel_id("dataInc"); S_min = sel_id("min");
    S_sec = sel_id("sec"); S_frame = sel_id("frame");
    inited = 1;
}

/* Length of the longest channel of an SCI1 sound resource, in 60 Hz ticks. */
static uint32_t song_length(int num)
{
    uint16_t sz;
    const uint8_t *d = res_load(RES_SOUND, num, &sz);
    if (!d) return 0;
    uint32_t best = 0;
    uint32_t p = 0;
    /* SCI1: for each device: type byte, then (u8 ? , u8 ?, u16 off, u16 size) x n, 0xff */
    while (p < sz && d[p] != 0xff) {
        p++;   /* device type */
        while (p + 6 <= sz && d[p] != 0xff) {
            uint32_t off = (uint32_t)(d[p + 2] | d[p + 3] << 8);
            uint32_t len = (uint32_t)(d[p + 4] | d[p + 5] << 8);
            p += 6;
            if (off + len > sz || len < 2) continue;
            const uint8_t *c = d + off + 2;      /* skip channel/poly header */
            uint32_t n = len - 2, i = 0, t = 0;
            uint8_t status = 0;
            while (i < n) {
                uint32_t delta = 0;
                while (i < n && c[i] == 0xf8) { delta += 240; i++; }
                if (i >= n) break;
                delta += c[i++];
                t += delta;
                if (i >= n) break;
                uint8_t b = c[i];
                if (b & 0x80) { status = b; i++; }
                if (status == 0xfc) break;
                if (status == 0xf0) { while (i < n && c[i] != 0xf7) i++; i++; continue; }
                switch (status & 0xf0) {
                case 0xc0: case 0xd0: i += 1; break;
                default: i += 2; break;
                }
            }
            if (t > best) best = t;
        }
        p++;
    }
    res_unlock(RES_SOUND, num);
    return best;
}

static Song *song_of(reg_t obj)
{
    for (int i = 0; i < MAX_SONGS; i++) if (songs[i].obj == obj) return &songs[i];
    return NULL;
}

static Song *song_new(reg_t obj)
{
    Song *s = song_of(obj);
    if (s) return s;
    for (int i = 0; i < MAX_SONGS; i++)
        if (!songs[i].obj) { memset(&songs[i], 0, sizeof songs[i]); songs[i].obj = obj; return &songs[i]; }
    return NULL;
}

static void song_end(Song *s)
{
    s->playing = 0;
    s->fading = 0;
    set_sel(s->obj, S_signal, 0xffff);
    plat_sound(9, s->number, 0, 0);
}

static void update_cues(Song *s)
{
    if (!s || !s->playing || s->paused) return;
    if (s->fading && plat_ticks() - s->fade_last >= (uint32_t)s->fade_ticks) {
        s->fade_last = plat_ticks();
        if (s->vol < s->fade_to) { s->vol += s->fade_step; if (s->vol > s->fade_to) s->vol = s->fade_to; }
        else { s->vol -= s->fade_step; if (s->vol < s->fade_to) s->vol = s->fade_to; }
        set_sel(s->obj, S_vol, (reg_t)s->vol);
        plat_sound(14, s->number, s->vol, 0);
        if (s->vol == s->fade_to) {
            s->fading = 0;
            if (s->fade_stop) { song_end(s); return; }
        }
    }
    uint32_t el = plat_ticks() - s->start;
    if (s->length && el >= s->length) {
        if (s->loop == -1 || s->loop > 1) {
            if (s->loop > 1) s->loop--;
            s->start = plat_ticks();
        } else {
            song_end(s);
            return;
        }
    }
    if (S_min >= 0) {
        uint32_t t = plat_ticks() - s->start;
        set_sel(s->obj, S_min, (reg_t)(t / 3600));
        set_sel(s->obj, S_sec, (reg_t)(t / 60 % 60));
        set_sel(s->obj, S_frame, (reg_t)(t % 60));
    }
}

reg_t ksound_call(int argc, uint16_t argv)
{
    if (!inited) init_sel();
    int cmd = ARG(0);
    reg_t obj = argc > 1 ? ARG(1) : 0;
    if (vm_late_sci1()) {
        /* later SCI1 (CD release) numbering, mapped onto the early one:
         * 4 GetAudioCapability 5 Suspend 6 Init 7 Dispose 8 Play 9 Stop
         * 10 Pause 11 Fade 12 SetHold 13 Dummy 14 SetVolume 15 SetPriority
         * 16 SetLoop 17 UpdateCues 18 SendMidi 19 GlobalReverb 20 UpdateHandle */
        static const int8_t late[21] = { 0, 1, 2, 3, -4, -5, 5, 6, 7, 8, 9, 10, 14, -13, -14, -15, -16, 11, 12, 13, 4 };
        int m = cmd >= 0 && cmd <= 20 ? late[cmd] : -13;
        if (vm.trace & 4) vm_log("DoSound late %d -> %d\n", cmd, m);
        if (m == -4) return 0;                 /* GetAudioCapability: no digital sfx */
        if (m == -5 || m == -13 || m == -15) return 0;
        if (m == -14 || m == -16) {            /* SetVolume / SetLoop: obj, value */
            if (!obj_is_object(obj) || argc < 3) return 0;
            Song *t = song_of(obj);
            int v = (int16_t)ARG(2);
            if (m == -14) {
                set_sel(obj, S_vol, (reg_t)v);
                if (t) { t->vol = v; if (t->playing) plat_sound(14, t->number, v, 0); }
            } else {
                set_sel(obj, S_loop, (reg_t)v);
                if (t) t->loop = v;
            }
            return 0;
        }
        cmd = m;
    }
    if (vm.trace & 4) vm_log("DoSound %d argc %d obj %s num %d\n", cmd, argc, obj_is_object(obj) ? obj_name(obj) : "-", obj_is_object(obj) ? get_sel(obj, S_number) : -1);
    Song *s;
    /* SCI1-early command numbers */
    switch (cmd) {
    case 0:   /* MasterVolume */
        if (argc > 1) { master_volume = ARG(1) & 15; plat_sound(0, master_volume, 0, 0); }
        return (reg_t)master_volume;
    case 1:   /* Mute */
        if (argc > 1) { muted = ARG(1) != 0; plat_sound(1, muted, 0, 0); }
        return (reg_t)muted;
    case 2: return 0;           /* Restore */
    case 3: return 32;          /* GetPolyphony */
    case 4: return 0;           /* UpdateHandle */
    case 5:                     /* Init */
        if (!obj_is_object(obj)) return 0;
        s = song_new(obj);
        if (s) {
            if (s->playing) plat_sound(9, s->number, 0, 0);   /* re-init cuts the old song */
            s->number = get_sel(obj, S_number);
            s->length = song_length(s->number);
            s->playing = 0;
            s->fading = 0;
            s->paused = 0;
        }
        set_sel(obj, S_nodePtr, obj);
        return 0;
    case 6:                     /* Dispose */
        s = song_of(obj);
        if (s) { if (s->playing) plat_sound(9, s->number, 0, 0); s->obj = 0; }
        if (obj_is_object(obj)) set_sel(obj, S_nodePtr, 0);
        return 0;
    case 7:                     /* Play */
        if (!obj_is_object(obj)) return 0;
        s = song_new(obj);
        if (!s) return 0;
        if (s->playing) plat_sound(9, s->number, 0, 0);
        if (s->number != get_sel(obj, S_number) || !s->length) {
            s->number = get_sel(obj, S_number);
            s->length = song_length(s->number);
        }
        s->loop = (int16_t)get_sel(obj, S_loop);
        s->playing = 1;
        s->paused = 0;
        s->fading = 0;
        s->start = plat_ticks();
        s->vol = get_sel(obj, S_vol);
        if (s->vol <= 0 || s->vol > 127) s->vol = 127;
        set_sel(obj, S_signal, 0);
        plat_sound(8, s->number, s->loop, s->vol);
        return 0;
    case 8:                     /* Stop */
        s = song_of(obj);
        if (s && s->playing) song_end(s);
        else if (obj_is_object(obj)) set_sel(obj, S_signal, 0xffff);
        return 0;
    case 9: {                   /* Pause: obj (0 = all), flag */
        int flag = argc > 2 ? ARG(2) != 0 : 1;
        for (int i = 0; i < MAX_SONGS; i++) {
            Song *t = &songs[i];
            if (!t->obj || !t->playing || (obj && t->obj != obj) || t->paused == flag) continue;
            t->paused = flag;
            if (flag) { t->pause_at = plat_ticks(); plat_sound(10, t->number, 1, 0); }
            else { t->start += plat_ticks() - t->pause_at; plat_sound(10, t->number, 0, 0); }
        }
        return 0;
    }
    case 10:                    /* Fade: obj, to, ticks, step, stopAfter */
        s = song_of(obj);
        if (!s || !s->playing) { if (obj_is_object(obj)) set_sel(obj, S_signal, 0xffff); return 0; }
        if (argc > 2) {
            s->fade_to = ARG(2);
            s->fade_ticks = argc > 3 ? ARG(3) : 1;
            s->fade_step = argc > 4 && ARG(4) ? ARG(4) : 1;
            s->fade_stop = argc > 5 ? ARG(5) != 0 : 1;
        } else {                /* old style: fade out and stop */
            s->fade_to = 0; s->fade_ticks = 1; s->fade_step = 4; s->fade_stop = 1;
        }
        s->fading = 1;
        s->fade_last = plat_ticks();
        return 0;
    case 11:                    /* UpdateCues */
        update_cues(song_of(obj));
        return 0;
    case 12: case 13: return 0; /* SendMidi, GlobalReverb */
    case 14: return 0;          /* SetHold */
    }
    return 0;
}

/* ------------------------------------------------------------ save state */

uint32_t ksound_state_size(void) { return sizeof songs + 2 * sizeof(int); }

void ksound_state_save(uint8_t *out)
{
    Song tmp[MAX_SONGS];
    memcpy(tmp, songs, sizeof songs);
    for (int i = 0; i < MAX_SONGS; i++)   /* store elapsed time, not absolute */
        if (tmp[i].playing) tmp[i].start = plat_ticks() - tmp[i].start;
    memcpy(out, tmp, sizeof tmp); out += sizeof tmp;
    memcpy(out, &master_volume, sizeof(int)); out += sizeof(int);
    memcpy(out, &muted, sizeof(int));
}

void ksound_state_load(const uint8_t *in)
{
    if (!inited) init_sel();
    for (int i = 0; i < MAX_SONGS; i++)
        if (songs[i].obj && songs[i].playing) plat_sound(9, songs[i].number, 0, 0);
    memcpy(songs, in, sizeof songs); in += sizeof songs;
    memcpy(&master_volume, in, sizeof(int)); in += sizeof(int);
    memcpy(&muted, in, sizeof(int));
    uint32_t now = plat_ticks();
    for (int i = 0; i < MAX_SONGS; i++) {
        Song *s = &songs[i];
        if (!s->obj || !s->playing) continue;
        s->start = now - s->start;
        plat_sound(8, s->number, s->loop, get_sel(s->obj, S_vol));
    }
}

void ksound_after_restore(void) { if (!inited) init_sel(); }

/* ------------------------------------------------------------ CD speech */
#include "voice.h"
#include "subtitle.h"

static int S_syncTime = -2, S_syncCue;
static const uint8_t *sync_data;
static uint16_t sync_size, sync_ptr;
static int sync_num = -1;

/* DoAudio (SCI1 CD): 1 WPlay(num) 2 Play(num) 3 Stop 4 Pause 5 Resume
 * 6 Position 7 Rate 8 Volume 9 Language. Play/WPlay return the length in
 * ticks, Position the elapsed ticks or -1 once finished. */
reg_t kdoaudio_call(int argc, uint16_t argv)
{
    int cmd = ARG(0);
    int arg = argc > 1 ? ARG(argc - 1) : 0;    /* number is the last argument */
    int r;
    switch (cmd) {
    case 1: case 2:
        r = voice_available() ? plat_voice(cmd, arg) : 0;
        if (cmd == 2 && r) subtitle_show(arg, r);
        break;
    case 3: subtitle_hide(); /* fall through */
    case 4: case 5: case 6: r = plat_voice(cmd, 0); break;
    case 8: r = plat_voice(8, arg); break;
    case 7: case 9: default: r = 0; break;
    }
    if (vm.trace & 4) vm_log("DoAudio %d argc %d arg %d -> %d\n", cmd, argc, arg, r);
    return (reg_t)r;
}

static void sync_next(reg_t obj)
{
    if (!sync_data || sync_ptr + 2 > sync_size) return;
    int16_t t = (int16_t)(sync_data[sync_ptr] | sync_data[sync_ptr + 1] << 8), c = -1;
    sync_ptr += 2;
    if (t != -1 && sync_ptr + 2 <= sync_size) {
        c = (int16_t)(sync_data[sync_ptr] | sync_data[sync_ptr + 1] << 8);
        sync_ptr += 2;
    }
    set_sel(obj, S_syncTime, (reg_t)t);
    set_sel(obj, S_syncCue, (reg_t)c);
}

static void sync_stop(void)
{
    if (sync_data) res_unlock(RES_SYNC, sync_num);
    sync_data = NULL;
    sync_num = -1;
}

/* DoSync: 0 Start(obj, num) 1 Next(obj) 2 Stop */
reg_t kdosync_call(int argc, uint16_t argv)
{
    if (S_syncTime == -2) { S_syncTime = sel_id("syncTime"); S_syncCue = sel_id("syncCue"); }
    int cmd = ARG(0);
    reg_t obj = argc > 1 ? ARG(1) : 0;
    switch (cmd) {
    case 0:
        sync_stop();
        sync_num = argc > 2 ? ARG(2) : -1;
        sync_data = sync_num >= 0 ? res_load(RES_SYNC, sync_num, &sync_size) : NULL;
        sync_ptr = 0;
        if (obj_is_object(obj)) set_sel(obj, S_syncCue, sync_data ? 0 : 0xffff);
        break;
    case 1:
        if (obj_is_object(obj)) sync_next(obj);
        break;
    case 2:
        sync_stop();
        break;
    }
    if (vm.trace & 4) vm_log("DoSync %d argc %d\n", cmd, argc);
    return 0;
}
