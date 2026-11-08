/* SCI kernel: core (non-graphics) functions and dispatch. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include "kernel.h"
#include "resource.h"

#define ARG(i)   hp16((uint16_t)(argv + 2 * (i)))
#define SARG(i)  ((int16_t)ARG(i))

static const char *k_names[] = {
    "Load", "UnLoad", "ScriptID", "DisposeScript", "Clone", "DisposeClone", "IsObject",
    "RespondsTo", "DrawPic", "Show", "PicNotValid", "Animate", "SetNowSeen", "NumLoops",
    "NumCels", "CelWide", "CelHigh", "DrawCel", "AddToPic", "NewWindow", "GetPort",
    "SetPort", "DisposeWindow", "DrawControl", "HiliteControl", "EditControl", "TextSize",
    "Display", "GetEvent", "GlobalToLocal", "LocalToGlobal", "MapKeyToDir", "DrawMenuBar",
    "MenuSelect", "AddMenu", "DrawStatus", "Parse", "Said", "SetSynonyms", "HaveMouse",
    "SetCursor", "SaveGame", "RestoreGame", "RestartGame", "GameIsRestarting", "DoSound",
    "NewList", "DisposeList", "NewNode", "FirstNode", "LastNode", "EmptyList", "NextNode",
    "PrevNode", "NodeValue", "AddAfter", "AddToFront", "AddToEnd", "FindKey", "DeleteKey",
    "Random", "Abs", "Sqrt", "GetAngle", "GetDistance", "Wait", "GetTime", "StrEnd",
    "StrCat", "StrCmp", "StrLen", "StrCpy", "Format", "GetFarText", "ReadNumber",
    "BaseSetter", "DirLoop", "CanBeHere", "OnControl", "InitBresen", "DoBresen",
    "DoAvoider", "SetJump", "SetDebug", "InspectObj", "ShowSends", "ShowObjs", "ShowFree",
    "MemoryInfo", "StackUsage", "Profiler", "GetMenu", "SetMenu", "GetSaveFiles", "GetCWD",
    "CheckFreeSpace", "ValidPath", "CoordPri", "StrAt", "DeviceInfo", "GetSaveDir",
    "CheckSaveGame", "ShakeScreen", "FlushResources", "SinMult", "CosMult", "SinDiv",
    "CosDiv", "Graph", "Joystick", "ShiftScreen", "Palette", "MemorySegment",
    "Intersections", "Memory", "ListOps", "FileIO", "DoAudio", "DoSync",
};
#define K_COUNT ((int)(sizeof k_names / sizeof *k_names))

const char *kernel_name(int num)
{
    return (num >= 0 && num < K_COUNT) ? k_names[num] : "?";
}

static int restarting_flag;
void kernel_set_restarting(int r) { restarting_flag = r; }

static int unimpl_seen[K_COUNT];

/* deferred script disposal: code may still be running in the script */
static int pending_dispose[16], npending;

void kernel_flush_disposals(void)
{
    for (int i = 0; i < npending; i++)
        script_unload(pending_dispose[i]);
    npending = 0;
}

void kernel_reset_disposals(void) { npending = 0; }
static void menus_reset(void);

void kernel_init(void)
{
    npending = 0;
    menus_reset();
    kgfx_init();
}

/* ------------------------------------------------------------- lists */
/* list: {first, last}; node: {prev, next, key, value} */
#define L_FIRST 0
#define L_LAST  2
#define N_PREV  0
#define N_NEXT  2
#define N_KEY   4
#define N_VALUE 6

static void list_add_end(reg_t list, reg_t node)
{
    reg_t last = hp16((uint16_t)(list + L_LAST));
    hw16((uint16_t)(node + N_PREV), last);
    hw16((uint16_t)(node + N_NEXT), 0);
    if (last) hw16((uint16_t)(last + N_NEXT), node);
    else hw16((uint16_t)(list + L_FIRST), node);
    hw16((uint16_t)(list + L_LAST), node);
}

static void list_add_front(reg_t list, reg_t node)
{
    reg_t first = hp16((uint16_t)(list + L_FIRST));
    hw16((uint16_t)(node + N_NEXT), first);
    hw16((uint16_t)(node + N_PREV), 0);
    if (first) hw16((uint16_t)(first + N_PREV), node);
    else hw16((uint16_t)(list + L_LAST), node);
    hw16((uint16_t)(list + L_FIRST), node);
}

static void list_add_after(reg_t list, reg_t after, reg_t node)
{
    reg_t next = hp16((uint16_t)(after + N_NEXT));
    hw16((uint16_t)(node + N_PREV), after);
    hw16((uint16_t)(node + N_NEXT), next);
    hw16((uint16_t)(after + N_NEXT), node);
    if (next) hw16((uint16_t)(next + N_PREV), node);
    else hw16((uint16_t)(list + L_LAST), node);
}

static void list_unlink(reg_t list, reg_t node)
{
    reg_t prev = hp16((uint16_t)(node + N_PREV)), next = hp16((uint16_t)(node + N_NEXT));
    if (prev) hw16((uint16_t)(prev + N_NEXT), next);
    else hw16((uint16_t)(list + L_FIRST), next);
    if (next) hw16((uint16_t)(next + N_PREV), prev);
    else hw16((uint16_t)(list + L_LAST), prev);
}

/* ------------------------------------------------------------- format */

static void do_format(uint16_t dst, uint16_t argv, int argc)
{
    static char out[1024];
    int o = 0, ai = 2;
    int used;
    const char *fmt = vm_string(ARG(1), argc > 2 ? ARG(2) : 0, &used);
    if (used) ai = 3;
    static char fmtbuf[512];
    strncpy(fmtbuf, fmt, sizeof fmtbuf - 1);
    fmtbuf[sizeof fmtbuf - 1] = 0;
    for (const char *f = fmtbuf; *f && o < (int)sizeof out - 64; f++) {
        if (*f != '%') { out[o++] = *f; continue; }
        f++;
        if (*f == '%') { out[o++] = '%'; continue; }
        int left = 0, center = 0, width = 0;
        while (*f == '-' || *f == '=' || *f == '0') {
            if (*f == '-') left = 1;
            if (*f == '=') center = 1;
            f++;
        }
        while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0');
        static char tmp[512];
        tmp[0] = 0;
        switch (*f) {
        case 'd': snprintf(tmp, sizeof tmp, "%d", ai < argc + 1 ? SARG(ai) : 0); ai++; break;
        case 'u': snprintf(tmp, sizeof tmp, "%u", ai < argc + 1 ? ARG(ai) : 0); ai++; break;
        case 'x': snprintf(tmp, sizeof tmp, "%x", ai < argc + 1 ? ARG(ai) : 0); ai++; break;
        case 'c': tmp[0] = (char)(ai < argc + 1 ? ARG(ai) : ' '); tmp[1] = 0; ai++; break;
        case 's': {
            reg_t a = ai < argc + 1 ? ARG(ai) : 0;
            int u2;
            const char *s = vm_string(a, ai + 1 < argc + 1 ? ARG(ai + 1) : 0, &u2);
            ai += 1 + u2;
            strncpy(tmp, s, sizeof tmp - 1);
            tmp[sizeof tmp - 1] = 0;
            break;
        }
        default:
            tmp[0] = '%'; tmp[1] = *f; tmp[2] = 0;
            if (!*f) f--;
            break;
        }
        int len = (int)strlen(tmp), pad = width > len ? width - len : 0;
        if (center) {
            for (int i = 0; i < pad / 2; i++) out[o++] = ' ';
            memcpy(out + o, tmp, len); o += len;
            for (int i = 0; i < pad - pad / 2; i++) out[o++] = ' ';
        } else if (left) {
            memcpy(out + o, tmp, len); o += len;
            for (int i = 0; i < pad; i++) out[o++] = ' ';
        } else {
            for (int i = 0; i < pad; i++) out[o++] = ' ';
            memcpy(out + o, tmp, len); o += len;
        }
    }
    out[o] = 0;
    strcpy(hstr(dst), out);
}

/* --------------------------------------------------------------- math */

static int16_t get_angle(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    int16_t xrel = (int16_t)(x2 - x1), yrel = (int16_t)(y1 - y2), angle;
    if (y1 < y2) yrel = (int16_t)-yrel;
    if (x2 < x1) xrel = (int16_t)-xrel;
    if (yrel == 0 && xrel == 0) return 0;
    angle = (int16_t)(100 * xrel / (xrel + yrel));
    if (y1 < y2) angle = (int16_t)(200 - angle);
    if (x2 < x1) angle = (int16_t)(400 - angle);
    angle = (int16_t)(angle - (angle + 9) / 10);
    return angle;
}

/* ---------------------------------------------------------------- menus */
/* Jones has no visible menu bar, but its function keys (F5 save, F7
 * restore, ...) are bound through menu item shortcuts, so the menu kernels
 * track items and match key presses. */

#define MAX_MENUS 8
#define MAX_ITEMS 16
typedef struct { uint16_t key, said, text, tag; uint8_t enabled, used; } MenuItem;
static MenuItem menus[MAX_MENUS][MAX_ITEMS];
static int nmenus;
static void menus_reset(void) { nmenus = 0; memset(menus, 0, sizeof menus); }

/* menu attributes (SetMenu / GetMenu) */
#define MA_SAID 0x6d
#define MA_TEXT 0x6e
#define MA_KEY  0x6f
#define MA_ENABLED 0x70
#define MA_TAG  0x71

static uint16_t parse_menu_key(const char *k)
{
    if (k[0] == '#') {                    /* function key */
        int n = atoi(k + 1);
        if (n == 0) n = 10;
        return (uint16_t)((0x3a + n) << 8);
    }
    if (k[0] == '^' && k[1]) return (uint16_t)(toupper((unsigned char)k[1]) - '@');
    if (k[0] == '@' && k[1]) return 0;    /* alt keys: not used */
    return (uint16_t)(unsigned char)k[0];
}

static void add_menu(const char *items)
{
    if (nmenus >= MAX_MENUS) return;
    int m = nmenus++, n = 0;
    const char *p = items;
    while (*p && n < MAX_ITEMS) {
        const char *end = strchr(p, ':');
        if (!end) end = p + strlen(p);
        MenuItem *it = &menus[m][n++];
        memset(it, 0, sizeof *it);
        it->used = 1;
        it->enabled = !(p[0] == '-' && p[1] == '-');
        const char *bt = memchr(p, '`', (size_t)(end - p));
        if (bt) it->key = parse_menu_key(bt + 1);
        p = *end ? end + 1 : end;
    }
}

static MenuItem *menu_item(uint16_t id)
{
    int m = (id >> 8) - 1, i = (id & 0xff) - 1;
    if (m < 0 || m >= MAX_MENUS || i < 0 || i >= MAX_ITEMS || !menus[m][i].used) return NULL;
    return &menus[m][i];
}

static reg_t menu_select(reg_t ev)
{
    static int s_type = -2, s_msg, s_claimed;
    if (s_type == -2) { s_type = sel_id("type"); s_msg = sel_id("message"); s_claimed = sel_id("claimed"); }
    if (!obj_is_object(ev) || get_sel(ev, s_type) != EV_KEYDOWN) return 0;
    uint16_t key = get_sel(ev, s_msg);
    if (key >= 'a' && key <= 'z') key = (uint16_t)(key - 32);
    for (int m = 0; m < nmenus; m++)
        for (int i = 0; i < MAX_ITEMS; i++) {
            MenuItem *it = &menus[m][i];
            uint16_t k = it->key;
            if (k >= 'a' && k <= 'z') k = (uint16_t)(k - 32);
            if (it->used && it->enabled && k && k == key) {
                if (s_claimed >= 0) set_sel(ev, s_claimed, 1);
                return (reg_t)((m + 1) << 8 | (i + 1));
            }
        }
    return 0;
}

/* ---------------------------------------------------------------- dispatch */

uint32_t kperf_waits, kperf_wait_arg, kperf_animates;   /* profiling counters */
static reg_t kernel_call_inner(int num, int argc, uint16_t argv);
reg_t kernel_call(int num, int argc, uint16_t argv)
{
    reg_t r = kernel_call_inner(num, argc, argv);
    if (vm.trace & 8) vm_log("    -> %04x\n", r);
    return r;
}

static reg_t kernel_call_inner(int num, int argc, uint16_t argv)
{
    if (vm.trace) {
        vm_log("  k%s(argc %d)", kernel_name(num), argc);
        for (int i = 0; i < argc && i < 10; i++) vm_log(" %04x", ARG(i));
        vm_log("\n");
    }
    int handled = 1;
    reg_t r = kgfx_call(num, argc, argv, &handled);
    if (handled)
        return r;
    r = ksave_call(num, argc, argv, &handled);
    if (handled)
        return r;

    switch (num) {
    case 0x74:                                        /* FileIO: no files */
        if (ARG(0) == 0) vm_log("FileIO open '%s' mode %d\n", hstr(ARG(1)), argc > 2 ? ARG(2) : 0);
        return ARG(0) == 0 ? 0xffff : 0;
    case 0x75: { extern reg_t kdoaudio_call(int, uint16_t); return kdoaudio_call(argc, argv); }
    case 0x76: { extern reg_t kdosync_call(int, uint16_t); return kdosync_call(argc, argv); }
    case 0x00: return ARG(1);                         /* Load */
    case 0x01: return 0;                              /* UnLoad */
    case 0x02: return script_export(ARG(0), argc > 1 ? ARG(1) : 0);
    case 0x03:                                        /* DisposeScript */
        if (ARG(0) != 0 && npending < 16) pending_dispose[npending++] = ARG(0);
        return 0;
    case 0x04: {                                      /* Clone */
        reg_t parent = ARG(0);
        if (!obj_is_object(parent)) return 0;
        reg_t clone = obj_clone(parent);
        if (!clone) return 0;
        reg_t info = obj_var(parent, SEL_INFO);
        obj_set_var(clone, SEL_INFO, (reg_t)((info & ~INFO_CLASS) | INFO_CLONE));
        obj_set_var(clone, SEL_SPECIES, clone);
        if (info & INFO_CLASS)
            obj_set_var(clone, SEL_SUPER, parent);
        return clone;
    }
    case 0x05: {                                      /* DisposeClone */
        reg_t o = ARG(0);
        if (obj_is_object(o) && (obj_var(o, SEL_INFO) & INFO_CLONE)) {
            obj_free(o);
        }
        return 0;
    }
    case 0x06: return obj_is_object(ARG(0));
    case 0x07: return obj_is_object(ARG(0)) && has_sel(ARG(0), ARG(1));

    case 0x21: return menu_select(ARG(0));            /* MenuSelect */
    case 0x22:                                        /* AddMenu */
        add_menu(vm_string(ARG(1), 0, NULL));
        return 0;
    case 0x5b: {                                      /* GetMenu */
        MenuItem *it = menu_item(ARG(0));
        if (!it) return 0;
        switch (ARG(1)) {
        case MA_SAID: return it->said;
        case MA_TEXT: return it->text;
        case MA_KEY: return it->key;
        case MA_ENABLED: return it->enabled;
        case MA_TAG: return it->tag;
        }
        return 0;
    }
    case 0x5c: {                                      /* SetMenu */
        MenuItem *it = menu_item(ARG(0));
        for (int i = 1; it && i + 1 < argc; i += 2) {
            reg_t v = ARG(i + 1);
            switch (ARG(i)) {
            case MA_SAID: it->said = v; break;
            case MA_TEXT: it->text = v; break;
            case MA_KEY: it->key = v; break;
            case MA_ENABLED: it->enabled = v != 0; break;
            case MA_TAG: it->tag = v; break;
            }
        }
        return 0;
    }
    case 0x2b: vm_request_restart(); return 0;        /* RestartGame */
    case 0x2c: { int r2 = restarting_flag; restarting_flag = 0; return (reg_t)r2; }

    /* lists */
    case 0x2e: return heap_alloc(4);
    case 0x2f: {
        reg_t l = ARG(0);
        if (!l) return 0;
        reg_t n = hp16((uint16_t)(l + L_FIRST));
        while (n) { reg_t nx = hp16((uint16_t)(n + N_NEXT)); heap_free(n); n = nx; }
        heap_free(l);
        return 0;
    }
    case 0x30: {
        reg_t n = heap_alloc(8);
        if (!n) return 0;
        hw16((uint16_t)(n + N_VALUE), ARG(0));
        hw16((uint16_t)(n + N_KEY), argc > 1 ? ARG(1) : ARG(0));
        return n;
    }
    case 0x31: return ARG(0) ? hp16((uint16_t)(ARG(0) + L_FIRST)) : 0;
    case 0x32: return ARG(0) ? hp16((uint16_t)(ARG(0) + L_LAST)) : 0;
    case 0x33: return ARG(0) ? hp16((uint16_t)(ARG(0) + L_FIRST)) == 0 : 1;
    case 0x34: return ARG(0) ? hp16((uint16_t)(ARG(0) + N_NEXT)) : 0;
    case 0x35: return ARG(0) ? hp16((uint16_t)(ARG(0) + N_PREV)) : 0;
    case 0x36: return ARG(0) ? hp16((uint16_t)(ARG(0) + N_VALUE)) : 0;
    case 0x37: list_add_after(ARG(0), ARG(1), ARG(2)); return 0;
    case 0x38: list_add_front(ARG(0), ARG(1)); return 0;
    case 0x39: list_add_end(ARG(0), ARG(1)); return 0;
    case 0x3a: {
        reg_t l = ARG(0);
        for (reg_t n = l ? hp16(l) : 0; n; n = hp16((uint16_t)(n + N_NEXT)))
            if (hp16((uint16_t)(n + N_KEY)) == ARG(1)) return n;
        return 0;
    }
    case 0x3b: {
        reg_t l = ARG(0);
        for (reg_t n = l ? hp16(l) : 0; n; n = hp16((uint16_t)(n + N_NEXT)))
            if (hp16((uint16_t)(n + N_KEY)) == ARG(1)) {
                list_unlink(l, n);
                heap_free(n);
                return 1;
            }
        return 0;
    }

    /* math */
    case 0x3c:
        if (argc >= 2) {
            int lo = SARG(0), hi = SARG(1);
            if (hi < lo) { int t = lo; lo = hi; hi = t; }
            return (reg_t)(lo + rand() % (hi - lo + 1));
        }
        return 0;
    case 0x3d: return (reg_t)abs(SARG(0));
    case 0x3e: return (reg_t)(int)sqrt((double)abs(SARG(0)));
    case 0x3f: return (reg_t)get_angle(SARG(0), SARG(1), SARG(2), SARG(3));
    case 0x40: {
        int dx = SARG(0) - (argc > 2 ? SARG(2) : 0);
        int dy = SARG(1) - (argc > 3 ? SARG(3) : 0);
        double ang = argc > 4 ? SARG(4) * 3.14159265 / 180.0 : 0;
        double ydist = dy / cos(ang);
        return (reg_t)(int)sqrt(dx * (double)dx + ydist * ydist);
    }
    case 0x68: return (reg_t)(int)(SARG(1) * sin(SARG(0) * 3.14159265 / 180.0));
    case 0x69: return (reg_t)(int)(SARG(1) * cos(SARG(0) * 3.14159265 / 180.0));
    case 0x6a: { double s = sin(SARG(0) * 3.14159265 / 180.0); return s ? (reg_t)(int)(SARG(1) / s) : 0; }
    case 0x6b: { double c = cos(SARG(0) * 3.14159265 / 180.0); return c ? (reg_t)(int)(SARG(1) / c) : 0; }

    /* time */
    case 0x41: {                                      /* Wait */
        static uint32_t last;
        int ticks = ARG(0);
        kperf_waits++; kperf_wait_arg = (uint32_t)ticks;
        uint32_t now = plat_ticks();
        if (ticks > 0 && now - last < (uint32_t)ticks)
            plat_sleep_ticks(ticks - (int)(now - last));
        now = plat_ticks();
        reg_t elapsed = (reg_t)(now - last);
        last = now;
        return elapsed;
    }
    case 0x42: {                                      /* GetTime */
        uint32_t t = plat_ticks();
        if (argc == 0 || ARG(0) == 0) return (reg_t)t;
        uint32_t secs = t / 60;
        int h = (int)(secs / 3600) % 24, m = (int)(secs / 60) % 60, s = (int)secs % 60;
        if (ARG(0) == 1) return (reg_t)(((h % 12) << 12) | (m << 6) | s);
        if (ARG(0) == 2) return (reg_t)((h << 11) | (m << 5) | (s >> 1));
        return (reg_t)(((1991 - 1980) << 9) | (1 << 5) | 1);
    }

    /* strings */
    case 0x43: return (reg_t)(ARG(0) + strlen(hstr(ARG(0))));
    case 0x44: strcat(hstr(ARG(0)), vm_string(ARG(1), 0, NULL)); return ARG(0);
    case 0x45:
        if (argc > 2) return (reg_t)(int16_t)strncmp(hstr(ARG(0)), hstr(ARG(1)), ARG(2));
        return (reg_t)(int16_t)strcmp(hstr(ARG(0)), hstr(ARG(1)));
    case 0x46: return (reg_t)strlen(vm_string(ARG(0), 0, NULL));
    case 0x47: {
        const char *src = vm_string(ARG(1), 0, NULL);
        if (argc > 2) {
            int16_t n = SARG(2);
            if (n >= 0) strncpy(hstr(ARG(0)), src, (size_t)n);
            else memmove(hstr(ARG(0)), hstr(ARG(1)), (size_t)-n);
        } else {
            memmove(hstr(ARG(0)), src, strlen(src) + 1);
        }
        return ARG(0);
    }
    case 0x48: do_format(ARG(0), argv, argc); return ARG(0);
    case 0x49: {
        const char *t = vm_text(ARG(0), ARG(1));
        strcpy(hstr(ARG(2)), t);
        return ARG(2);
    }
    case 0x4a: return (reg_t)atoi(vm_string(ARG(0), 0, NULL));
    case 0x62: {                                      /* StrAt */
        uint16_t a = (uint16_t)(ARG(0) + ARG(1));
        reg_t old = vm_heap[a];
        if (argc > 2) vm_heap[a] = (uint8_t)ARG(2);
        return old;
    }

    /* debug / system */
    case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: case 0x59: case 0x5a:
    case 0x67:
        return 0;
    case 0x58:                                        /* MemoryInfo */
        switch (ARG(0)) {
        case 0: return heap_free_bytes();
        case 1: return heap_largest_free();
        default: return 0x7fff;
        }
    case 0x5e: strcpy(hstr(ARG(0)), "C:\\JONES"); return ARG(0);   /* GetCWD */
    case 0x64: strcpy(hstr(ARG(0)), "C:\\JONES"); return ARG(0);   /* GetSaveDir */
    case 0x5f: return 1;                              /* CheckFreeSpace */
    case 0x60: return 1;                              /* ValidPath */
    case 0x63:                                        /* DeviceInfo */
        switch (ARG(0)) {
        case 0: strcpy(hstr(ARG(2)), "C:"); return ARG(2);
        case 1: strcpy(hstr(ARG(1)), "C:"); return ARG(1);
        case 2: return !strcmp(hstr(ARG(1)), hstr(ARG(2)));
        case 3: return 0;
        case 4: return 1;
        default: return 0;
        }
    case 0x72:                                        /* Memory */
        switch (ARG(0)) {
        case 1: case 2: return heap_alloc(ARG(1));
        case 3: heap_free(ARG(1)); return 0;
        case 4: memmove(&vm_heap[ARG(1)], &vm_heap[ARG(2)], ARG(3)); return 0;
        case 5: return hp16(ARG(1));
        case 6: hw16(ARG(1), ARG(2)); return 0;
        }
        return 0;
    }

    if (num < K_COUNT && !unimpl_seen[num]) {
        unimpl_seen[num] = 1;
        vm_log("unimplemented kernel %s (argc %d)\n", kernel_name(num), argc);
    }
    return 0;
}
