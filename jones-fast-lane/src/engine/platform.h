/* Platform interface implemented by the host (macOS test runner / NDS). */
#ifndef SCI_PLATFORM_H
#define SCI_PLATFORM_H

#include <stdint.h>
#include <stdarg.h>

/* SCI event types */
#define EV_NULL      0x0000
#define EV_MOUSEDOWN 0x0001
#define EV_MOUSEUP   0x0002
#define EV_KEYDOWN   0x0004
#define EV_KEYUP     0x0008
#define EV_JOYSTICK  0x0040
#define EV_ALL       0x7fff
#define EV_PEEK      0x8000

typedef struct {
    uint16_t type;
    uint16_t message;    /* key code / direction */
    uint16_t modifiers;
    int16_t  x, y;       /* SCI screen coords (320x200) */
} PlatEvent;

void     plat_vlog(const char *fmt, va_list ap);
uint32_t plat_ticks(void);                 /* 60 Hz */
void     plat_sleep_ticks(int ticks);      /* yield; keeps input/display alive */
void     plat_present(void);               /* push gfx.visual + palette to screen */
int      plat_poll_event(PlatEvent *ev);   /* 1 if an event was dequeued */
void     plat_mouse(int16_t *x, int16_t *y);
void     plat_set_cursor(const uint8_t *view_or_cursor, int type, int loop, int cel, int visible);
void     plat_sound(int cmd, int a, int b, int c);
int      plat_save(int slot, const void *data, uint32_t size);   /* 1 on success */
int      plat_load(int slot, void *data, uint32_t maxsize);      /* bytes read, 0 if none */

#endif
