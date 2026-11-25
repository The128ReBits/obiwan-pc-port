/*
 * Direct3D 11 renderer for the pushbuffer executor, from The Two Towers
 * Recompiled (github.com/Gumpolas/TooTawersRecomp, commit 81d11ad, MIT;
 * "The Two Towers Recompiled contributors", on xboxrecomp 0.12.0 by sp00nz,
 * MIT). The files are kept as they are in gpu_tt/ with their licences; this
 * unit compiles them as one module beside our v0.16 executor.
 *
 * Split of work (hybrid):
 *   - nv2a_pb_exec.c (v0.16) keeps everything that synchronises the title
 *     with the GPU: the semaphore, the KickOff latch (subch 5 0x0310), the
 *     NOP swap interval, the flip methods, the flip hold and frame counters.
 *   - Every other method is handed to tt_pb_exec_method(), which draws with
 *     Direct3D 11 (vertex programs and combiners on the GPU).
 *   - At FLIP_STALL v0.16 calls nv2a_gpu_tt_present() instead of copying the
 *     guest framebuffer to the window.
 *
 * RECOMP_RENDERER=software (or a machine without D3D 11_0) leaves the v0.16
 * software path exactly as before.
 */
#if defined(_WIN32)

#ifdef _MSC_VER
/* The Two Towers builds with clang/MinGW; these are its only GNU spellings. */
#define __attribute__(x)
#define typeof __typeof__
#pragma warning(disable : 4013 4018 4024 4047 4057 4100 4101 4113 4127 4133 4189 4204 4244 4245 4267 4305 4389 4456 4457 4459 4701 4703 4706)
#endif

/* Our executor owns these two names; theirs become tt_*. */
#define nv2a_pb_exec_method tt_pb_exec_method
#define nv2a_pb_exec_report tt_pb_exec_report

#include "gpu_tt/kernel/nv2a_pb_exec.c"

#undef nv2a_pb_exec_method
#undef nv2a_pb_exec_report

/* Claude completo-4: RECOMP_CAPTURA_FRAME=1 (or =<folder>), off by default.
 * Turns on the frame inspector that is already here (F12 in the game window,
 * see fb_present.c; F9 still works but also toggles MKB's mouse capture:
 * the next frame's draws as [FDRAW]/[FSTAGE] lines in the log, every texture
 * stage's registers, and the finished picture as inspect-<pid>-<frame>.bmp)
 * by giving it a folder: RECOMP_GPU_INSPECT_DIR, read by fb_present.c at the
 * key press and by nv2a_d3d11.inc when it writes the picture. With =1 the
 * folder is %LOCALAPPDATA%\ObiWanRecomp-dev\capturas. An explicit
 * RECOMP_GPU_INSPECT_DIR wins. */
static void captura_frame_setup(void)
{
    const wchar_t *v = _wgetenv(L"RECOMP_CAPTURA_FRAME");
    wchar_t dir[600];
    if (!v || !v[0] || !wcscmp(v, L"0") || _wgetenv(L"RECOMP_GPU_INSPECT_DIR")) return;
    if (!wcscmp(v, L"1")) {
        const wchar_t *la = _wgetenv(L"LOCALAPPDATA");
        if (!la) return;
        _snwprintf(dir, 600, L"%ls\\ObiWanRecomp-dev", la); dir[599] = 0;
        CreateDirectoryW(dir, NULL);
        _snwprintf(dir, 600, L"%ls\\ObiWanRecomp-dev\\capturas", la);
    } else _snwprintf(dir, 600, L"%ls", v);
    dir[599] = 0;
    CreateDirectoryW(dir, NULL);
    if (_wputenv_s(L"RECOMP_GPU_INSPECT_DIR", dir) == 0)
        fprintf(stderr, "  [CAPTURA] frame capture ready: F12 in the game window captures the next frame "
                        "(draw list in this log, picture in %s)\n",
                wcscmp(v, L"1") ? "the RECOMP_CAPTURA_FRAME folder" : "%LOCALAPPDATA%\\ObiWanRecomp-dev\\capturas");
}

/* 1: the Direct3D 11 renderer draws. Decided once, at the first method. */
int nv2a_gpu_tt_active(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RECOMP_RENDERER");
        on = (e && !_stricmp(e, "software")) ? 0 : gpu_on();
        fprintf(stderr, "  [GPU-TT] renderer: %s\n",
                on ? "Direct3D 11 (The Two Towers Recompiled)" : "software (v0.16)");
        fflush(stderr);
        if (on) captura_frame_setup();
    }
    return on;
}

/* A method this executor keeps (a flip, the semaphore, ...): what TT would
 * do before it -- send the draws it has gathered. */
void nv2a_gpu_tt_note(uint32_t subch, uint32_t method, uint32_t param)
{
    if (pb_note_change1(subch, method, param))
        gpu_mb_flush();
}

extern int xbox_FramebufferTakeGpuInspect(void);

/* (Claude completo-9) pushbuffer segments the title kicked off (xbox_memory_layout.c) */
volatile long g_pb_segments;

static void gpu_report_timing(void)
{
    static int enabled = -1;
    if (enabled < 0) {
        const char *e = getenv("RECOMP_GPU_TIMING");
        enabled = e && e[0] == '1';
    }
    if (!enabled) return;
    extern volatile long long g_perf_rt_ticks;
    static LARGE_INTEGER first, hz;
    static long long walks, present;
    static unsigned frames, draws, native_draws;
    static long segs;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (!first.QuadPart) {
        QueryPerformanceFrequency(&hz);
        first = now;
        walks = g_perf_rt_ticks;
        present = g_perf_present_ticks;
        draws = s_3d.batches;
        native_draws = g_hwvp_stat_hw;
        segs = g_pb_segments;
        return;
    }
    ++frames;
    if (now.QuadPart - first.QuadPart < hz.QuadPart) return;
    double ms_per_tick = 1000.0 / (double)hz.QuadPart;
    fprintf(stderr, "  [PORTPERF] pid %lu ms %.1f frames %u batch %u hwvp %u walk_ms %.2f present_ms %.2f segs %ld\n",
            (unsigned long)GetCurrentProcessId(), (now.QuadPart - first.QuadPart) * ms_per_tick, frames,
            s_3d.batches - draws, g_hwvp_stat_hw - native_draws, (g_perf_rt_ticks - walks) * ms_per_tick,
            (g_perf_present_ticks - present) * ms_per_tick, g_pb_segments - segs);
    {   /* (Claude completo-7) RECOMP_PERFRT=1: the render thread's draw phases. Their
         * [PERFRT] line lives in NV097_FLIP_STALL, which this title never sends, so the
         * counters were gathered and never shown. ms per second, rdtsc calibrated against
         * the performance counter over the same second. */
        static unsigned long long tsc0;
        unsigned long long tsc = __rdtsc();
        if (g_pt_on > 0 && tsc0) {
            double f = (now.QuadPart - first.QuadPart) * ms_per_tick / (double)(tsc - tsc0);
            fprintf(stderr, "  [PORTPERF-RT] ms/s: setup %.1f targets %.1f indices %.1f fetch %.1f raw %.1f vpconst %.1f stages %.1f (tex %.1f) submit %.1f batch %.1f flush %.1f consts %.1f inline %.1f; raw draws %u old draws %u\n",
                    g_pt[PT_SETUP] * f, g_pt[PT_TARGETS] * f, g_pt[PT_WALK] * f, g_pt[PT_FETCH] * f, g_pt[PT_RAW] * f,
                    g_pt[PT_VPC] * f, g_pt[PT_STAGES] * f, g_pt[PT_TEX] * f, g_pt[PT_SUBMIT] * f, g_pt[PT_BATCH] * f,
                    g_pt[PT_FLUSH] * f, g_pt[PT_CONSTS] * f, g_pt[PT_INLINE] * f, g_pt_raw_n, g_pt_old_n);
            memset(g_pt, 0, sizeof g_pt); g_pt_raw_n = g_pt_old_n = 0;
        }
        tsc0 = tsc;
    }
    first = now;
    frames = 0;
    walks = g_perf_rt_ticks;
    present = g_perf_present_ticks;
    draws = s_3d.batches;
    native_draws = g_hwvp_stat_hw;
    segs = g_pb_segments;
}

/* ---------------------------------------------------------------------
 * Claude completo-4: RECOMP_FRAMETIME=1 (a measure, off by default).
 *
 * One CSV row per presented frame, a [FRAMETIME] summary every 10 s and,
 * at the first frame, what the window and the monitor are. The relaunched
 * process inherits the environment (kernel_bridge.c, CreateProcessW with a
 * NULL environment), so it measures too; each process writes its own file,
 * named by its pid.
 *
 * Columns: frame, t_ms (since the first frame), dt_ms (since the last
 * present returned), vblanks (60 Hz guest vblanks in that time), walk_ms
 * (pushbuffer walking, g_perf_rt_ticks), present_ms (frame_limit + Present),
 * gate_ms (dispatch gate held; -1 unless RECOMP_GATE_STATS=1), gpu_waits
 * (title waits on the GPU), input_ms (the first pad change the game read
 * since the last frame, to this present returning: a lower bound of input
 * latency -- the display's scan-out is not in it; -1 with no change), and
 * the title's last swap NOP (interval 1..3, immediate 0/1).
 * RECOMP_FRAMETIME_DIR=<folder> chooses where; otherwise
 * %LOCALAPPDATA%\ObiWanRecomp-dev\logs.
 * --------------------------------------------------------------------- */
#include <dwmapi.h>
extern uint32_t xbox_VblankCount(void);
extern long long xbox_GateHeldTotalNs(void);
extern long xbox_GpuWaitCount(void);
extern volatile LONG64 g_input_change_qpc;
extern volatile uint32_t g_nv2a_last_swap;

static int ft_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RECOMP_FRAMETIME");
        on = e && e[0] && strcmp(e, "0") != 0;
    }
    return on;
}

#define FT_WIN 16384
static FILE *ft_csv;
static double ft_ms;                       /* ms per QPC tick */
static LARGE_INTEGER ft_first, ft_prev, ft_win_t;
static uint32_t ft_vbl_prev, ft_win_vbl;
static long long ft_walk_prev, ft_pres_prev, ft_gate_prev;
static long ft_wait_prev;
static unsigned ft_n;
static float ft_win_dt[FT_WIN];
static unsigned ft_win_n, ft_win_over33, ft_win_over50;
static double ft_win_walk, ft_win_pres, ft_win_gate, ft_in_sum;
static float ft_in_max;
static unsigned ft_in_n, ft_win_wait;

static void ft_say(const char *line)
{
    fprintf(stderr, "  [FRAMETIME] %s\n", line);
    if (ft_csv) fprintf(ft_csv, "# %s\n", line);
}

static void ft_open(void)
{
    wchar_t dir[600], path[700];
    const wchar_t *d = _wgetenv(L"RECOMP_FRAMETIME_DIR");
    SYSTEMTIME st;
    if (d && d[0]) {
        _snwprintf(dir, 600, L"%ls", d);
    } else {
        const wchar_t *la = _wgetenv(L"LOCALAPPDATA");
        if (!la) return;
        _snwprintf(dir, 600, L"%ls\\ObiWanRecomp-dev", la);
        CreateDirectoryW(dir, NULL);
        _snwprintf(dir, 600, L"%ls\\ObiWanRecomp-dev\\logs", la);
    }
    dir[599] = 0;
    CreateDirectoryW(dir, NULL);
    GetLocalTime(&st);
    _snwprintf(path, 700, L"%ls\\frametime-%lu-%04u%02u%02u-%02u%02u%02u.csv", dir,
               (unsigned long)GetCurrentProcessId(), st.wYear, st.wMonth, st.wDay,
               st.wHour, st.wMinute, st.wSecond);
    path[699] = 0;
    ft_csv = _wfopen(path, L"w");
    fprintf(stderr, "  [FRAMETIME] CSV %s: frametime-%lu-%04u%02u%02u-%02u%02u%02u.csv in %s\n",
            ft_csv ? "opened" : "could not be opened", (unsigned long)GetCurrentProcessId(),
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
            (d && d[0]) ? "RECOMP_FRAMETIME_DIR" : "%LOCALAPPDATA%\\ObiWanRecomp-dev\\logs");
}

/* What the picture goes out to: monitor, refresh, window or full screen,
 * the swap chain, and what DXGI and DWM say about timing. */
static void ft_display(void)
{
    char l[512];
    HWND h = g_sc_hwnd;
    HMONITOR mon = MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEXW mi;
    DEVMODEW dm;
    RECT wr = { 0 }, cr = { 0 };
    memset(&mi, 0, sizeof mi); mi.cbSize = sizeof mi;
    memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    GetMonitorInfoW(mon, (MONITORINFO *)&mi);
    if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
        snprintf(l, sizeof l, "monitor %ls: %lux%lu at %lu Hz (EnumDisplaySettings), %lu bits, primary %s",
                 mi.szDevice, dm.dmPelsWidth, dm.dmPelsHeight, dm.dmDisplayFrequency, dm.dmBitsPerPel,
                 (mi.dwFlags & MONITORINFOF_PRIMARY) ? "yes" : "no");
        ft_say(l);
    }
    GetWindowRect(h, &wr); GetClientRect(h, &cr);
    {
        LONG style = GetWindowLongW(h, GWL_STYLE);
        int covers = wr.left <= mi.rcMonitor.left && wr.top <= mi.rcMonitor.top
                     && wr.right >= mi.rcMonitor.right && wr.bottom >= mi.rcMonitor.bottom;
        BOOL fs = FALSE;
        if (g_sc) IDXGISwapChain_GetFullscreenState(g_sc, &fs, NULL);
        snprintf(l, sizeof l, "window: %s; client area %ldx%ld; window %ld,%ld-%ld,%ld; monitor %ld,%ld-%ld,%ld",
                 fs ? "exclusive full screen (DXGI)"
                    : (covers && !(style & WS_CAPTION)) ? "borderless window covering the monitor" : "window",
                 cr.right - cr.left, cr.bottom - cr.top, wr.left, wr.top, wr.right, wr.bottom,
                 mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right, mi.rcMonitor.bottom);
        ft_say(l);
    }
    {
        typedef HRESULT (WINAPI *PFN_DGCTI)(HWND, DWM_TIMING_INFO *);
        HMODULE dm_dll = LoadLibraryW(L"dwmapi.dll");
        PFN_DGCTI f = dm_dll ? (PFN_DGCTI)(void *)GetProcAddress(dm_dll, "DwmGetCompositionTimingInfo") : NULL;
        DWM_TIMING_INFO ti;
        memset(&ti, 0, sizeof ti); ti.cbSize = sizeof ti;
        if (f && SUCCEEDED(f(NULL, &ti)) && ti.rateRefresh.uiDenominator) {
            LARGE_INTEGER fq; QueryPerformanceFrequency(&fq);
            snprintf(l, sizeof l, "DWM composition: %.3f Hz, period %.3f ms",
                     (double)ti.rateRefresh.uiNumerator / ti.rateRefresh.uiDenominator,
                     ti.qpcRefreshPeriod * 1000.0 / (double)fq.QuadPart);
        } else snprintf(l, sizeof l, "DWM composition: not available");
        ft_say(l);
    }
    if (g_sc) {
        IDXGIOutput *o = NULL;
        DXGI_SWAP_CHAIN_DESC sd;
        if (SUCCEEDED(IDXGISwapChain_GetContainingOutput(g_sc, &o)) && o) {
            DXGI_OUTPUT_DESC od;
            if (SUCCEEDED(IDXGIOutput_GetDesc(o, &od))) {
                snprintf(l, sizeof l, "DXGI output %ls: desktop %ld,%ld-%ld,%ld, rotation %d",
                         od.DeviceName, od.DesktopCoordinates.left, od.DesktopCoordinates.top,
                         od.DesktopCoordinates.right, od.DesktopCoordinates.bottom, (int)od.Rotation);
                ft_say(l);
            }
            IDXGIOutput_Release(o);
        }
        if (SUCCEEDED(IDXGISwapChain_GetDesc(g_sc, &sd))) {
            snprintf(l, sizeof l, "swap chain: %ux%u, %u buffers, effect %d (3 = flip sequential, 4 = flip discard), flags 0x%X%s",
                     sd.BufferDesc.Width, sd.BufferDesc.Height, sd.BufferCount, (int)sd.SwapEffect, sd.Flags,
                     (sd.Flags & DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING) ? " (tearing allowed)" : "");
            ft_say(l);
        }
    }
    {
        const char *fc = getenv("RECOMP_FRAME_CAP");
        snprintf(l, sizeof l, "present: sync interval %d (RECOMP_VSYNC%s), tearing %s, frame limit %d Hz (title) / RECOMP_FRAME_CAP %s, renderer %s",
                 g_vsync_on > 0 ? 1 : 0, g_vsync_on > 0 ? "=1" : " off", g_tear ? "allowed" : "off",
                 xbox_frame_cap_hz, fc ? fc : "unset", s_d3d11_vulkan ? "D3D11 on Vulkan (DXVK)" : "D3D11");
        ft_say(l);
    }
    ft_say("G-Sync/FreeSync: Windows gives a program no way to ask whether variable refresh is on; "
           "the dt_ms column shows it (with VRR on and vsync off, frames follow dt_ms without tearing)");
    if (ft_csv)
        fprintf(ft_csv, "frame,t_ms,dt_ms,vblanks,walk_ms,present_ms,gate_ms,gpu_waits,input_ms,swap_interval,swap_immediate\n");
}

static int ft_cmp(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return x < y ? -1 : x > y;
}

static float ft_pct(const float *s, unsigned n, double p)
{
    unsigned k = (unsigned)(p * n + 0.999999);   /* nearest rank */
    if (k < 1) k = 1;
    if (k > n) k = n;
    return s[k - 1];
}

static void ft_summary(LARGE_INTEGER now)
{
    static float sorted[FT_WIN];
    double secs = (now.QuadPart - ft_win_t.QuadPart) * ft_ms / 1000.0;
    uint32_t vbl = xbox_VblankCount();
    char l[640], gs[32], is[96];
    if (!ft_win_n || secs <= 0) return;
    memcpy(sorted, ft_win_dt, ft_win_n * sizeof sorted[0]);
    qsort(sorted, ft_win_n, sizeof sorted[0], ft_cmp);
    if (ft_win_gate < 0) snprintf(gs, sizeof gs, "n/a");
    else snprintf(gs, sizeof gs, "%.0f", ft_win_gate / secs);
    if (ft_in_n) snprintf(is, sizeof is, "mean %.1f ms max %.1f ms (%u changes)", ft_in_sum / ft_in_n, ft_in_max, ft_in_n);
    else snprintf(is, sizeof is, "no pad change");
    snprintf(l, sizeof l,
             "pid %lu %.1f s: %u frames (%.1f/s), frame ms p50 %.2f p90 %.2f p99 %.2f p99.9 %.2f max %.2f, "
             "over 33 ms %u, over 50 ms %u; vblanks %.2f/s (game clock vs real: %.1f%%); "
             "per second walk %.0f ms present %.0f ms gate %s ms gpu waits %u; input to present %s",
             (unsigned long)GetCurrentProcessId(), secs, ft_win_n, ft_win_n / secs,
             ft_pct(sorted, ft_win_n, 0.50), ft_pct(sorted, ft_win_n, 0.90), ft_pct(sorted, ft_win_n, 0.99),
             ft_pct(sorted, ft_win_n, 0.999), sorted[ft_win_n - 1], ft_win_over33, ft_win_over50,
             (vbl - ft_win_vbl) / secs, (vbl - ft_win_vbl) / secs / 60.0 * 100.0,
             ft_win_walk / secs, ft_win_pres / secs, gs, ft_win_wait, is);
    ft_say(l);
    if (ft_csv) fflush(ft_csv);
    fflush(stderr);
    ft_win_t = now; ft_win_vbl = vbl;
    ft_win_n = ft_win_over33 = ft_win_over50 = ft_win_wait = ft_in_n = 0;
    ft_win_walk = ft_win_pres = ft_in_sum = 0; ft_in_max = 0;
    ft_win_gate = xbox_GateHeldTotalNs() < 0 ? -1 : 0;
}

static void ft_frame(void)
{
    extern volatile long long g_perf_rt_ticks;
    LARGE_INTEGER now;
    uint32_t vbl = xbox_VblankCount(), sw = g_nv2a_last_swap;
    long long walk = g_perf_rt_ticks, pres = g_perf_present_ticks, gate = xbox_GateHeldTotalNs();
    long waits = xbox_GpuWaitCount();
    LONG64 chg = InterlockedExchange64(&g_input_change_qpc, 0);
    QueryPerformanceCounter(&now);
    if (!ft_first.QuadPart) {
        LARGE_INTEGER f; QueryPerformanceFrequency(&f);
        ft_ms = 1000.0 / (double)f.QuadPart;
        ft_first = ft_prev = ft_win_t = now;
        ft_vbl_prev = ft_win_vbl = vbl;
        ft_walk_prev = walk; ft_pres_prev = pres; ft_gate_prev = gate; ft_wait_prev = waits;
        ft_win_gate = gate < 0 ? -1 : 0;
        ft_open();
        ft_display();
        return;
    }
    {
        double dt = (now.QuadPart - ft_prev.QuadPart) * ft_ms;
        double wk = (walk - ft_walk_prev) * ft_ms, pr = (pres - ft_pres_prev) * ft_ms;
        double gt = gate < 0 ? -1.0 : (gate - ft_gate_prev) / 1e6;
        double in = chg ? (now.QuadPart - chg) * ft_ms : -1.0;
        long nw = waits - ft_wait_prev;
        ++ft_n;
        if (ft_csv)
            fprintf(ft_csv, "%u,%.3f,%.3f,%u,%.3f,%.3f,%.3f,%ld,%.3f,%u,%u\n", ft_n,
                    (now.QuadPart - ft_first.QuadPart) * ft_ms, dt, vbl - ft_vbl_prev, wk, pr, gt, nw, in,
                    sw ? (sw >> 5) & 7u : 0u, sw ? (sw >> 8) & 1u : 0u);
        if (ft_win_n < FT_WIN) ft_win_dt[ft_win_n++] = (float)dt;
        if (dt > 33.4) ft_win_over33++;
        if (dt > 50.0) ft_win_over50++;
        ft_win_walk += wk; ft_win_pres += pr; ft_win_wait += (unsigned)nw;
        if (gt >= 0 && ft_win_gate >= 0) ft_win_gate += gt;
        if (in >= 0) { ft_in_sum += in; ft_in_n++; if (in > ft_in_max) ft_in_max = (float)in; }
        if (ft_csv && (ft_n & 127) == 0) fflush(ft_csv);
    }
    ft_prev = now; ft_vbl_prev = vbl;
    ft_walk_prev = walk; ft_pres_prev = pres; ft_gate_prev = gate; ft_wait_prev = waits;
    if ((now.QuadPart - ft_win_t.QuadPart) * ft_ms >= 10000.0) ft_summary(now);
}

/* The frame the title just finished, to the window (from TT's FLIP_STALL). */
void nv2a_gpu_tt_present(void)
{
    uint32_t done = s_gpu.drawn_offset ? s_gpu.drawn_offset : s_gpu.color_offset;
    cap_on_flip();
    g_nv2a_flips++;
    unsigned frame = g_present_n;
    { extern void xbox_MaxShadowsOnFlip(void); xbox_MaxShadowsOnFlip(); }   /* (Claude completo-8) RECOMP_MAXSHADOWS */
    { extern void xbox_FpsClampOnFlip(void); xbox_FpsClampOnFlip(); }   /* (Claude completo-9) RECOMP_FPSCLAMP */
    if (s_gpu.pitch && done)
        gpu_present(dma_resolve(done), s_gpu.pitch & 0xFFFF);
    {   /* (Claude completo-8) RECOMP_FLIP_LOG=1 (a measure, off by default):
         * one line per flip -- draws since the last one, the surface drawn,
         * the one now bound, and whether a frame reached the window. */
        static int fl = -1; static uint32_t d0;
        if (fl < 0) fl = getenv("RECOMP_FLIP_LOG") != NULL;
        if (fl) fprintf(stderr, "  [FLIPLOG] present %u draws %u done %08X color %08X shown %d\n",
                        frame, s_gpu.draws - d0, done, s_gpu.color_offset, g_present_n != frame);
        d0 = s_gpu.draws;
    }
    if (g_present_n == frame) return;
    gpu_report_timing();
    if (ft_on()) ft_frame();
    if (g_inspect_frame == frame) {
        fprintf(stderr, "  [INSPECT] end frame %u\n", frame);
        g_inspect_frame = UINT32_MAX;
    }
    /* (Claude completo-7) RECOMP_GPU_INSPECT_EVERY=n: the same
     * capture as F12 (RECOMP_CAPTURA_FRAME) every n presents, without a
     * keyboard, so a scripted run (RECOMP_PAD_SCRIPT) can be studied. */
    static int insp_every = -1;
    static int insp_rng; static unsigned long long insp_from, insp_to;
    if (insp_every < 0) {
        const char *e = getenv("RECOMP_GPU_INSPECT_EVERY");
        insp_every = e && atoi(e) > 0 ? atoi(e) : 0;
        /* (Claude completo-8) RECOMP_GPU_INSPECT_FROM=f, RECOMP_GPU_INSPECT_N=n:
         * only presents f .. f+n-1, so that EVERY=1 captures a run of
         * consecutive frames (finding the draw that flickers). */
        e = getenv("RECOMP_GPU_INSPECT_FROM");
        if (e && atol(e) >= 0) {
            const char *n = getenv("RECOMP_GPU_INSPECT_N");
            insp_rng = 1;
            insp_from = (unsigned long long)atol(e);
            insp_to = insp_from + (unsigned long long)(n && atol(n) > 0 ? atol(n) : 60);
        }
    }
    if (xbox_FramebufferTakeGpuInspect() ||
        (insp_every && g_present_n % (unsigned)insp_every == 0 &&
         (!insp_rng || (g_present_n >= insp_from && g_present_n < insp_to)))) {
        g_inspect_frame = g_present_n;
        fprintf(stderr, "  [INSPECT] begin frame %u pid %lu\n", g_inspect_frame,
                (unsigned long)GetCurrentProcessId());
    }
}

/* ---------------------------------------------------------------------
 * Claude completo-7: RECOMP_PROF=1 (a measure, off by default).
 *
 * A sampling profiler inside the process, because the render thread's own
 * counters ([PORTPERF-RT]) leave half of walk_ms unexplained and this machine
 * has wpr but nothing to read its traces with. A thread registers itself
 * (xbox_ProfRegisterSelf); every millisecond the sampler suspends it, reads
 * its instruction pointer and resumes it -- nothing that can take a lock runs
 * while it is suspended. Every RECOMP_PROF_SECS seconds (default 20) the
 * samples are named through the .pdb next to the executable (dbghelp) and the
 * top functions are written as [PROF] lines, then counting starts again.
 */
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "winmm.lib")
#define PROF_MAXTH 8
#define PROF_HASH 65536
static HANDLE s_prof_th[PROF_MAXTH];
static char s_prof_name[PROF_MAXTH][32];
static volatile LONG s_prof_register_lock;
static unsigned s_prof_title_id;
static volatile LONG s_prof_n;
static struct { DWORD64 pc; unsigned n; } s_prof_h[PROF_MAXTH][PROF_HASH];
static unsigned s_prof_total[PROF_MAXTH], s_prof_lost[PROF_MAXTH];

static void prof_report(int t)
{
    static struct { char name[96]; unsigned n; char file[MAX_PATH]; DWORD line; } agg[4096];
    static int syms;
    static struct { char name[MAX_PATH]; unsigned n; } mods[256];
    int na = 0, nm_count = 0, i, j, top_mod = 0;
    unsigned waits = 0, total = s_prof_total[t];
    if (!total) {
        fprintf(stderr, "  [PROF] thread %s: 0 samples, top-module/espera: ?; 0.0%% en esperas (0 lost)\n",
                s_prof_name[t]);
        return;
    }
    if (!syms) {
        char dir[MAX_PATH], *sl;
        GetModuleFileNameA(NULL, dir, sizeof dir);
        if ((sl = strrchr(dir, 0x5C)) != NULL) *sl = 0;
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
        syms = SymInitialize(GetCurrentProcess(), dir, TRUE) ? 1 : -1;
        if (syms < 0 && GetLastError() == ERROR_INVALID_PARAMETER) {   /* already initialised (crash handler) */
            SymSetSearchPath(GetCurrentProcess(), dir);
            SymRefreshModuleList(GetCurrentProcess());
            syms = 1;
        }
        fprintf(stderr, "  [PROF] symbols from %s: %s (error %lu)\n", dir, syms > 0 ? "ok" : "failed", GetLastError());
    }
    for (i = 0; i < PROF_HASH; i++) {
        char nm[96];
        unsigned c = s_prof_h[t][i].n;
        if (!c) continue;
        {
            union { SYMBOL_INFO si; char b[sizeof(SYMBOL_INFO) + 256]; } u;
            DWORD64 disp = 0;
            memset(&u.si, 0, sizeof u.si);
            u.si.SizeOfStruct = sizeof(SYMBOL_INFO); u.si.MaxNameLen = 255;
            if (syms > 0 && SymFromAddr(GetCurrentProcess(), s_prof_h[t][i].pc, &disp, &u.si))
                snprintf(nm, sizeof nm, "%s", u.si.Name);
            else {
                HMODULE m = NULL; char mod[MAX_PATH] = "?";
                if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                       (LPCSTR)(uintptr_t)s_prof_h[t][i].pc, &m) && m) {
                    char *b; GetModuleFileNameA(m, mod, sizeof mod);
                    b = strrchr(mod, '\\');
                    snprintf(nm, sizeof nm, "[%s]", b ? b + 1 : mod);
                } else snprintf(nm, sizeof nm, "[?]");
            }
        }
        /* (Claude completo-9) Leaf PCs often sit in ntdll's native waits,
         * rather than the title's Sleep/KeWait wrappers. This percentage
         * measures sampled residency, not elapsed wait duration. */
        if (strstr(nm, "Sleep") || strstr(nm, "DelayExecution") ||
            strstr(nm, "KeWait") || strstr(nm, "WaitForSingleObject") ||
            strstr(nm, "WaitForMultipleObjects") || strstr(nm, "NtWait") ||
            strstr(nm, "ZwWait"))
            waits += c;
        {
            HMODULE m = NULL;
            char mod[MAX_PATH] = "?", *base = mod;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                  GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  (LPCSTR)(uintptr_t)s_prof_h[t][i].pc, &m) && m) {
                GetModuleFileNameA(m, mod, sizeof mod);
                base = strrchr(mod, 0x5C);
                base = base ? base + 1 : mod;
            }
            for (j = 0; j < nm_count && strcmp(mods[j].name, base); j++) ;
            if (j == nm_count && nm_count < 256) {
                snprintf(mods[j].name, sizeof mods[j].name, "%s", base);
                mods[j].n = 0;
                nm_count++;
            }
            if (j < nm_count) mods[j].n += c;
        }
        for (j = 0; j < na && strcmp(agg[j].name, nm); j++) ;
        if (j == na) {
            IMAGEHLP_LINE64 li;
            DWORD ld = 0;
            if (na == 4096) continue;
            memcpy(agg[na].name, nm, sizeof nm); agg[na].n = 0;
            strcpy(agg[na].file, "?"); agg[na].line = 0;
            memset(&li, 0, sizeof li); li.SizeOfStruct = sizeof li;
            /* (Claude completo-9) Resolve lines only during a report, after
             * resuming sampled threads. No extra work on method hot paths. */
            if (syms > 0 && SymGetLineFromAddr64(GetCurrentProcess(), s_prof_h[t][i].pc, &ld, &li)) {
                snprintf(agg[na].file, sizeof agg[na].file, "%s", li.FileName);
                agg[na].line = li.LineNumber;
            }
            na++;
        }
        agg[j].n += c;
    }
    for (j = 1; j < nm_count; j++)
        if (mods[j].n > mods[top_mod].n) top_mod = j;
    fprintf(stderr, "  [PROF] thread %s: %u samples, top-module/espera: %s %.1f%%; %.1f%% en esperas (%u lost)\n",
            s_prof_name[t], total, nm_count ? mods[top_mod].name : "?",
            nm_count ? 100.0 * mods[top_mod].n / total : 0.0,
            100.0 * waits / total, s_prof_lost[t]);
    for (i = 0; i < 40 && i < na; i++) {
        int b = i;
        for (j = i + 1; j < na; j++) if (agg[j].n > agg[b].n) b = j;
        if (b != i) { char tmp[sizeof agg[0]]; memcpy(tmp, &agg[i], sizeof tmp); memcpy(&agg[i], &agg[b], sizeof tmp); memcpy(&agg[b], tmp, sizeof tmp); }
        fprintf(stderr, "  [PROF]   %5.1f%%  %s  %s:%lu\n", 100.0 * agg[i].n / total, agg[i].name, agg[i].file, (unsigned long)agg[i].line);
    }
    memset(s_prof_h[t], 0, sizeof s_prof_h[t]);
    s_prof_total[t] = s_prof_lost[t] = 0;
}

static DWORD WINAPI prof_thread(LPVOID arg)
{
    const char *e = getenv("RECOMP_PROF_SECS");
    DWORD every = (e && atoi(e) > 0 ? atoi(e) : 20) * 1000u, t0 = GetTickCount();
    (void)arg;
    timeBeginPeriod(1);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    for (;;) {
        int t, n = (int)s_prof_n;
        Sleep(1);
        for (t = 0; t < n; t++) {
            CONTEXT c;
            DWORD64 pc = 0;
            unsigned k, p;
            memset(&c, 0, sizeof c);
            c.ContextFlags = CONTEXT_CONTROL;
            if (SuspendThread(s_prof_th[t]) == (DWORD)-1) continue;
            if (GetThreadContext(s_prof_th[t], &c)) pc = c.Rip;
            ResumeThread(s_prof_th[t]);
            if (!pc) continue;
            s_prof_total[t]++;
            k = (unsigned)((pc * 0x9E3779B97F4A7C15ull) >> 48) & (PROF_HASH - 1);
            for (p = 0; p < 64; p++, k = (k + 1) & (PROF_HASH - 1)) {
                if (s_prof_h[t][k].pc == pc) { s_prof_h[t][k].n++; break; }
                if (!s_prof_h[t][k].n) { s_prof_h[t][k].pc = pc; s_prof_h[t][k].n = 1; break; }
            }
            if (p == 64) s_prof_lost[t]++;
        }
        if (GetTickCount() - t0 >= every) {
            for (t = 0; t < n; t++) prof_report(t);
            t0 = GetTickCount();
        }
    }
    return 0;
}

/* (Claude completo-9) The pushbuffer alone cannot explain title frame time.
 * Call at title thread startup only, gated by RECOMP_PROF at each caller:
 * no profiler call is added to guest dispatch/frame/wait hot paths.
 * Serialize registration and publish complete slots to the sampler.
 * NULL names get stable title-N storage, never a worker's stack buffer. */
void xbox_ProfRegisterSelf(const char *name)
{
    const char *e = getenv("RECOMP_PROF");
    HANDLE th;
    LONG i;
    if (!e || *e != '1') return;
    while (InterlockedCompareExchange(&s_prof_register_lock, 1, 0))
        Sleep(0);
    i = s_prof_n;
    if (i >= PROF_MAXTH ||
        !DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &th,
                         THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0)) {
        InterlockedExchange(&s_prof_register_lock, 0);
        return;
    }
    s_prof_th[i] = th;
    if (name) snprintf(s_prof_name[i], sizeof s_prof_name[i], "%s", name);
    else snprintf(s_prof_name[i], sizeof s_prof_name[i], "title-%u", ++s_prof_title_id);
    InterlockedIncrement(&s_prof_n);
    InterlockedExchange(&s_prof_register_lock, 0);
    fprintf(stderr, "  [PROF] sampling thread %s (tid %lu)\n", s_prof_name[i], GetCurrentThreadId());
    if (i == 0) CloseHandle(CreateThread(NULL, 0, prof_thread, NULL, 0, NULL));
}

#endif /* _WIN32 */
