/* Location panel for the DS top screen.
 *
 * While a location window is open (a store, the bank, the university...),
 * its contents are collected from what the scripts draw into it: text rows
 * (Display), buttons and hit areas (DrawControl), and the window's background
 * view, which names the location. The top screen redraws them as a panel in
 * the Life Goals style (topstatus.c), and the d-pad picks an entry, which is
 * clicked in the game. */
#ifndef SCI_LOCPANEL_H
#define SCI_LOCPANEL_H
#include <stdint.h>

#define LOC_MAX_ENTRIES 20

enum { LOC_MESSAGE, LOC_ITEM, LOC_BUTTON };

typedef struct {
    char    label[48];
    char    price[12];             /* "$85", or "" */
    int16_t x0, y0, x1, y1;        /* screen rect, 320x200 */
    uint8_t kind;                  /* LOC_* */
    uint8_t enabled;
} LocEntry;

/* --- fed by kgraphics.c (screen coordinates, right/bottom exclusive) --- */
void locpanel_reset(void);
void locpanel_window_open(int port, int x0, int y0, int x1, int y1);
void locpanel_window_close(int port);
void locpanel_text(int port, const char *s, int x0, int y0, int x1, int y1);
void locpanel_control(int port, const char *obj, int type, int state, int view,
                      int x0, int y0, int x1, int y1);

/* --- read by the panel and the DS input code --- */
int  locpanel_active(void);
const char *locpanel_title(void);
const char *locpanel_cash(void);            /* calculator display, "" if unknown */
int  locpanel_entries(const LocEntry **out);
int  locpanel_selected(void);               /* index into entries, -1 if none */
void locpanel_move(int delta);              /* d-pad: next/previous selectable */
int  locpanel_target(int16_t *x, int16_t *y);  /* click point of the selection */
uint32_t locpanel_signature(void);          /* changes when the panel must redraw */

#endif
