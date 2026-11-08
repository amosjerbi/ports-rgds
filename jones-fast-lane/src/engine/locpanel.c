/* Location panel contents; see locpanel.h. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "locpanel.h"

/* Background views of the location windows (from the CD resources). */
static const struct { int view; const char *name; } places[] = {
    { 700, "Low-Cost Housing" }, { 701, "Rent Office" }, { 702, "Le Securite Apts" },
    { 705, "The Factory" }, { 706, "Employment Office" }, { 710, "Monolith Burgers" },
    { 712, "Pawn Shop" }, { 803, "Black's Market" }, { 804, "Bank" },
    { 807, "Hi-Tech University" }, { 808, "Socket City" }, { 809, "QT Clothing" },
    { 810, "Monolith Burgers" }, { 811, "Z-Mart" },
};

/* The calculator (cash display) on the town map, screen coords. */
#define CALC_X0 248
#define CALC_Y0 156

#define MAX_RAW 24

typedef struct { char text[96]; int16_t x0, y0, x1, y1; } RawText;
typedef struct { char name[32]; int16_t x0, y0, x1, y1; uint8_t type, enabled; } RawCtl;

static int port_open, view;
static int16_t wx0, wy0, wx1, wy1;
static RawText texts[MAX_RAW];
static RawCtl ctls[MAX_RAW];
static int ntexts, nctls;
static LocEntry entries[LOC_MAX_ENTRIES];
static int nentries, sel = -1;
static int user_moved;          /* keep the highlight only once the player has moved it */
static char cash[20];
static uint32_t sig;

static int overlap(int ax0, int ay0, int ax1, int ay1, int bx0, int by0, int bx1, int by1)
{
    return ax0 < bx1 && bx0 < ax1 && ay0 < by1 && by0 < ay1;
}

static int selectable(const LocEntry *e) { return e->kind != LOC_MESSAGE && e->enabled; }

/* "pawnButton" -> "Pawn", "exitButton" -> "Done", "jrCollege" -> "Jr College" */
static void pretty(char *out, size_t n, const char *obj)
{
    char tmp[32];
    snprintf(tmp, sizeof tmp, "%s", obj);
    size_t len = strlen(tmp);
    static const char *suffix[] = { "Button", "Btn", "Icon", "Ctl", NULL };
    for (int i = 0; suffix[i]; i++) {
        size_t sl = strlen(suffix[i]);
        if (len > sl && !strcmp(tmp + len - sl, suffix[i])) { tmp[len - sl] = 0; break; }
    }
    if (!strcmp(tmp, "exit") || !strcmp(tmp, "done") || !strcmp(tmp, "quit")) { snprintf(out, n, "Done"); return; }
    size_t o = 0;
    for (size_t i = 0; tmp[i] && o + 2 < n; i++) {
        char c = tmp[i];
        if (i && isupper((unsigned char)c) && islower((unsigned char)tmp[i - 1])) out[o++] = ' ';
        out[o++] = i == 0 ? (char)toupper((unsigned char)c) : c;
    }
    out[o] = 0;
}

/* "Hamburgers.........|$85" -> label "Hamburgers", price "$85" */
static void parse_row(const char *s, char *label, size_t ln, char *price, size_t pn)
{
    char t[96];
    size_t o = 0;
    for (size_t i = 0; s[i] && o + 1 < sizeof t; i++) {
        char c = s[i];
        if (c == '\n' || c == '\r' || c == '|') c = ' ';
        if (c == ' ' && o && t[o - 1] == ' ') continue;
        t[o++] = c;
    }
    t[o] = 0;
    char *d = strchr(t, '$');
    price[0] = 0;
    if (d && d > t) {
        snprintf(price, pn, "%s", d);
        size_t pl = strlen(price);
        while (pl && price[pl - 1] == ' ') price[--pl] = 0;
        *d = 0;
    }
    size_t l = strlen(t);
    while (l && (t[l - 1] == '.' || t[l - 1] == ' ')) t[--l] = 0;
    char *b = t;
    while (*b == ' ') b++;
    snprintf(label, ln, "%s", b);
}

static void compose(void)
{
    char keep[48] = "";
    if (user_moved && sel >= 0 && sel < nentries) snprintf(keep, sizeof keep, "%s", entries[sel].label);
    nentries = 0;
    /* text rows, top to bottom (two columns in some stores: left one first) */
    int order[MAX_RAW];
    for (int i = 0; i < ntexts; i++) order[i] = i;
    for (int i = 1; i < ntexts; i++)
        for (int j = i; j > 0; j--) {
            RawText *a = &texts[order[j - 1]], *b = &texts[order[j]];
            int ca = a->x0 >= (wx0 + wx1) / 2 + 20, cb = b->x0 >= (wx0 + wx1) / 2 + 20;
            if (ca > cb || (ca == cb && a->y0 > b->y0)) { int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t; }
        }
    for (int k = 0; k < ntexts && nentries < LOC_MAX_ENTRIES; k++) {
        RawText *t = &texts[order[k]];
        LocEntry *e = &entries[nentries];
        parse_row(t->text, e->label, sizeof e->label, e->price, sizeof e->price);
        if (!e->label[0] && !e->price[0]) continue;
        e->x0 = t->x0; e->y0 = t->y0; e->x1 = t->x1; e->y1 = t->y1;
        /* priced rows and short one-line labels are choices (the employers
         * list, school courses); longer or multi-line text is a message */
        e->kind = (e->price[0] || (!strchr(t->text, '\n') && strlen(e->label) <= 28)) ? LOC_ITEM : LOC_MESSAGE;
        e->enabled = 1;
        for (int c = 0; c < nctls; c++)        /* a hit area over the text: clickable */
            if (ctls[c].type != 4 && ctls[c].type != 1 &&
                overlap(t->x0, t->y0, t->x1, t->y1, ctls[c].x0, ctls[c].y0, ctls[c].x1, ctls[c].y1)) {
                e->kind = LOC_ITEM;
                e->enabled = ctls[c].enabled;
                e->x0 = ctls[c].x0; e->y0 = ctls[c].y0; e->x1 = ctls[c].x1; e->y1 = ctls[c].y1;
                break;
            }
        nentries++;
    }
    /* hit areas with no text over them (picture items), then buttons left
     * to right with Done last */
    int corder[MAX_RAW];
    for (int i = 0; i < nctls; i++) corder[i] = i;
    for (int i = 1; i < nctls; i++)
        for (int j = i; j > 0; j--) {
            RawCtl *a = &ctls[corder[j - 1]], *b = &ctls[corder[j]];
            int da = !strcmp(a->name, "exitButton"), db = !strcmp(b->name, "exitButton");
            if (da > db || (da == db && (a->y0 / 8 > b->y0 / 8 || (a->y0 / 8 == b->y0 / 8 && a->x0 > b->x0)))) {
                int t = corder[j]; corder[j] = corder[j - 1]; corder[j - 1] = t;
            }
        }
    for (int pass = 0; pass < 2; pass++)
        for (int ci = 0; ci < nctls && nentries < LOC_MAX_ENTRIES; ci++) {
            RawCtl *k = &ctls[corder[ci]];
            int button = k->type == 4 || k->type == 1;
            if (button != pass) continue;
            if (!button) {
                int covered = 0;
                for (int i = 0; i < ntexts; i++)
                    if (overlap(texts[i].x0, texts[i].y0, texts[i].x1, texts[i].y1, k->x0, k->y0, k->x1, k->y1)) covered = 1;
                if (covered) continue;
            }
            LocEntry *e = &entries[nentries++];
            pretty(e->label, sizeof e->label, k->name);
            e->price[0] = 0;
            e->x0 = k->x0; e->y0 = k->y0; e->x1 = k->x1; e->y1 = k->y1;
            e->kind = button ? LOC_BUTTON : LOC_ITEM;
            e->enabled = k->enabled;
        }
    /* keep the highlight on the same entry if it is still there */
    sel = -1;
    for (int i = 0; i < nentries; i++)
        if (selectable(&entries[i]) && keep[0] && !strcmp(entries[i].label, keep)) { sel = i; break; }
    if (sel < 0)
        for (int i = 0; i < nentries; i++) if (selectable(&entries[i])) { sel = i; break; }
    sig++;
}

void locpanel_reset(void)
{
    port_open = 0;
    ntexts = nctls = nentries = 0;
    sel = -1;
    user_moved = 0;
    sig++;
}

void locpanel_window_open(int port, int x0, int y0, int x1, int y1)
{
    if (x1 - x0 < 120 || y1 - y0 < 80) return;     /* message boxes etc. */
    port_open = port;
    view = 0;
    wx0 = (int16_t)x0; wy0 = (int16_t)y0; wx1 = (int16_t)x1; wy1 = (int16_t)y1;
    ntexts = nctls = nentries = 0;
    sel = -1;
    user_moved = 0;
    sig++;
}

void locpanel_window_close(int port)
{
    if (port && port == port_open) locpanel_reset();
}

void locpanel_text(int port, const char *s, int x0, int y0, int x1, int y1)
{
    if (x0 >= CALC_X0 && y0 >= CALC_Y0) {             /* calculator readout */
        char d[16]; size_t o = 0;
        for (; *s && o + 1 < sizeof d; s++) if (isdigit((unsigned char)*s) || *s == '-') d[o++] = *s;
        d[o] = 0;
        if (o) {
            char c[20];
            snprintf(c, sizeof c, "$%s", d);
            if (strcmp(c, cash)) { snprintf(cash, sizeof cash, "%s", c); sig++; }
        }
        return;
    }
    if (!port_open || port != port_open || !*s) return;
    for (int i = 0; i < ntexts; i++)                   /* drop shadow / redraws */
        if (!strncmp(texts[i].text, s, sizeof texts[i].text - 1)) return;
    if (ntexts == MAX_RAW) return;
    RawText *t = &texts[ntexts++];
    snprintf(t->text, sizeof t->text, "%s", s);
    t->x0 = (int16_t)x0; t->y0 = (int16_t)y0; t->x1 = (int16_t)x1; t->y1 = (int16_t)y1;
    compose();
}

void locpanel_control(int port, const char *obj, int type, int state, int v,
                      int x0, int y0, int x1, int y1)
{
    if (!port_open || port != port_open) return;
    if (!strcmp(obj, "background")) {                  /* a new screen in this window */
        view = v;
        ntexts = nctls = 0;
        user_moved = 0;
        compose();
        return;
    }
    if (!strcmp(obj, "calc")) return;
    for (int i = 0; i < nctls; i++)
        if (!strcmp(ctls[i].name, obj)) {
            uint8_t en = (state & 1) != 0;
            if (ctls[i].enabled != en || ctls[i].x0 != x0 || ctls[i].y0 != y0) {
                ctls[i].enabled = en;
                ctls[i].x0 = (int16_t)x0; ctls[i].y0 = (int16_t)y0; ctls[i].x1 = (int16_t)x1; ctls[i].y1 = (int16_t)y1;
                compose();
            }
            return;
        }
    if (nctls == MAX_RAW) return;
    RawCtl *k = &ctls[nctls++];
    snprintf(k->name, sizeof k->name, "%s", obj);
    k->type = (uint8_t)type;
    k->enabled = (state & 1) != 0;
    k->x0 = (int16_t)x0; k->y0 = (int16_t)y0; k->x1 = (int16_t)x1; k->y1 = (int16_t)y1;
    compose();
}

int locpanel_active(void) { return port_open && view && nentries; }

const char *locpanel_title(void)
{
    for (size_t i = 0; i < sizeof places / sizeof *places; i++)
        if (places[i].view == view) return places[i].name;
    return "";
}

const char *locpanel_cash(void) { return cash; }

int locpanel_entries(const LocEntry **out) { *out = entries; return nentries; }

int locpanel_selected(void) { return sel; }

void locpanel_move(int delta)
{
    if (!nentries) return;
    int i = sel < 0 ? 0 : sel;
    for (int step = 0; step < nentries; step++) {
        i = (i + delta + nentries) % nentries;
        if (selectable(&entries[i])) { sel = i; user_moved = 1; sig++; return; }
    }
}

int locpanel_target(int16_t *x, int16_t *y)
{
    if (sel < 0 || sel >= nentries || !selectable(&entries[sel])) return 0;
    *x = (int16_t)((entries[sel].x0 + entries[sel].x1) / 2);
    *y = (int16_t)((entries[sel].y0 + entries[sel].y1) / 2);
    return 1;
}

uint32_t locpanel_signature(void) { return sig; }
