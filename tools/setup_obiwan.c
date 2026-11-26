/* setup_obiwan.c -- "Instalar-Obi-Wan.exe": installer for normal users (Claude, 2026-11-26).
 *
 * The game's translated code can't be shared, so this installer builds it on the player's PC:
 *   1. unpacks the port's source and a small Python 3.12 + capstone (resource 300, payload.zip);
 *   2. checks the player's ISO is Star Wars: Obi-Wan (USA) (tools/instalar_obiwan.py --comprobar-iso);
 *   3. if missing, downloads Microsoft's Visual Studio 2022 Build Tools (C++) from Microsoft, checks
 *      it is signed by Microsoft and installs it (asks for administrator permission once);
 *   4. runs tools/instalar_obiwan.py (extract game files, translate code to C, compile);
 *   5. creates shortcuts and an entry in "Installed apps" (uninstall: /desinstalar).
 * Nothing of the game is inside this exe.
 *
 * Build: tools\build_setup_obiwan.cmd
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <urlmon.h>
#include <wintrust.h>
#include <softpub.h>
#include <wincrypt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define APP_VERSION   L"0.2.0"
#define APP_NAME      L"Star Wars Obi-Wan (PC)"
#define FOLDER_NAME   L"Star Wars Obi-Wan PC"
#define UNINST_KEY    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ObiWanPCPort"
#define MARKER        L"obiwan_instalacion.txt"
#define UNINST_EXE    L"Desinstalar Obi-Wan.exe"
#define VS_URL        L"https://aka.ms/vs/17/release/vs_BuildTools.exe"
#define VS_LICENSE    L"https://visualstudio.microsoft.com/license-terms/"
#define RES_PAYLOAD   300
#define NEED_BYTES    (4ull << 30)

#define WM_APP_LOG    (WM_APP + 1)
#define WM_APP_STATUS (WM_APP + 2)
#define WM_APP_DONE   (WM_APP + 3)
#define WM_APP_ASK    (WM_APP + 4)

enum { ID_ISO = 100, ID_ISO_BTN, ID_DIR, ID_DIR_BTN, ID_DESKTOP, ID_STATUS, ID_PROGRESS, ID_LOG, ID_GO, ID_CLOSE };

static BOOL g_es;
#define T(es, en) (g_es ? L##es : L##en)

static HWND g_wnd, g_iso, g_dir, g_desktop, g_status, g_progress, g_log, g_go, g_close, g_isobtn, g_dirbtn;
static HFONT g_font, g_title_font, g_mono_font;
static int g_dpi = 96;
static wchar_t g_iso_path[1024], g_dir_path[1024];
static BOOL g_want_desktop = TRUE;
static volatile LONG g_busy;
static BOOL g_installed;
static HANDLE g_job, g_logfile = INVALID_HANDLE_VALUE;

static int S(int v) { return MulDiv(v, g_dpi, 96); }

/* ---------------------------------------------------------------- log / UI messages (worker -> UI) */

static void log_file_write(const wchar_t *s)
{
    if (g_logfile == INVALID_HANDLE_VALUE) return;
    char buf[8192];
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, buf, (int)sizeof buf - 2, NULL, NULL);
    if (n <= 0) return;
    n--; buf[n++] = '\r'; buf[n++] = '\n';
    DWORD w; WriteFile(g_logfile, buf, (DWORD)n, &w, NULL);
}

static void say(const wchar_t *fmt, ...)
{
    wchar_t buf[4096];
    va_list ap; va_start(ap, fmt);
    _vsnwprintf_s(buf, 4096, _TRUNCATE, fmt, ap);
    va_end(ap);
    log_file_write(buf);
    PostMessageW(g_wnd, WM_APP_LOG, 0, (LPARAM)_wcsdup(buf));
}

static void status(const wchar_t *s)
{
    log_file_write(s);
    PostMessageW(g_wnd, WM_APP_STATUS, 0, (LPARAM)_wcsdup(s));
}

/* Ask the user a yes/no question from the worker thread (runs MessageBox on the UI thread). */
typedef struct { const wchar_t *text; UINT flags; HANDLE ev; int answer; } ask_t;
static int ask(const wchar_t *text, UINT flags)
{
    ask_t a;
    a.text = text; a.flags = flags; a.answer = 0;
    a.ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    PostMessageW(g_wnd, WM_APP_ASK, 0, (LPARAM)&a);
    WaitForSingleObject(a.ev, INFINITE);
    CloseHandle(a.ev);
    return a.answer;
}

/* ---------------------------------------------------------------- helpers */

static BOOL file_exists(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL dir_exists(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

/* Create dir and its parents. Returns 0 or the Windows error. */
static DWORD make_dirs(const wchar_t *dir)
{
    wchar_t p[1024];
    wcsncpy_s(p, 1024, dir, _TRUNCATE);
    for (wchar_t *s = p + 3; ; s++) {   /* p is "X:\..." */
        wchar_t c = *s;
        if (c != L'\\' && c != 0) continue;
        *s = 0;
        if (!dir_exists(p) && !CreateDirectoryW(p, NULL)) {
            DWORD e = GetLastError();
            if (e != ERROR_ALREADY_EXISTS || !dir_exists(p)) return e;
        }
        *s = c;
        if (!c) return 0;
    }
}

static BOOL is_ascii(const wchar_t *s)
{
    for (; *s; s++) if (*s > 0x7E) return FALSE;
    return TRUE;
}

/* Convert one line of child output: UTF-8 if valid, else the console code page. */
static void emit_line(const char *line, wchar_t *out, int cap)
{
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line, -1, out, cap);
    if (n <= 0) n = MultiByteToWideChar(CP_OEMCP, 0, line, -1, out, cap);
    if (n <= 0) out[0] = 0;
}

static void status_from_line(const wchar_t *w)
{
    if (!wcsncmp(w, L"Extracting ", 11))
        status(T("Copiando los archivos del juego desde tu ISO…", "Copying the game files from your ISO…"));
    else if (!wcsncmp(w, L"Translating ", 12))
        status(T("Traduciendo el código del juego a C (varios minutos)…", "Translating the game code to C (several minutes)…"));
    else if (!wcsncmp(w, L"Game / Juego", 12))
        status(T("Compilando el juego (puede tardar bastante)…", "Compiling the game (this can take a while)…"));
    else if (!wcsncmp(w, L"Launcher / Lanzador", 19))
        status(T("Compilando el lanzador…", "Compiling the launcher…"));
    else if (!wcsncmp(w, L"Settings / Configurador", 23))
        status(T("Compilando el configurador…", "Compiling the settings tool…"));
}

/* Run a hidden child process, show its output in the log, return its exit code (or -1).
 * The child runs in a job: closing the installer stops it and everything it started.
 * Output is read until the main process ends (helpers such as mspdbsrv may keep the pipe open). */
static DWORD run(const wchar_t *cmdline, const wchar_t *cwd, wchar_t *capture, int capcap)
{
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE rd, wr;
    if (capture && capcap) capture[0] = 0;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return (DWORD)-1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    STARTUPINFOW si = { sizeof si };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul; si.hStdOutput = wr; si.hStdError = wr;
    PROCESS_INFORMATION pi;
    wchar_t *cl = _wcsdup(cmdline);
    BOOL ok = CreateProcessW(NULL, cl, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                             NULL, cwd, &si, &pi);
    free(cl);
    CloseHandle(wr);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!ok) { CloseHandle(rd); return (DWORD)-1; }
    if (g_job) AssignProcessToJobObject(g_job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    static char line[16384];
    static wchar_t wline[16384];
    size_t n = 0;
    for (;;) {
        BOOL done = WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0;
        for (;;) {
            DWORD avail = 0, got = 0;
            char buf[4096];
            if (!PeekNamedPipe(rd, NULL, 0, NULL, &avail, NULL) || !avail) break;
            if (!ReadFile(rd, buf, avail < (DWORD)sizeof buf ? avail : (DWORD)sizeof buf, &got, NULL) || !got) break;
            for (DWORD i = 0; i < got; i++) {
                char c = buf[i];
                if (c == '\r') continue;
                if (c == '\n' || n == sizeof line - 1) {
                    line[n] = 0; n = 0;
                    emit_line(line, wline, 16384);
                    say(L"%s", wline);
                    status_from_line(wline);
                    if (capture) wcsncat_s(capture, capcap, wline, _TRUNCATE), wcsncat_s(capture, capcap, L"\n", _TRUNCATE);
                    if (c == '\n') continue;
                }
                line[n++] = c;
            }
        }
        if (done) break;
    }
    if (n) {
        line[n] = 0;
        emit_line(line, wline, 16384);
        say(L"%s", wline);
        if (capture) wcsncat_s(capture, capcap, wline, _TRUNCATE);
    }
    DWORD code = (DWORD)-1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(rd);
    return code;
}

/* ---------------------------------------------------------------- Visual Studio Build Tools */

static void vswhere_path(wchar_t *out, int cap)
{
    wchar_t pf[MAX_PATH];
    if (!GetEnvironmentVariableW(L"ProgramFiles(x86)", pf, MAX_PATH)) wcscpy_s(pf, MAX_PATH, L"C:\\Program Files (x86)");
    _snwprintf_s(out, cap, _TRUNCATE, L"%s\\Microsoft Visual Studio\\Installer\\vswhere.exe", pf);
}

/* First line of vswhere output for the given arguments, or empty. */
static BOOL vswhere(const wchar_t *args, wchar_t *out, int cap)
{
    wchar_t exe[MAX_PATH], cmd[2048];
    static wchar_t cap_buf[8192];
    out[0] = 0;
    vswhere_path(exe, MAX_PATH);
    if (!file_exists(exe)) return FALSE;
    _snwprintf_s(cmd, 2048, _TRUNCATE, L"\"%s\" -nologo %s", exe, args);
    if (run(cmd, NULL, cap_buf, 8192) != 0) return FALSE;
    wchar_t *nl = wcschr(cap_buf, L'\n');
    if (nl) *nl = 0;
    wcsncpy_s(out, cap, cap_buf, _TRUNCATE);
    return out[0] != 0;
}

#define VS_COMPONENTS L"Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.CMake.Project"

static BOOL find_vs(wchar_t *out, int cap)
{
    return vswhere(L"-version [17.0,18.0) -products * -requires " VS_COMPONENTS L" -property installationPath", out, cap);
}

static BOOL signed_by_microsoft(const wchar_t *path)
{
    WINTRUST_FILE_INFO fi = { sizeof fi };
    fi.pcwszFilePath = path;
    WINTRUST_DATA wd = { sizeof wd };
    wd.dwUIChoice = WTD_UI_NONE;
    wd.fdwRevocationChecks = WTD_REVOKE_NONE;
    wd.dwUnionChoice = WTD_CHOICE_FILE;
    wd.pFile = &fi;
    wd.dwStateAction = WTD_STATEACTION_VERIFY;
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    BOOL ok = FALSE;
    if (WinVerifyTrust(NULL, &action, &wd) == ERROR_SUCCESS) {
        CRYPT_PROVIDER_DATA *pd = WTHelperProvDataFromStateData(wd.hWVTStateData);
        CRYPT_PROVIDER_SGNR *sg = pd ? WTHelperGetProvSignerFromChain(pd, 0, FALSE, 0) : NULL;
        CRYPT_PROVIDER_CERT *pc = sg ? WTHelperGetProvCertFromChain(sg, 0) : NULL;
        if (pc && pc->pCert) {
            wchar_t name[256];
            CertGetNameStringW(pc->pCert, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, name, 256);
            ok = wcscmp(name, L"Microsoft Corporation") == 0;
        }
    }
    wd.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &action, &wd);
    return ok;
}

static BOOL install_vs(wchar_t *vs, int cap)
{
    if (ask(T("Falta el compilador de Microsoft (Visual Studio 2022 Build Tools, la parte de C++).\n\n"
              "El instalador lo descargará de Microsoft (varios GB) y lo instalará. Windows te pedirá permiso "
              "de administrador una vez. Puede tardar bastante.\n\n"
              "Al continuar aceptas la licencia de Microsoft:\n" VS_LICENSE "\n\n¿Continuar?",
              "Microsoft's compiler is missing (Visual Studio 2022 Build Tools, the C++ part).\n\n"
              "The installer will download it from Microsoft (several GB) and install it. Windows will ask for "
              "administrator permission once. It can take a while.\n\n"
              "By continuing you accept Microsoft's license:\n" VS_LICENSE "\n\nContinue?"),
            MB_YESNO | MB_ICONQUESTION) != IDYES)
        return FALSE;

    status(T("Descargando el instalador de Microsoft…", "Downloading Microsoft's installer…"));
    wchar_t tmp[MAX_PATH], boot[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    _snwprintf_s(boot, MAX_PATH, _TRUNCATE, L"%svs_BuildTools_obiwan.exe", tmp);
    DeleteFileW(boot);
    HRESULT hr = URLDownloadToFileW(NULL, VS_URL, boot, 0, NULL);
    if (FAILED(hr) || !file_exists(boot)) {
        say(T("No se ha podido descargar %s (error 0x%08X). ¿Hay Internet?", "Could not download %s (error 0x%08X). Is there Internet?"),
            VS_URL, (unsigned)hr);
        return FALSE;
    }
    if (!signed_by_microsoft(boot)) {
        say(T("El archivo descargado no tiene la firma de Microsoft; no lo ejecuto.",
              "The downloaded file is not signed by Microsoft; not running it."));
        DeleteFileW(boot);
        return FALSE;
    }
    say(T("Instalador de Microsoft descargado y con firma de Microsoft.", "Microsoft's installer downloaded and signed by Microsoft."));

    /* An existing Build Tools 2022 without the C++ parts is modified; otherwise install. */
    wchar_t existing[1024], params[2048];
    vswhere(L"-version [17.0,18.0) -products Microsoft.VisualStudio.Product.BuildTools -property installationPath", existing, 1024);
    if (existing[0])
        _snwprintf_s(params, 2048, _TRUNCATE, L"modify --installPath \"%s\" --add Microsoft.VisualStudio.Workload.VCTools "
                     L"--add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended --passive --wait --norestart", existing);
    else
        wcscpy_s(params, 2048, L"--add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.CMake.Project "
                               L"--includeRecommended --passive --wait --norestart");

    status(T("Instalando el compilador de Microsoft (sale su propia ventana; puede tardar bastante)…",
             "Installing Microsoft's compiler (it shows its own window; this can take a while)…"));
    SHELLEXECUTEINFOW se = { sizeof se };
    se.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    se.lpVerb = L"runas";
    se.lpFile = boot;
    se.lpParameters = params;
    se.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&se) || !se.hProcess) {
        say(T("No se ha podido iniciar el instalador de Microsoft (¿has dicho que no al permiso de administrador?).",
              "Could not start Microsoft's installer (did you decline the administrator prompt?)."));
        return FALSE;
    }
    if (g_job) AssignProcessToJobObject(g_job, se.hProcess);
    WaitForSingleObject(se.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(se.hProcess, &code);
    CloseHandle(se.hProcess);
    DeleteFileW(boot);
    say(T("El instalador de Microsoft ha terminado (código %lu).", "Microsoft's installer finished (code %lu)."), code);
    if (code == 3010)
        say(T("Microsoft recomienda reiniciar el PC más tarde; sigo.", "Microsoft recommends restarting the PC later; continuing."));
    if (!find_vs(vs, cap)) {
        say(T("Aun así no se encuentra el compilador de C++ de Visual Studio 2022.", "Visual Studio 2022's C++ compiler still not found."));
        return FALSE;
    }
    return TRUE;
}

/* ---------------------------------------------------------------- payload, shortcuts, uninstall entry */

static BOOL unpack_payload(const wchar_t *dir)
{
    HRSRC r = FindResourceW(NULL, MAKEINTRESOURCEW(RES_PAYLOAD), RT_RCDATA);
    HGLOBAL g = r ? LoadResource(NULL, r) : NULL;
    const void *p = g ? LockResource(g) : NULL;
    DWORD sz = r ? SizeofResource(NULL, r) : 0;
    if (!p || !sz) { say(L"payload missing"); return FALSE; }
    wchar_t zip[1024], sys[MAX_PATH], cmd[4096];
    _snwprintf_s(zip, 1024, _TRUNCATE, L"%s\\_instalador.zip", dir);
    HANDLE f = CreateFileW(zip, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { say(T("No se puede escribir en %s", "Cannot write to %s"), dir); return FALSE; }
    DWORD w = 0;
    BOOL ok = WriteFile(f, p, sz, &w, NULL) && w == sz;
    CloseHandle(f);
    if (!ok) { DeleteFileW(zip); say(T("No se puede escribir en %s", "Cannot write to %s"), dir); return FALSE; }
    GetSystemDirectoryW(sys, MAX_PATH);
    _snwprintf_s(cmd, 4096, _TRUNCATE, L"\"%s\\tar.exe\" -xf \"%s\" -C \"%s\"", sys, zip, dir);
    DWORD code = run(cmd, dir, NULL, 0);
    DeleteFileW(zip);
    if (code != 0) { say(T("Falló la descompresión (tar.exe, código %ld).", "Unpacking failed (tar.exe, code %ld)."), (long)code); return FALSE; }
    return TRUE;
}

static BOOL make_link(const wchar_t *lnk, const wchar_t *target, const wchar_t *workdir, const wchar_t *args)
{
    IShellLinkW *sl = NULL;
    IPersistFile *pf = NULL;
    BOOL ok = FALSE;
    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl))) {
        IShellLinkW_SetPath(sl, target);
        IShellLinkW_SetWorkingDirectory(sl, workdir);
        if (args) IShellLinkW_SetArguments(sl, args);
        if (SUCCEEDED(IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf))) {
            ok = SUCCEEDED(IPersistFile_Save(pf, lnk, TRUE));
            IPersistFile_Release(pf);
        }
        IShellLinkW_Release(sl);
    }
    return ok;
}

static void known_folder(REFKNOWNFOLDERID id, wchar_t *out, int cap)
{
    PWSTR p = NULL;
    out[0] = 0;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, NULL, &p))) wcsncpy_s(out, cap, p, _TRUNCATE);
    CoTaskMemFree(p);
}

static void make_shortcuts(const wchar_t *dir, BOOL desktop)
{
    wchar_t base[MAX_PATH], menu[1024], lnk[1024], exe[1024];
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    known_folder(&FOLDERID_Programs, base, MAX_PATH);
    _snwprintf_s(menu, 1024, _TRUNCATE, L"%s\\%s", base, APP_NAME);
    CreateDirectoryW(menu, NULL);
    _snwprintf_s(exe, 1024, _TRUNCATE, L"%s\\Jugar Obi-Wan.exe", dir);
    _snwprintf_s(lnk, 1024, _TRUNCATE, L"%s\\%s.lnk", menu, T("Jugar Obi-Wan", "Play Obi-Wan"));
    make_link(lnk, exe, dir, NULL);
    _snwprintf_s(exe, 1024, _TRUNCATE, L"%s\\Configurar Obi-Wan.exe", dir);
    _snwprintf_s(lnk, 1024, _TRUNCATE, L"%s\\%s.lnk", menu, T("Configurar Obi-Wan", "Obi-Wan settings"));
    make_link(lnk, exe, dir, NULL);
    _snwprintf_s(exe, 1024, _TRUNCATE, L"%s\\" UNINST_EXE, dir);
    _snwprintf_s(lnk, 1024, _TRUNCATE, L"%s\\%s.lnk", menu, T("Desinstalar Obi-Wan", "Uninstall Obi-Wan"));
    make_link(lnk, exe, dir, L"/desinstalar");
    if (desktop) {
        known_folder(&FOLDERID_Desktop, base, MAX_PATH);
        _snwprintf_s(exe, 1024, _TRUNCATE, L"%s\\Jugar Obi-Wan.exe", dir);
        _snwprintf_s(lnk, 1024, _TRUNCATE, L"%s\\%s.lnk", base, APP_NAME);
        if (!make_link(lnk, exe, dir, NULL))
            say(T("No se ha podido crear el acceso directo del escritorio (¿«Acceso controlado a carpetas» activado?). "
                  "Tienes el juego en el menú Inicio.",
                  "Could not create the desktop shortcut (is \"Controlled folder access\" on?). "
                  "The game is in the Start menu."));
    }
    CoUninitialize();
}

static void reg_str(HKEY k, const wchar_t *name, const wchar_t *v)
{
    RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)v, (DWORD)((wcslen(v) + 1) * sizeof(wchar_t)));
}

static void register_uninstall(const wchar_t *dir)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, UNINST_KEY, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL)) return;
    wchar_t v[2048];
    reg_str(k, L"DisplayName", APP_NAME);
    reg_str(k, L"DisplayVersion", APP_VERSION);
    reg_str(k, L"Publisher", L"The128ReBits (fan project)");
    reg_str(k, L"InstallLocation", dir);
    _snwprintf_s(v, 2048, _TRUNCATE, L"\"%s\\" UNINST_EXE L"\" /desinstalar", dir);
    reg_str(k, L"UninstallString", v);
    _snwprintf_s(v, 2048, _TRUNCATE, L"%s\\" UNINST_EXE, dir);
    reg_str(k, L"DisplayIcon", v);
    DWORD one = 1, kb = 3700u * 1024u;
    RegSetValueExW(k, L"NoModify", 0, REG_DWORD, (const BYTE *)&one, sizeof one);
    RegSetValueExW(k, L"NoRepair", 0, REG_DWORD, (const BYTE *)&one, sizeof one);
    RegSetValueExW(k, L"EstimatedSize", 0, REG_DWORD, (const BYTE *)&kb, sizeof kb);
    RegCloseKey(k);
}

/* Version written in the marker by a previous install (empty if none). */
static void read_marker(const wchar_t *dir, wchar_t *ver, int cap)
{
    wchar_t p[1024];
    char buf[256] = { 0 };
    ver[0] = 0;
    _snwprintf_s(p, 1024, _TRUNCATE, L"%s\\" MARKER, dir);
    HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    DWORD got = 0;
    ReadFile(f, buf, (DWORD)sizeof buf - 1, &got, NULL);
    CloseHandle(f);
    char *v = strstr(buf, "version=");
    if (!v) return;
    v += 8;
    char *e = v; while (*e && *e != '\r' && *e != '\n') e++;
    *e = 0;
    MultiByteToWideChar(CP_UTF8, 0, v, -1, ver, cap);
}

static void write_marker(const wchar_t *dir)
{
    wchar_t p[1024];
    _snwprintf_s(p, 1024, _TRUNCATE, L"%s\\" MARKER, dir);
    HANDLE f = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    char buf[512];
    int n = _snprintf_s(buf, sizeof buf, _TRUNCATE,
                        "Star Wars Obi-Wan PC port, installed by Instalar-Obi-Wan.exe\r\nversion=%ls\r\n"
                        "The uninstaller deletes this whole folder.\r\n", APP_VERSION);
    DWORD w; WriteFile(f, buf, (DWORD)n, &w, NULL);
    CloseHandle(f);
}

/* ---------------------------------------------------------------- the install itself (worker thread) */

static BOOL do_install(void)
{
    const wchar_t *dir = g_dir_path;
    wchar_t py[1024], cmd[4096], vs[1024], prev[64];

    status(T("Preparando la carpeta…", "Preparing the folder…"));
    DWORD r = make_dirs(dir);
    if (r) {
        say(T("Windows no deja crear la carpeta %s (error %lu). Si tienes activado «Acceso controlado a carpetas», "
              "Documentos, Escritorio, Imágenes… están protegidas: elige otra carpeta (la que sale por defecto vale).",
              "Windows does not allow creating the folder %s (error %lu). If \"Controlled folder access\" is on, "
              "Documents, Desktop, Pictures… are protected: pick another folder (the default one works)."), dir, r);
        return FALSE;
    }
    wchar_t logp[1024];
    _snwprintf_s(logp, 1024, _TRUNCATE, L"%s\\instalacion.log", dir);
    g_logfile = CreateFileW(logp, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    say(L"Instalar-Obi-Wan " APP_VERSION L" -> %s", dir);

    ULARGE_INTEGER freeb;
    if (GetDiskFreeSpaceExW(dir, &freeb, NULL, NULL) && freeb.QuadPart < NEED_BYTES) {
        say(T("No hay espacio: hacen falta unos 4 GB libres en esa unidad y hay %.1f GB.",
              "Not enough space: about 4 GB free are needed on that drive and there are %.1f GB."),
            freeb.QuadPart / 1073741824.0);
        return FALSE;
    }
    read_marker(dir, prev, 64);

    status(T("Copiando los archivos del instalador…", "Copying the installer files…"));
    if (!unpack_payload(dir)) return FALSE;
    _snwprintf_s(py, 1024, _TRUNCATE, L"%s\\python\\python.exe", dir);
    if (!file_exists(py)) { say(L"python\\python.exe missing after unpacking"); return FALSE; }

    status(T("Comprobando tu ISO…", "Checking your ISO…"));
    _snwprintf_s(cmd, 4096, _TRUNCATE, L"\"%s\" -B tools\\instalar_obiwan.py --comprobar-iso --iso \"%s\"", py, g_iso_path);
    if (run(cmd, dir, NULL, 0) != 0) {
        say(T("Esa ISO no sirve. De momento solo funciona Star Wars: Obi-Wan (USA).",
              "That ISO can't be used. For now only Star Wars: Obi-Wan (USA) works."));
        return FALSE;
    }

    status(T("Buscando el compilador de Microsoft…", "Looking for Microsoft's compiler…"));
    if (find_vs(vs, 1024))
        say(L"Visual Studio: %s", vs);
    else if (!install_vs(vs, 1024))
        return FALSE;

    /* A different version installed before: translate the game code again with the new tools. */
    BOOL regen = prev[0] && wcscmp(prev, APP_VERSION) != 0;
    _snwprintf_s(cmd, 4096, _TRUNCATE, L"\"%s\" -B tools\\instalar_obiwan.py --iso \"%s\"%s", py, g_iso_path,
                 regen ? L" --regenerar" : L"");
    status(T("Preparando el juego (esto tarda: copia, traduce y compila)…", "Preparing the game (this takes a while: copy, translate, compile)…"));
    DWORD code = run(cmd, dir, NULL, 0);
    if (code != 0) {
        say(T("Ha fallado la preparación del juego (código %ld). Si abres una issue en GitHub, adjunta %s",
              "Preparing the game failed (code %ld). If you open an issue on GitHub, attach %s"), (long)code, logp);
        return FALSE;
    }
    wchar_t game[1024];
    _snwprintf_s(game, 1024, _TRUNCATE, L"%s\\Jugar Obi-Wan.exe", dir);
    if (!file_exists(game)) { say(L"Jugar Obi-Wan.exe missing"); return FALSE; }

    status(T("Creando accesos directos…", "Creating shortcuts…"));
    wchar_t self[1024], un[1024];
    GetModuleFileNameW(NULL, self, 1024);
    _snwprintf_s(un, 1024, _TRUNCATE, L"%s\\" UNINST_EXE, dir);
    if (_wcsicmp(self, un) != 0) CopyFileW(self, un, FALSE);
    write_marker(dir);
    make_shortcuts(dir, g_want_desktop);
    register_uninstall(dir);
    return TRUE;
}

static DWORD WINAPI worker(LPVOID unused)
{
    (void)unused;
    BOOL ok = do_install();
    if (g_logfile != INVALID_HANDLE_VALUE) { CloseHandle(g_logfile); g_logfile = INVALID_HANDLE_VALUE; }
    PostMessageW(g_wnd, WM_APP_DONE, ok, 0);
    return 0;
}

/* ---------------------------------------------------------------- uninstall (/desinstalar) */

static int uninstall(void)
{
    wchar_t self[1024], dir[1024], marker[1024], base[MAX_PATH], p[1024];
    GetModuleFileNameW(NULL, self, 1024);
    wcscpy_s(dir, 1024, self);
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) *slash = 0;
    _snwprintf_s(marker, 1024, _TRUNCATE, L"%s\\" MARKER, dir);
    if (!file_exists(marker)) {
        MessageBoxW(NULL, T("Esta carpeta no parece una instalación de Obi-Wan; no borro nada.",
                            "This folder does not look like an Obi-Wan install; nothing deleted."), APP_NAME, MB_ICONERROR);
        return 1;
    }
    wchar_t q[2048];
    _snwprintf_s(q, 2048, _TRUNCATE,
                 T("Se borrará la carpeta\n%s\ncon todo lo que contiene (los archivos del juego copiados de tu ISO y lo compilado), "
                   "los accesos directos y la entrada de «Aplicaciones instaladas».\n\n"
                   "Tus partidas guardadas y tu ISO no se tocan.\n\n¿Desinstalar?",
                   "This deletes the folder\n%s\nwith everything in it (game files copied from your ISO and the compiled game), "
                   "the shortcuts and the \"Installed apps\" entry.\n\nYour saved games and your ISO are not touched.\n\nUninstall?"),
                 dir);
    if (MessageBoxW(NULL, q, APP_NAME, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return 0;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    known_folder(&FOLDERID_Desktop, base, MAX_PATH);
    _snwprintf_s(p, 1024, _TRUNCATE, L"%s\\%s.lnk", base, APP_NAME);
    DeleteFileW(p);
    known_folder(&FOLDERID_Programs, base, MAX_PATH);
    const wchar_t *links[] = { L"Jugar Obi-Wan", L"Play Obi-Wan", L"Configurar Obi-Wan", L"Obi-Wan settings",
                               L"Desinstalar Obi-Wan", L"Uninstall Obi-Wan" };
    for (int i = 0; i < 6; i++) {
        _snwprintf_s(p, 1024, _TRUNCATE, L"%s\\%s\\%s.lnk", base, APP_NAME, links[i]);
        DeleteFileW(p);
    }
    _snwprintf_s(p, 1024, _TRUNCATE, L"%s\\%s", base, APP_NAME);
    RemoveDirectoryW(p);
    CoUninitialize();
    RegDeleteKeyW(HKEY_CURRENT_USER, UNINST_KEY);

    MessageBoxW(NULL, T("Obi-Wan se ha desinstalado.", "Obi-Wan has been uninstalled."), APP_NAME, MB_ICONINFORMATION);

    /* This exe lives in the folder: a hidden cmd deletes it once we have exited (it retries for ~40 s,
       because the exe cannot be deleted while it is still running). */
    wchar_t sys[MAX_PATH], tmp[MAX_PATH], cmd[2048];
    GetSystemDirectoryW(sys, MAX_PATH);
    GetTempPathW(MAX_PATH, tmp);
    _snwprintf_s(cmd, 2048, _TRUNCATE,
                 L"\"%s\\cmd.exe\" /d /c for /l %%i in (1,1,20) do @(ping -n 3 127.0.0.1 >nul & rmdir /s /q \"%s\" 2>nul & if not exist \"%s\" exit)",
                 sys, dir, dir);
    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, tmp, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return 0;
}

/* ---------------------------------------------------------------- window */

static void append_log(const wchar_t *s)
{
    int len = GetWindowTextLengthW(g_log);
    if (len > 3000000) { SetWindowTextW(g_log, L""); len = 0; }
    SendMessageW(g_log, EM_SETSEL, len, len);
    SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)s);
    len = GetWindowTextLengthW(g_log);
    SendMessageW(g_log, EM_SETSEL, len, len);
    SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
}

static void set_enabled(BOOL on)
{
    EnableWindow(g_iso, on); EnableWindow(g_isobtn, on);
    EnableWindow(g_dir, on); EnableWindow(g_dirbtn, on);
    EnableWindow(g_desktop, on); EnableWindow(g_go, on);
}

static void pick_iso(HWND h)
{
    wchar_t file[1024] = { 0 };
    GetWindowTextW(g_iso, file, 1024);
    OPENFILENAMEW of = { sizeof of };
    of.hwndOwner = h;
    of.lpstrFilter = L"Xbox ISO (*.iso)\0*.iso\0*.*\0*.*\0";
    of.lpstrFile = file;
    of.nMaxFile = 1024;
    of.lpstrTitle = T("Elige tu ISO de Star Wars: Obi-Wan (USA)", "Pick your Star Wars: Obi-Wan (USA) ISO");
    of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&of)) SetWindowTextW(g_iso, file);
}

static void pick_dir(HWND h)
{
    IFileOpenDialog *d = NULL;
    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void **)&d))) return;
    DWORD opt = 0;
    IFileOpenDialog_GetOptions(d, &opt);
    IFileOpenDialog_SetOptions(d, opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    IFileOpenDialog_SetTitle(d, T("¿Dónde lo instalo?", "Where should it be installed?"));
    if (SUCCEEDED(IFileOpenDialog_Show(d, h))) {
        IShellItem *it = NULL;
        if (SUCCEEDED(IFileOpenDialog_GetResult(d, &it))) {
            PWSTR p = NULL;
            if (SUCCEEDED(IShellItem_GetDisplayName(it, SIGDN_FILESYSPATH, &p))) {
                wchar_t out[1024];
                size_t n = wcslen(p);
                const wchar_t *sep = (n && p[n - 1] == L'\\') ? L"" : L"\\";
                const wchar_t *last = wcsrchr(p, L'\\');
                if (last && !_wcsicmp(last + 1, FOLDER_NAME)) wcsncpy_s(out, 1024, p, _TRUNCATE);
                else _snwprintf_s(out, 1024, _TRUNCATE, L"%s%s%s", p, sep, FOLDER_NAME);
                SetWindowTextW(g_dir, out);
                CoTaskMemFree(p);
            }
            IShellItem_Release(it);
        }
    }
    IFileOpenDialog_Release(d);
}

static void start_install(HWND h)
{
    GetWindowTextW(g_iso, g_iso_path, 1024);
    GetWindowTextW(g_dir, g_dir_path, 1024);
    size_t n = wcslen(g_dir_path);
    while (n && (g_dir_path[n - 1] == L'\\' || g_dir_path[n - 1] == L' ')) g_dir_path[--n] = 0;
    g_want_desktop = SendMessageW(g_desktop, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (!file_exists(g_iso_path)) {
        MessageBoxW(h, T("Elige primero tu ISO de Star Wars: Obi-Wan (USA).", "First pick your Star Wars: Obi-Wan (USA) ISO."),
                    APP_NAME, MB_ICONWARNING);
        return;
    }
    if (n < 4 || g_dir_path[1] != L':') {
        MessageBoxW(h, T("Elige una carpeta de instalación completa (por ejemplo C:\\Juegos\\" FOLDER_NAME L").",
                         "Pick a full install folder (for example C:\\Games\\" FOLDER_NAME L")."), APP_NAME, MB_ICONWARNING);
        return;
    }
    if (!is_ascii(g_dir_path) &&
        MessageBoxW(h, T("La carpeta tiene tildes, eñes u otros caracteres especiales y la compilación podría fallar. "
                         "Mejor algo como C:\\Juegos\\" FOLDER_NAME L". ¿Seguir de todas formas?",
                         "The folder has accents or other special characters and compiling might fail. "
                         "Better something like C:\\Games\\" FOLDER_NAME L". Continue anyway?"),
                    APP_NAME, MB_YESNO | MB_ICONWARNING) != IDYES)
        return;
    set_enabled(FALSE);
    SetWindowTextW(g_log, L"");
    SendMessageW(g_progress, PBM_SETMARQUEE, TRUE, 30);
    InterlockedExchange(&g_busy, 1);
    HANDLE t = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (t) CloseHandle(t);
}

static HWND add(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id, HFONT font)
{
    HWND c = CreateWindowExW(!wcscmp(cls, L"EDIT") ? WS_EX_CLIENTEDGE : 0, cls, text, WS_CHILD | WS_VISIBLE | style,
                             S(x), S(y), S(w), S(h), g_wnd, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)font, TRUE);
    return c;
}

static void default_dir(wchar_t *out, int cap)
{
    wchar_t base[MAX_PATH];
    known_folder(&FOLDERID_UserProgramFiles, base, MAX_PATH);
    if (base[0] && is_ascii(base)) _snwprintf_s(out, cap, _TRUNCATE, L"%s\\%s", base, FOLDER_NAME);
    else _snwprintf_s(out, cap, _TRUNCATE, L"C:\\%s\\%s", T("Juegos", "Games"), FOLDER_NAME);
}

static void create_controls(void)
{
    add(L"STATIC", APP_NAME, 0, 16, 12, 590, 30, 0, g_title_font);
    add(L"STATIC", T("Prepara el juego a partir de tu propia ISO de Star Wars: Obi-Wan (USA). Este instalador no incluye "
                     "el juego: lo copia de tu ISO y lo compila en tu PC. Si falta el compilador de Microsoft lo descarga "
                     "(hace falta Internet). Necesitas unos 4 GB libres y tarda un rato.",
                     "Prepares the game from your own Star Wars: Obi-Wan (USA) ISO. This installer does not include the "
                     "game: it copies it from your ISO and compiles it on your PC. If Microsoft's compiler is missing it "
                     "downloads it (Internet needed). You need about 4 GB free and it takes a while."),
        0, 16, 46, 590, 66, 0, g_font);
    add(L"STATIC", T("Tu ISO de Star Wars: Obi-Wan (USA):", "Your Star Wars: Obi-Wan (USA) ISO:"), 0, 16, 120, 590, 18, 0, g_font);
    g_iso = add(L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, 16, 140, 480, 26, ID_ISO, g_font);
    g_isobtn = add(L"BUTTON", T("Buscar…", "Browse…"), BS_PUSHBUTTON | WS_TABSTOP, 504, 139, 102, 28, ID_ISO_BTN, g_font);
    add(L"STATIC", T("Carpeta de instalación:", "Install folder:"), 0, 16, 176, 590, 18, 0, g_font);
    wchar_t d[1024];
    default_dir(d, 1024);
    g_dir = add(L"EDIT", d, ES_AUTOHSCROLL | WS_TABSTOP, 16, 196, 480, 26, ID_DIR, g_font);
    g_dirbtn = add(L"BUTTON", T("Cambiar…", "Change…"), BS_PUSHBUTTON | WS_TABSTOP, 504, 195, 102, 28, ID_DIR_BTN, g_font);
    g_desktop = add(L"BUTTON", T("Crear acceso directo en el escritorio", "Create a desktop shortcut"),
                    BS_AUTOCHECKBOX | WS_TABSTOP, 16, 232, 590, 22, ID_DESKTOP, g_font);
    SendMessageW(g_desktop, BM_SETCHECK, BST_CHECKED, 0);
    g_status = add(L"STATIC", T("Elige tu ISO y pulsa Instalar.", "Pick your ISO and press Install."), SS_ENDELLIPSIS,
                   16, 264, 590, 20, ID_STATUS, g_font);
    g_progress = add(PROGRESS_CLASSW, L"", PBS_MARQUEE, 16, 288, 590, 14, ID_PROGRESS, g_font);
    g_log = add(L"EDIT", L"", ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_HSCROLL | ES_AUTOHSCROLL,
                16, 310, 590, 200, ID_LOG, g_mono_font);
    SendMessageW(g_log, EM_SETLIMITTEXT, 0, 0);
    g_go = add(L"BUTTON", T("Instalar", "Install"), BS_DEFPUSHBUTTON | WS_TABSTOP, 396, 522, 102, 30, ID_GO, g_font);
    g_close = add(L"BUTTON", T("Cerrar", "Close"), BS_PUSHBUTTON | WS_TABSTOP, 504, 522, 102, 30, ID_CLOSE, g_font);
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_CREATE:
        g_wnd = h;
        create_controls();
        return 0;
    case WM_CTLCOLORSTATIC:
        if ((HWND)lp != g_log) {
            SetBkMode((HDC)wp, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }
        break;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ISO_BTN: pick_iso(h); return 0;
        case ID_DIR_BTN: pick_dir(h); return 0;
        case ID_GO:
            if (g_installed) {
                wchar_t exe[1024];
                _snwprintf_s(exe, 1024, _TRUNCATE, L"%s\\Jugar Obi-Wan.exe", g_dir_path);
                ShellExecuteW(h, L"open", exe, NULL, g_dir_path, SW_SHOWNORMAL);
                DestroyWindow(h);
            } else if (!g_busy) start_install(h);
            return 0;
        case ID_CLOSE: SendMessageW(h, WM_CLOSE, 0, 0); return 0;
        }
        break;
    case WM_APP_LOG:
        append_log((wchar_t *)lp);
        free((void *)lp);
        return 0;
    case WM_APP_STATUS:
        SetWindowTextW(g_status, (wchar_t *)lp);
        free((void *)lp);
        return 0;
    case WM_APP_ASK: {
        ask_t *a = (ask_t *)lp;
        a->answer = MessageBoxW(h, a->text, APP_NAME, a->flags);
        SetEvent(a->ev);
        return 0;
    }
    case WM_APP_DONE:
        InterlockedExchange(&g_busy, 0);
        SendMessageW(g_progress, PBM_SETMARQUEE, FALSE, 0);
        if (wp) {
            g_installed = TRUE;
            SetWindowLongPtrW(g_progress, GWL_STYLE, GetWindowLongPtrW(g_progress, GWL_STYLE) & ~PBS_MARQUEE);
            SendMessageW(g_progress, PBM_SETRANGE32, 0, 100);
            SendMessageW(g_progress, PBM_SETPOS, 100, 0);
            SetWindowTextW(g_status, T("¡Listo! Ya puedes jugar.", "Done! You can play now."));
            SetWindowTextW(g_go, T("Jugar", "Play"));
            EnableWindow(g_go, TRUE);
            MessageBoxW(h, T("Instalación terminada. Abre el juego con el acceso directo «" APP_NAME L"» o con el botón Jugar.",
                             "Installation finished. Start the game with the \"" APP_NAME L"\" shortcut or the Play button."),
                        APP_NAME, MB_ICONINFORMATION);
        } else {
            SetWindowTextW(g_status, T("No se ha podido terminar. Mira el registro de abajo.", "Could not finish. See the log below."));
            set_enabled(TRUE);
            SetWindowTextW(g_go, T("Reintentar", "Retry"));
        }
        return 0;
    case WM_CLOSE:
        if (g_busy && MessageBoxW(h, T("La instalación no ha terminado. ¿Cancelarla y cerrar?",
                                       "The installation has not finished. Cancel it and close?"),
                                  APP_NAME, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
            return 0;
        if (g_job) TerminateJobObject(g_job, 1);
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmdline, int show)
{
    (void)prev;
    g_es = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_SPANISH;
    if (wcsstr(cmdline, L"/desinstalar")) return uninstall();
    if (wcsstr(cmdline, L"/comprobar-firma ")) { /* test hook: exit 0 if the file is signed by Microsoft */
        wchar_t f[1024];
        wcsncpy_s(f, 1024, wcsstr(cmdline, L"/comprobar-firma ") + 17, _TRUNCATE);
        wchar_t *p = f;
        if (*p == L'"') p++;
        size_t n = wcslen(p);
        if (n && p[n - 1] == L'"') p[n - 1] = 0;
        return signed_by_microsoft(p) ? 0 : 1;
    }

    SetEnvironmentVariableW(L"PYTHONHOME", NULL);
    SetEnvironmentVariableW(L"PYTHONPATH", NULL);
    SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");
    SetEnvironmentVariableW(L"PYTHONNOUSERSITE", L"1");

    g_job = CreateJobObjectW(NULL, NULL);
    if (g_job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li = { 0 };
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &li, sizeof li);
    }
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    HDC dc = GetDC(NULL);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    NONCLIENTMETRICSW ncm = { sizeof ncm };
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof ncm, &ncm, 0);
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW lf = ncm.lfMessageFont;
    lf.lfHeight = -S(20);
    lf.lfWeight = FW_SEMIBOLD;
    g_title_font = CreateFontIndirectW(&lf);
    lf = ncm.lfMessageFont;
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Consolas");
    g_mono_font = CreateFontIndirectW(&lf);

    WNDCLASSEXW wc = { sizeof wc };
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = L"ObiWanSetup";
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    RegisterClassExW(&wc);
    RECT r = { 0 };
    r.right = S(622); r.bottom = S(566);
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&r, style, FALSE);
    wchar_t title[128];
    _snwprintf_s(title, 128, _TRUNCATE, L"%s - %s %s", T("Instalar", "Install"), APP_NAME, APP_VERSION);
    HWND h = CreateWindowExW(0, wc.lpszClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                             r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    ShowWindow(h, show);
    UpdateWindow(h);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!IsDialogMessageW(h, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (g_job) CloseHandle(g_job);
    CoUninitialize();
    return 0;
}
