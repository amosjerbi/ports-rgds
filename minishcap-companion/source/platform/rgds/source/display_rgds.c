#include "display_rgds.h"

#include <SDL2/SDL.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void Port_SecondScreen_3DS_OnTap(int x, int y, int longPress); /* engine side; plain C types */

static SDL_Window* sTopWin;
static SDL_Window* sBotWin;
static SDL_Renderer* sTopRen;
static SDL_Renderer* sBotRen;
static SDL_Texture* sTopTex;
static SDL_Texture* sBotTex;
static int sTopTexW, sTopTexH, sBotTexW, sBotTexH;
static SDL_GameController* sPads[4];
static uint32_t sHeld;     /* GBA bit layout, active high */
static uint32_t sPrevHeld;
static uint32_t sDownEdges;
static float sStickX, sStickY;
static bool sQuit;
static int sTouchFd = -1; /* bottom-panel touchscreen, read straight from evdev */
static int sTouchX, sTouchY;
static bool sTouchNew, sTouchHaveX, sTouchHaveY;
static bool sBotDirty;
static uint32_t sBotWinId, sTopWinId;

enum { K_A = 1u << 0, K_B = 1u << 1, K_SELECT = 1u << 2, K_START = 1u << 3, K_RIGHT = 1u << 4, K_LEFT = 1u << 5,
       K_UP = 1u << 6, K_DOWN = 1u << 7, K_R = 1u << 8, K_L = 1u << 9 };

static SDL_Window* MakeWindow(const char* title, int display, bool windowed) {
    Uint32 flags = SDL_WINDOW_SHOWN;
    int x = SDL_WINDOWPOS_UNDEFINED_DISPLAY(display), y = SDL_WINDOWPOS_UNDEFINED_DISPLAY(display);
    if (windowed) {
        flags |= SDL_WINDOW_RESIZABLE;
        x = SDL_WINDOWPOS_UNDEFINED;
        y = SDL_WINDOWPOS_UNDEFINED;
    } else {
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS;
    }
    return SDL_CreateWindow(title, x, y, RGDS_PANEL_W, RGDS_PANEL_H, flags);
}

/* The RG DS has two Goodix controllers, one per panel, each reporting 0..639 x 0..479. EmulationStation
 * attaches them to a different Wayland seat than the one SDL binds, so Wayland touch never arrives; reading
 * the bottom panel's evdev node directly sidesteps that. fe5e0000.i2c is the bottom panel (libinput offsets
 * it to the right half of the 1280x480 layout). RGDS_TOUCH_DEV overrides the node. */
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
                    if (strstr(link, "fe5e0000")) { /* bottom panel: prefer it outright */
                        snprintf(best, sizeof best, "%s", path);
                        fclose(f);
                        break;
                    }
                }
                snprintf(best, sizeof best, "%s", path); /* otherwise the last Goodix node */
            }
            fclose(f);
        }
    }
    if (!best[0])
        return -1;
    int fd = open(best, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    fprintf(stderr, "[rgds] bottom touch: %s (%s)\n", best, fd >= 0 ? "ok" : "cannot open");
    return fd;
}

static void PumpTouch(void) {
    struct input_event ev[32];
    ssize_t n;
    while (sTouchFd >= 0 && (n = read(sTouchFd, ev, sizeof ev)) > 0) {
        for (size_t i = 0; i < (size_t)n / sizeof ev[0]; i++) {
            if (ev[i].type == EV_ABS) {
                if (ev[i].code == ABS_MT_POSITION_X || ev[i].code == ABS_X) {
                    sTouchX = ev[i].value;
                    sTouchHaveX = true;
                } else if (ev[i].code == ABS_MT_POSITION_Y || ev[i].code == ABS_Y) {
                    sTouchY = ev[i].value;
                    sTouchHaveY = true;
                } else if (ev[i].code == ABS_MT_TRACKING_ID && ev[i].value >= 0) {
                    sTouchNew = true; /* a finger just landed */
                    sTouchHaveX = sTouchHaveY = false;
                }
            } else if (ev[i].type == EV_KEY && ev[i].code == BTN_TOUCH && ev[i].value == 1) {
                sTouchNew = true;
            } else if (ev[i].type == EV_SYN && ev[i].code == SYN_REPORT && sTouchNew && sTouchHaveX && sTouchHaveY) {
                sTouchNew = false;
                Port_SecondScreen_3DS_OnTap(sTouchX, sTouchY, 0); /* device coords == painted 640x480 coords */
            }
        }
    }
}

static bool InitImpl(bool withTop);

bool RgdsDisplay_Init(void) {
    return InitImpl(true);
}

bool RgdsDisplay_InitBottomOnly(void) {
    return InitImpl(false);
}

static bool InitImpl(bool withTop) {
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
    const int displays = SDL_GetNumVideoDisplays();
    const bool windowed = getenv("RGDS_WINDOWED") != NULL || displays < 2;
    fprintf(stderr, "[rgds] %d display(s), %s\n", displays, windowed ? "windowed" : "one fullscreen window per panel");
    /* The sway config moves windows titled "...Bottom Screen" to DSI-1; the display index covers other setups. */
    sTopWin = withTop ? MakeWindow("The Minish Cap", 0, windowed) : NULL;
    sBotWin = MakeWindow("The Minish Cap - Bottom Screen", displays > 1 ? 1 : 0, windowed);
    if ((withTop && !sTopWin) || !sBotWin) {
        fprintf(stderr, "window creation failed: %s\n", SDL_GetError());
        return false;
    }
    sTopWinId = sTopWin ? SDL_GetWindowID(sTopWin) : 0;
    sBotWinId = SDL_GetWindowID(sBotWin);
    if (sTopWin)
        sTopRen = SDL_CreateRenderer(sTopWin, -1, SDL_RENDERER_ACCELERATED);
    sBotRen = SDL_CreateRenderer(sBotWin, -1, SDL_RENDERER_ACCELERATED);
    if (sTopWin && !sTopRen)
        sTopRen = SDL_CreateRenderer(sTopWin, -1, SDL_RENDERER_SOFTWARE);
    if (!sBotRen)
        sBotRen = SDL_CreateRenderer(sBotWin, -1, SDL_RENDERER_SOFTWARE);
    if ((sTopWin && !sTopRen) || !sBotRen) {
        fprintf(stderr, "renderer creation failed: %s\n", SDL_GetError());
        return false;
    }
    SDL_ShowCursor(SDL_DISABLE);
    sTouchFd = OpenBottomTouch();
    for (int i = 0, n = SDL_NumJoysticks(); i < n && i < 4; i++)
        if (SDL_IsGameController(i))
            sPads[i] = SDL_GameControllerOpen(i);
    return true;
}

void RgdsDisplay_Shutdown(void) {
    if (sTouchFd >= 0)
        close(sTouchFd);
    if (sTopTex) SDL_DestroyTexture(sTopTex);
    if (sBotTex) SDL_DestroyTexture(sBotTex);
    if (sTopRen) SDL_DestroyRenderer(sTopRen);
    if (sBotRen) SDL_DestroyRenderer(sBotRen);
    if (sTopWin) SDL_DestroyWindow(sTopWin);
    if (sBotWin) SDL_DestroyWindow(sBotWin);
    SDL_Quit();
}

static void Upload(SDL_Renderer* ren, SDL_Texture** tex, int* tw, int* th, const uint32_t* px, int w, int h, int pitch) {
    if (!*tex || *tw != w || *th != h) {
        if (*tex)
            SDL_DestroyTexture(*tex);
        *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, w, h);
        *tw = w;
        *th = h;
        if (!*tex)
            return;
    }
    SDL_UpdateTexture(*tex, NULL, px, pitch * 4);
}

void RgdsDisplay_PresentTop(const uint32_t* abgr, int w, int h, int pitch) {
    if (!sTopRen)
        return;
    Upload(sTopRen, &sTopTex, &sTopTexW, &sTopTexH, abgr, w, h, pitch);
    if (!sTopTex)
        return;
    int ow, oh;
    SDL_GetRendererOutputSize(sTopRen, &ow, &oh);
    /* Fit inside the panel keeping the GBA aspect. */
    SDL_Rect dst;
    if ((long)ow * h > (long)oh * w) {
        dst.h = oh;
        dst.w = oh * w / h;
    } else {
        dst.w = ow;
        dst.h = ow * h / w;
    }
    dst.x = (ow - dst.w) / 2;
    dst.y = (oh - dst.h) / 2;
    SDL_SetRenderDrawColor(sTopRen, 0, 0, 0, 255);
    SDL_RenderClear(sTopRen);
    SDL_RenderCopy(sTopRen, sTopTex, NULL, &dst);
    SDL_RenderPresent(sTopRen);
}

/* RGDS_DUMP_BOTTOM=path.bmp: overwrite the file at every bottom present, so the last frame is kept (tests). */
static void MaybeDumpBottom(const uint32_t* abgr, int w, int h, int pitch) {
    const char* path = getenv("RGDS_DUMP_BOTTOM");
    if (!path)
        return;
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom((void*)abgr, w, h, 32, pitch * 4, SDL_PIXELFORMAT_ABGR8888);
    if (s) {
        SDL_SaveBMP(s, path);
        SDL_FreeSurface(s);
    }
}

void RgdsDisplay_PresentBottom(const uint32_t* abgr, int w, int h, int pitch) {
    MaybeDumpBottom(abgr, w, h, pitch);
    Upload(sBotRen, &sBotTex, &sBotTexW, &sBotTexH, abgr, w, h, pitch);
    if (!sBotTex)
        return;
    SDL_RenderClear(sBotRen);
    SDL_RenderCopy(sBotRen, sBotTex, NULL, NULL); /* the second screen is 4:3, like the panel */
    SDL_RenderPresent(sBotRen);
    sBotDirty = false;
}

static uint32_t ButtonBit(SDL_GameControllerButton b) {
    switch (b) {
        /* Physical positions: the right-hand face button is GBA A, the one below it is GBA B. */
        case SDL_CONTROLLER_BUTTON_B: return K_A;
        case SDL_CONTROLLER_BUTTON_A: return K_B;
        case SDL_CONTROLLER_BUTTON_BACK: return K_SELECT;
        case SDL_CONTROLLER_BUTTON_START: return K_START;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return K_RIGHT;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return K_LEFT;
        case SDL_CONTROLLER_BUTTON_DPAD_UP: return K_UP;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return K_DOWN;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return K_R;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return K_L;
        default: return 0;
    }
}

static uint32_t KeyBit(SDL_Keycode k) {
    switch (k) {
        case SDLK_x: return K_A;
        case SDLK_z: return K_B;
        case SDLK_BACKSPACE: return K_SELECT;
        case SDLK_RETURN: return K_START;
        case SDLK_RIGHT: return K_RIGHT;
        case SDLK_LEFT: return K_LEFT;
        case SDLK_UP: return K_UP;
        case SDLK_DOWN: return K_DOWN;
        case SDLK_s: return K_R;
        case SDLK_a: return K_L;
        default: return 0;
    }
}

bool RgdsDisplay_PumpEvents(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                sQuit = true;
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_GUIDE)
                    sQuit = true;
                sHeld |= ButtonBit((SDL_GameControllerButton)e.cbutton.button);
                break;
            case SDL_CONTROLLERBUTTONUP:
                sHeld &= ~ButtonBit((SDL_GameControllerButton)e.cbutton.button);
                break;
            case SDL_CONTROLLERAXISMOTION:
                if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
                    sStickX = e.caxis.value / 32768.0f;
                else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
                    sStickY = -e.caxis.value / 32768.0f;
                break;
            case SDL_CONTROLLERDEVICEADDED:
                if (e.cdevice.which < 4 && !sPads[e.cdevice.which])
                    sPads[e.cdevice.which] = SDL_GameControllerOpen(e.cdevice.which);
                break;
            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE)
                    sQuit = true;
                if (!e.key.repeat)
                    sHeld |= KeyBit(e.key.keysym.sym);
                break;
            case SDL_KEYUP:
                sHeld &= ~KeyBit(e.key.keysym.sym);
                break;
            case SDL_FINGERDOWN:
                /* Fallback only: when evdev is open it already delivers the tap. */
                if (sTouchFd < 0 && (e.tfinger.windowID == sBotWinId || e.tfinger.windowID == 0))
                    Port_SecondScreen_3DS_OnTap((int)(e.tfinger.x * RGDS_PANEL_W), (int)(e.tfinger.y * RGDS_PANEL_H), 0);
                break;
            default:
                break;
        }
    }
    PumpTouch();
    sDownEdges |= sHeld & ~sPrevHeld;
    sPrevHeld = sHeld;
    return !sQuit;
}

uint16_t RgdsDisplay_KeyInput(void) {
    return (uint16_t)(~sHeld & 0x3FFu);
}

uint32_t RgdsDisplay_KeysDownMask(void) {
    const uint32_t d = sDownEdges;
    sDownEdges = 0;
    return d;
}

void RgdsDisplay_LeftStick(float* x, float* y) {
    if (x) *x = sStickX;
    if (y) *y = sStickY;
}
