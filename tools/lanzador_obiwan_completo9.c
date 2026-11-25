/* "Jugar Obi-Wan 11.exe" -- lanzador de la variante obiwan-claude-completo-9
 * (Claude completo-9, 2026-11-12). Copia de lanzador_obiwan_completo8.c (v10) cambiando solo la variante y el prefijo del log.
 *
 * Igual que v5, y ademas:
 *  - Lee obiwan.ini (tabla unica tools/obiwan_ajustes.h). Un valor igual al de
 *    por defecto no pone ninguna variable: sin ini, o con el ini sin tocar, el
 *    juego arranca exactamente con las mismas variables que v5.
 *  - Espera a TODOS los procesos del juego (Job Object). El juego se relanza a
 *    si mismo al cargar el nivel (kernel_bridge.c:1854, CreateProcessW sin
 *    CREATE_BREAKAWAY_FROM_JOB): v1-v5 solo esperaban al primero.
 *  - Elige la edicion por la firma del default.xbe, no por el nombre de la
 *    carpeta.
 *
 * Argumentos (en cualquier orden):
 *   medir            RECOMP_MEDIR=1 (como v5)
 *   --en-seco        escribe las variables y la linea de ordenes y se cierra
 *                    SIN arrancar el juego
 *   --ini <ruta>     usa ese ini
 *   --perfil <nom>   usa perfiles\<nom>.ini
 *   --plantilla <r>  escribe en <r> un obiwan.ini con los valores por defecto
 *                    y se cierra (para pruebas y para regenerarlo)
 *
 * Compilar: tools\build_lanzador_obiwan_completo9.cmd (escrito por
 * tools\claude4\escribir_cmds.py). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <shellapi.h>
#include "obiwan_ajustes.h"

#define LANZ_BUILD_W L"obiwan-claude-completo-9"
#define LANZ_BUILD_A "obiwan-claude-completo-9"
#define LANZ_LOG_W   L"claude-completo9-normal"

/* Copiadas tal cual de tools/lanzador_obiwan.c (v1-v5). */
static const wchar_t *const k_clear[] = {
    L"OBIWAN_NIVEL", L"RECOMP_PAD_SCRIPT", L"RECOMP_PAD_SCRIPT_RELAUNCH",
    L"RECOMP_PAD_LIVE", L"RECOMP_WATCHDOG_SECS", L"RECOMP_PROFILE",
    L"RECOMP_PERFRT", L"RECOMP_APU_TRACE", L"RECOMP_TRACE_FILE_READS",
    L"RECOMP_AUDIO_HOST", L"RECOMP_AUDIO_WAV", L"RECOMP_PRESENT_DUMP",
    L"RECOMP_GPU_TIMING", L"RECOMP_GPU_INSPECT_DIR", L"RECOMP_GUEST_LOCK",
    L"RECOMP_GUEST_CPUS", L"RECOMP_FLIP_PACING", L"RECOMP_IRQ_SAFE_FORCE",
    L"RECOMP_KERNEL_LOG", L"RECOMP_TEX_STATS", L"RECOMP_STALL_SITES",
    L"RECOMP_GATE_STATS",
};

static const wchar_t *const k_set[][2] = {
    { L"RECOMP_APU_FE_NOTIFY", L"1" },
    { L"RECOMP_FB_WINDOW", L"1" },
    { L"RECOMP_TITLE_KEVENTS", L"1" },
    { L"RECOMP_VBLANK", L"paced" },
    { L"RECOMP_PB_EXEC", L"1" },
    { L"RECOMP_USB", L"1" },
    { L"RECOMP_MKB", L"1" },
    { L"RECOMP_SDL_PADS", L"1" },
    { L"RECOMP_AC97_READY", L"1" },
    { L"RECOMP_DISPATCH_GATE", L"1" },
};

static const wchar_t *const k_default[][2] = {
    { L"RECOMP_GPU_NATIVE_BC", L"1" },
    { L"RECOMP_AUDIO_BUF_COUNT", L"6" },
};

static int g_seco;
static HANDLE g_out = INVALID_HANDLE_VALUE;   /* --en-seco */
static HANDLE g_log = INVALID_HANDLE_VALUE;

static void sal(const char *fmt, ...)
{
    char b[2048]; va_list ap; int n; DWORD wr;
    va_start(ap, fmt);
    n = _vsnprintf_s(b, sizeof b, _TRUNCATE, fmt, ap);
    va_end(ap);
    if (n < 0) n = (int)strlen(b);
    if (g_seco && g_out != INVALID_HANDLE_VALUE) WriteFile(g_out, b, (DWORD)n, &wr, NULL);
    if (!g_seco && g_log != INVALID_HANDLE_VALUE) WriteFile(g_log, b, (DWORD)n, &wr, NULL);
}

static void a_utf8(const wchar_t *w, char *o, int n)
{
    if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, o, n, NULL, NULL)) o[0] = 0;
}

static void a_wide(const char *s, wchar_t *o, int n)
{
    if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, o, n)) o[0] = 0;
}

static void fail(const wchar_t *msg)
{
    if (g_seco) {
        char m[1024]; a_utf8(msg, m, sizeof m);
        sal("ERROR: %s\r\n", m);
        return;
    }
    MessageBoxW(NULL, msg, L"Obi-Wan", MB_OK | MB_ICONERROR);
}

static int is_dir(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

/* Como v5 (OBIWAN_JUEGO, private\game\usa, primera carpeta), pero con la
 * carpeta de la edicion elegida antes que usa. */
static int find_game_dir(const wchar_t *base, const wchar_t *ed, wchar_t *out, size_t n)
{
    WIN32_FIND_DATAW fd;
    HANDLE h;
    wchar_t pat[MAX_PATH];

    if (GetEnvironmentVariableW(L"OBIWAN_JUEGO", out, (DWORD)n) && is_dir(out))
        return 1;
    _snwprintf_s(out, n, _TRUNCATE, L"%s\\private\\game\\%s", base, ed);
    if (is_dir(out))
        return 1;
    _snwprintf_s(out, n, _TRUNCATE, L"%s\\private\\game\\usa", base);
    if (is_dir(out))
        return 1;
    _snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\private\\game\\*", base);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    do {
        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) {
            _snwprintf_s(out, n, _TRUNCATE, L"%s\\private\\game\\%s", base, fd.cFileName);
            FindClose(h);
            return 1;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return 0;
}

static void poner_ini(const char *var, const char *val, int i, void *ctx)
{
    wchar_t wv[128], wval[AJ_VMAX];
    (void)ctx;
    a_wide(var, wv, 128);
    a_wide(val, wval, AJ_VMAX);
    SetEnvironmentVariableW(wv, wval);
    sal("[AJUSTES] %s=%s  (%s%s%s)\r\n", var, val,
        i >= 0 ? aj_defs[i].seccion : "ini", i >= 0 ? "." : "", i >= 0 ? aj_defs[i].clave : "");
}

static int cmp_str(const void *a, const void *b)
{
    return _wcsicmp(*(const wchar_t *const *)a, *(const wchar_t *const *)b);
}

/* --en-seco: todas las RECOMP_*, OBIWAN_*, DXVK_* del entorno que recibira el juego, ordenadas. */
static void listar_entorno(void)
{
    wchar_t *env = GetEnvironmentStringsW(), *p;
    const wchar_t *lista[512];
    int n = 0, i;
    if (!env) return;
    for (p = env; *p; p += wcslen(p) + 1)
        if ((!_wcsnicmp(p, L"RECOMP_", 7) || !_wcsnicmp(p, L"OBIWAN_", 7) || !_wcsnicmp(p, L"DXVK_", 5)) && n < 512)
            lista[n++] = p;
    qsort((void *)lista, (size_t)n, sizeof lista[0], cmp_str);
    sal("variables (%d):\r\n", n);
    for (i = 0; i < n; i++) {
        char u[1024]; a_utf8(lista[i], u, sizeof u);
        sal("  %s\r\n", u);
    }
    FreeEnvironmentStringsW(env);
}

/* Espera a que acaben todos los procesos del Job; devuelve el codigo del
 * ultimo que termino. */
typedef struct { DWORD pid; HANDLE h; } Proc;
static DWORD esperar_job(HANDLE job, HANDLE port, HANDLE first, DWORD first_pid)
{
    Proc ps[64]; int np = 0, i; DWORD last = 0, nb; ULONG_PTR key; LPOVERLAPPED ov;
    ps[np].pid = first_pid; ps[np].h = first; np++;
    (void)job;
    for (;;) {
        if (!GetQueuedCompletionStatus(port, &nb, &key, &ov, INFINITE)) break;
        if (nb == JOB_OBJECT_MSG_NEW_PROCESS) {
            DWORD pid = (DWORD)(ULONG_PTR)ov;
            HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (pid != first_pid) {
                sal("[LAUNCH] proceso nuevo del juego: pid %lu\r\n", pid);
                if (h && np < 64) { ps[np].pid = pid; ps[np].h = h; np++; } else if (h) CloseHandle(h);
            } else if (h) CloseHandle(h);
        } else if (nb == JOB_OBJECT_MSG_EXIT_PROCESS || nb == JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS) {
            DWORD pid = (DWORD)(ULONG_PTR)ov, code = 0;
            for (i = 0; i < np; i++)
                if (ps[i].pid == pid && ps[i].h) {
                    WaitForSingleObject(ps[i].h, 5000);
                    GetExitCodeProcess(ps[i].h, &code);
                    last = code;
                }
            sal("[LAUNCH] proceso %lu terminado: %lu%s\r\n", pid, code,
                nb == JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS ? " (anormal)" : "");
        } else if (nb == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO) {
            break;
        }
    }
    for (i = 0; i < np; i++) if (ps[i].h && ps[i].h != first) CloseHandle(ps[i].h);
    return last;
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    static AjConf conf;
    wchar_t base[MAX_PATH], exe[MAX_PATH], game[MAX_PATH], saves[MAX_PATH], xbe[MAX_PATH];
    wchar_t logdir[MAX_PATH], log[MAX_PATH], local[MAX_PATH], dev[MAX_PATH], ini[MAX_PATH];
    wchar_t cmdline[4 * MAX_PATH], msg[2 * MAX_PATH], wed[32], wvar[AJ_VMAX], *slash;
    wchar_t **argv; int argc, a;
    const wchar_t *ini_arg = NULL, *perfil = NULL;
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    SYSTEMTIME st;
    DWORD code = 0;
    const AjEdicion *ed, *ed_xbe;
    char hex[65], u8[4 * MAX_PATH];
    size_t i;
    int medir = 0, origen, k;

    (void)inst; (void)prev; (void)cmd; (void)show;
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (a = 1; argv && a < argc; a++) {
        if (!_wcsicmp(argv[a], L"medir")) medir = 1;
        else if (!_wcsicmp(argv[a], L"--en-seco")) g_seco = 1;
        else if (!_wcsicmp(argv[a], L"--ini") && a + 1 < argc) ini_arg = argv[++a];
        else if (!_wcsicmp(argv[a], L"--perfil") && a + 1 < argc) perfil = argv[++a];
        else if (!_wcsicmp(argv[a], L"--plantilla") && a + 1 < argc) {
            /* escribe un obiwan.ini con todos los valores por defecto y sale */
            DWORD e;
            aj_por_defecto(&conf);
            e = aj_guardar(argv[a + 1], &conf);
            LocalFree(argv);
            return (int)e;
        }
    }
    if (g_seco) {
        g_out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!g_out || g_out == INVALID_HANDLE_VALUE || GetFileType(g_out) == FILE_TYPE_UNKNOWN) {
            if (AttachConsole(ATTACH_PARENT_PROCESS))
                g_out = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        }
        if (GetFileType(g_out) == FILE_TYPE_CHAR) SetConsoleOutputCP(CP_UTF8);
        sal("Jugar Obi-Wan 11 --en-seco: no arranca el juego.\r\n");
    }

    GetModuleFileNameW(NULL, base, MAX_PATH);
    slash = wcsrchr(base, L'\\');
    if (slash) *slash = 0;
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH)) {
        fail(L"Falta la variable LOCALAPPDATA.");
        return 1;
    }
    _snwprintf_s(dev, MAX_PATH, _TRUNCATE, L"%s\\ObiWanRecomp-dev", local);

    /* 1. ini */
    if (ini_arg) {
        wcsncpy_s(ini, MAX_PATH, ini_arg, _TRUNCATE); origen = 3;
    } else if (perfil) {
        wchar_t nom[MAX_PATH];
        _snwprintf_s(nom, MAX_PATH, _TRUNCATE, L"perfiles\\%s.ini", perfil);
        origen = aj_ruta_ini(base, nom, ini, MAX_PATH);
    } else {
        origen = aj_ruta_ini(base, L"obiwan.ini", ini, MAX_PATH);
    }
    k = aj_cargar(ini, &conf);
    a_utf8(ini, u8, sizeof u8);
    sal("ini: %s (%s)\r\n", u8, k == 0 ? "no existe: valores por defecto" : k < 0 ? "roto: valores por defecto" :
                               k == 2 ? "leido con avisos" : "leido");
    (void)origen;

    /* 2. edicion y variante */
    {
        int ie = aj_indice("Language", "edicion");
        ed = aj_edicion(conf.val[ie]);
        if (!ed || !ed->build) {
            aj_aviso(&conf, "la edicion \"%s\" no tiene build todavia (falta compilar la edicion PAL): uso usa", conf.val[ie]);
            ed = aj_edicion("usa");
        }
    }
    a_wide(ed->id, wed, 32);
    _snwprintf_s(exe, MAX_PATH, _TRUNCATE, L"%s\\build\\" LANZ_BUILD_W L"\\obiwan_recomp.exe", base);
    if (aj_dev_activo(&conf)) {
        int iv = aj_indice("Developer", "variante");
        /* (completo-9) "obiwan-claude-completo-4" es el valor por defecto de la
         * tabla (obiwan_ajustes.h, compartida con v6): aqui significa "la de
         * este lanzador", no la build de v6. */
        if (_stricmp(conf.val[iv], LANZ_BUILD_A) && _stricmp(conf.val[iv], "obiwan-claude-completo-4")) {
            wchar_t alt[MAX_PATH];
            a_wide(conf.val[iv], wvar, AJ_VMAX);
            _snwprintf_s(alt, MAX_PATH, _TRUNCATE, L"%s\\build\\%s\\obiwan_recomp.exe", base, wvar);
            if (GetFileAttributesW(alt) != INVALID_FILE_ATTRIBUTES) wcscpy_s(exe, MAX_PATH, alt);
            else aj_aviso(&conf, "[Developer] variante = %s: no hay build\\%s\\obiwan_recomp.exe; uso " LANZ_BUILD_A,
                          conf.val[iv], conf.val[iv]);
        }
    }
    {   /* Vulkan solo si hay carpeta dxvk junto al exe (d3d11_dyn.h:34) */
        int im = aj_indice("Compat", "motor");
        if (!_stricmp(conf.val[im], "vulkan")) {
            wchar_t d[MAX_PATH]; wcscpy_s(d, MAX_PATH, exe);
            slash = wcsrchr(d, L'\\'); if (slash) *slash = 0;
            wcscat_s(d, MAX_PATH, L"\\dxvk");
            if (!is_dir(d)) {
                aj_aviso(&conf, "[Compat] motor = vulkan pero no hay carpeta dxvk junto al juego: uso d3d11");
                strcpy_s(conf.val[im], AJ_VMAX, "d3d11");
            }
        }
    }
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) {
        _snwprintf_s(msg, 2 * MAX_PATH, _TRUNCATE, L"No encuentro el juego compilado:\n%s", exe);
        fail(msg);
        return 1;
    }

    /* 3. carpeta del juego y firma del XBE */
    {
        int ij = aj_indice("Paths", "carpeta_juego");
        game[0] = 0;
        if (conf.val[ij][0]) {
            a_wide(conf.val[ij], game, MAX_PATH);
            if (!is_dir(game)) {
                aj_aviso(&conf, "[Paths] carpeta_juego no existe: busco la de siempre");
                game[0] = 0;
            }
        }
        if (!game[0] && !find_game_dir(base, wed, game, MAX_PATH)) {
            fail(L"No encuentro la carpeta del juego en private\\game.");
            return 1;
        }
    }
    _snwprintf_s(xbe, MAX_PATH, _TRUNCATE, L"%s\\default.xbe", game);
    ed_xbe = aj_edicion_xbe(xbe, hex);
    if (ed_xbe && !ed_xbe->build) {
        wchar_t wn[64]; a_wide(ed_xbe->nombre_es, wn, 64);
        _snwprintf_s(msg, 2 * MAX_PATH, _TRUNCATE,
                     L"La carpeta del juego tiene la edicion %s.\nFalta compilar la edicion PAL: hoy solo se puede jugar la de EE. UU.\n%s",
                     wn, game);
        fail(msg);
        return 1;
    }
    if (ed_xbe && ed_xbe != ed) {
        aj_aviso(&conf, "la carpeta del juego tiene la edicion %s (por la firma del XBE), no %s: uso %s", ed_xbe->id, ed->id, ed_xbe->id);
        ed = ed_xbe;
    }
    if (!ed_xbe)
        aj_aviso(&conf, "default.xbe con firma desconocida (%s): el juego dira si lo acepta", hex[0] ? hex : "no se pudo leer");

    /* 4. partidas */
    {
        int ip = aj_indice("Paths", "carpeta_partidas");
        CreateDirectoryW(dev, NULL);
        _snwprintf_s(logdir, MAX_PATH, _TRUNCATE, L"%s\\logs", dev);
        CreateDirectoryW(logdir, NULL);
        if (conf.val[ip][0]) {
            a_wide(conf.val[ip], saves, MAX_PATH);
            if (!is_dir(saves) && !CreateDirectoryW(saves, NULL)) {
                aj_aviso(&conf, "[Paths] carpeta_partidas no existe y no se puede crear: uso la de siempre");
                saves[0] = 0;
            }
        } else saves[0] = 0;
        if (!saves[0]) {
            wchar_t sub[64]; a_wide(ed->partidas, sub, 64);
            _snwprintf_s(saves, MAX_PATH, _TRUNCATE, L"%s\\saves", dev);
            CreateDirectoryW(saves, NULL);
            _snwprintf_s(saves, MAX_PATH, _TRUNCATE, L"%s\\saves\\%s", dev, sub);
            CreateDirectoryW(saves, NULL);
        }
    }

    /* 5. log (no en seco) */
    GetLocalTime(&st);
    if (!g_seco) {
        _snwprintf_s(log, MAX_PATH, _TRUNCATE, L"%s\\" LANZ_LOG_W L"-%04u%02u%02u-%02u%02u%02u.log",
                     logdir, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        g_log = CreateFileW(log, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_log == INVALID_HANDLE_VALUE) {
            _snwprintf_s(msg, 2 * MAX_PATH, _TRUNCATE, L"No puedo crear el log:\n%s", log);
            fail(msg);
            return 1;
        }
        sal("[LAUNCH] %02u/%02u/%04u %02u:%02u:%02u build=%s modo=%s fe_notify=1 lanzador=exe11 edicion=%s\r\n",
            st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond,
            LANZ_BUILD_A, medir ? "medir" : "normal", ed->id);
        sal("[AJUSTES] ini %s (%s)\r\n", u8, k == 0 ? "no existe" : k < 0 ? "roto" : k == 2 ? "con avisos" : "bien");
    }

    /* 6. entorno: como v5, y despues el ini */
    for (i = 0; i < sizeof k_clear / sizeof k_clear[0]; i++)
        SetEnvironmentVariableW(k_clear[i], NULL);
    for (i = 0; i < sizeof k_set / sizeof k_set[0]; i++)
        SetEnvironmentVariableW(k_set[i][0], k_set[i][1]);
    for (i = 0; i < sizeof k_default / sizeof k_default[0]; i++)
        if (!GetEnvironmentVariableW(k_default[i][0], NULL, 0))
            SetEnvironmentVariableW(k_default[i][0], k_default[i][1]);
    SetEnvironmentVariableW(L"RECOMP_MEDIR", medir ? L"1" : NULL);
    if (g_seco) sal("ajustes del ini que cambian algo:\r\n");
    aj_entorno(&conf, poner_ini, NULL);
    if (aj_dev_activo(&conf)) sal("[AJUSTES] modo desarrollador activo\r\n");
    {   /* avisos */
        const char *q = conf.avisos;
        if (g_seco) sal("avisos (%d):\r\n", conf.navisos);
        while (*q) {
            const char *e = strchr(q, '\n'); int n = e ? (int)(e - q) : (int)strlen(q);
            sal("[AJUSTES] aviso: %.*s\r\n", n, q);
            q += n; if (*q) q++;
        }
    }

    _snwprintf_s(cmdline, 4 * MAX_PATH, _TRUNCATE, L"\"%s\" \"%s\" \"%s\"", exe, game, saves);
    if (g_seco) {
        a_utf8(cmdline, u8, sizeof u8);
        sal("edicion: %s (firma del XBE: %s)\r\n", ed->id, ed_xbe ? ed_xbe->id : "desconocida");
        listar_entorno();
        sal("orden: %s\r\n", u8);
        a_utf8(base, u8, sizeof u8);
        sal("carpeta de trabajo: %s\r\n", u8);
        sal("en seco: no se arranca el juego.\r\n");
        if (g_out != INVALID_HANDLE_VALUE) FlushFileBuffers(g_out);
        LocalFree(argv);
        return 0;
    }

    /* 7. arrancar dentro de un Job Object y esperar a todos */
    {
        HANDLE job = CreateJobObjectW(NULL, NULL), port = NULL;
        JOBOBJECT_ASSOCIATE_COMPLETION_PORT acp;
        int en_job = 0;
        ZeroMemory(&si, sizeof si);
        si.cb = sizeof si;
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = g_log;
        si.hStdError = g_log;
        if (job) {
            port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
            acp.CompletionKey = job; acp.CompletionPort = port;
            if (!port || !SetInformationJobObject(job, JobObjectAssociateCompletionPortInformation, &acp, sizeof acp)) {
                if (port) CloseHandle(port);
                CloseHandle(job); job = NULL; port = NULL;
            }
        }
        if (!CreateProcessW(exe, cmdline, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                            NULL, base, &si, &pi)) {
            _snwprintf_s(msg, 2 * MAX_PATH, _TRUNCATE, L"No se pudo arrancar el juego (error %lu).", GetLastError());
            CloseHandle(g_log);
            fail(msg);
            return 1;
        }
        if (job && AssignProcessToJobObject(job, pi.hProcess)) en_job = 1;
        else sal("[LAUNCH] sin Job Object (error %lu): solo espero al primer proceso, como v5\r\n", GetLastError());
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
        if (en_job) {
            code = esperar_job(job, port, pi.hProcess, pi.dwProcessId);
            sal("[LAUNCH] todos los procesos del juego han terminado; codigo del ultimo: %lu\r\n", code);
        } else {
            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, &code);
            sal("[LAUNCH] primer proceso finalizado: %lu\r\n", code);
        }
        CloseHandle(pi.hProcess);
        if (port) CloseHandle(port);
        if (job) CloseHandle(job);
    }
    CloseHandle(g_log);
    if (code != 0) {
        _snwprintf_s(msg, 2 * MAX_PATH, _TRUNCATE, L"El juego termino con codigo %lu.\nLog:\n%s", code, log);
        fail(msg);
    }
    LocalFree(argv);
    return (int)code;
}
