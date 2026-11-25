/*
 * What the Two Towers renderer (nv2a_gpu_tt.c) expects from its own runtime
 * and ours does not have. Values are The Two Towers Recompiled's defaults
 * (video/gfx_menu.c, 81d11ad) with its title-specific extras off: no
 * Ultra texture pack, no options overlay, toasts or banners, no HUD moves.
 *
 * Environment (as in The Two Towers): RECOMP_RENDER_SCALE (1-12),
 * RECOMP_ANISO (1-16), RECOMP_MIPMAPS (0/1), RECOMP_AA (0 off, 1 fxaa,
 * 2/3/4 msaa 2/4/8, 5/6 msaa 4/8 + fxaa), RECOMP_VSYNC (0/1),
 * RECOMP_HW_VP=0 (vertex programs on the CPU), RECOMP_ASPECT (4:3 default).
 */
#if defined(_WIN32)
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "gpu_tt/video/gfx_menu.h"

/* ---- options (gfx_menu.c) ---- */

volatile long xbox_gfx_rev = 1;
volatile int xbox_frontend_hint = -1;

static int env_int(const char *name, int def, int lo, int hi)
{
    const char *e = getenv(name);
    int v;
    if (!e || !*e) return def;
    v = atoi(e);
    return v < lo ? lo : v > hi ? hi : v;
}

static int aa_index(void) { return env_int("RECOMP_AA", 0, 0, 6); }

float xbox_GfxScaleF(void)
{
    return (float)env_int("RECOMP_RENDER_SCALE", 1, 1, 12);
}

int xbox_GfxMsaa(void)
{
    static const int v[7] = { 1, 1, 2, 4, 8, 4, 8 };
    return v[aa_index()];
}

int xbox_GfxFxaa(void)
{
    int i = aa_index();
    return i == 1 || i == 5 || i == 6;
}

int xbox_GfxGet(int id)
{
    switch (id) {
    case GFX_SCALE: return env_int("RECOMP_RENDER_SCALE", 1, 1, 12);
    case GFX_ANISO: return env_int("RECOMP_ANISO", 1, 1, 16);
    case GFX_MIPS:  return env_int("RECOMP_MIPMAPS", 1, 0, 1);
    case GFX_AA:    return aa_index();
    case GFX_VSYNC: return env_int("RECOMP_VSYNC", 0, 0, 1);
    case GFX_VP:    return env_int("RECOMP_HW_VP", 1, 0, 1);
    case GFX_LOOKSTR: return 3;
    case GFX_HDRPAPER: return 1;
    default:        return 0;   /* fps 0, stretch off, HUD at edges, Ultra/E3/HDR off */
    }
}

float xbox_GfxGetF(int id)
{
    switch (id) {
    case GFX_BRIGHT:   return 1.0f;    /* "Original" */
    case GFX_COLOR:    return 1.0f;    /* "Original" */
    case GFX_LOOKSTR:  return 1.0f;    /* "Full" (no grading is chosen anyway) */
    case GFX_HDRPAPER: return 200.0f;
    default:           return 0.0f;    /* sharpening, LOD bias, movie sharpening */
    }
}

float xbox_GfxAspect(void)
{
    const char *e = getenv("RECOMP_ASPECT");
    if (e) {
        if (!strcmp(e, "16:9")) return 16.0f / 9.0f;
        if (!strcmp(e, "16:10")) return 16.0f / 10.0f;
        if (!strcmp(e, "21:9")) return 21.0f / 9.0f;
    }
    return 4.0f / 3.0f;
}

void xbox_GfxMenuSetAvailable(int on) { (void)on; }

int xbox_GfxMenuRender(int scw, int sch, const uint32_t **px, int *w, int *h, unsigned *ver)
{
    (void)scw; (void)sch; (void)px; (void)w; (void)h; (void)ver;
    return 0;
}

int xbox_ToastRender(int scw, int sch, const uint32_t **px, int *w, int *h, unsigned *ver, int *x, int *y)
{
    (void)scw; (void)sch; (void)px; (void)w; (void)h; (void)ver; (void)x; (void)y;
    return 0;
}

int xbox_BannerRender(int scw, int sch, const uint32_t **px, int *w, int *h, unsigned *ver, int *x, int *y)
{
    (void)scw; (void)sch; (void)px; (void)w; (void)h; (void)ver; (void)x; (void)y;
    return 0;
}

/* ---- runtime counters the renderer's reports read (not kept here) ---- */

volatile unsigned g_apu_periods, g_apu_short, g_apu_feheld;
volatile int g_apu_list_n[3], g_apu_list_active[3];
volatile uint32_t g_pb_runs;
volatile long long g_perf_pb_ticks, g_perf_rt_ticks;

int xbox_bridge_handles_used(void) { return 0; }
uint32_t xbox_ContiguousFreeBytes(void) { return 0; }

/* Depth uploads also need the real mapped extent for their bounds check. */
extern size_t xbox_GetMappedSize(void);
size_t xbox_GetMemorySize(void) { return xbox_GetMappedSize(); }
/* Only the renderer's capture replay (RECOMP_REPLAY) uses this. */
void xbox_ContiguousSetAllocatedBytes(uint32_t n) { (void)n; }

/* The renderer widens nothing while a film plays; ours has no such state
 * to report, and the picture stays 4:3. */
int xbox_MoviePlaying(void) { return 0; }

/* getenv with the answer kept: the renderer asks in its hot paths. Names
 * are string literals, so the pointer is the key. */
const char *recomp_cgetenv(const char *name)
{
    enum { N = 512, HOT = 256 };
    static const char *volatile keys[N];
    static const char *vals[N];
    static volatile LONG used;
    /* The renderer repeats a small group of queries in its draw loop.
     * A thread-local front cache keeps the shared process answers and avoids
     * scanning the full list on every use of a hot option. */
    static _Thread_local struct { const char *key, *value; } fast[HOT];
    uintptr_t address = (uintptr_t)name;
    unsigned slot = (unsigned)((address >> 3) ^ (address >> 11)) & (HOT - 1);
    if (fast[slot].key == name) return fast[slot].value;
    LONG i, n = used;
    const char *v;
    for (i = 0; i < n && i < N; i++) {
        if (keys[i] == name) {
            v = vals[i];
            goto found;
        }
    }
    v = getenv(name);
    i = InterlockedIncrement(&used) - 1;
    if (i < N) { vals[i] = v; MemoryBarrier(); keys[i] = name; }
found:
    fast[slot].value = v;
    fast[slot].key = name;
    return v;
}
/* Wait until the performance counter reaches deadline: sleep while it is
 * far, spin for the last stretch. */
void xbox_precise_wait_until(LONGLONG deadline)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER n;
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    for (;;) {
        LONGLONG left;
        QueryPerformanceCounter(&n);
        left = deadline - n.QuadPart;
        if (left <= 0) return;
        if (left * 1000 / f.QuadPart > 2) Sleep(1);
        else YieldProcessor();
    }
}
#endif /* _WIN32 */
