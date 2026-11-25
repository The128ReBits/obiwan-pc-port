/* configurador_obiwan.c -- "Configurar Obi-Wan.exe" (Claude completo-4, 2026-11-06)
 *
 * Ventana para cambiar obiwan.ini sin editarlo a mano. No arranca nada por
 * su cuenta: solo "Jugar" (y "Jugar y medir") abren el lanzador 6, y solo
 * si los pulsa la persona que juega.
 *
 * Win32 + comctl32 v6, sin dependencias. Compilar con
 * tools\build_configurador_obiwan.cmd (cl /W4 /utf-8).
 *
 *   tools/obiwan_ajustes.h            tabla de ajustes (la comparte el lanzador 6)
 *   tools/configurador_obiwan_textos.h textos de la ventana (es, en)
 *   tools/configurador_obiwan_tareas.inc lo que se hace sin ventana (ISO, zip...)
 *   tools/configurador_obiwan_paginas.inc el contenido de cada página
 *
 * Línea de órdenes (para pruebas; no abren la ventana):
 *   --importar <iso> [--destino <carpeta>]   --identificar <iso>
 *   --verificar <carpeta>   --diagnostico <zip>   --dispositivos
 *   --captura <carpeta> [--dpi N] [--tema claro|oscuro] [--idioma es|en]
 *   --ini <ruta>  usa ese obiwan.ini */

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <tlhelp32.h>
#include <mmdeviceapi.h>
#include <propidl.h>
#include <functiondiscoverykeys_devpkey.h>
#include <xaudio2.h>
#include <xinput.h>
#include <wincodec.h>
#include <mmsystem.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "obiwan_ajustes.h"

#pragma warning(disable: 4204)   /* inicializadores no constantes (C) */
#pragma warning(disable: 4505)   /* funciones static que no se usan en esta herramienta */

/* ------------------------------------------------------------------ */
/* textos                                                              */
/* ------------------------------------------------------------------ */
enum {
#define TX(id, ...) id,
#include "configurador_obiwan_textos.h"
#undef TX
    T_N
};
static const char *const g_idiomas[] = { TX_IDIOMAS };
#define N_IDIOMAS ((int)(sizeof g_idiomas / sizeof g_idiomas[0]))
static const char *const g_tx[T_N][N_IDIOMAS] = {
#define TX(id, ...) { __VA_ARGS__ },
#include "configurador_obiwan_textos.h"
#undef TX
};

/* ------------------------------------------------------------------ */
/* estado global                                                       */
/* ------------------------------------------------------------------ */
static HINSTANCE g_inst;
static wchar_t g_base[MAX_PATH];        /* carpeta del configurador (la del port) */
static wchar_t g_local_raiz[MAX_PATH];  /* %LOCALAPPDATA% */
static wchar_t g_local[MAX_PATH];       /* %LOCALAPPDATA%\ObiWanRecomp-dev */
static wchar_t g_ini[MAX_PATH];
static AjConf g_conf, g_guardada;
static int g_lang;                      /* columna de g_tx */
static int g_dpi = 96, g_dpi_forzado;
static int g_oscuro, g_tema_forzado;
static int g_captura;
static HANDLE g_out = INVALID_HANDLE_VALUE;

#include "configurador_obiwan_tareas.inc"

static wchar_t *g_txw[T_N];
static const wchar_t *T(int id)
{
    if (id < 0 || id >= T_N) return L"";
    if (!g_txw[id]) {
        const char *s = g_tx[id][g_lang];
        int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
        g_txw[id] = (wchar_t *)malloc((size_t)(n > 0 ? n : 1) * sizeof(wchar_t));
        if (!g_txw[id]) return L"";
        if (n <= 0 || !MultiByteToWideChar(CP_UTF8, 0, s, -1, g_txw[id], n)) g_txw[id][0] = 0;
    }
    return g_txw[id];
}
static void textos_libres(void)
{
    int i;
    for (i = 0; i < T_N; i++) { free(g_txw[i]); g_txw[i] = NULL; }
}
/* UTF-8 -> UTF-16 en un anillo de 8 búferes (para usar en el momento). */
static const wchar_t *W(const char *s)
{
    static wchar_t r[8][1024]; static int k;
    k = (k + 1) & 7;
    u8w(s ? s : "", r[k], 1024);
    return r[k];
}
static void fmt(wchar_t *o, size_t n, int id, ...)
{
    va_list ap; va_start(ap, id);
    _vsnwprintf_s(o, n, _TRUNCATE, T(id), ap);
    va_end(ap);
}
/* textos de un ajuste: español en la columna 0; cualquier otra, inglés */
static const char *aj_nombre(const AjDef *d) { return g_lang == 0 ? d->es : d->en; }
static const char *aj_ayuda(const AjDef *d)  { return g_lang == 0 ? d->ayuda_es : d->ayuda_en; }
static const char *aj_opcs(const AjDef *d)   { return g_lang == 0 ? d->opc_es : d->opc_en; }
/* n-ésimo texto de la lista "a;b;c" */
static void campo_n(const char *lista, int n, wchar_t *o, size_t on)
{
    const char *p = lista, *e; char t[256]; int k = 0;
    o[0] = 0;
    while (p && *p) {
        e = strchr(p, ';'); if (!e) e = p + strlen(p);
        if (k == n) { size_t l = (size_t)(e - p); if (l >= sizeof t) l = sizeof t - 1; memcpy(t, p, l); t[l] = 0; u8w(t, o, (int)on); return; }
        k++; p = *e ? e + 1 : e;
    }
}
static int AJ(const char *sec, const char *clave)
{
    int i = aj_indice(sec, clave);
    if (i < 0) { /* la tabla y este programa se compilan juntos: no puede pasar */
        MessageBoxW(NULL, W(clave), L"obiwan_ajustes.h", MB_ICONERROR); ExitProcess(3);
    }
    return i;
}

/* salida de consola para la línea de órdenes */
static void consola(void)
{
    g_out = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!g_out || g_out == INVALID_HANDLE_VALUE || GetFileType(g_out) == FILE_TYPE_UNKNOWN) {
        if (AttachConsole(ATTACH_PARENT_PROCESS))
            g_out = CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    }
    if (g_out && g_out != INVALID_HANDLE_VALUE && GetFileType(g_out) == FILE_TYPE_CHAR) SetConsoleOutputCP(CP_UTF8);
}
static void sal(const wchar_t *f, ...)
{
    wchar_t w[4096]; char u[12288]; va_list ap; DWORD wr;
    va_start(ap, f); _vsnwprintf_s(w, 4096, _TRUNCATE, f, ap); va_end(ap);
    wu8(w, u, sizeof u);
    if (g_out && g_out != INVALID_HANDLE_VALUE) WriteFile(g_out, u, (DWORD)strlen(u), &wr, NULL);
}

/* ------------------------------------------------------------------ */
/* aspecto: tamaños, letras y colores                                  */
/* ------------------------------------------------------------------ */
#define S(x) MulDiv((x), g_dpi, 96)
enum { C_FONDO, C_PANEL, C_CAMPO, C_BOTON, C_BORDE, C_TEXTO, C_GRIS, C_ACENTO, C_AVISO, C_OK, C_ERROR,
       C_SEL, C_TEXTO_ACENTO, C_N };
static COLORREF g_col[C_N];
static HBRUSH g_br[C_N];
static HFONT g_f_normal, g_f_semi, g_f_seccion, g_f_titulo, g_f_mono;

static void colores(void)
{
    int i;
    if (g_oscuro) {
        g_col[C_FONDO] = RGB(32, 32, 32);   g_col[C_PANEL] = RGB(28, 28, 28);  g_col[C_CAMPO] = RGB(45, 45, 45);
        g_col[C_BOTON] = RGB(55, 55, 55);   g_col[C_BORDE] = RGB(80, 80, 80);  g_col[C_TEXTO] = RGB(240, 240, 240);
        g_col[C_GRIS] = RGB(170, 170, 170); g_col[C_ACENTO] = RGB(96, 165, 250); g_col[C_AVISO] = RGB(250, 190, 80);
        g_col[C_OK] = RGB(110, 200, 120);   g_col[C_ERROR] = RGB(240, 110, 110); g_col[C_SEL] = RGB(50, 50, 50);
        g_col[C_TEXTO_ACENTO] = RGB(16, 24, 40);
    } else {
        g_col[C_FONDO] = RGB(249, 249, 249); g_col[C_PANEL] = RGB(243, 243, 243); g_col[C_CAMPO] = RGB(255, 255, 255);
        g_col[C_BOTON] = RGB(251, 251, 251); g_col[C_BORDE] = RGB(200, 200, 200); g_col[C_TEXTO] = RGB(26, 26, 26);
        g_col[C_GRIS] = RGB(96, 96, 96);     g_col[C_ACENTO] = RGB(0, 95, 184);   g_col[C_AVISO] = RGB(157, 93, 0);
        g_col[C_OK] = RGB(16, 124, 16);      g_col[C_ERROR] = RGB(196, 43, 28);   g_col[C_SEL] = RGB(228, 228, 228);
        g_col[C_TEXTO_ACENTO] = RGB(255, 255, 255);
    }
    for (i = 0; i < C_N; i++) { if (g_br[i]) DeleteObject(g_br[i]); g_br[i] = CreateSolidBrush(g_col[i]); }
}

static HFONT fuente(int decimas_pt, int peso, const wchar_t *cara)
{
    return CreateFontW(-MulDiv(decimas_pt, g_dpi, 720), 0, 0, 0, peso, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, cara);
}
static void fuentes(void)
{
    HFONT *f[] = { &g_f_normal, &g_f_semi, &g_f_seccion, &g_f_titulo, &g_f_mono }; int i;
    for (i = 0; i < 5; i++) if (*f[i]) { DeleteObject(*f[i]); *f[i] = NULL; }
    g_f_normal  = fuente(100, FW_NORMAL, L"Segoe UI");
    g_f_semi    = fuente(100, FW_SEMIBOLD, L"Segoe UI");
    g_f_seccion = fuente(115, FW_SEMIBOLD, L"Segoe UI");
    g_f_titulo  = fuente(150, FW_SEMIBOLD, L"Segoe UI");
    g_f_mono    = fuente(95, FW_NORMAL, L"Consolas");
}

static int tema_del_sistema(void)
{
    DWORD v = 1, n = sizeof v;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &v, &n) != ERROR_SUCCESS) return 0;
    return v == 0;
}

/* Modo oscuro de los controles comunes (funciones sin nombre de uxtheme,
 * las mismas que usa el Explorador; si no están, todo sigue en claro). */
typedef int  (WINAPI *PSetPreferredAppMode)(int);
typedef BOOL (WINAPI *PAllowDarkModeForWindow)(HWND, BOOL);
static PAllowDarkModeForWindow p_allow_dark;
static void modo_oscuro_app(void)
{
    HMODULE ux = GetModuleHandleW(L"uxtheme.dll");
    if (!ux) ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux) return;
    {
        PSetPreferredAppMode set = (PSetPreferredAppMode)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(135));
        p_allow_dark = (PAllowDarkModeForWindow)(void *)GetProcAddress(ux, MAKEINTRESOURCEA(133));
        if (set) set(g_oscuro ? 1 : 0);
    }
}
static void tema_control(HWND h, const wchar_t *oscuro, const wchar_t *claro)
{
    if (p_allow_dark) p_allow_dark(h, g_oscuro);
    SetWindowTheme(h, g_oscuro ? oscuro : claro, NULL);
}
static void tema_ventana(HWND h)
{
    BOOL d = g_oscuro; int mica = 2;
    if (p_allow_dark) p_allow_dark(h, g_oscuro);
    DwmSetWindowAttribute(h, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &d, sizeof d);
    DwmSetWindowAttribute(h, 38 /* DWMWA_SYSTEMBACKDROP_TYPE: Mica */, &mica, sizeof mica);
}

/* ------------------------------------------------------------------ */
/* ventanas y controles                                                */
/* ------------------------------------------------------------------ */
#define WM_APP_REHACER  (WM_APP + 1)
#define WM_APP_PROGRESO (WM_APP + 2)
#define WM_APP_FIN      (WM_APP + 3)
#define ID_GUARDAR   101
#define ID_DESCARTAR 102
#define ID_CERRAR    103
#define ID_NAV       104
#define ID_CTL       2000
#define TIMER_MANDO  7

static HWND g_main, g_nav, g_pagina, g_estado, g_sucio, g_b_guardar, g_b_descartar, g_b_cerrar;
static int g_pag, g_scroll, g_alto, g_construyendo, g_rehacer_pendiente, g_todo_pendiente;
static wchar_t g_msg[1024];   /* último mensaje de la barra de abajo */
static int g_msg_color = C_GRIS;

/* botones dibujados a mano: GWLP_USERDATA = estilo | marcado << 8 */
enum { B_NORMAL, B_ACENTO, B_CHECK, B_RADIO };

enum { K_TEXTO, K_AJ_COMBO, K_AJ_EDIT, K_AJ_LISTA, K_AJ_CHECK, K_ACCION, K_EDICION, K_OTRO };
typedef struct { HWND h; int kind, aj, sub; } Ctl;
#define MAX_CTL 700
static Ctl g_ctl[MAX_CTL];
static int g_nctl;

static int hh(void) { return S(72); }   /* cabecera */
static int nw(void) { return S(210); }  /* navegación */
static int bh(void) { return S(56); }   /* barra de abajo */

static void poner_estado(int color, const wchar_t *f, ...)
{
    va_list ap; va_start(ap, f);
    _vsnwprintf_s(g_msg, 1024, _TRUNCATE, f, ap);
    va_end(ap);
    g_msg_color = color;
    if (g_estado) { SetWindowLongPtrW(g_estado, GWLP_USERDATA, color + 1); SetWindowTextW(g_estado, g_msg); InvalidateRect(g_estado, NULL, TRUE); }
}
static int hay_cambios(void) { return memcmp(g_conf.val, g_guardada.val, sizeof g_conf.val) != 0; }
static void actualizar_sucio(void)
{
    int c = hay_cambios();
    if (!g_sucio) return;
    SetWindowLongPtrW(g_sucio, GWLP_USERDATA, (c ? C_AVISO : C_GRIS) + 1);
    SetWindowTextW(g_sucio, T(c ? T_HAY_CAMBIOS : T_SIN_CAMBIOS));
    InvalidateRect(g_sucio, NULL, TRUE);
    EnableWindow(g_b_descartar, c);
}

static int medir_texto(HFONT f, const wchar_t *s, int ancho, int *w_out)
{
    HDC dc = GetDC(NULL); HGDIOBJ old = SelectObject(dc, f); RECT r = { 0, 0, ancho > 0 ? ancho : 100000, 0 };
    DrawTextW(dc, s, -1, &r, DT_CALCRECT | DT_NOPREFIX | (ancho > 0 ? DT_WORDBREAK : DT_SINGLELINE));
    SelectObject(dc, old); ReleaseDC(NULL, dc);
    if (w_out) *w_out = r.right;
    return r.bottom;
}

static int nuevo_ctl(HWND h, int kind, int aj, int sub)
{
    if (g_nctl >= MAX_CTL) return -1;
    g_ctl[g_nctl].h = h; g_ctl[g_nctl].kind = kind; g_ctl[g_nctl].aj = aj; g_ctl[g_nctl].sub = sub;
    return g_nctl++;
}

/* Crea un control hijo de la página en coordenadas de contenido (y sin desplazar). */
static HWND crear(const wchar_t *clase, const wchar_t *texto, DWORD estilo, DWORD ex, int x, int y, int w, int h,
                  int kind, int aj, int sub, HFONT f)
{
    int id = ID_CTL + g_nctl;
    HWND c = CreateWindowExW(ex, clase, texto, WS_CHILD | WS_VISIBLE | estilo, x, y - g_scroll, w, h, g_pagina,
                             (HMENU)(INT_PTR)id, g_inst, NULL);
    if (!c) return NULL;
    SendMessageW(c, WM_SETFONT, (WPARAM)(f ? f : g_f_normal), FALSE);
    if (nuevo_ctl(c, kind, aj, sub) < 0) { DestroyWindow(c); return NULL; }
    return c;
}
static Ctl *ctl_de(HWND h)
{
    int i;
    for (i = 0; i < g_nctl; i++) if (g_ctl[i].h == h) return &g_ctl[i];
    return NULL;
}

/* texto fijo; devuelve su alto */
static int texto_en(int x, int y, int w, const wchar_t *s, HFONT f, int color)
{
    int h = medir_texto(f, s, w, NULL);
    HWND c = crear(L"STATIC", s, SS_NOPREFIX | SS_LEFT, 0, x, y, w, h, K_TEXTO, -1, 0, f);
    if (c) SetWindowLongPtrW(c, GWLP_USERDATA, color + 1);
    return h;
}

static HWND boton(const wchar_t *s, int estilo, int marcado, int x, int y, int w, int h, int kind, int aj, int sub)
{
    HWND b = crear(L"BUTTON", s, BS_OWNERDRAW | WS_TABSTOP | WS_GROUP, 0, x, y, w, h, kind, aj, sub,
                   estilo == B_ACENTO ? g_f_semi : g_f_normal);
    if (b) SetWindowLongPtrW(b, GWLP_USERDATA, estilo | (marcado << 8));
    return b;
}
static int ancho_boton(const wchar_t *s, HFONT f)
{
    int w; medir_texto(f, s, 0, &w);
    return w + S(36);
}
static int ancho_check(const wchar_t *s)
{
    int w; medir_texto(g_f_normal, s, 0, &w);
    return w + S(34);
}
static void marcar(HWND b, int m)
{
    LONG_PTR u = GetWindowLongPtrW(b, GWLP_USERDATA);
    SetWindowLongPtrW(b, GWLP_USERDATA, (u & 0xFF) | (m ? 0x100 : 0));
    InvalidateRect(b, NULL, FALSE);
}
static int marcado(HWND b) { return (GetWindowLongPtrW(b, GWLP_USERDATA) & 0x100) != 0; }

static void redondo(HDC dc, RECT r, int radio, COLORREF relleno, COLORREF borde)
{
    HBRUSH br = CreateSolidBrush(relleno); HPEN pen = CreatePen(PS_SOLID, S(1) > 0 ? S(1) : 1, borde);
    HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radio, radio);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(br); DeleteObject(pen);
}

static void dibujar_boton(const DRAWITEMSTRUCT *di)
{
    HDC dc = di->hDC; RECT r = di->rcItem; wchar_t s[256];
    LONG_PTR u = GetWindowLongPtrW(di->hwndItem, GWLP_USERDATA);
    int estilo = (int)(u & 0xFF), m = (u & 0x100) != 0;
    int foco = (di->itemState & ODS_FOCUS) != 0, apagado = (di->itemState & ODS_DISABLED) != 0;
    int pulsado = (di->itemState & ODS_SELECTED) != 0;
    HWND padre = GetParent(di->hwndItem);
    COLORREF fondo = g_col[padre == g_main ? C_PANEL : C_FONDO];
    COLORREF texto = g_col[apagado ? C_GRIS : C_TEXTO];
    HGDIOBJ of;
    GetWindowTextW(di->hwndItem, s, 256);
    FillRect(dc, &r, padre == g_main ? g_br[C_PANEL] : g_br[C_FONDO]);
    SetBkMode(dc, TRANSPARENT);
    of = SelectObject(dc, (HFONT)SendMessageW(di->hwndItem, WM_GETFONT, 0, 0));
    if (estilo == B_NORMAL || estilo == B_ACENTO) {
        COLORREF rel = estilo == B_ACENTO && !apagado ? g_col[C_ACENTO] : g_col[C_BOTON];
        COLORREF bor = foco ? (estilo == B_ACENTO ? g_col[C_TEXTO] : g_col[C_ACENTO]) : g_col[C_BORDE];
        RECT t = r;
        if (pulsado) rel = estilo == B_ACENTO ? RGB(GetRValue(rel) * 85 / 100, GetGValue(rel) * 85 / 100, GetBValue(rel) * 85 / 100) : g_col[C_SEL];
        InflateRect(&r, -S(1), -S(1));
        redondo(dc, r, S(8), rel, bor);
        if (foco) { RECT f2 = r; InflateRect(&f2, -S(1), -S(1)); { HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH); HPEN p = CreatePen(PS_SOLID, S(1), bor);
            HGDIOBJ a = SelectObject(dc, nb), b = SelectObject(dc, p); RoundRect(dc, f2.left, f2.top, f2.right, f2.bottom, S(6), S(6));
            SelectObject(dc, a); SelectObject(dc, b); DeleteObject(p); } }
        SetTextColor(dc, estilo == B_ACENTO && !apagado ? g_col[C_TEXTO_ACENTO] : texto);
        DrawTextW(dc, s, -1, &t, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    } else {
        int c = S(18), y0 = r.top + (r.bottom - r.top - c) / 2;
        RECT caja = { r.left + S(2), y0, r.left + S(2) + c, y0 + c }, t = r;
        if (estilo == B_CHECK) {
            redondo(dc, caja, S(5), m ? g_col[apagado ? C_GRIS : C_ACENTO] : g_col[C_CAMPO], m ? g_col[apagado ? C_GRIS : C_ACENTO] : g_col[C_GRIS]);
            if (m) {
                HPEN p = CreatePen(PS_SOLID, S(2), g_col[C_TEXTO_ACENTO]); HGDIOBJ op = SelectObject(dc, p);
                MoveToEx(dc, caja.left + c * 22 / 100, caja.top + c * 52 / 100, NULL);
                LineTo(dc, caja.left + c * 42 / 100, caja.top + c * 72 / 100);
                LineTo(dc, caja.left + c * 78 / 100, caja.top + c * 30 / 100);
                SelectObject(dc, op); DeleteObject(p);
            }
        } else {
            HBRUSH b = CreateSolidBrush(g_col[C_CAMPO]); HPEN p = CreatePen(PS_SOLID, S(1), g_col[m && !apagado ? C_ACENTO : C_GRIS]);
            HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
            Ellipse(dc, caja.left, caja.top, caja.right, caja.bottom);
            SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b); DeleteObject(p);
            if (m) {
                HBRUSH b2 = CreateSolidBrush(g_col[apagado ? C_GRIS : C_ACENTO]); HGDIOBJ o2 = SelectObject(dc, b2), o3 = SelectObject(dc, GetStockObject(NULL_PEN));
                int k = c * 28 / 100;
                Ellipse(dc, caja.left + k, caja.top + k, caja.right - k + 1, caja.bottom - k + 1);
                SelectObject(dc, o2); SelectObject(dc, o3); DeleteObject(b2);
            }
        }
        t.left = caja.right + S(8);
        SetTextColor(dc, texto);
        DrawTextW(dc, s, -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        if (foco) {
            RECT f2; int w; medir_texto((HFONT)SendMessageW(di->hwndItem, WM_GETFONT, 0, 0), s, 0, &w);
            f2.left = r.left; f2.top = r.top; f2.right = t.left + w + S(4); f2.bottom = r.bottom;
            if (f2.right > r.right) f2.right = r.right;
            {
                HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH); HPEN p = CreatePen(PS_SOLID, S(1), g_col[C_ACENTO]);
                HGDIOBJ a = SelectObject(dc, nb), b = SelectObject(dc, p);
                RoundRect(dc, f2.left, f2.top, f2.right, f2.bottom, S(6), S(6));
                SelectObject(dc, a); SelectObject(dc, b); DeleteObject(p);
            }
        }
    }
    SelectObject(dc, of);
    (void)fondo;
}

/* ------------------------------------------------------------------ */
/* desplazamiento de la página                                         */
/* ------------------------------------------------------------------ */
static int alto_pagina(void) { RECT r; GetClientRect(g_pagina, &r); return r.bottom; }
static void barra_scroll(void)
{
    SCROLLINFO si; si.cbSize = sizeof si; si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
    si.nMin = 0; si.nMax = g_alto > 0 ? g_alto - 1 : 0; si.nPage = (UINT)alto_pagina(); si.nPos = g_scroll; si.nTrackPos = 0;
    SetScrollInfo(g_pagina, SB_VERT, &si, TRUE);
}
static void desplazar_a(int y)
{
    int max = g_alto - alto_pagina(), d;
    if (max < 0) max = 0;
    if (y > max) y = max;
    if (y < 0) y = 0;
    d = g_scroll - y;
    if (!d) return;
    g_scroll = y;
    ScrollWindowEx(g_pagina, 0, d, NULL, NULL, NULL, NULL, SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    barra_scroll();
    UpdateWindow(g_pagina);
}
static void hacer_visible(HWND h)
{
    RECT r; POINT a, b; int alto = alto_pagina(), m = S(12);
    if (!h || !IsChild(g_pagina, h)) return;
    while (GetParent(h) != g_pagina) h = GetParent(h);   /* la edición de un combo */
    GetWindowRect(h, &r);
    a.x = r.left; a.y = r.top; b.x = r.right; b.y = r.bottom;
    ScreenToClient(g_pagina, &a); ScreenToClient(g_pagina, &b);
    if (a.y < m) desplazar_a(g_scroll + a.y - m);
    else if (b.y > alto - m) desplazar_a(g_scroll + (b.y - alto + m));
}

/* ------------------------------------------------------------------ */
/* páginas                                                             */
/* ------------------------------------------------------------------ */
static void pedir_rehacer(void)
{
    if (!g_rehacer_pendiente) { g_rehacer_pendiente = 1; PostMessageW(g_main, WM_APP_REHACER, 0, 0); }
}
static void construir_pagina(void);   /* configurador_obiwan_paginas.inc */
static void comando_pagina(HWND h, int cod);
static void tarea_terminada(WPARAM w, LPARAM l);
static void pagina_saliendo(void);
static int tarea_en_marcha(void);
static void cancelar_tarea_y_esperar(void);
static int guardar(void);
static void temporizador_mando(void);

static void rehacer_pagina(int conservar_scroll)
{
    HWND c, f = GetFocus(); Ctl antes = { 0 }; int habia_foco = 0, i;
    Ctl *fc = f && IsChild(g_pagina, f) ? ctl_de(GetParent(f) == g_pagina ? f : GetParent(f)) : NULL;
    if (fc) { antes = *fc; habia_foco = 1; }
    g_rehacer_pendiente = 0;
    if (!conservar_scroll) g_scroll = 0;
    SendMessageW(g_pagina, WM_SETREDRAW, FALSE, 0);
    while ((c = GetWindow(g_pagina, GW_CHILD)) != NULL) DestroyWindow(c);
    g_nctl = 0;
    g_construyendo = 1;
    construir_pagina();
    g_construyendo = 0;
    {
        int max = g_alto - alto_pagina();
        if (max < 0) max = 0;
        if (g_scroll > max) {   /* la página se quedó más corta */
            int d = g_scroll - max;
            for (i = 0; i < g_nctl; i++) {
                RECT r; POINT p; GetWindowRect(g_ctl[i].h, &r); p.x = r.left; p.y = r.top; ScreenToClient(g_pagina, &p);
                SetWindowPos(g_ctl[i].h, NULL, p.x, p.y + d, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            g_scroll = max;
        }
    }
    barra_scroll();
    SendMessageW(g_pagina, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(g_pagina, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    if (habia_foco)
        for (i = 0; i < g_nctl; i++)
            if (g_ctl[i].kind == antes.kind && g_ctl[i].aj == antes.aj && g_ctl[i].sub == antes.sub && IsWindowEnabled(g_ctl[i].h)) {
                SetFocus(g_ctl[i].h); break;
            }
}

static void ir_a_pagina(int p)
{
    if (p < 0) p = AJP_N - 1;
    if (p >= AJP_N) p = 0;
    if (p != g_pag) pagina_saliendo();
    g_pag = p;
    SendMessageW(g_nav, LB_SETCURSEL, (WPARAM)p, 0);
    rehacer_pagina(0);
}

static LRESULT CALLBACK pagina_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, g_br[C_FONDO]); return 1; }
    case WM_CTLCOLORSTATIC: {
        HWND c = (HWND)l; wchar_t cls[16]; LONG_PTR u = GetWindowLongPtrW(c, GWLP_USERDATA);
        GetClassNameW(c, cls, 16);
        SetBkMode((HDC)w, TRANSPARENT);
        if (!_wcsicmp(cls, L"Edit")) {   /* edición de solo lectura */
            SetBkMode((HDC)w, OPAQUE); SetBkColor((HDC)w, g_col[C_CAMPO]); SetTextColor((HDC)w, g_col[C_TEXTO]);
            return (LRESULT)g_br[C_CAMPO];
        }
        SetTextColor((HDC)w, g_col[u > 0 && u <= C_N ? u - 1 : C_TEXTO]);
        return (LRESULT)g_br[C_FONDO]; }
    case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX:
        SetBkColor((HDC)w, g_col[C_CAMPO]); SetTextColor((HDC)w, g_col[C_TEXTO]);
        return (LRESULT)g_br[C_CAMPO];
    case WM_DRAWITEM: dibujar_boton((const DRAWITEMSTRUCT *)l); return TRUE;
    case WM_COMMAND:
        if (l && !g_construyendo) comando_pagina((HWND)l, HIWORD(w));
        return 0;
    case WM_NOTIFY: {
        NMHDR *n = (NMHDR *)l;
        if (n->code == UDN_DELTAPOS) return 0;
        break; }
    case WM_VSCROLL: {
        SCROLLINFO si; int y = g_scroll, linea = S(40);
        si.cbSize = sizeof si; si.fMask = SIF_ALL; GetScrollInfo(h, SB_VERT, &si);
        switch (LOWORD(w)) {
        case SB_LINEUP: y -= linea; break;   case SB_LINEDOWN: y += linea; break;
        case SB_PAGEUP: y -= (int)si.nPage - linea; break; case SB_PAGEDOWN: y += (int)si.nPage - linea; break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: y = si.nTrackPos; break;
        case SB_TOP: y = 0; break; case SB_BOTTOM: y = g_alto; break;
        default: break;
        }
        desplazar_a(y);
        return 0; }
    case WM_MOUSEWHEEL: {
        static int resto; int d = GET_WHEEL_DELTA_WPARAM(w) + resto, lineas = 3;
        SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lineas, 0);
        if (lineas <= 0 || lineas > 20) lineas = 3;
        resto = d % WHEEL_DELTA;
        desplazar_a(g_scroll - (d / WHEEL_DELTA) * lineas * S(20));
        return 0; }
    case WM_TIMER:
        if (w == TIMER_MANDO) temporizador_mando();
        return 0;
    case WM_SIZE:
        barra_scroll();
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ------------------------------------------------------------------ */
/* ventana principal                                                   */
/* ------------------------------------------------------------------ */
static const int k_nav_textos[AJP_N] = { T_NAV_JUGAR, T_NAV_PANTALLA, T_NAV_GRAFICOS, T_NAV_AUDIO, T_NAV_CONTROLES,
                                         T_NAV_IDIOMA, T_NAV_HERRAMIENTAS, T_NAV_DEV, T_NAV_CREDITOS };

static void colocar(void)
{
    RECT r; int W_, H_, x, wg, wd, wc, y, alto = S(34), sep = S(8);
    if (!g_main) return;
    GetClientRect(g_main, &r); W_ = r.right; H_ = r.bottom;
    SetWindowPos(g_nav, NULL, 0, hh(), nw(), H_ - hh() - bh(), SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_pagina, NULL, nw(), hh(), W_ - nw(), H_ - hh() - bh(), SWP_NOZORDER | SWP_NOACTIVATE);
    wg = ancho_boton(T(T_GUARDAR), g_f_semi); wd = ancho_boton(T(T_DESCARTAR), g_f_normal); wc = ancho_boton(T(T_CERRAR), g_f_normal);
    y = H_ - bh() + (bh() - alto) / 2;
    x = W_ - S(16) - wc;
    SetWindowPos(g_b_cerrar, NULL, x, y, wc, alto, SWP_NOZORDER | SWP_NOACTIVATE);
    x -= sep + wd;
    SetWindowPos(g_b_descartar, NULL, x, y, wd, alto, SWP_NOZORDER | SWP_NOACTIVATE);
    x -= sep + wg;
    SetWindowPos(g_b_guardar, NULL, x, y, wg, alto, SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_estado, NULL, S(16), H_ - bh() + S(7), x - S(32), S(20), SWP_NOZORDER | SWP_NOACTIVATE);
    SetWindowPos(g_sucio, NULL, S(16), H_ - bh() + S(29), x - S(32), S(20), SWP_NOZORDER | SWP_NOACTIVATE);
    SendMessageW(g_nav, LB_SETITEMHEIGHT, 0, S(36));
    InvalidateRect(g_main, NULL, TRUE);
}

static void poner_fuentes_principales(void)
{
    SendMessageW(g_nav, WM_SETFONT, (WPARAM)g_f_normal, TRUE);
    SendMessageW(g_estado, WM_SETFONT, (WPARAM)g_f_normal, TRUE);
    SendMessageW(g_sucio, WM_SETFONT, (WPARAM)g_f_normal, TRUE);
    SendMessageW(g_b_guardar, WM_SETFONT, (WPARAM)g_f_semi, TRUE);
    SendMessageW(g_b_descartar, WM_SETFONT, (WPARAM)g_f_normal, TRUE);
    SendMessageW(g_b_cerrar, WM_SETFONT, (WPARAM)g_f_normal, TRUE);
}

/* Cambio de idioma o de tema: textos fijos de la ventana y página. */
static void rehacer_todo(void)
{
    int i;
    textos_libres();
    SetWindowTextW(g_main, T(T_TITULO));
    SendMessageW(g_nav, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < AJP_N; i++) SendMessageW(g_nav, LB_ADDSTRING, 0, (LPARAM)T(k_nav_textos[i]));
    SendMessageW(g_nav, LB_SETCURSEL, (WPARAM)g_pag, 0);
    SetWindowTextW(g_b_guardar, T(T_GUARDAR));
    SetWindowTextW(g_b_descartar, T(T_DESCARTAR));
    SetWindowTextW(g_b_cerrar, T(T_CERRAR));
    tema_ventana(g_main);
    tema_control(g_nav, L"DarkMode_Explorer", L"Explorer");
    tema_control(g_pagina, L"DarkMode_Explorer", L"Explorer");
    poner_fuentes_principales();
    colocar();
    actualizar_sucio();
    rehacer_pagina(1);
    RedrawWindow(g_main, NULL, NULL, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

static void dibujar_nav(const DRAWITEMSTRUCT *di)
{
    HDC dc = di->hDC; RECT r = di->rcItem; wchar_t s[128];
    int sel = (di->itemState & ODS_SELECTED) != 0, foco = (di->itemState & ODS_FOCUS) != 0 && GetFocus() == g_nav;
    if ((int)di->itemID < 0) { FillRect(dc, &r, g_br[C_PANEL]); return; }
    FillRect(dc, &r, g_br[C_PANEL]);
    SendMessageW(g_nav, LB_GETTEXT, di->itemID, (LPARAM)s);
    {
        RECT b = r; b.left += S(8); b.right -= S(8); b.top += S(2); b.bottom -= S(2);
        if (sel) redondo(dc, b, S(8), g_col[C_SEL], g_col[C_SEL]);
        if (foco) {
            HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH); HPEN p = CreatePen(PS_SOLID, S(2) > 1 ? S(2) : 1, g_col[C_ACENTO]);
            HGDIOBJ a = SelectObject(dc, nb), c = SelectObject(dc, p);
            RoundRect(dc, b.left, b.top, b.right, b.bottom, S(8), S(8));
            SelectObject(dc, a); SelectObject(dc, c); DeleteObject(p);
        }
        if (sel) {
            RECT bar = { b.left + S(1), b.top + S(8), b.left + S(4), b.bottom - S(8) };
            FillRect(dc, &bar, g_br[C_ACENTO]);
        }
        b.left += S(16);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, g_col[C_TEXTO]);
        {
            HGDIOBJ of = SelectObject(dc, sel ? g_f_semi : g_f_normal);
            DrawTextW(dc, s, -1, &b, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
            SelectObject(dc, of);
        }
    }
}

static void pintar_cabecera(HDC dc)
{
    RECT r, t; GetClientRect(g_main, &r);
    FillRect(dc, &r, g_br[C_PANEL]);
    SetBkMode(dc, TRANSPARENT);
    {
        HGDIOBJ of = SelectObject(dc, g_f_titulo);
        t.left = S(24); t.top = S(12); t.right = r.right - S(24); t.bottom = S(42);
        SetTextColor(dc, g_col[C_TEXTO]);
        DrawTextW(dc, T(T_TITULO), -1, &t, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER);
        SelectObject(dc, g_f_normal);
        t.top = S(42); t.bottom = S(64);
        SetTextColor(dc, g_col[C_GRIS]);
        DrawTextW(dc, T(T_SUBTITULO), -1, &t, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER | DT_END_ELLIPSIS);
        SelectObject(dc, of);
    }
    {   /* raya entre la página y la barra de abajo */
        RECT l = { nw(), r.bottom - bh(), r.right, r.bottom - bh() + 1 };
        FillRect(dc, &l, g_br[C_BORDE]);
    }
}

static void fuentes_y_rehacer(void)
{
    fuentes();
    rehacer_todo();
}

static LRESULT CALLBACK principal_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        pintar_cabecera(dc);
        EndPaint(h, &ps);
        return 0; }
    case WM_CTLCOLORSTATIC: {
        LONG_PTR u = GetWindowLongPtrW((HWND)l, GWLP_USERDATA);
        SetBkMode((HDC)w, TRANSPARENT);
        SetTextColor((HDC)w, g_col[u > 0 && u <= C_N ? u - 1 : C_GRIS]);
        return (LRESULT)g_br[C_PANEL]; }
    case WM_CTLCOLORLISTBOX:
        SetBkColor((HDC)w, g_col[C_PANEL]); SetTextColor((HDC)w, g_col[C_TEXTO]);
        return (LRESULT)g_br[C_PANEL];
    case WM_MEASUREITEM: {
        MEASUREITEMSTRUCT *mi = (MEASUREITEMSTRUCT *)l;
        if (mi->CtlType == ODT_LISTBOX) { mi->itemHeight = (UINT)S(36); return TRUE; }
        break; }
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT *di = (const DRAWITEMSTRUCT *)l;
        if (di->CtlType == ODT_LISTBOX) dibujar_nav(di); else dibujar_boton(di);
        return TRUE; }
    case WM_COMMAND: {
        int id = LOWORD(w), cod = HIWORD(w);
        if (id == ID_NAV && cod == LBN_SELCHANGE) {
            int s = (int)SendMessageW(g_nav, LB_GETCURSEL, 0, 0);
            if (s >= 0 && s != g_pag) ir_a_pagina(s);
            return 0;
        }
        if (id == ID_NAV) return 0;
        if (id == ID_GUARDAR && (cod == BN_CLICKED || cod == BN_DOUBLECLICKED)) { guardar(); return 0; }
        if (id == ID_DESCARTAR && (cod == BN_CLICKED || cod == BN_DOUBLECLICKED)) {
            int lang_antes = g_lang;
            memcpy(g_conf.val, g_guardada.val, sizeof g_conf.val);
            {
                int ii = AJ("Language", "interfaz"), k;
                for (k = 0; k < N_IDIOMAS; k++) if (!_stricmp(g_conf.val[ii], g_idiomas[k])) g_lang = k;
            }
            poner_estado(C_GRIS, L"%s", T(T_DESCARTADOS));
            if (g_lang != lang_antes) rehacer_todo(); else { actualizar_sucio(); rehacer_pagina(1); }
            return 0;
        }
        if (id == ID_CERRAR || id == IDCANCEL) { PostMessageW(h, WM_CLOSE, 0, 0); return 0; }
        if (id == IDOK) {   /* Intro: pulsa el botón que tiene el foco */
            HWND f = GetFocus(); wchar_t cls[16];
            if (f) { GetClassNameW(f, cls, 16); if (!_wcsicmp(cls, L"Button")) SendMessageW(f, BM_CLICK, 0, 0); }
            return 0;
        }
        if (id >= ID_CTL && l) { comando_pagina((HWND)l, cod); return 0; }
        break; }
    case WM_APP_REHACER:
        if (g_todo_pendiente) { g_todo_pendiente = 0; g_rehacer_pendiente = 0; rehacer_todo(); }
        else rehacer_pagina(1);
        return 0;
    case WM_APP_PROGRESO: case WM_APP_FIN: tarea_terminada(w, l | (m == WM_APP_FIN ? 0x10000 : 0)); return 0;
    case WM_SIZE:
        if (g_nav) { colocar(); pedir_rehacer(); }
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)l;
        mm->ptMinTrackSize.x = S(720); mm->ptMinTrackSize.y = S(480);
        if (g_captura) { mm->ptMaxTrackSize.x = 30000; mm->ptMaxTrackSize.y = 30000; }
        return 0; }
    case WM_DPICHANGED:
        if (!g_dpi_forzado) {
            RECT *r = (RECT *)l;
            g_dpi = HIWORD(w);
            fuentes();
            SetWindowPos(h, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            rehacer_todo();
        }
        return 0;
    case WM_SETTINGCHANGE:
        if (!g_tema_forzado && l && !lstrcmpiW((const wchar_t *)l, L"ImmersiveColorSet")) {
            int o = tema_del_sistema();
            if (o != g_oscuro) { g_oscuro = o; modo_oscuro_app(); colores(); rehacer_todo(); }
        }
        break;
    case WM_CLOSE:
        if (tarea_en_marcha()) {
            if (MessageBoxW(h, T(T_TAREA_EN_MARCHA), T(T_TITULO), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return 0;
            cancelar_tarea_y_esperar();
        }
        if (!g_captura && hay_cambios()) {
            int r = MessageBoxW(h, T(T_PREG_SALIR), T(T_TITULO), MB_YESNOCANCEL | MB_ICONQUESTION);
            if (r == IDCANCEL) return 0;
            if (r == IDYES && !guardar()) return 0;
        }
        pagina_saliendo();
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static int crear_ventana(int x, int y, int show)
{
    WNDCLASSEXW wc; HICON ic = LoadIconW(g_inst, MAKEINTRESOURCEW(1));
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc; wc.hInstance = g_inst; wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpfnWndProc = principal_proc; wc.lpszClassName = L"OWConfigurador"; wc.hIcon = ic; wc.hIconSm = ic;
    if (!RegisterClassExW(&wc)) return 0;
    wc.lpfnWndProc = pagina_proc; wc.lpszClassName = L"OWPagina"; wc.hIcon = NULL; wc.hIconSm = NULL;
    if (!RegisterClassExW(&wc)) return 0;
    g_main = CreateWindowExW(WS_EX_CONTROLPARENT, L"OWConfigurador", T(T_TITULO),
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, S(1060), S(760), NULL, NULL, g_inst, NULL);
    if (!g_main) return 0;
    if (!g_dpi_forzado) {
        int d = (int)GetDpiForWindow(g_main);
        if (d && d != g_dpi) {
            g_dpi = d; fuentes();
            SetWindowPos(g_main, NULL, 0, 0, S(1060), S(760), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    g_nav = CreateWindowExW(0, L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | LBS_NOTIFY |
                            LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT, 0, 0, 10, 10, g_main,
                            (HMENU)(INT_PTR)ID_NAV, g_inst, NULL);
    g_pagina = CreateWindowExW(WS_EX_CONTROLPARENT, L"OWPagina", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN | WS_TABSTOP,
                               0, 0, 10, 10, g_main, NULL, g_inst, NULL);
    g_b_guardar = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | BS_OWNERDRAW, 0, 0, 10, 10,
                                  g_main, (HMENU)(INT_PTR)ID_GUARDAR, g_inst, NULL);
    g_b_descartar = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | BS_OWNERDRAW, 0, 0, 10, 10,
                                    g_main, (HMENU)(INT_PTR)ID_DESCARTAR, g_inst, NULL);
    g_b_cerrar = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_GROUP | BS_OWNERDRAW, 0, 0, 10, 10,
                                 g_main, (HMENU)(INT_PTR)ID_CERRAR, g_inst, NULL);
    g_estado = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_ENDELLIPSIS, 0, 0, 10, 10, g_main, NULL, g_inst, NULL);
    g_sucio = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_ENDELLIPSIS, 0, 0, 10, 10, g_main, NULL, g_inst, NULL);
    SetWindowLongPtrW(g_b_guardar, GWLP_USERDATA, B_ACENTO);
    SetWindowLongPtrW(g_b_descartar, GWLP_USERDATA, B_NORMAL);
    SetWindowLongPtrW(g_b_cerrar, GWLP_USERDATA, B_NORMAL);
    rehacer_todo();
    if (g_msg[0]) poner_estado(g_msg_color, L"%s", g_msg);
    ShowWindow(g_main, show);
    UpdateWindow(g_main);
    return 1;
}

/* ¿Es un combo desplegado (o su edición)? Entonces Esc y la rueda son suyos. */
static int combo_desplegado(HWND f)
{
    wchar_t cls[16];
    if (!f) return 0;
    GetClassNameW(f, cls, 16);
    if (!_wcsicmp(cls, L"Edit") && GetParent(f)) { GetClassNameW(GetParent(f), cls, 16); f = GetParent(f); }
    return !_wcsicmp(cls, L"ComboBox") && SendMessageW(f, CB_GETDROPPEDSTATE, 0, 0);
}

static int bucle(void)
{
    MSG msg; HWND foco_antes = NULL;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && GetKeyState(VK_CONTROL) < 0 && (msg.hwnd == g_main || IsChild(g_main, msg.hwnd))) {
            if (msg.wParam == 'S') { guardar(); continue; }
            if (msg.wParam == VK_TAB && !combo_desplegado(GetFocus())) {
                ir_a_pagina(g_pag + (GetKeyState(VK_SHIFT) < 0 ? -1 : 1));
                SetFocus(g_nav);
                continue;
            }
        }
        if (msg.message == WM_MOUSEWHEEL && msg.hwnd != g_pagina && IsChild(g_pagina, msg.hwnd)) {
            wchar_t cls[16]; LONG_PTR st = GetWindowLongPtrW(msg.hwnd, GWL_STYLE);
            GetClassNameW(msg.hwnd, cls, 16);
            if (!combo_desplegado(msg.hwnd) && !(!_wcsicmp(cls, L"Edit") && (st & ES_MULTILINE))) {
                SendMessageW(g_pagina, WM_MOUSEWHEEL, msg.wParam, msg.lParam);
                continue;
            }
        }
        if (!IsDialogMessageW(g_main, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        {
            HWND f = GetFocus();
            if (f != foco_antes) { foco_antes = f; if (f && IsChild(g_pagina, f)) hacer_visible(f); if (g_nav) InvalidateRect(g_nav, NULL, FALSE); }
        }
    }
    return (int)msg.wParam;
}

#include "configurador_obiwan_paginas.inc"
