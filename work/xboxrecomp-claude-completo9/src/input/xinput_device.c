/**
 * Xbox Input Compatibility Layer
 *
 * Translates the Xbox controller API to a host gamepad backend.
 * Handles the structural differences between the Xbox gamepad
 * (analog buttons as bytes, separate trigger channels) and the host
 * (digital face buttons, trigger axes).
 *
 *   _WIN32 -> Windows XInput, plus SDL2 GameController for every other
 *             pad when built with XBOXRECOMP_SDL_PADS
 *   POSIX  -> SDL2 GameController
 */

#include "xinput_xbox.h"
#include "keyboard.h"
#include "mkb_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if !defined(_WIN32) || defined(XBOXRECOMP_SDL_PADS)
#define SDL_MAIN_HANDLED
#include <SDL.h>

/* One SDL game controller read as an Xbox pad. SDL names the buttons by
 * position (A is the bottom face button on any pad), which is also where
 * the Xbox pad has them. */
static void sdl_pad_read(SDL_GameController *c, DWORD packet,
                         XBOX_INPUT_STATE *pState)
{
    WORD btn = 0;

    memset(pState, 0, sizeof(XBOX_INPUT_STATE));
    pState->dwPacketNumber = packet;

    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP))    btn |= XBOX_GAMEPAD_DPAD_UP;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  btn |= XBOX_GAMEPAD_DPAD_DOWN;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  btn |= XBOX_GAMEPAD_DPAD_LEFT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) btn |= XBOX_GAMEPAD_DPAD_RIGHT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START))      btn |= XBOX_GAMEPAD_START;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK))       btn |= XBOX_GAMEPAD_BACK;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSTICK))  btn |= XBOX_GAMEPAD_LEFT_THUMB;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSTICK)) btn |= XBOX_GAMEPAD_RIGHT_THUMB;
    pState->Gamepad.wButtons = btn;

    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_B] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_X] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_X) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_Y] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_Y) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_BLACK] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_WHITE] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ? 255 : 0;

    /* SDL trigger axes are 0..32767 -> Xbox analog button 0..255 */
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] =
        (BYTE)(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7);
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] =
        (BYTE)(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7);

    /* SDL Y axis points down; the Xbox Y axis points up -- invert.
     * Use (-1 - v) so v = -32768 does not overflow SHORT. */
    pState->Gamepad.sThumbLX = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    pState->Gamepad.sThumbLY =
        (SHORT)(-1 - SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY));
    pState->Gamepad.sThumbRX = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX);
    pState->Gamepad.sThumbRY =
        (SHORT)(-1 - SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY));
}
#endif

/* Merge the keyboard on top of a pad on port 0 rather than only standing in
 * for a missing one.
 *
 * The first version put the keyboard behind XInput's failure, on the
 * assumption that with nothing plugged in the call would fail. It does
 * not: under Wine XInputGetState returns ERROR_SUCCESS and a gamepad
 * with every button at rest, so the fallback was unreachable and
 * pressing a key did nothing at all.
 *
 * Merging is also the better rule. A real pad keeps working -- its
 * buttons are already in pState and the keyboard only adds to them --
 * and there is no special case left to get wrong. */
static void keyboard_merge(XBOX_INPUT_STATE *pState)
{
    XBOX_INPUT_STATE kb;
    int i;

    if (!xbox_InputKeyboardPoll(&kb))
        return;
    pState->Gamepad.wButtons |= kb.Gamepad.wButtons;
    for (i = 0; i < 8; i++)
        if (kb.Gamepad.bAnalogButtons[i] > pState->Gamepad.bAnalogButtons[i])
            pState->Gamepad.bAnalogButtons[i] = kb.Gamepad.bAnalogButtons[i];
    if (kb.Gamepad.sThumbLX) pState->Gamepad.sThumbLX = kb.Gamepad.sThumbLX;
    if (kb.Gamepad.sThumbLY) pState->Gamepad.sThumbLY = kb.Gamepad.sThumbLY;
    if (kb.Gamepad.sThumbRX) pState->Gamepad.sThumbRX = kb.Gamepad.sThumbRX;
    if (kb.Gamepad.sThumbRY) pState->Gamepad.sThumbRY = kb.Gamepad.sThumbRY;
    /* The input layer records edges, so an unchanged packet number is
     * read as the same state and the press never happens. */
    pState->dwPacketNumber = kb.dwPacketNumber;
}

/* ======================================================================== */
#if defined(_WIN32)
/* ====================  XInput backend  ================================== */
/* ======================================================================== */

#include <xinput.h>
#pragma comment(lib, "xinput.lib")

static BOOL  g_controller_connected[XBOX_MAX_CONTROLLERS] = { FALSE };
static DWORD g_last_packet[XBOX_MAX_CONTROLLERS] = { 0 };

/* The keyboard on port 0 (RECOMP_KEYBOARD=1) comes from keyboard.c, shared
 * with the SDL backend; the GDI window in fb_present.c records the keys. */

#if defined(XBOXRECOMP_SDL_PADS)
/* Every other pad -- PlayStation, Switch, DirectInput pads -- through SDL2's
 * game controller layer and its mapping database. XInput keeps the Xbox
 * pads: SDL sees those as well, so it skips them by type here, and the
 * other pads take the ports XInput leaves empty, in the order they were
 * plugged in. RECOMP_SDL_PADS=0 turns this off. A gamecontrollerdb.txt
 * next to the exe adds mappings for pads SDL does not know; a pad without
 * one is named once on stderr so it can be added.
 *
 * Nothing calls xbox_InputInit, so SDL comes up on the first poll, and all
 * SDL calls go through one lock: the USB pad and the rumble path poll from
 * the emulation threads. */
static SRWLOCK g_sdl_lock = SRWLOCK_INIT;
static int g_sdl_state;                       /* 0 not tried, 1 up, -1 off */
static SDL_GameController *g_sdl_pads[XBOX_MAX_CONTROLLERS];
static DWORD g_sdl_packet[XBOX_MAX_CONTROLLERS];
static DWORD g_sdl_scanned;                   /* GetTickCount of last scan */
static SDL_JoystickID g_sdl_unmapped[16];     /* already named on stderr */
static int g_sdl_nunmapped;
static BOOL g_xinput_has[XBOX_MAX_CONTROLLERS];

static int sdl_is_xinput_pad(int index)
{
    SDL_GameControllerType t = SDL_GameControllerTypeForIndex(index);
    return t == SDL_CONTROLLER_TYPE_XBOX360 || t == SDL_CONTROLLER_TYPE_XBOXONE;
}

static int sdl_is_open(SDL_JoystickID id)
{
    for (int s = 0; s < XBOX_MAX_CONTROLLERS; s++)
        if (g_sdl_pads[s] && SDL_JoystickInstanceID(
                SDL_GameControllerGetJoystick(g_sdl_pads[s])) == id)
            return 1;
    return 0;
}

static void sdl_name_unmapped(int index)
{
    SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(index);
    char guid[33];

    for (int i = 0; i < g_sdl_nunmapped; i++)
        if (g_sdl_unmapped[i] == id)
            return;
    if (g_sdl_nunmapped < (int)(sizeof g_sdl_unmapped / sizeof g_sdl_unmapped[0]))
        g_sdl_unmapped[g_sdl_nunmapped++] = id;
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof guid);
    fprintf(stderr, "[INPUT] mando sin mapeo, no se usa: %s (GUID %s); "
            "anadelo a gamecontrollerdb.txt junto al exe\n",
            SDL_JoystickNameForIndex(index) ? SDL_JoystickNameForIndex(index) : "?",
            guid);
}

/* Drop the pads that went away, keeping the rest in order, then open the
 * new ones. */
static void sdl_pads_scan(void)
{
    int kept = 0, n;

    for (int s = 0; s < XBOX_MAX_CONTROLLERS; s++) {
        SDL_GameController *c = g_sdl_pads[s];
        g_sdl_pads[s] = NULL;
        if (!c)
            continue;
        if (SDL_GameControllerGetAttached(c)) {
            g_sdl_pads[kept++] = c;
        } else {
            fprintf(stderr, "[INPUT] mando generico desconectado\n");
            SDL_GameControllerClose(c);
        }
    }

    n = SDL_NumJoysticks();
    for (int i = 0; i < n && kept < XBOX_MAX_CONTROLLERS; i++) {
        if (sdl_is_xinput_pad(i) || sdl_is_open(SDL_JoystickGetDeviceInstanceID(i)))
            continue;
        if (!SDL_IsGameController(i)) {
            sdl_name_unmapped(i);
            continue;
        }
        g_sdl_pads[kept] = SDL_GameControllerOpen(i);
        if (g_sdl_pads[kept]) {
            const char *name = SDL_GameControllerName(g_sdl_pads[kept]);
            fprintf(stderr, "[INPUT] mando generico %d: %s\n", kept,
                    name ? name : "?");
            kept++;
        }
    }
    g_sdl_scanned = GetTickCount();
}

static int sdl_pads_up(void)
{
    char db[MAX_PATH + 32];
    const char *env;
    DWORD len;

    if (g_sdl_state)
        return g_sdl_state > 0;
    g_sdl_state = -1;
    env = getenv("RECOMP_SDL_PADS");
    if (env && env[0] == '0')
        return 0;

    SDL_SetMainReady();
    /* Device arrival on SDL's own thread; raw input and WGI need a window
     * message loop on the polling thread, which the emulation threads do
     * not run, so pads come through XInput, HIDAPI and DirectInput. */
    SDL_SetHint(SDL_HINT_JOYSTICK_THREAD, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "[INPUT] SDL2 no arranca, solo mandos XInput: %s\n",
                SDL_GetError());
        return 0;
    }
    /* Nothing reads SDL's event queue: poll state instead of queueing. */
    SDL_JoystickEventState(SDL_IGNORE);
    SDL_GameControllerEventState(SDL_IGNORE);

    len = GetModuleFileNameA(NULL, db, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        char *slash = strrchr(db, '\\');
        if (slash) {
            int added;
            /* db has 32 bytes past MAX_PATH for the name. */
            memcpy(slash + 1, "gamecontrollerdb.txt", sizeof "gamecontrollerdb.txt");
            added = SDL_GameControllerAddMappingsFromFile(db);
            if (added > 0)
                fprintf(stderr, "[INPUT] %d mapeos de %s\n", added, db);
        }
    }

    g_sdl_state = 1;
    sdl_pads_scan();
    return 1;
}

/* The generic pad for an Xbox port: the k-th one, k being the number of
 * lower ports XInput left empty. */
static SDL_GameController *sdl_pad_for_port(DWORD dwPort, DWORD *slot)
{
    DWORD k = 0;

    for (DWORD j = 0; j < dwPort; j++)
        if (!g_xinput_has[j])
            k++;
    *slot = k;
    return k < XBOX_MAX_CONTROLLERS ? g_sdl_pads[k] : NULL;
}

static int sdl_pads_get_state(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    SDL_GameController *c;
    DWORD slot;
    int ok = 0;

    AcquireSRWLockExclusive(&g_sdl_lock);
    if (sdl_pads_up()) {
        SDL_GameControllerUpdate();
        if (GetTickCount() - g_sdl_scanned >= 1000)
            sdl_pads_scan();
        c = sdl_pad_for_port(dwPort, &slot);
        if (c && SDL_GameControllerGetAttached(c)) {
            sdl_pad_read(c, ++g_sdl_packet[slot], pState);
            ok = 1;
        }
    }
    ReleaseSRWLockExclusive(&g_sdl_lock);
    return ok;
}

static DWORD sdl_pads_set_state(DWORD dwPort, const XBOX_VIBRATION *pVibration)
{
    SDL_GameController *c;
    DWORD slot, rc = ERROR_DEVICE_NOT_CONNECTED;

    AcquireSRWLockExclusive(&g_sdl_lock);
    if (g_sdl_state > 0) {
        c = sdl_pad_for_port(dwPort, &slot);
        /* SDL rumble needs a duration; the game refreshes it continuously. */
        if (c && SDL_GameControllerRumble(c, pVibration->wLeftMotorSpeed,
                                          pVibration->wRightMotorSpeed, 1000) == 0)
            rc = ERROR_SUCCESS;
    }
    ReleaseSRWLockExclusive(&g_sdl_lock);
    return rc;
}

static int sdl_pads_present(DWORD dwPort)
{
    DWORD slot;
    int on;

    AcquireSRWLockExclusive(&g_sdl_lock);
    on = g_sdl_state > 0 && sdl_pad_for_port(dwPort, &slot) != NULL;
    ReleaseSRWLockExclusive(&g_sdl_lock);
    return on;
}
#endif /* XBOXRECOMP_SDL_PADS */

void xbox_InputInit(void)
{
    for (DWORD i = 0; i < XBOX_MAX_CONTROLLERS; i++) {
        XINPUT_STATE state;
        DWORD result = XInputGetState(i, &state);
        g_controller_connected[i] = (result == ERROR_SUCCESS);
    }
}

static DWORD input_get_state(DWORD dwPort, XBOX_INPUT_STATE *pState);

/* Claude completo-4 (2026-11-05), options for a real pad (XInput or SDL),
 * applied before the keyboard and mouse are merged on top. All are off unless
 * set, and then the pad reads exactly as before.
 *   RECOMP_PAD_DEADZONE=N        1-50: a round dead zone of N % on each stick;
 *                                past it the travel is rescaled, so a stick
 *                                pushed all the way still reads all the way.
 *   RECOMP_PAD_INVERT=lx,ly,rx,ry  any of them: flips those axes.
 *   RECOMP_PAD_SWAP_STICKS=1     the left and right sticks trade places. */
static SHORT pad_neg(SHORT v)
{
    return v == -32768 ? 32767 : (SHORT)-v;
}

static SHORT pad_clamp(double v)
{
    return (SHORT)(v > 32767.0 ? 32767 : v < -32768.0 ? -32768 : (long)v);
}

static void pad_deadzone(SHORT *x, SHORT *y, int pct)
{
    double fx = *x / 32767.0, fy = *y / 32767.0;
    double r = sqrt(fx * fx + fy * fy), d = pct / 100.0, s;
    if (r <= d) {
        *x = 0;
        *y = 0;
        return;
    }
    s = (r > 1.0 ? 1.0 : (r - d) / (1.0 - d)) / r;
    *x = pad_clamp(fx * s * 32767.0);
    *y = pad_clamp(fy * s * 32767.0);
}

static void pad_adjust(XBOX_GAMEPAD *g)
{
    static volatile int init;
    static int dz, inv, swap;
    if (!init) {
        const char *e = getenv("RECOMP_PAD_DEADZONE");
        dz = e ? atoi(e) : 0;
        if (dz < 0) dz = 0;
        if (dz > 50) dz = 50;
        e = getenv("RECOMP_PAD_INVERT");
        inv = 0;
        if (e) {
            if (strstr(e, "lx")) inv |= 1;
            if (strstr(e, "ly")) inv |= 2;
            if (strstr(e, "rx")) inv |= 4;
            if (strstr(e, "ry")) inv |= 8;
        }
        e = getenv("RECOMP_PAD_SWAP_STICKS");
        swap = e && *e == '1';
        if (dz || inv || swap)
            fprintf(stderr, "  [PAD] options: dead zone %d %%, inverted axes 0x%X, sticks %s\n",
                    dz, inv, swap ? "swapped" : "as they are");
        init = 1;
    }
    if (!dz && !inv && !swap)
        return;
    if (dz) {
        pad_deadzone(&g->sThumbLX, &g->sThumbLY, dz);
        pad_deadzone(&g->sThumbRX, &g->sThumbRY, dz);
    }
    if (inv & 1) g->sThumbLX = pad_neg(g->sThumbLX);
    if (inv & 2) g->sThumbLY = pad_neg(g->sThumbLY);
    if (inv & 4) g->sThumbRX = pad_neg(g->sThumbRX);
    if (inv & 8) g->sThumbRY = pad_neg(g->sThumbRY);
    if (swap) {
        SHORT t;
        t = g->sThumbLX; g->sThumbLX = g->sThumbRX; g->sThumbRX = t;
        t = g->sThumbLY; g->sThumbLY = g->sThumbRY; g->sThumbRY = t;
    }
}

/* Claude completo-4 (2026-11-05), measure. With RECOMP_FRAMETIME set, the
 * first time a read of the pad shows a change (a button, an analogue button,
 * or a stick moving by more than a tenth of its range) its performance
 * counter is kept; the frame-time measure takes it at the next picture it
 * sends to the window and writes the gap as the frame's input_ms. That is the
 * time from the port seeing the change to the next picture, not the whole
 * chain (the game may answer a frame or two later); read with that in mind.
 * The pad is only read here, never written. */
volatile LONG64 g_input_change_qpc;

DWORD xbox_InputGetState(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    static int on = -1;
    static XBOX_GAMEPAD last[XBOX_MAX_CONTROLLERS];
    DWORD r = input_get_state(dwPort, pState);
    if (on < 0) {
        const char *e = getenv("RECOMP_FRAMETIME");
        on = e && *e && *e != '0';
    }
    if (on && r == ERROR_SUCCESS && dwPort < XBOX_MAX_CONTROLLERS && pState) {
        const XBOX_GAMEPAD *g = &pState->Gamepad, *o = &last[dwPort];
        int moved = g->wButtons != o->wButtons
            || memcmp(g->bAnalogButtons, o->bAnalogButtons,
                      sizeof g->bAnalogButtons)
            || abs(g->sThumbLX - o->sThumbLX) > 3277
            || abs(g->sThumbLY - o->sThumbLY) > 3277
            || abs(g->sThumbRX - o->sThumbRX) > 3277
            || abs(g->sThumbRY - o->sThumbRY) > 3277;
        if (moved) {
            LARGE_INTEGER c;
            last[dwPort] = *g;
            QueryPerformanceCounter(&c);
            InterlockedCompareExchange64(&g_input_change_qpc, c.QuadPart, 0);
        }
    }
    return r;
}

static DWORD input_get_state(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    XINPUT_STATE xi_state;
    DWORD result;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;

    result = XInputGetState(dwPort, &xi_state);
#if defined(XBOXRECOMP_SDL_PADS)
    g_xinput_has[dwPort] = (result == ERROR_SUCCESS);
    if (result != ERROR_SUCCESS && sdl_pads_get_state(dwPort, pState)) {
        g_controller_connected[dwPort] = TRUE;
        pad_adjust(&pState->Gamepad);
        if (dwPort == 0) {
            keyboard_merge(pState);
            mkb_merge(dwPort, pState);
        }
        return ERROR_SUCCESS;
    }
#endif
    if (result != ERROR_SUCCESS) {
        g_controller_connected[dwPort] = FALSE;
        if (dwPort == 0) {
            /* Keyboard and mouse stand in for an absent pad, so the game is
             * playable with no controller plugged in. */
            int kb;
            memset(pState, 0, sizeof *pState);
            kb = xbox_InputKeyboardPoll(pState);
            if (mkb_merge(dwPort, pState))
                g_controller_connected[dwPort] = TRUE;
            if (kb || mkb_enabled())
                return ERROR_SUCCESS;
        }
        return result;
    }

    g_controller_connected[dwPort] = TRUE;
    g_last_packet[dwPort] = xi_state.dwPacketNumber;

    memset(pState, 0, sizeof(XBOX_INPUT_STATE));
    pState->dwPacketNumber = xi_state.dwPacketNumber;
    pState->Gamepad.wButtons = xi_state.Gamepad.wButtons & 0x00FF;

    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_A) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_B] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_X] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_X) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_Y] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_BLACK] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_WHITE] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] = xi_state.Gamepad.bLeftTrigger;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] = xi_state.Gamepad.bRightTrigger;

    pState->Gamepad.sThumbLX = xi_state.Gamepad.sThumbLX;
    pState->Gamepad.sThumbLY = xi_state.Gamepad.sThumbLY;
    pState->Gamepad.sThumbRX = xi_state.Gamepad.sThumbRX;
    pState->Gamepad.sThumbRY = xi_state.Gamepad.sThumbRY;
    pad_adjust(&pState->Gamepad);

    if (dwPort == 0)
        keyboard_merge(pState);

    /* Mouse+keyboard merges on top of a real pad too: the mouse owns the
     * aim stick while it is moving, WASD owns movement while held. */
    if (dwPort == 0)
        mkb_merge(dwPort, pState);

    return ERROR_SUCCESS;
}

DWORD xbox_InputSetState(DWORD dwPort, const XBOX_VIBRATION *pVibration)
{
    XINPUT_VIBRATION xi_vib;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pVibration)
        return ERROR_DEVICE_NOT_CONNECTED;

    xi_vib.wLeftMotorSpeed = pVibration->wLeftMotorSpeed;
    xi_vib.wRightMotorSpeed = pVibration->wRightMotorSpeed;
#if defined(XBOXRECOMP_SDL_PADS)
    if (!g_xinput_has[dwPort])
        return sdl_pads_set_state(dwPort, pVibration);
#endif
    return XInputSetState(dwPort, &xi_vib);
}

BOOL xbox_InputIsConnected(DWORD dwPort)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS) return FALSE;
    return g_controller_connected[dwPort];
}

DWORD xbox_InputGetCapabilities(DWORD dwPort, DWORD dwFlags, XBOX_INPUT_CAPABILITIES *pCaps)
{
    XINPUT_CAPABILITIES xi_caps;
    DWORD result;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pCaps)
        return ERROR_DEVICE_NOT_CONNECTED;

    result = XInputGetCapabilities(dwPort, dwFlags, &xi_caps);
#if defined(XBOXRECOMP_SDL_PADS)
    if (result != ERROR_SUCCESS && sdl_pads_present(dwPort)) {
        memset(pCaps, 0, sizeof(XBOX_INPUT_CAPABILITIES));
        pCaps->Type    = 1;   /* XINPUT_DEVTYPE_GAMEPAD */
        pCaps->SubType = 1;   /* XINPUT_DEVSUBTYPE_GAMEPAD */
        return ERROR_SUCCESS;
    }
#endif
    if (result != ERROR_SUCCESS) return result;

    memset(pCaps, 0, sizeof(XBOX_INPUT_CAPABILITIES));
    pCaps->Type = xi_caps.Type;
    pCaps->SubType = xi_caps.SubType;
    pCaps->Flags = xi_caps.Flags;
    return ERROR_SUCCESS;
}

/* ======================================================================== */
#else /* !_WIN32 */
/* ====================  SDL2 GameController backend  ===================== */
/* ======================================================================== */

static SDL_GameController *g_pads[XBOX_MAX_CONTROLLERS];
static BOOL  g_controller_connected[XBOX_MAX_CONTROLLERS];
static DWORD g_packet[XBOX_MAX_CONTROLLERS];

/* Set while the framebuffer window's main thread pumps SDL events
 * (src/video/fb_present_sdl.c). The pump already updates the controllers,
 * and SDL_GameControllerUpdate pumps events itself, which on macOS must only
 * happen on the main thread; so then this side only reads them. */
static volatile int g_main_pump;

void xbox_InputSetMainPump(int on)
{
    g_main_pump = on;
}

/* Whether any controller is open: SDL must stay up for it at shutdown. */
int xbox_InputAnyOpen(void)
{
    for (int i = 0; i < XBOX_MAX_CONTROLLERS; i++)
        if (g_pads[i])
            return 1;
    return 0;
}

/* Open up to XBOX_MAX_CONTROLLERS attached game controllers. */
static void open_controllers(void)
{
    int slot = 0;
    for (int i = 0; i < SDL_NumJoysticks() && slot < XBOX_MAX_CONTROLLERS; i++) {
        if (!SDL_IsGameController(i))
            continue;
        if (!g_pads[slot]) {
            g_pads[slot] = SDL_GameControllerOpen(i);
            g_controller_connected[slot] = (g_pads[slot] != NULL);
        }
        slot++;
    }
}

void xbox_InputInit(void)
{
    /* Keep the default SIGINT/SIGTERM: SDL would turn them into an SDL_QUIT
     * that nothing reads without a window, and Ctrl-C would stop working. */
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER))
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    open_controllers();
}

DWORD xbox_InputGetState(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;

    SDL_GameController *c = g_pads[dwPort];
    if (!c || !SDL_GameControllerGetAttached(c)) {
        g_controller_connected[dwPort] = FALSE;
        if (dwPort == 0 && xbox_InputKeyboardPoll(pState))
            return ERROR_SUCCESS;
        return ERROR_DEVICE_NOT_CONNECTED;
    }

    if (!g_main_pump)
        SDL_GameControllerUpdate();
    g_controller_connected[dwPort] = TRUE;

    sdl_pad_read(c, ++g_packet[dwPort], pState);

    if (dwPort == 0)
        keyboard_merge(pState);
    return ERROR_SUCCESS;
}

DWORD xbox_InputSetState(DWORD dwPort, const XBOX_VIBRATION *pVibration)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pVibration)
        return ERROR_DEVICE_NOT_CONNECTED;

    SDL_GameController *c = g_pads[dwPort];
    if (!c) return ERROR_DEVICE_NOT_CONNECTED;

    /* SDL rumble needs a duration; refresh for ~1s on each call (the game
     * polls vibration continuously). */
    SDL_GameControllerRumble(c, pVibration->wLeftMotorSpeed,
                             pVibration->wRightMotorSpeed, 1000);
    return ERROR_SUCCESS;
}

BOOL xbox_InputIsConnected(DWORD dwPort)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS) return FALSE;
    return g_controller_connected[dwPort];
}

DWORD xbox_InputGetCapabilities(DWORD dwPort, DWORD dwFlags, XBOX_INPUT_CAPABILITIES *pCaps)
{
    (void)dwFlags;
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pCaps)
        return ERROR_DEVICE_NOT_CONNECTED;
    if (!g_pads[dwPort])
        return ERROR_DEVICE_NOT_CONNECTED;

    memset(pCaps, 0, sizeof(XBOX_INPUT_CAPABILITIES));
    pCaps->Type    = 1;   /* XINPUT_DEVTYPE_GAMEPAD */
    pCaps->SubType = 1;   /* XINPUT_DEVSUBTYPE_GAMEPAD */
    pCaps->Flags   = 0;
    return ERROR_SUCCESS;
}

#endif /* _WIN32 */
