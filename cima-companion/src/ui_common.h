#pragma once
/* Shared drawing + touch helpers (SDL2, SDL2_ttf, evdev). */
/* ------------------------------------------------------------------------------------------------------------------ */
/* drawing helpers                                                                                                    */
/* ------------------------------------------------------------------------------------------------------------------ */
static SDL_Window* sWin;
static SDL_Renderer* sRen;
static TTF_Font *sFont18, *sFont14, *sFont22, *sFont12;

typedef struct { uint8_t r, g, b; } Col;
static const Col C_BG = { 16, 12, 24 }, C_PANEL = { 30, 22, 42 }, C_PANEL2 = { 44, 30, 58 }, C_GOLD = { 214, 176, 88 },
                 C_CREAM = { 240, 230, 208 }, C_DIM = { 128, 118, 140 }, C_RED = { 168, 34, 52 }, C_BLUE = { 70, 110, 200 },
                 C_GREEN = { 74, 170, 98 }, C_YELLOW = { 226, 196, 70 }, C_WHITE = { 255, 255, 255 }, C_SEL = { 92, 36, 60 };

static void SetCol(Col c, int a) { SDL_SetRenderDrawColor(sRen, c.r, c.g, c.b, (Uint8)a); }
static void FillRect(int x, int y, int w, int h, Col c) {
    SDL_Rect r = { x, y, w, h };
    SetCol(c, 255);
    SDL_RenderFillRect(sRen, &r);
}
static void FrameRect(int x, int y, int w, int h, Col c) {
    SDL_Rect r = { x, y, w, h };
    SetCol(c, 255);
    SDL_RenderDrawRect(sRen, &r);
}

typedef struct { char key[240]; SDL_Texture* tex; int w, h; Uint32 last; } TexEntry;
static TexEntry sCache[384];

static SDL_Texture* GetText(TTF_Font* f, const char* s, Col c, int* w, int* h) {
    char key[240];
    snprintf(key, sizeof key, "%p|%02x%02x%02x|%s", (void*)f, c.r, c.g, c.b, s);
    const size_t n = sizeof sCache / sizeof sCache[0];
    TexEntry* slot = NULL;
    for (size_t i = 0; i < n; i++) {
        TexEntry* e = &sCache[i];
        if (e->tex && strcmp(e->key, key) == 0) {
            e->last = SDL_GetTicks();
            *w = e->w;
            *h = e->h;
            return e->tex;
        }
        if (!slot || (slot->tex && (!e->tex || e->last < slot->last)))
            slot = e;
    }
    if (slot->tex)
        SDL_DestroyTexture(slot->tex);
    slot->tex = NULL;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s[0] ? s : " ", (SDL_Color){ c.r, c.g, c.b, 255 });
    if (!surf)
        return NULL;
    slot->tex = SDL_CreateTextureFromSurface(sRen, surf);
    slot->w = surf->w;
    slot->h = surf->h;
    snprintf(slot->key, sizeof slot->key, "%s", key);
    slot->last = SDL_GetTicks();
    SDL_FreeSurface(surf);
    *w = slot->w;
    *h = slot->h;
    return slot->tex;
}

/* align: 0 left, 1 centre, 2 right. Returns drawn width. */
static int Text(TTF_Font* f, int x, int y, int align, Col c, const char* fmt, ...) {
    char buf[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    int w = 0, h = 0;
    SDL_Texture* t = GetText(f, buf, c, &w, &h);
    if (!t)
        return 0;
    SDL_Rect d = { align == 0 ? x : align == 1 ? x - w / 2 : x - w, y, w, h };
    SDL_RenderCopy(sRen, t, NULL, &d);
    return w;
}

static int TextWidth(TTF_Font* f, const char* s) {
    int w = 0, h = 0;
    TTF_SizeUTF8(f, s, &w, &h);
    return w;
}

/* Word-wrapped paragraph, at most maxLines lines. */
static void Paragraph(TTF_Font* f, int x, int y, int width, int lineH, int maxLines, Col c, const char* s) {
    char line[260] = "", word[96];
    int lines = 0;
    const char* p = s;
    while (*p && lines < maxLines) {
        int n = 0;
        while (*p == ' ')
            p++;
        while (*p && *p != ' ' && n < 90)
            word[n++] = *p++;
        word[n] = 0;
        if (!n)
            break;
        char test[260];
        snprintf(test, sizeof test, "%s%s%s", line, line[0] ? " " : "", word);
        if (line[0] && TextWidth(f, test) > width) {
            Text(f, x, y + lines * lineH, 0, c, "%s", line);
            lines++;
            snprintf(line, sizeof line, "%s", word);
        } else {
            snprintf(line, sizeof line, "%s", test);
        }
    }
    if (line[0] && lines < maxLines)
        Text(f, x, y + lines * lineH, 0, c, "%s", line);
}

/* ------------------------------------------------------------------------------------------------------------------ */
/* touch                                                                                                              */
/* ------------------------------------------------------------------------------------------------------------------ */
typedef struct { int x, y, w, h, id; } Hit;
static Hit sHits[160];
static int sHitCount;
static void AddHit(int x, int y, int w, int h, int id) {
    if (sHitCount < (int)(sizeof sHits / sizeof sHits[0]))
        sHits[sHitCount++] = (Hit){ x, y, w, h, id };
}
static int HitTest(int x, int y) {
    for (int i = sHitCount - 1; i >= 0; i--)
        if (x >= sHits[i].x && x < sHits[i].x + sHits[i].w && y >= sHits[i].y && y < sHits[i].y + sHits[i].h)
            return sHits[i].id;
    return -1;
}

static int sTouchFd = -1;
static int sTx, sTy, sDownX, sDownY, sLastY;
static bool sDown, sHaveX, sHaveY, sMoved, sPendingTap;
static int sTapX, sTapY;

static int OpenBottomTouch(void) {
    const char* forced = getenv("RGDS_TOUCH_DEV");
    char path[128], best[128] = "";
    if (forced) {
        snprintf(best, sizeof best, "%s", forced);
    } else {
        for (int i = 0; i < 32; i++) {
            char name[128] = "", sysfs[256], link[512];
            snprintf(sysfs, sizeof sysfs, "/sys/class/input/event%d/device/name", i);
            FILE* f = fopen(sysfs, "r");
            if (!f)
                continue;
            if (fgets(name, sizeof name, f) && strstr(name, "Goodix")) {
                snprintf(path, sizeof path, "/dev/input/event%d", i);
                snprintf(sysfs, sizeof sysfs, "/sys/class/input/event%d/device", i);
                ssize_t n = readlink(sysfs, link, sizeof link - 1);
                if (n > 0) {
                    link[n] = 0;
                    if (strstr(link, "fe5e0000")) {
                        snprintf(best, sizeof best, "%s", path);
                        fclose(f);
                        break;
                    }
                }
                snprintf(best, sizeof best, "%s", path);
            }
            fclose(f);
        }
    }
    if (!best[0])
        return -1;
    int fd = open(best, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    fprintf(stderr, "[cima] bottom touch: %s (%s)\n", best, fd >= 0 ? "ok" : "cannot open");
    return fd;
}

static int sDragDy; /* accumulated vertical drag since last consumed */

static void PumpTouch(void) {
    struct input_event ev[32];
    ssize_t n;
    while (sTouchFd >= 0 && (n = read(sTouchFd, ev, sizeof ev)) > 0) {
        for (size_t i = 0; i < (size_t)n / sizeof ev[0]; i++) {
            if (ev[i].type == EV_ABS) {
                if (ev[i].code == ABS_MT_POSITION_X || ev[i].code == ABS_X) {
                    sTx = ev[i].value;
                    sHaveX = true;
                } else if (ev[i].code == ABS_MT_POSITION_Y || ev[i].code == ABS_Y) {
                    sTy = ev[i].value;
                    sHaveY = true;
                } else if (ev[i].code == ABS_MT_TRACKING_ID) {
                    if (ev[i].value >= 0) {
                        sDown = true;
                        sMoved = false;
                        sDownX = sDownY = -1;
                    } else if (sDown) {
                        sDown = false;
                        if (!sMoved) {
                            sPendingTap = true;
                            sTapX = sDownX >= 0 ? sDownX : sTx;
                            sTapY = sDownY >= 0 ? sDownY : sTy;
                        }
                    }
                }
            } else if (ev[i].type == EV_KEY && ev[i].code == BTN_TOUCH) {
                if (ev[i].value == 1 && !sDown) {
                    sDown = true;
                    sMoved = false;
                    sDownX = sDownY = -1;
                } else if (ev[i].value == 0 && sDown) {
                    sDown = false;
                    if (!sMoved) {
                        sPendingTap = true;
                        sTapX = sDownX >= 0 ? sDownX : sTx;
                        sTapY = sDownY >= 0 ? sDownY : sTy;
                    }
                }
            } else if (ev[i].type == EV_SYN && ev[i].code == SYN_REPORT && sDown && sHaveX && sHaveY) {
                if (sDownX < 0) {
                    sDownX = sTx;
                    sDownY = sTy;
                    sLastY = sTy;
                } else {
                    if (abs(sTx - sDownX) > 14 || abs(sTy - sDownY) > 14)
                        sMoved = true;
                    if (sMoved) {
                        sDragDy += sTy - sLastY;
                        sLastY = sTy;
                    }
                }
            }
        }
    }
}

