/* configurador_obiwan_paginas.inc -- contenido de cada página, tareas en
 * segundo plano, línea de órdenes, capturas y wWinMain (Claude completo-4,
 * 2026-11-06). Lo incluye tools/configurador_obiwan.c al final.
 *
 * Reglas que cumple:
 *  - nada arranca el juego salvo los botones Jugar / Jugar y medir / Medir
 *    todo, y solo si los pulsa la persona (en --captura no hacen nada);
 *  - "Probar mando" solo lee el mando (XInputGetState / joyGetPosEx), nunca
 *    vibra ni escribe, y solo con el juego cerrado;
 *  - nada se borra: las copias y restauraciones copian, la caché va a la
 *    papelera y "Restablecer" guarda antes obiwan.ini.bak-<fecha>. */

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

/* ------------------------------------------------------------------ */
/* ajustes en memoria                                                  */
/* ------------------------------------------------------------------ */
/* aj_cargar copia con strcpy_s y deja bytes viejos tras el 0: los limpia
 * para que hay_cambios (memcmp) solo vea cambios de verdad. */
static void limpiar_conf(AjConf *c)
{
    int i;
    for (i = 0; i < AJ_N; i++) {
        size_t l = strnlen(c->val[i], AJ_VMAX - 1);
        memset(c->val[i] + l, 0, AJ_VMAX - l);
    }
}
static const char *V(const char *sec, const char *clave) { return g_conf.val[AJ(sec, clave)]; }
static void poner_val(int i, const char *v)
{
    memset(g_conf.val[i], 0, AJ_VMAX);
    strcpy_s(g_conf.val[i], AJ_VMAX, v);
}
static const AjEdicion *ed_actual(void)
{
    const AjEdicion *e = aj_edicion(V("Language", "edicion"));
    return e ? e : &aj_ediciones[0];
}
static const char *ed_nombre(const AjEdicion *e) { return g_lang == 0 ? e->nombre_es : e->nombre_en; }
static const char *ed_idioma(const AjEdicion *e) { return g_lang == 0 ? e->idioma_es : e->idioma_en; }
static void lang_de_conf(void)
{
    int ii = AJ("Language", "interfaz"), k;
    for (k = 0; k < N_IDIOMAS; k++) if (!_stricmp(g_conf.val[ii], g_idiomas[k])) g_lang = k;
}

/* ------------------------------------------------------------------ */
/* rutas (las mismas reglas que el lanzador 6)                         */
/* ------------------------------------------------------------------ */
#define BUILD_LANZ "obiwan-claude-completo-4"
#define BUILD_LANZ_W L"obiwan-claude-completo-4"

/* lanzador_obiwan_completo4.c:288-298 */
static void ruta_exe(wchar_t *o, size_t n)
{
    _snwprintf_s(o, n, _TRUNCATE, L"%s\\build\\" BUILD_LANZ_W L"\\obiwan_recomp.exe", g_base);
    if (aj_dev_activo(&g_conf)) {
        const char *v = V("Developer", "variante");
        if (_stricmp(v, BUILD_LANZ)) {
            wchar_t wv[AJ_VMAX], alt[MAX_PATH];
            u8w(v, wv, AJ_VMAX);
            _snwprintf_s(alt, MAX_PATH, _TRUNCATE, L"%s\\build\\%s\\obiwan_recomp.exe", g_base, wv);
            if (es_arch(alt)) wcsncpy_s(o, n, alt, _TRUNCATE);
        }
    }
}
/* lanzador_obiwan_completo4.c:109 (find_game_dir) y :323-331 (carpeta_juego) */
static int carpeta_del_juego(wchar_t *o, size_t n)
{
    const char *cj = V("Paths", "carpeta_juego"); wchar_t wid[32], pat[MAX_PATH]; WIN32_FIND_DATAW fd; HANDLE h;
    if (cj[0]) { u8w(cj, o, (int)n); if (es_dir(o)) return 1; }
    if (GetEnvironmentVariableW(L"OBIWAN_JUEGO", o, (DWORD)n) && es_dir(o)) return 1;
    u8w(ed_actual()->id, wid, 32);
    _snwprintf_s(o, n, _TRUNCATE, L"%s\\private\\game\\%s", g_base, wid);
    if (es_dir(o)) return 1;
    _snwprintf_s(o, n, _TRUNCATE, L"%s\\private\\game\\usa", g_base);
    if (es_dir(o)) return 1;
    _snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\private\\game\\*", g_base);
    h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) {
                _snwprintf_s(o, n, _TRUNCATE, L"%s\\private\\game\\%s", g_base, fd.cFileName);
                FindClose(h);
                return 1;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    o[0] = 0;
    return 0;
}
/* lanzador_obiwan_completo4.c:351-370 */
static void ruta_partidas(const AjEdicion *ed, wchar_t *o, size_t n)
{
    const char *cp = V("Paths", "carpeta_partidas"); wchar_t sub[64];
    if (cp[0]) { u8w(cp, o, (int)n); return; }
    u8w(ed->partidas, sub, 64);
    _snwprintf_s(o, n, _TRUNCATE, L"%s\\saves\\%s", g_local, sub);
}
static void ruta_lanzador(wchar_t *o, size_t n) { _snwprintf_s(o, n, _TRUNCATE, L"%s\\Jugar Obi-Wan 6.exe", g_base); }
static void ruta_medir(wchar_t *o, size_t n) { _snwprintf_s(o, n, _TRUNCATE, L"%s\\MEDIR TODO - Obi-Wan 6.bat", g_base); }

/* ------------------------------------------------------------------ */
/* estado del juego (página Jugar y diagnóstico)                       */
/* ------------------------------------------------------------------ */
enum { XBE_NO, XBE_OK, XBE_OTRA, XBE_PAL, XBE_RARA };
typedef struct {
    int hecho;
    wchar_t lanzador[MAX_PATH]; int hay_lanzador;
    wchar_t exe[MAX_PATH]; int hay_exe; double exe_mb; wchar_t exe_fecha[32]; wchar_t exe_var[AJ_VMAX];
    wchar_t juego[MAX_PATH]; int hay_juego;
    int xbe; const AjEdicion *ed_xbe; char hex[65];
    int v_total, v_ok, v_sin;
    wchar_t partidas[MAX_PATH]; int npartidas;
    int abierto;
    UltimoLog u;
    int ndif;
} Estado;
static Estado g_est;

static void calcular_estado(void)
{
    Estado *e = &g_est; wchar_t xbe[MAX_PATH]; const AjEdicion *ed = ed_actual(); int i;
    memset(e, 0, sizeof *e);
    ruta_lanzador(e->lanzador, MAX_PATH); e->hay_lanzador = es_arch(e->lanzador);
    ruta_exe(e->exe, MAX_PATH); e->hay_exe = es_arch(e->exe);
    if (e->hay_exe) {
        WIN32_FILE_ATTRIBUTE_DATA fa; SYSTEMTIME st; FILETIME lft; const wchar_t *p, *q;
        if (GetFileAttributesExW(e->exe, GetFileExInfoStandard, &fa)) {
            e->exe_mb = ((((unsigned long long)fa.nFileSizeHigh) << 32) | fa.nFileSizeLow) / 1048576.0;
            FileTimeToLocalFileTime(&fa.ftLastWriteTime, &lft); FileTimeToSystemTime(&lft, &st);
            _snwprintf_s(e->exe_fecha, 32, _TRUNCATE, L"%04u-%02u-%02u %02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
        }
        q = wcsrchr(e->exe, L'\\');
        for (p = q ? q - 1 : e->exe; p > e->exe && *p != L'\\'; p--) {}
        if (q && *p == L'\\') { size_t l = (size_t)(q - p - 1); if (l >= AJ_VMAX) l = AJ_VMAX - 1; wmemcpy(e->exe_var, p + 1, l); e->exe_var[l] = 0; }
    }
    e->hay_juego = carpeta_del_juego(e->juego, MAX_PATH);
    e->xbe = XBE_NO;
    if (e->hay_juego) {
        _snwprintf_s(xbe, MAX_PATH, _TRUNCATE, L"%s\\default.xbe", e->juego);
        if (es_arch(xbe)) {
            e->ed_xbe = aj_edicion_xbe(xbe, e->hex);
            if (!e->ed_xbe) e->xbe = XBE_RARA;
            else if (!e->ed_xbe->build) e->xbe = XBE_PAL;
            else e->xbe = e->ed_xbe == ed ? XBE_OK : XBE_OTRA;
        }
        {
            Verif v; memset(&v, 0, sizeof v);
            if (!verificar(e->juego, 1, &v, NULL, NULL) || v.sin_json) e->v_sin = 1;
            e->v_total = v.total; e->v_ok = v.ok;
            free(v.detalle.p);
        }
    }
    ruta_partidas(e->ed_xbe ? e->ed_xbe : ed, e->partidas, MAX_PATH);
    e->npartidas = es_dir(e->partidas) ? contar_arbol(e->partidas) : 0;
    e->abierto = juego_abierto() > 0;
    leer_ultimo_log(&e->u);
    for (i = 0; i < AJ_N; i++) if (!aj_es_defecto(&g_guardada, i)) e->ndif++;
    e->hecho = 1;
}
static int listo_para_jugar(void)
{
    return g_est.hay_lanzador && g_est.hay_exe && g_est.hay_juego && g_est.xbe != XBE_NO && g_est.xbe != XBE_PAL;
}

/* ------------------------------------------------------------------ */
/* colocación                                                          */
/* ------------------------------------------------------------------ */
static int g_x0, g_y, g_ancho, g_bx;
static void empezar(void)
{
    RECT r; GetClientRect(g_pagina, &r);
    g_x0 = S(28);
    g_ancho = r.right - S(56);
    if (g_ancho > S(900)) g_ancho = S(900);
    if (g_ancho < S(400)) g_ancho = S(400);
    g_y = S(20);
}
static void titulo_pagina(int id) { g_y += texto_en(g_x0, g_y, g_ancho, T(id), g_f_titulo, C_TEXTO) + S(10); }
static void seccion(const wchar_t *s)
{
    g_y += S(16);
    g_y += texto_en(g_x0, g_y, g_ancho, s, g_f_seccion, C_TEXTO) + S(8);
}
static void nota(const wchar_t *s, int color) { g_y += texto_en(g_x0, g_y, g_ancho, s, g_f_normal, color) + S(10); }
static void kv(const wchar_t *k, const wchar_t *v, int color)
{
    int kw = S(230), h1, h2;
    if (kw > g_ancho / 2) kw = g_ancho / 2;
    h1 = texto_en(g_x0, g_y, kw - S(12), k, g_f_normal, C_GRIS);
    h2 = texto_en(g_x0 + kw, g_y, g_ancho - kw, v, g_f_normal, color);
    g_y += (h1 > h2 ? h1 : h2) + S(6);
}
static int alto_boton(void) { return S(34); }
static void botones_ini(void) { g_bx = g_x0; }
static HWND boton_fila(const wchar_t *s, int estilo, int sub)
{
    HFONT f = estilo == B_ACENTO ? g_f_semi : g_f_normal; int w = ancho_boton(s, f); HWND b;
    if (g_bx > g_x0 && g_bx + w > g_x0 + g_ancho) { g_bx = g_x0; g_y += alto_boton() + S(8); }
    b = boton(s, estilo, 0, g_bx, g_y, w, alto_boton(), K_ACCION, -1, sub);
    g_bx += w + S(8);
    return b;
}
static void botones_fin(void) { g_y += alto_boton() + S(12); }

static HWND combo_en(int x, int y, int w, DWORD tipo, int kind, int aj, int sub)
{
    HWND c = crear(L"COMBOBOX", L"", tipo | WS_VSCROLL | WS_TABSTOP | WS_GROUP, 0, x, y, w, S(380), kind, aj, sub, NULL);
    if (c) tema_control(c, L"DarkMode_CFD", L"Explorer");
    return c;
}
static HWND edit_en(const wchar_t *s, int x, int y, int w, int h, DWORD extra, int kind, int aj, int sub, HFONT f)
{
    HWND c = crear(L"EDIT", s, ES_AUTOHSCROLL | WS_TABSTOP | WS_GROUP | WS_BORDER | extra, 0, x, y, w, h, kind, aj, sub, f);
    if (c) tema_control(c, L"DarkMode_CFD", L"Explorer");
    return c;
}

/* ------------------------------------------------------------------ */
/* teclas                                                              */
/* ------------------------------------------------------------------ */
static const struct { int vk; const char *es, *en; } k_teclas[] = {
    { VK_SPACE, "Espacio", "Space" }, { VK_RETURN, "Intro", "Enter" }, { VK_ESCAPE, "Esc", "Esc" },
    { VK_TAB, "Tab", "Tab" }, { VK_BACK, "Retroceso", "Backspace" },
    { VK_LSHIFT, "Mayús izquierda", "Left Shift" }, { VK_RSHIFT, "Mayús derecha", "Right Shift" },
    { VK_LCONTROL, "Ctrl izquierdo", "Left Ctrl" }, { VK_RCONTROL, "Ctrl derecho", "Right Ctrl" },
    { VK_LMENU, "Alt izquierdo", "Left Alt" }, { VK_SHIFT, "Mayús (cualquiera)", "Shift (any)" },
    { VK_CONTROL, "Ctrl (cualquiera)", "Ctrl (any)" },
    { VK_UP, "Flecha arriba", "Up arrow" }, { VK_DOWN, "Flecha abajo", "Down arrow" },
    { VK_LEFT, "Flecha izquierda", "Left arrow" }, { VK_RIGHT, "Flecha derecha", "Right arrow" },
    { VK_INSERT, "Insert", "Insert" }, { VK_DELETE, "Supr", "Delete" }, { VK_HOME, "Inicio", "Home" },
    { VK_END, "Fin", "End" }, { VK_PRIOR, "Re Pág", "Page Up" }, { VK_NEXT, "Av Pág", "Page Down" },
    { VK_CAPITAL, "Bloq Mayús", "Caps Lock" },
};
static void nombre_tecla(int vk, wchar_t *o, size_t n)
{
    int i;
    if (!vk) { wcsncpy_s(o, n, T(T_NINGUNA), _TRUNCATE); return; }
    for (i = 0; i < (int)(sizeof k_teclas / sizeof k_teclas[0]); i++)
        if (k_teclas[i].vk == vk) { wcsncpy_s(o, n, W(g_lang == 0 ? k_teclas[i].es : k_teclas[i].en), _TRUNCATE); return; }
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) { _snwprintf_s(o, n, _TRUNCATE, L"%c", (wchar_t)vk); return; }
    if (vk >= VK_F1 && vk <= VK_F24) { _snwprintf_s(o, n, _TRUNCATE, L"F%d", vk - VK_F1 + 1); return; }
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) { _snwprintf_s(o, n, _TRUNCATE, g_lang == 0 ? L"Num %d" : L"Numpad %d", vk - VK_NUMPAD0); return; }
    _snwprintf_s(o, n, _TRUNCATE, L"VK %d", vk);
}
/* orden del desplegable */
static int lista_teclas(int *o, int max)
{
    int n = 0, i;
    o[n++] = 0;
    for (i = 'A'; i <= 'Z' && n < max; i++) o[n++] = i;
    for (i = '0'; i <= '9' && n < max; i++) o[n++] = i;
    for (i = 0; i < (int)(sizeof k_teclas / sizeof k_teclas[0]) && n < max; i++) o[n++] = k_teclas[i].vk;
    for (i = VK_F1; i <= VK_F10 && n < max; i++) if (i != VK_F9) o[n++] = i;
    for (i = VK_NUMPAD0; i <= VK_NUMPAD9 && n < max; i++) o[n++] = i;
    return n;
}

/* ------------------------------------------------------------------ */
/* textos de un valor                                                  */
/* ------------------------------------------------------------------ */
static const char *const k_inv_es[] = { "Stick izq. X", "Stick izq. Y", "Stick der. X", "Stick der. Y" };
static const char *const k_inv_en[] = { "Left stick X", "Left stick Y", "Right stick X", "Right stick Y" };

static void texto_valor(int i, const char *v, wchar_t *o, size_t n)
{
    const AjDef *d = &aj_defs[i];
    switch (d->tipo) {
    case AJ_ELEGIR: {
        int k = aj_opcion_idx(d, v);
        o[0] = 0;
        if (k >= 0 && aj_opcs(d)) campo_n(aj_opcs(d), k, o, n);
        if (!o[0]) wcsncpy_s(o, n, W(v), _TRUNCATE);
        break; }
    case AJ_TECLA: nombre_tecla((int)strtol(v, NULL, 0), o, n); break;
    case AJ_NUM: wcsncpy_s(o, n, W(v), _TRUNCATE); break;
    default:
        if (!v[0]) wcsncpy_s(o, n, !strcmp(d->clave, "dispositivo") ? T(T_PREDETERMINADA) : T(T_VACIO_DEFECTO), _TRUNCATE);
        else wcsncpy_s(o, n, W(v), _TRUNCATE);
    }
}
static void rango_textos(const AjDef *d, wchar_t *lo, wchar_t *hi)
{
    const char *p = d->valores ? d->valores : "", *q = strstr(p, ".."); char a[32]; size_t l = q ? (size_t)(q - p) : 0;
    if (l > 31) l = 31;
    memcpy(a, p, l); a[l] = 0;
    u8w(a, lo, 32); u8w(q ? q + 2 : "", hi, 32);
    if (g_lang == 0) { wchar_t *c; for (c = lo; *c; c++) if (*c == L'.') *c = L','; for (c = hi; *c; c++) if (*c == L'.') *c = L','; }
}

/* ------------------------------------------------------------------ */
/* filas de ajustes                                                    */
/* ------------------------------------------------------------------ */
static SalidaAudio g_salidas[32];
static int g_nsalidas = -1;
static HWND g_h_mando, g_h_copias, g_h_perfil, g_h_perfiles, g_h_lic, g_h_lic_txt;
static int g_mando_activo, g_mando_ticks, g_copia_sel, g_perfil_sel, g_lic_sel;
static wchar_t g_perfil_nombre[64];
static wchar_t *g_entorno;   /* salida de "Ver qué pondría el lanzador" */

static int fila_activa(const AjDef *d) { return !d->dev || aj_dev_activo(&g_conf); }

static int linea_recomendado(int i, int x, int y, int w, int en)
{
    const AjDef *d = &aj_defs[i]; wchar_t r[300], s[400];
    texto_valor(i, d->recomendado ? d->recomendado : d->defecto, r, 300);
    _snwprintf_s(s, 400, _TRUNCATE, L"%s%s", T(T_RECOMENDADO), r);
    return texto_en(x, y, w, s, g_f_normal, en ? C_ACENTO : C_GRIS);
}

static void llenar_salidas(void)
{
    if (g_nsalidas < 0) g_nsalidas = listar_salidas(g_salidas, 32);
}

/* control de la derecha; devuelve su alto */
static int control_aj(int i, int x, int y, int w, int en)
{
    const AjDef *d = &aj_defs[i]; const char *v = g_conf.val[i]; HWND c = NULL; wchar_t s[600]; int k, n, h = S(32);
    if (d->tipo == AJ_ELEGIR) {
        char id[64];
        c = combo_en(x, y, w, CBS_DROPDOWNLIST, K_AJ_COMBO, i, 0);
        for (n = 0; aj_opcion(d, n, id, sizeof id, NULL, NULL); n++) {
            s[0] = 0;
            if (aj_opcs(d)) campo_n(aj_opcs(d), n, s, 500);
            if (!s[0]) wcsncpy_s(s, 500, W(id), _TRUNCATE);
            if (d->recomendado && !_stricmp(id, d->recomendado)) wcscat_s(s, 600, T(T_RECOMENDADO_SUF));
            SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)s);
        }
        SendMessageW(c, CB_SETCURSEL, (WPARAM)aj_opcion_idx(d, v), 0);
    } else if (d->tipo == AJ_TECLA) {
        int vk[96], nv = lista_teclas(vk, 96), cur = (int)strtol(v, NULL, 0), sel = -1;
        c = combo_en(x, y, w, CBS_DROPDOWNLIST, K_AJ_COMBO, i, 0);
        for (k = 0; k < nv; k++) {
            int it;
            nombre_tecla(vk[k], s, 600);
            if (d->recomendado && vk[k] == atoi(d->recomendado)) wcscat_s(s, 600, T(T_RECOMENDADO_SUF));
            it = (int)SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)s);
            SendMessageW(c, CB_SETITEMDATA, (WPARAM)it, (LPARAM)vk[k]);
            if (vk[k] == cur) sel = it;
        }
        if (sel < 0) {
            nombre_tecla(cur, s, 600);
            sel = (int)SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)s);
            SendMessageW(c, CB_SETITEMDATA, (WPARAM)sel, (LPARAM)cur);
        }
        SendMessageW(c, CB_SETCURSEL, (WPARAM)sel, 0);
    } else if (d->tipo == AJ_NUM) {
        double lo, hi; int dec = 1, ew = w < S(150) ? w : S(150);
        c = edit_en(W(v), x, y + S(2), ew, S(28), 0, K_AJ_EDIT, i, 0, NULL);
        if (aj_rango(d, &lo, &hi, &dec) && !dec && c) {
            HWND ud = crear(UPDOWN_CLASSW, NULL, UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS,
                            0, 0, 0, 0, 0, K_OTRO, i, 1, NULL);
            if (ud) {
                SendMessageW(ud, UDM_SETBUDDY, (WPARAM)c, 0);
                SendMessageW(ud, UDM_SETRANGE32, (WPARAM)(int)lo, (LPARAM)(int)hi);
                SendMessageW(ud, UDM_SETPOS32, 0, (LPARAM)atoi(v));
                EnableWindow(ud, en);
            }
        }
    } else if (!strcmp(d->clave, "dispositivo")) {
        int sel = 0, it; wchar_t xid[300]; char u[900];
        llenar_salidas();
        c = combo_en(x, y, w, CBS_DROPDOWNLIST, K_AJ_COMBO, i, 0);
        _snwprintf_s(s, 600, _TRUNCATE, L"%s%s", T(T_PREDETERMINADA), T(T_RECOMENDADO_SUF));
        it = (int)SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)s);
        SendMessageW(c, CB_SETITEMDATA, (WPARAM)it, (LPARAM)-1);
        for (k = 0; k < g_nsalidas; k++) {
            it = (int)SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)g_salidas[k].nombre);
            SendMessageW(c, CB_SETITEMDATA, (WPARAM)it, (LPARAM)k);
            id_xaudio2(g_salidas[k].id, xid, 300); wu8(xid, u, sizeof u);
            if (v[0] && !_stricmp(u, v)) sel = it;
        }
        if (v[0] && sel == 0) {
            fmt(s, 600, T_NO_CONECTADA, W(v));
            sel = (int)SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)s);
            SendMessageW(c, CB_SETITEMDATA, (WPARAM)sel, (LPARAM)-2);
        }
        SendMessageW(c, CB_SETCURSEL, (WPARAM)sel, 0);
    } else if (!strcmp(d->clave, "variante")) {
        wchar_t pat[MAX_PATH], ex[MAX_PATH]; WIN32_FIND_DATAW fd; HANDLE hf;
        c = combo_en(x, y, w, CBS_DROPDOWN | CBS_AUTOHSCROLL | CBS_SORT, K_AJ_COMBO, i, 0);
        _snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\build\\*", g_base);
        hf = FindFirstFileW(pat, &fd);
        if (hf != INVALID_HANDLE_VALUE) {
            do {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
                _snwprintf_s(ex, MAX_PATH, _TRUNCATE, L"%s\\build\\%s\\obiwan_recomp.exe", g_base, fd.cFileName);
                if (es_arch(ex)) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)fd.cFileName);
            } while (FindNextFileW(hf, &fd));
            FindClose(hf);
        }
        SetWindowTextW(c, W(v));
    } else {   /* texto */
        c = edit_en(W(v), x, y + S(2), w, S(28), 0, K_AJ_EDIT, i, 0, NULL);
        if (c) SendMessageW(c, EM_SETLIMITTEXT, AJ_VMAX - 2, 0);
    }
    if (c) EnableWindow(c, en);
    return h;
}

static void fila_check(int i)
{
    const AjDef *d = &aj_defs[i]; const wchar_t *nom = W(aj_nombre(d));
    boton(nom, B_CHECK, !_stricmp(g_conf.val[i], "si"), g_x0, g_y, ancho_check(nom) + S(8), S(30), K_AJ_CHECK, i, 0);
    g_y += S(32);
    g_y += texto_en(g_x0 + S(28), g_y, g_ancho - S(28), W(aj_ayuda(d)), g_f_normal, C_GRIS) + S(2);
    g_y += linea_recomendado(i, g_x0 + S(28), g_y, g_ancho - S(28), 1) + S(16);
}

static void fila(int i)
{
    const AjDef *d = &aj_defs[i]; int en = fila_activa(d), cw, nwid, xc, hl, hc, k;
    wchar_t s[900];
    int ancha = d->tipo == AJ_LISTA || (d->tipo == AJ_TEXTO && (!strcmp(d->seccion, "Paths") || !strcmp(d->clave, "extra")));
    if (!strcmp(d->seccion, "Developer") && !strcmp(d->clave, "activo")) { fila_check(i); return; }
    cw = g_ancho * 45 / 100; if (cw > S(300)) cw = S(300);
    nwid = ancha ? g_ancho : g_ancho - cw - S(16);
    xc = g_x0 + g_ancho - cw;
    hl = S(5) + texto_en(g_x0, g_y + S(5), nwid, W(aj_nombre(d)), g_f_semi, en ? C_TEXTO : C_GRIS);
    if (d->tipo == AJ_TECLA) {
        int cur = (int)strtol(g_conf.val[i], NULL, 0);
        if (cur) for (k = 0; k < AJ_N; k++)
            if (k != i && aj_defs[k].tipo == AJ_TECLA && (int)strtol(g_conf.val[k], NULL, 0) == cur) {
                fmt(s, 900, T_TECLA_REPETIDA, W(aj_nombre(&aj_defs[k])));
                hl += S(2) + texto_en(g_x0, g_y + hl + S(2), nwid, s, g_f_normal, C_AVISO);
                break;
            }
    } else {
        wcsncpy_s(s, 900, W(aj_ayuda(d)), _TRUNCATE);
        if (d->tipo == AJ_NUM) {
            wchar_t lo[32], hi[32], r[96];
            rango_textos(d, lo, hi);
            fmt(r, 96, T_RANGO, lo, hi);
            wcscat_s(s, 900, L" ("); wcscat_s(s, 900, r); wcscat_s(s, 900, L").");
        }
        hl += S(2) + texto_en(g_x0, g_y + hl + S(2), nwid, s, g_f_normal, C_GRIS);
    }
    hl += S(2) + linea_recomendado(i, g_x0, g_y + hl + S(2), nwid, en);
    if (!ancha) {
        hc = control_aj(i, xc, g_y, cw, en);
        g_y += (hl > hc ? hl : hc) + S(16);
        return;
    }
    g_y += hl + S(8);
    if (d->tipo == AJ_LISTA) {   /* una casilla por ficha */
        const char *p = d->valores; int x = g_x0, t = 0;
        while (*p) {
            const char *e = strchr(p, ','); size_t l = e ? (size_t)(e - p) : strlen(p); char tok[16]; const char *q; int m = 0;
            if (l >= sizeof tok) l = sizeof tok - 1;
            memcpy(tok, p, l); tok[l] = 0;
            for (q = g_conf.val[i]; (q = strstr(q, tok)) != NULL; q += l)
                if ((q == g_conf.val[i] || q[-1] == ',') && (q[l] == 0 || q[l] == ',')) { m = 1; break; }
            {
                const wchar_t *lab = t < 4 && !strcmp(d->clave, "mando_invertir") ? W(g_lang == 0 ? k_inv_es[t] : k_inv_en[t]) : W(tok);
                int bw = ancho_check(lab) + S(8);
                HWND b;
                if (x > g_x0 && x + bw > g_x0 + g_ancho) { x = g_x0; g_y += S(34); }
                b = boton(lab, B_CHECK, m, x, g_y, bw, S(30), K_AJ_LISTA, i, t);
                if (b) EnableWindow(b, en);
                x += bw + S(12);
            }
            t++;
            p += l; if (*p == ',') p++;
        }
        g_y += S(30) + S(16);
        return;
    }
    {   /* texto ancho, con "Elegir…" en las carpetas */
        int bw = 0; HWND c;
        if (!strcmp(d->seccion, "Paths")) bw = ancho_boton(T(T_ELEGIR_CARPETA), g_f_normal);
        c = edit_en(W(g_conf.val[i]), g_x0, g_y + S(3), g_ancho - (bw ? bw + S(8) : 0), S(28), 0, K_AJ_EDIT, i, 0, NULL);
        if (c) { SendMessageW(c, EM_SETLIMITTEXT, AJ_VMAX - 2, 0); EnableWindow(c, en); }
        if (bw) {
            HWND b = boton(T(T_ELEGIR_CARPETA), B_NORMAL, 0, g_x0 + g_ancho - bw, g_y, bw, alto_boton(), K_ACCION, i, 0 /* A_ELEGIR */);
            if (b) EnableWindow(b, en);
        }
        g_y += alto_boton() + S(16);
    }
}

static void filas_de(int pagina, int (*filtro)(const AjDef *))
{
    int i;
    for (i = 0; i < AJ_N; i++)
        if (aj_defs[i].pestana == pagina && (!filtro || filtro(&aj_defs[i]))) fila(i);
}
static int f_calidad(const AjDef *d)
{
    return !strcmp(d->clave, "escala_interna") || !strcmp(d->clave, "antialias") || !strcmp(d->clave, "anisotropico") || !strcmp(d->clave, "mipmaps");
}
static int f_avanzado(const AjDef *d) { return !strcmp(d->seccion, "Compat"); }
static int f_mando(const AjDef *d) { return !strcmp(d->clave, "vibracion") || !strncmp(d->clave, "mando_", 6); }
static int f_raton(const AjDef *d) { return !strncmp(d->clave, "raton_", 6); }
static int f_teclas(const AjDef *d) { return d->tipo == AJ_TECLA; }
static int f_arreglo(const AjDef *d) { return d->valores && !strncmp(d->valores, "si;no>RECOMP_FIX_", 17); }
static int f_medida(const AjDef *d) { return d->valores && !strncmp(d->valores, "no;si>", 6) && strcmp(d->clave, "activo"); }
static int f_otro_dev(const AjDef *d) { return d->tipo == AJ_TEXTO; }

/* ------------------------------------------------------------------ */
/* acciones (sub de K_ACCION; aj >= 0 = "Elegir…" de esa fila)         */
/* ------------------------------------------------------------------ */
enum { A_ELEGIR, A_JUGAR, A_JUGAR_MEDIR, A_COMPROBAR, A_ABRIR_LOG, A_ABRIR_LOGS, A_PROBAR_SONIDO, A_PROBAR_MANDO,
       A_IMPORTAR, A_ABRIR_PARTIDAS, A_ABRIR_JUEGO, A_COPIAR, A_RESTAURAR, A_MEDIR, A_DIAG, A_VERIFICAR,
       A_CACHE, A_GUARDAR_PERFIL, A_CARGAR_PERFIL, A_RESTABLECER, A_VER_ENTORNO };
enum { O_COPIAS = 1, O_PERFIL, O_PERFILES, O_LIC, O_LIC_TXT, O_ENTORNO, O_MANDO };

/* ------------------------------------------------------------------ */
/* páginas                                                             */
/* ------------------------------------------------------------------ */
static void pagina_jugar(void)
{
    Estado *e = &g_est; wchar_t s[1200], c[MAX_PATH]; int listo;
    if (!e->hecho) calcular_estado();
    titulo_pagina(T_NAV_JUGAR);
    listo = listo_para_jugar();
    g_y += texto_en(g_x0, g_y, g_ancho, T(listo ? T_LISTO : T_NO_LISTO), g_f_seccion, listo ? C_OK : C_ERROR) + S(12);
    botones_ini();
    { HWND b = boton_fila(T(T_JUGAR), B_ACENTO, A_JUGAR); if (b && !listo) EnableWindow(b, FALSE); }
    { HWND b = boton_fila(T(T_JUGAR_MEDIR), B_NORMAL, A_JUGAR_MEDIR); if (b && !listo) EnableWindow(b, FALSE); }
    boton_fila(T(T_COMPROBAR), B_NORMAL, A_COMPROBAR);
    botones_fin();
    nota(T(T_JUGAR_AYUDA), C_GRIS);

    seccion(T(T_ESTADO_JUEGO));
    fmt(s, 1200, e->hay_lanzador ? T_ENCONTRADO : T_FALTA, L"Jugar Obi-Wan 6.exe");
    kv(T(T_LANZADOR), s, e->hay_lanzador ? C_TEXTO : C_ERROR);
    ruta_corta(e->exe, c, MAX_PATH);
    if (e->hay_exe) fmt(s, 1200, T_COMPILADO_V, e->exe_var, e->exe_mb, e->exe_fecha); else fmt(s, 1200, T_FALTA, c);
    kv(T(T_COMPILADO), s, e->hay_exe ? C_TEXTO : C_ERROR);
    kv(T(T_EDICION), W(ed_nombre(ed_actual())), C_TEXTO);
    if (e->hay_juego) { ruta_corta(e->juego, c, MAX_PATH); kv(T(T_CARPETA_JUEGO), c, C_TEXTO); }
    else kv(T(T_CARPETA_JUEGO), T(T_SIN_CARPETA), C_ERROR);
    if (e->hay_juego) {
        int col = C_OK;
        switch (e->xbe) {
        case XBE_OK: case XBE_OTRA: fmt(s, 1200, T_FIRMA_OK, W(ed_nombre(e->ed_xbe))); col = e->xbe == XBE_OK ? C_OK : C_AVISO; break;
        case XBE_PAL: fmt(s, 1200, T_FIRMA_PAL, W(ed_nombre(e->ed_xbe))); col = C_ERROR; break;
        case XBE_RARA: wcsncpy_s(s, 1200, T(T_FIRMA_DESCONOCIDA), _TRUNCATE); col = C_AVISO; break;
        default: wcsncpy_s(s, 1200, T(T_FIRMA_NO_XBE), _TRUNCATE); col = C_ERROR;
        }
        kv(T(T_FIRMA), s, col);
        if (e->v_sin) kv(T(T_ARCHIVOS), T(T_ARCH_SIN), C_GRIS);
        else if (e->v_ok == e->v_total) { fmt(s, 1200, T_ARCH_OK, e->v_total); kv(T(T_ARCHIVOS), s, C_OK); }
        else { fmt(s, 1200, T_ARCH_MAL, e->v_total - e->v_ok, e->v_total); kv(T(T_ARCHIVOS), s, C_AVISO); }
    }
    ruta_corta(e->partidas, c, MAX_PATH);
    if (e->npartidas > 0) fmt(s, 1200, T_PART_N, e->npartidas, c); else fmt(s, 1200, T_PART_VACIA, c);
    kv(T(T_PARTIDAS), s, C_TEXTO);
    ruta_corta(g_ini, c, MAX_PATH);
    if (!es_arch(g_ini)) wcsncpy_s(s, 1200, T(T_INI_NO), _TRUNCATE);
    else if (e->ndif) fmt(s, 1200, T_INI_N, e->ndif, c);
    else fmt(s, 1200, T_INI_DEFECTO, c);
    kv(T(T_AJUSTES), s, C_TEXTO);
    if (g_guardada.navisos) { fmt(s, 1200, T_INI_AVISOS, g_guardada.navisos); kv(L"", s, C_AVISO); }
    kv(T(T_ABIERTO), e->abierto ? T(T_ABIERTO_SI) : T(T_NO), e->abierto ? C_AVISO : C_TEXTO);

    seccion(T(T_ULTIMA));
    if (!e->u.hay) nota(T(T_ULT_NINGUNA), C_GRIS);
    else {
        UltimoLog *u = &e->u;
        _snwprintf_s(s, 1200, _TRUNCATE, L"%04u-%02u-%02u %02u:%02u · %s", u->fecha.wYear, u->fecha.wMonth, u->fecha.wDay,
                     u->fecha.wHour, u->fecha.wMinute, u->nombre);
        kv(T(T_ULT_FECHA), s, C_TEXTO);
        if (!u->cierre) kv(T(T_ULT_RESULTADO), T(T_ULT_SIN), C_AVISO);
        else if (!u->codigo) kv(T(T_ULT_RESULTADO), T(T_ULT_BIEN), C_OK);
        else { fmt(s, 1200, T_ULT_ERROR, u->codigo); kv(T(T_ULT_RESULTADO), s, C_ERROR); }
        if (u->fps_n) { fmt(s, 1200, T_ULT_FPS_V, u->fps_suma / u->fps_n, u->fps_n); kv(T(T_ULT_FPS), s, C_TEXTO); }
        else kv(T(T_ULT_FPS), T(T_SIN_DATOS), C_GRIS);
        if (u->ft_n) { fmt(s, 1200, T_ULT_TIRONES_V, u->ft_33, u->ft_50, u->ft_frames); kv(T(T_ULT_TIRONES), s, u->ft_50 ? C_AVISO : C_TEXTO); }
        else kv(T(T_ULT_TIRONES), T(T_ULT_TIRONES_NO), C_GRIS);
        if (u->modo[0]) kv(T(T_ULT_MODO), W(u->modo), C_TEXTO);
        g_y += S(6);
        botones_ini();
        boton_fila(T(T_ABRIR_LOG), B_NORMAL, A_ABRIR_LOG);
        boton_fila(T(T_ABRIR_LOGS), B_NORMAL, A_ABRIR_LOGS);
        botones_fin();
    }
}

typedef struct { wchar_t *o; size_t n; int k; } MonCtx;
static BOOL CALLBACK monitor_cb(HMONITOR hm, HDC dc, LPRECT r, LPARAM l)
{
    MonCtx *c = (MonCtx *)l; MONITORINFO mi; wchar_t t[128];
    (void)dc; (void)r;
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(hm, &mi)) return TRUE;
    _snwprintf_s(t, 128, _TRUNCATE, L"%s%d: %d×%d%s%s%s", c->k ? L" · " : L"", c->k,
                 (int)(mi.rcMonitor.right - mi.rcMonitor.left), (int)(mi.rcMonitor.bottom - mi.rcMonitor.top),
                 (mi.dwFlags & MONITORINFOF_PRIMARY) ? L" (" : L"", (mi.dwFlags & MONITORINFOF_PRIMARY) ? T(T_PRINCIPAL) : L"",
                 (mi.dwFlags & MONITORINFOF_PRIMARY) ? L")" : L"");
    wcscat_s(c->o, c->n, t);
    c->k++;
    return TRUE;
}
static void texto_monitores(wchar_t *o, size_t n)
{
    MonCtx c; c.o = o; c.n = n; c.k = 0; o[0] = 0;
    EnumDisplayMonitors(NULL, NULL, monitor_cb, (LPARAM)&c);
}

static void pagina_pantalla(void)
{
    wchar_t m[600], s[800];
    titulo_pagina(T_NAV_PANTALLA);
    nota(T(T_NOTA_PANTALLA), C_GRIS);
    texto_monitores(m, 600);
    fmt(s, 800, T_MONITORES, m);
    nota(s, C_GRIS);
    g_y += S(6);
    filas_de(AJP_PANTALLA, NULL);
}

static void pagina_graficos(void)
{
    titulo_pagina(T_NAV_GRAFICOS);
    fila(AJ("Graphics", "preset"));
    seccion(T(T_SEC_CALIDAD));
    filas_de(AJP_GRAFICOS, f_calidad);
    seccion(T(T_SEC_AVANZADO));
    filas_de(AJP_GRAFICOS, f_avanzado);
    g_y += S(4);
    nota(T(T_NOTA_ULTRA), C_GRIS);
    nota(T(T_NOTA_ARREGLOS), C_GRIS);
}

static void pagina_audio(void)
{
    titulo_pagina(T_NAV_AUDIO);
    filas_de(AJP_AUDIO, NULL);
    botones_ini();
    boton_fila(T(T_PROBAR_SONIDO), B_NORMAL, A_PROBAR_SONIDO);
    botones_fin();
    nota(T(T_PROBAR_SONIDO_AYUDA), C_GRIS);
    g_y += S(8);
    nota(T(T_NOTA_VOLUMENES), C_GRIS);
}

static void pagina_controles(void)
{
    wchar_t exe[MAX_PATH], mkb[MAX_PATH], s[1200], c[MAX_PATH], *sl;
    titulo_pagina(T_NAV_CONTROLES);
    ruta_exe(exe, MAX_PATH);
    wcsncpy_s(mkb, MAX_PATH, exe, _TRUNCATE);
    sl = wcsrchr(mkb, L'\\');
    if (sl) { *sl = 0; wcscat_s(mkb, MAX_PATH, L"\\mkb.ini"); }
    if (es_arch(mkb)) { ruta_corta(mkb, c, MAX_PATH); fmt(s, 1200, T_MKB_INI, c); nota(s, C_AVISO); }
    seccion(T(T_SEC_MANDO));
    filas_de(AJP_CONTROLES, f_mando);
    botones_ini();
    boton_fila(T(g_mando_activo ? T_PARAR_MANDO : T_PROBAR_MANDO), B_NORMAL, A_PROBAR_MANDO);
    botones_fin();
    nota(T(T_PROBAR_MANDO_AYUDA), C_GRIS);
    {
        int h = medir_texto(g_f_mono, L"X\nX", g_ancho, NULL);
        g_h_mando = crear(L"STATIC", L"", SS_NOPREFIX | SS_LEFT, 0, g_x0, g_y, g_ancho, h, K_OTRO, -1, O_MANDO, g_f_mono);
        if (g_h_mando) SetWindowLongPtrW(g_h_mando, GWLP_USERDATA, C_TEXTO + 1);
        g_y += h + S(8);
    }
    seccion(T(T_SEC_RATON));
    filas_de(AJP_CONTROLES, f_raton);
    seccion(T(T_SEC_TECLAS));
    fila(AJ("Input", "teclado_cambiar_sticks"));
    filas_de(AJP_CONTROLES, f_teclas);
    nota(T(T_NOTA_TECLAS), C_GRIS);
}

static int edicion_importada(const AjEdicion *e, wchar_t *donde, size_t n)
{
    wchar_t wid[32], j[MAX_PATH];
    u8w(e->id, wid, 32);
    _snwprintf_s(donde, n, _TRUNCATE, L"%s\\private\\game\\%s", g_base, wid);
    _snwprintf_s(j, MAX_PATH, _TRUNCATE, L"%s\\EXTRACCION.json", donde);
    if (es_arch(j)) return 1;
    _snwprintf_s(donde, n, _TRUNCATE, L"%s\\game\\%s", g_local, wid);
    _snwprintf_s(j, MAX_PATH, _TRUNCATE, L"%s\\EXTRACCION.json", donde);
    return es_arch(j);
}

static void pagina_idioma(void)
{
    int k, cols = g_ancho >= S(640), ie = AJ("Language", "edicion");
    titulo_pagina(T_NAV_IDIOMA);
    fila(AJ("Language", "interfaz"));
    seccion(T(T_SEC_EDICIONES));
    nota(T(T_NOTA_EDICIONES), C_GRIS);
    {
        int w0 = S(220), w1 = S(90), w2 = S(130), w3 = S(210), x1 = g_x0 + w0, x2 = x1 + w1, x3 = x2 + w2, x4 = x3 + w3, w4 = g_x0 + g_ancho - x4;
        if (cols) {
            texto_en(g_x0 + S(28), g_y, w0 - S(28), T(T_EDICION), g_f_semi, C_GRIS);
            texto_en(x1, g_y, w1, T(T_COL_IDIOMA), g_f_semi, C_GRIS);
            texto_en(x2, g_y, w2, T(T_COL_FIRMA), g_f_semi, C_GRIS);
            texto_en(x3, g_y, w3, T(T_COL_ESTADO), g_f_semi, C_GRIS);
            g_y += texto_en(x4, g_y, w4, T(T_COL_CARPETA), g_f_semi, C_GRIS) + S(6);
        }
        for (k = 0; k < AJ_NED; k++) {
            const AjEdicion *e = &aj_ediciones[k]; wchar_t firma[40], est[200], car[MAX_PATH + 64], d[MAX_PATH], cd[MAX_PATH];
            const wchar_t *nom = W(ed_nombre(e)); HWND b; int h = S(30);
            _snwprintf_s(firma, 40, _TRUNCATE, L"%.12s…", W(e->sha256));
            wcsncpy_s(est, 200, T(e->build ? T_ED_LISTA : T_ED_FALTA), _TRUNCATE);
            if (edicion_importada(e, d, MAX_PATH)) { ruta_corta(d, cd, MAX_PATH); fmt(car, MAX_PATH + 64, T_ED_CARPETA_SI, cd); }
            else wcsncpy_s(car, MAX_PATH + 64, T(T_ED_CARPETA_NO), _TRUNCATE);
            b = boton(nom, B_RADIO, !_stricmp(g_conf.val[ie], e->id), g_x0, g_y, cols ? w0 - S(8) : g_ancho, S(30), K_EDICION, ie, k);
            if (b && !e->build) EnableWindow(b, FALSE);
            if (cols) {
                int t;
                t = texto_en(x1, g_y + S(6), w1 - S(8), W(ed_idioma(e)), g_f_normal, C_TEXTO); if (t + S(6) > h) h = t + S(6);
                t = texto_en(x2, g_y + S(6), w2 - S(8), firma, g_f_mono, C_GRIS); if (t + S(6) > h) h = t + S(6);
                t = texto_en(x3, g_y + S(6), w3 - S(8), est, g_f_normal, e->build ? C_OK : C_AVISO); if (t + S(6) > h) h = t + S(6);
                t = texto_en(x4, g_y + S(6), w4, car, g_f_normal, C_GRIS); if (t + S(6) > h) h = t + S(6);
                g_y += h + S(8);
            } else {
                wchar_t s[600];
                g_y += S(32);
                _snwprintf_s(s, 600, _TRUNCATE, L"%s · %s · %s · %s", W(ed_idioma(e)), firma, est, car);
                g_y += texto_en(g_x0 + S(28), g_y, g_ancho - S(28), s, g_f_normal, C_GRIS) + S(10);
            }
        }
    }
    g_y += S(6);
    botones_ini();
    boton_fila(T(T_IMPORTAR), B_ACENTO, A_IMPORTAR);
    botones_fin();
    nota(T(T_IMPORTAR_AYUDA), C_GRIS);
}

static void dir_copias(wchar_t *o, size_t n) { _snwprintf_s(o, n, _TRUNCATE, L"%s\\copias-partidas", g_local); }

/* nombres de carpetas o .ini de un patrón, de más nuevo a más viejo por nombre */
static int cmp_wdesc(const void *a, const void *b) { return -wcscmp((const wchar_t *)a, (const wchar_t *)b); }
static int listar_nombres(const wchar_t *pat, int dirs, wchar_t (*o)[MAX_PATH], int max, int quitar_ext)
{
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW(pat, &fd); int n = 0, k;
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        int esd = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (fd.cFileName[0] == L'.' || esd != dirs) continue;
        wcsncpy_s(o[n], MAX_PATH, fd.cFileName, _TRUNCATE);
        if (quitar_ext) { wchar_t *p = wcsrchr(o[n], L'.'); if (p) *p = 0; }
        for (k = 0; k < n; k++) if (!_wcsicmp(o[k], o[n])) break;
        if (k == n) n++;
    } while (n < max && FindNextFileW(h, &fd));
    FindClose(h);
    qsort(o, (size_t)n, sizeof o[0], cmp_wdesc);
    return n;
}

static void pagina_herramientas(void)
{
    wchar_t s[1200], c[MAX_PATH], d[MAX_PATH], pat[MAX_PATH]; static wchar_t nombres[64][MAX_PATH]; int n, k, w;
    titulo_pagina(T_NAV_HERRAMIENTAS);
    seccion(T(T_SEC_CARPETAS));
    filas_de(AJP_HERRAMIENTAS, NULL);
    botones_ini();
    boton_fila(T(T_ABRIR_PARTIDAS), B_NORMAL, A_ABRIR_PARTIDAS);
    boton_fila(T(T_ABRIR_JUEGO), B_NORMAL, A_ABRIR_JUEGO);
    botones_fin();

    seccion(T(T_SEC_COPIAS));
    dir_copias(d, MAX_PATH); ruta_corta(d, c, MAX_PATH);
    fmt(s, 1200, T_COPIAS_AYUDA, c);
    nota(s, C_GRIS);
    botones_ini();
    boton_fila(T(T_COPIAR), B_NORMAL, A_COPIAR);
    botones_fin();
    _snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\partidas-*", d);
    n = listar_nombres(pat, 1, nombres, 64, 0);
    w = g_ancho - ancho_boton(T(T_RESTAURAR), g_f_normal) - S(8);
    if (w > S(420)) w = S(420);
    g_h_copias = combo_en(g_x0, g_y + S(2), w, CBS_DROPDOWNLIST, K_OTRO, -1, O_COPIAS);
    if (!n) SendMessageW(g_h_copias, CB_ADDSTRING, 0, (LPARAM)T(T_SIN_COPIAS));
    for (k = 0; k < n; k++) SendMessageW(g_h_copias, CB_ADDSTRING, 0, (LPARAM)nombres[k]);
    if (g_copia_sel >= (n ? n : 1)) g_copia_sel = 0;
    SendMessageW(g_h_copias, CB_SETCURSEL, (WPARAM)g_copia_sel, 0);
    { HWND b = boton(T(T_RESTAURAR), B_NORMAL, 0, g_x0 + w + S(8), g_y, ancho_boton(T(T_RESTAURAR), g_f_normal), alto_boton(), K_ACCION, -1, A_RESTAURAR);
      if (b && !n) EnableWindow(b, FALSE); if (!n) EnableWindow(g_h_copias, FALSE); }
    g_y += alto_boton() + S(12);

    seccion(T(T_SEC_MEDIR));
    nota(T(T_MEDIR_AYUDA), C_GRIS);
    botones_ini(); boton_fila(T(T_MEDIR_TODO), B_NORMAL, A_MEDIR); botones_fin();

    seccion(T(T_SEC_DIAG));
    nota(T(T_DIAG_AYUDA), C_GRIS);
    botones_ini(); boton_fila(T(T_EXPORTAR_DIAG), B_NORMAL, A_DIAG); botones_fin();

    seccion(T(T_SEC_VERIF));
    nota(T(T_VERIF_AYUDA), C_GRIS);
    botones_ini(); boton_fila(T(T_VERIFICAR), B_NORMAL, A_VERIFICAR); botones_fin();

    seccion(T(T_SEC_PERFILES));
    nota(T(T_PERFIL_AYUDA), C_GRIS);
    {
        int lw = S(170), bw = ancho_boton(T(T_GUARDAR_PERFIL), g_f_normal), ew = g_ancho - lw - bw - S(16);
        if (ew > S(260)) ew = S(260);
        texto_en(g_x0, g_y + S(7), lw - S(8), T(T_PERFIL_NOMBRE), g_f_normal, C_TEXTO);
        g_h_perfil = edit_en(g_perfil_nombre, g_x0 + lw, g_y + S(3), ew, S(28), 0, K_OTRO, -1, O_PERFIL, NULL);
        if (g_h_perfil) SendMessageW(g_h_perfil, EM_SETLIMITTEXT, 60, 0);
        boton(T(T_GUARDAR_PERFIL), B_NORMAL, 0, g_x0 + lw + ew + S(8), g_y, bw, alto_boton(), K_ACCION, -1, A_GUARDAR_PERFIL);
        g_y += alto_boton() + S(8);
        {
            static wchar_t p1[64][MAX_PATH], p2[64][MAX_PATH]; int n1, n2, j; HWND b;
            _snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\perfiles\\*.ini", g_base);
            n1 = listar_nombres(pat, 0, p1, 64, 1);
            _snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\perfiles\\*.ini", g_local);
            n2 = listar_nombres(pat, 0, p2, 64, 1);
            for (j = 0; j < n2 && n1 < 64; j++) {
                int q; for (q = 0; q < n1; q++) if (!_wcsicmp(p1[q], p2[j])) break;
                if (q == n1) wcsncpy_s(p1[n1++], MAX_PATH, p2[j], _TRUNCATE);
            }
            g_h_perfiles = combo_en(g_x0 + lw, g_y + S(2), ew, CBS_DROPDOWNLIST, K_OTRO, -1, O_PERFILES);
            if (!n1) SendMessageW(g_h_perfiles, CB_ADDSTRING, 0, (LPARAM)T(T_SIN_PERFILES));
            for (j = 0; j < n1; j++) SendMessageW(g_h_perfiles, CB_ADDSTRING, 0, (LPARAM)p1[j]);
            if (g_perfil_sel >= (n1 ? n1 : 1)) g_perfil_sel = 0;
            SendMessageW(g_h_perfiles, CB_SETCURSEL, (WPARAM)g_perfil_sel, 0);
            b = boton(T(T_CARGAR_PERFIL), B_NORMAL, 0, g_x0 + lw + ew + S(8), g_y, ancho_boton(T(T_CARGAR_PERFIL), g_f_normal), alto_boton(), K_ACCION, -1, A_CARGAR_PERFIL);
            if (!n1) { EnableWindow(g_h_perfiles, FALSE); if (b) EnableWindow(b, FALSE); }
            g_y += alto_boton() + S(12);
        }
    }

    seccion(T(T_SEC_MANTENIMIENTO));
    nota(T(T_CACHE_AYUDA), C_GRIS);
    botones_ini(); boton_fila(T(T_BORRAR_CACHE), B_NORMAL, A_CACHE); botones_fin();
    botones_ini(); boton_fila(T(T_RESTABLECER), B_NORMAL, A_RESTABLECER); botones_fin();
}

static void pagina_dev(void)
{
    titulo_pagina(T_NAV_DEV);
    fila(AJ("Developer", "activo"));
    if (!aj_dev_activo(&g_conf)) nota(T(T_DEV_INACTIVO), C_AVISO);
    seccion(T(T_SEC_ARREGLOS));
    filas_de(AJP_DEV, f_arreglo);
    seccion(T(T_SEC_MEDIDAS));
    filas_de(AJP_DEV, f_medida);
    seccion(T(T_SEC_OTROS));
    filas_de(AJP_DEV, f_otro_dev);
    botones_ini(); boton_fila(T(T_VER_ENTORNO), B_NORMAL, A_VER_ENTORNO); botones_fin();
    nota(T(T_VER_ENTORNO_AYUDA), C_GRIS);
    if (g_entorno) {
        int lineas = 1, h; const wchar_t *p;
        for (p = g_entorno; *p; p++) if (*p == L'\n') lineas++;
        h = medir_texto(g_f_mono, L"X", 0, NULL) * (lineas < 24 ? lineas : 24) + S(12);
        edit_en(g_entorno, g_x0, g_y, g_ancho, h, ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL, K_OTRO, -1, O_ENTORNO, g_f_mono);
        g_y += h + S(12);
    }
}

/* licencias: RCDATA 201..208 (configurador_obiwan.rc) */
static const char *const k_lic_nombres[] = {
    "Port Obi-Wan (obiwan-claude-completo) — LICENSE",
    "xboxrecomp (claude-completo4) — LICENSE",
    "gpu_tt — Two Towers Recompiled",
    "gpu_tt — xboxrecomp 0.12.0",
    "LGPL 2.1",
    "LICENSES/README.md",
    "NOTICE",
    "SDL2 — LICENSE.txt",
};
#define N_LIC ((int)(sizeof k_lic_nombres / sizeof k_lic_nombres[0]))
static wchar_t *texto_licencia(int k)
{
    HRSRC r = FindResourceW(g_inst, MAKEINTRESOURCEW(201 + k), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
    HGLOBAL g; const char *p; DWORD n; int wn, i, j; wchar_t *w, *o; UINT cp = CP_UTF8;
    if (!r || !(g = LoadResource(g_inst, r)) || !(p = (const char *)LockResource(g))) return NULL;
    n = SizeofResource(g_inst, r);
    if (n >= 3 && !memcmp(p, "\xEF\xBB\xBF", 3)) { p += 3; n -= 3; }
    wn = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, (int)n, NULL, 0);
    if (wn <= 0) { cp = 1252; wn = MultiByteToWideChar(cp, 0, p, (int)n, NULL, 0); }
    if (wn < 0) wn = 0;
    w = (wchar_t *)malloc(((size_t)wn + 1) * sizeof *w);
    o = (wchar_t *)malloc(((size_t)wn * 2 + 1) * sizeof *o);
    if (!w || !o) { free(w); free(o); return NULL; }
    MultiByteToWideChar(cp, 0, p, (int)n, w, wn);
    for (i = 0, j = 0; i < wn; i++) {   /* LF -> CRLF */
        if (w[i] == L'\n' && (i == 0 || w[i - 1] != L'\r')) o[j++] = L'\r';
        o[j++] = w[i];
    }
    o[j] = 0;
    free(w);
    return o;
}
static void poner_licencia(void)
{
    wchar_t *t;
    if (!g_h_lic_txt) return;
    t = texto_licencia(g_lic_sel);
    SetWindowTextW(g_h_lic_txt, t ? t : L"");
    free(t);
}
static void pagina_creditos(void)
{
    int k;
    titulo_pagina(T_NAV_CREDITOS);
    nota(T(T_CREDITOS_TEXTO), C_TEXTO);
    nota(T(T_MARCAS), C_GRIS);
    seccion(T(T_LICENCIA));
    g_h_lic = combo_en(g_x0, g_y, g_ancho < S(460) ? g_ancho : S(460), CBS_DROPDOWNLIST, K_OTRO, -1, O_LIC);
    for (k = 0; k < N_LIC; k++) SendMessageW(g_h_lic, CB_ADDSTRING, 0, (LPARAM)W(k_lic_nombres[k]));
    if (g_lic_sel < 0 || g_lic_sel >= N_LIC) g_lic_sel = 0;
    SendMessageW(g_h_lic, CB_SETCURSEL, (WPARAM)g_lic_sel, 0);
    g_y += S(40);
    g_h_lic_txt = edit_en(L"", g_x0, g_y, g_ancho, S(380), ES_MULTILINE | ES_READONLY | WS_VSCROLL | ES_AUTOVSCROLL, K_OTRO, -1, O_LIC_TXT, g_f_mono);
    if (g_h_lic_txt) { SendMessageW(g_h_lic_txt, EM_SETLIMITTEXT, 0, 0); poner_licencia(); }
    g_y += S(380) + S(12);
}

static void construir_pagina(void)
{
    g_h_mando = g_h_copias = g_h_perfil = g_h_perfiles = g_h_lic = g_h_lic_txt = NULL;
    empezar();
    switch (g_pag) {
    case AJP_JUGAR: pagina_jugar(); break;
    case AJP_PANTALLA: pagina_pantalla(); break;
    case AJP_GRAFICOS: pagina_graficos(); break;
    case AJP_AUDIO: pagina_audio(); break;
    case AJP_CONTROLES: pagina_controles(); break;
    case AJP_IDIOMA: pagina_idioma(); break;
    case AJP_HERRAMIENTAS: pagina_herramientas(); break;
    case AJP_DEV: pagina_dev(); break;
    default: pagina_creditos(); break;
    }
    g_alto = g_y + S(24);
}

/* ------------------------------------------------------------------ */
/* cambios de ajustes                                                  */
/* ------------------------------------------------------------------ */
static void preset_desde_calidad(void)
{
    const char *e = V("Graphics", "escala_interna"), *a = V("Graphics", "antialias"), *an = V("Graphics", "anisotropico"), *m = V("Graphics", "mipmaps");
    const char *p = V("Graphics", "preset"), *nuevo;
    if (!strcmp(e, "1") && !strcmp(a, "no") && !strcmp(an, "1") && !strcmp(m, "si")) nuevo = !strcmp(p, "rendimiento") ? "rendimiento" : "original";
    else if (!strcmp(e, "2") && !strcmp(a, "msaa4") && !strcmp(an, "16") && !strcmp(m, "si")) nuevo = "calidad";
    else nuevo = "personalizado";
    poner_val(AJ("Graphics", "preset"), nuevo);
}
static void al_cambiar(int i)
{
    const AjDef *d = &aj_defs[i];
    if (!strcmp(d->clave, "interfaz")) {
        int antes = g_lang; lang_de_conf();
        if (g_lang != antes) { g_todo_pendiente = 1; pedir_rehacer(); }
    } else if (!strcmp(d->clave, "preset")) {
        const char *p = g_conf.val[i];
        if (!strcmp(p, "original") || !strcmp(p, "rendimiento")) {
            poner_val(AJ("Graphics", "escala_interna"), "1"); poner_val(AJ("Graphics", "antialias"), "no");
            poner_val(AJ("Graphics", "anisotropico"), "1"); poner_val(AJ("Graphics", "mipmaps"), "si");
        } else if (!strcmp(p, "calidad")) {
            poner_val(AJ("Graphics", "escala_interna"), "2"); poner_val(AJ("Graphics", "antialias"), "msaa4");
            poner_val(AJ("Graphics", "anisotropico"), "16"); poner_val(AJ("Graphics", "mipmaps"), "si");
        }
        pedir_rehacer();
    } else if (f_calidad(d)) {
        preset_desde_calidad();
        pedir_rehacer();
    }
    if (!strcmp(d->clave, "activo") || d->tipo == AJ_TECLA || !strcmp(d->clave, "edicion")) pedir_rehacer();
    if (!strcmp(d->clave, "edicion") || !strcmp(d->seccion, "Paths") || !strcmp(d->clave, "variante")) g_est.hecho = 0;
    actualizar_sucio();
}
static void valor_no_vale(int i, const wchar_t *txt)
{
    const AjDef *d = &aj_defs[i]; wchar_t por[128], s[1200];
    if (d->tipo == AJ_NUM) { wchar_t lo[32], hi[32]; rango_textos(d, lo, hi); fmt(por, 128, T_RANGO, lo, hi); }
    else wcsncpy_s(por, 128, T(T_TEXTO_MAL), _TRUNCATE);
    fmt(s, 1200, T_VALOR_NO_VALE, txt, por);
    poner_estado(C_AVISO, L"%s", s);
}
static int poner_texto(int i, const wchar_t *txt)
{
    char u[AJ_VMAX * 3], out[AJ_VMAX];
    wu8(txt, u, sizeof u);
    if (strlen(u) < AJ_VMAX - 1 && aj_validar(i, u, out, sizeof out)) {
        if (strcmp(g_conf.val[i], out)) { poner_val(i, out); al_cambiar(i); }
        return 1;
    }
    valor_no_vale(i, txt);
    return 0;
}

/* ------------------------------------------------------------------ */
/* diálogos de Windows                                                 */
/* ------------------------------------------------------------------ */
static int dialogo(int guardar_, int carpeta, int filtro, const wchar_t *ext, const wchar_t *nombre, wchar_t *out, size_t n)
{
    IFileDialog *fd = NULL; IShellItem *it = NULL; PWSTR p = NULL; HRESULT hr; DWORD op = 0; int ok = 0;
    COMDLG_FILTERSPEC fs; wchar_t pat[16];
    hr = CoCreateInstance(guardar_ ? &CLSID_FileSaveDialog : &CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, &IID_IFileDialog, (void **)&fd);
    if (FAILED(hr) || !fd) return 0;
    IFileDialog_GetOptions(fd, &op);
    op |= FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR;
    if (carpeta) op |= FOS_PICKFOLDERS | FOS_PATHMUSTEXIST;
    else if (guardar_) op |= FOS_OVERWRITEPROMPT;
    else op |= FOS_FILEMUSTEXIST;
    IFileDialog_SetOptions(fd, op);
    if (!carpeta && ext) {
        _snwprintf_s(pat, 16, _TRUNCATE, L"*.%s", ext);
        fs.pszName = T(filtro); fs.pszSpec = pat;
        IFileDialog_SetFileTypes(fd, 1, &fs);
        IFileDialog_SetDefaultExtension(fd, ext);
    }
    if (nombre) IFileDialog_SetFileName(fd, nombre);
    if (SUCCEEDED(IFileDialog_Show(fd, g_main)) && SUCCEEDED(IFileDialog_GetResult(fd, &it)) && it) {
        if (SUCCEEDED(IShellItem_GetDisplayName(it, SIGDN_FILESYSPATH, &p)) && p) {
            wcsncpy_s(out, n, p, _TRUNCATE); ok = 1; CoTaskMemFree(p);
        }
        IShellItem_Release(it);
    }
    IFileDialog_Release(fd);
    return ok;
}
static void abrir(const wchar_t *ruta)
{
    wchar_t s[MAX_PATH + 64], c[MAX_PATH];
    ruta_corta(ruta, c, MAX_PATH);
    if ((INT_PTR)ShellExecuteW(g_main, L"open", ruta, NULL, NULL, SW_SHOWNORMAL) <= 32) { fmt(s, MAX_PATH + 64, T_NO_ENCUENTRO, c); poner_estado(C_ERROR, L"%s", s); }
    else { fmt(s, MAX_PATH + 64, T_ABIERTO_OK, c); poner_estado(C_GRIS, L"%s", s); }
}

/* ------------------------------------------------------------------ */
/* tareas en segundo plano (una a la vez)                              */
/* ------------------------------------------------------------------ */
enum { TA_NADA, TA_IMPORTAR, TA_VERIFICAR, TA_SONIDO, TA_ENTORNO };
static struct {
    int tipo; HANDLE hilo; volatile LONG cancelar; CRITICAL_SECTION cs; int cs_ok;
    int hecho, total; wchar_t que[MAX_PATH]; DWORD ultimo;
    Importacion im; int im_reintento;
    wchar_t dir[MAX_PATH]; Verif v; int vr;
    wchar_t sonido_id[300], sonido_nombre[160]; int volumen, fallback; HRESULT hr;
    wchar_t lanz[MAX_PATH], tmpini[MAX_PATH]; Buf salida; DWORD codigo;
} g_t;

static int tarea_en_marcha(void) { return g_t.tipo != TA_NADA; }

static int progreso(void *ctx, int hecho, int total, const wchar_t *que)
{
    DWORD t = GetTickCount();
    (void)ctx;
    if (g_t.cancelar) return 1;
    if (hecho != 0 && hecho != total && t - g_t.ultimo < 100) return 0;
    g_t.ultimo = t;
    EnterCriticalSection(&g_t.cs);
    g_t.hecho = hecho; g_t.total = total;
    wcsncpy_s(g_t.que, MAX_PATH, que ? que : L"", _TRUNCATE);
    LeaveCriticalSection(&g_t.cs);
    PostMessageW(g_main, WM_APP_PROGRESO, 0, 0);
    return 0;
}

/* "Jugar Obi-Wan 6.exe --en-seco --ini <tmp>": salida por tubería, sin ventana. */
static void ejecutar_seco(void)
{
    SECURITY_ATTRIBUTES sa; HANDLE rd = NULL, wr = NULL; STARTUPINFOW si; PROCESS_INFORMATION pi; wchar_t cmd[3 * MAX_PATH];
    DWORD t0 = GetTickCount(), av, n; char b[4096];
    g_t.codigo = (DWORD)-1;
    sa.nLength = sizeof sa; sa.lpSecurityDescriptor = NULL; sa.bInheritHandle = TRUE;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    memset(&si, 0, sizeof si); si.cb = sizeof si; si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = wr; si.hStdError = wr; si.hStdInput = NULL;
    _snwprintf_s(cmd, 3 * MAX_PATH, _TRUNCATE, L"\"%s\" --en-seco --ini \"%s\"", g_t.lanz, g_t.tmpini);
    if (!CreateProcessW(g_t.lanz, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, g_base, &si, &pi)) {
        g_t.codigo = GetLastError(); CloseHandle(rd); CloseHandle(wr); return;
    }
    CloseHandle(wr);
    for (;;) {
        if (PeekNamedPipe(rd, NULL, 0, NULL, &av, NULL) && av) {
            if (!ReadFile(rd, b, sizeof b, &n, NULL) || !n) break;
            if (g_t.salida.n < (1u << 20)) buf_add(&g_t.salida, b, n);
            continue;
        }
        if (WaitForSingleObject(pi.hProcess, 20) == WAIT_OBJECT_0) {
            while (PeekNamedPipe(rd, NULL, 0, NULL, &av, NULL) && av && ReadFile(rd, b, sizeof b, &n, NULL) && n) buf_add(&g_t.salida, b, n);
            break;
        }
        if (GetTickCount() - t0 > 15000) { TerminateProcess(pi.hProcess, 1); break; }
    }
    GetExitCodeProcess(pi.hProcess, &g_t.codigo);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(rd);
}

static DWORD WINAPI hilo_tarea(LPVOID p)
{
    (void)p;
    switch (g_t.tipo) {
    case TA_IMPORTAR: importar_iso(&g_t.im, progreso, NULL, &g_t.cancelar); break;
    case TA_VERIFICAR: memset(&g_t.v, 0, sizeof g_t.v); g_t.vr = verificar(g_t.dir, 0, &g_t.v, progreso, NULL); break;
    case TA_SONIDO:
        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        g_t.fallback = g_t.sonido_id[0] && FAILED(xa2_probar_id(g_t.sonido_id));
        g_t.hr = tocar_tono(g_t.sonido_id, g_t.volumen);
        CoUninitialize();
        break;
    case TA_ENTORNO: ejecutar_seco(); break;
    default: break;
    }
    PostMessageW(g_main, WM_APP_FIN, 0, 0);
    return 0;
}
static int empezar_tarea(int tipo)
{
    if (g_t.tipo) { poner_estado(C_AVISO, L"%s", T(T_OCUPADO)); return 0; }
    g_t.tipo = tipo; g_t.cancelar = 0; g_t.ultimo = 0; g_t.hecho = g_t.total = 0; g_t.que[0] = 0;
    g_t.hilo = CreateThread(NULL, 0, hilo_tarea, NULL, 0, NULL);
    if (!g_t.hilo) { g_t.tipo = 0; poner_estado(C_ERROR, L"CreateThread: %lu", GetLastError()); return 0; }
    return 1;
}
static void cancelar_tarea_y_esperar(void)
{
    if (!g_t.tipo) return;
    InterlockedExchange(&g_t.cancelar, 1);
    if (g_t.hilo) { WaitForSingleObject(g_t.hilo, 60000); CloseHandle(g_t.hilo); g_t.hilo = NULL; }
    if (g_t.tipo == TA_ENTORNO && g_t.tmpini[0]) DeleteFileW(g_t.tmpini);
    free(g_t.salida.p); memset(&g_t.salida, 0, sizeof g_t.salida);
    free(g_t.v.detalle.p); memset(&g_t.v, 0, sizeof g_t.v);
    g_t.tipo = 0;
}

static void texto_error_imp(const Importacion *im, wchar_t *o, size_t n)
{
    wchar_t c[MAX_PATH], d[MAX_PATH];
    ruta_corta(im->donde, c, MAX_PATH);
    switch (im->error) {
    case T_E_YA_IMPORTADA: case T_E_DESTINO: case T_E_TEMPORAL: fmt(o, n, im->error, c); break;
    case T_E_ESPACIO: { double a = 0, b = 0; swscanf_s(im->detalle, L"%lf|%lf", &a, &b); fmt(o, n, T_E_ESPACIO, a, b); } break;
    case T_E_ESCRITURA: ruta_corta(im->detalle, d, MAX_PATH); fmt(o, n, T_E_ESCRITURA, d, im->winerr); break;
    case T_E_ESTRUCTURA: fmt(o, n, T_E_ESTRUCTURA, im->detalle); break;
    default: wcsncpy_s(o, n, T(im->error), _TRUNCATE);
    }
}

static void escribir_informe_verif(const Verif *v, wchar_t *ruta, size_t n)
{
    wchar_t dir[MAX_PATH], f[32];
    _snwprintf_s(dir, MAX_PATH, _TRUNCATE, L"%s\\logs", g_local);
    crear_dirs(dir);
    fecha_ahora(f, 32);
    _snwprintf_s(ruta, n, _TRUNCATE, L"%s\\verificacion-%s.txt", dir, f);
    if (!escribir_todo(ruta, v->detalle.p ? v->detalle.p : "", v->detalle.n)) ruta[0] = 0;
}

static void tarea_terminada(WPARAM w, LPARAM l)
{
    wchar_t s[1400], c[MAX_PATH];
    (void)w;
    if (!g_t.tipo) return;
    if (!(l & 0x10000)) {   /* progreso */
        int h, t; wchar_t q[MAX_PATH];
        EnterCriticalSection(&g_t.cs); h = g_t.hecho; t = g_t.total; wcsncpy_s(q, MAX_PATH, g_t.que, _TRUNCATE); LeaveCriticalSection(&g_t.cs);
        if (g_t.tipo == TA_IMPORTAR) {
            if (q[0]) fmt(s, 1400, T_IMP_COPIANDO, h + 1, t, q);
            else if (h == t && t > 1) wcsncpy_s(s, 1400, T(T_IMP_VERIFICANDO), _TRUNCATE);
            else wcsncpy_s(s, 1400, T(T_IMP_LEYENDO), _TRUNCATE);
        } else fmt(s, 1400, T_VERIF_PROGRESO, h, t);
        poner_estado(C_GRIS, L"%s", s);
        return;
    }
    if (g_t.hilo) { WaitForSingleObject(g_t.hilo, INFINITE); CloseHandle(g_t.hilo); g_t.hilo = NULL; }
    switch (g_t.tipo) {
    case TA_IMPORTAR: {
        Importacion *im = &g_t.im;
        if (im->error == T_E_ESCRITURA && im->winerr == ERROR_ACCESS_DENIED && !g_t.im_reintento && im->ed) {
            wchar_t wid[32]; u8w(im->ed->id, wid, 32);   /* Acceso controlado a carpetas: a LOCALAPPDATA */
            _snwprintf_s(im->destino, MAX_PATH, _TRUNCATE, L"%s\\game\\%s", g_local, wid);
            g_t.im_reintento = 1; g_t.tipo = 0;
            empezar_tarea(TA_IMPORTAR);
            return;
        }
        if (!im->error) {
            if (g_t.im_reintento) { char u[MAX_PATH * 3]; wu8(im->destino, u, sizeof u); poner_val(AJ("Paths", "carpeta_juego"), u); }
            if (im->ed->build) poner_val(AJ("Language", "edicion"), im->ed->id);
            ruta_corta(im->donde, c, MAX_PATH);
            fmt(s, 1400, T_IMP_OK, W(ed_nombre(im->ed)), im->narch, c);
            poner_estado(C_OK, L"%s", s);
            actualizar_sucio();
        } else {
            wchar_t e[1000]; texto_error_imp(im, e, 1000);
            fmt(s, 1400, T_IMP_ERR, e);
            poner_estado(im->error == T_CANCELADO ? C_GRIS : C_ERROR, L"%s", s);
        }
        g_est.hecho = 0;
        break; }
    case TA_VERIFICAR:
        if (g_t.v.cancelado) poner_estado(C_GRIS, L"%s", T(T_CANCELADO));
        else if (!g_t.vr || g_t.v.sin_json) poner_estado(C_AVISO, L"%s", T(T_VERIF_SIN));
        else if (!g_t.v.cambiados && !g_t.v.faltan && !g_t.v.sobran) { fmt(s, 1400, T_VERIF_OK, g_t.v.total); poner_estado(C_OK, L"%s", s); }
        else {
            wchar_t r[MAX_PATH]; escribir_informe_verif(&g_t.v, r, MAX_PATH); ruta_corta(r, c, MAX_PATH);
            fmt(s, 1400, T_VERIF_MAL, g_t.v.cambiados, g_t.v.faltan, g_t.v.sobran, c);
            poner_estado(C_AVISO, L"%s", s);
        }
        free(g_t.v.detalle.p); memset(&g_t.v, 0, sizeof g_t.v);
        g_est.hecho = 0;
        break;
    case TA_SONIDO:
        if (SUCCEEDED(g_t.hr)) { fmt(s, 1400, T_SONIDO_OK, g_t.fallback || !g_t.sonido_id[0] ? T(T_PREDETERMINADA) : g_t.sonido_nombre, g_t.volumen); poner_estado(C_OK, L"%s", s); }
        else { fmt(s, 1400, T_SONIDO_MAL, (unsigned long)g_t.hr); poner_estado(C_ERROR, L"%s", s); }
        break;
    case TA_ENTORNO: {
        int wn; wchar_t *wt, *o; int i, j;
        DeleteFileW(g_t.tmpini);
        sanear(&g_t.salida);
        buf_add(&g_t.salida, "", 1);
        wn = MultiByteToWideChar(CP_UTF8, 0, g_t.salida.p, -1, NULL, 0);
        wt = (wchar_t *)malloc(((size_t)(wn > 0 ? wn : 1)) * sizeof *wt);
        o = (wchar_t *)malloc(((size_t)(wn > 0 ? wn : 1) * 2 + 64) * sizeof *o);
        if (wt && o) {
            if (wn <= 0 || !MultiByteToWideChar(CP_UTF8, 0, g_t.salida.p, -1, wt, wn)) wt[0] = 0;
            for (i = 0, j = 0; wt[i]; i++) { if (wt[i] == L'\n' && (i == 0 || wt[i - 1] != L'\r')) o[j++] = L'\r'; o[j++] = wt[i]; }
            o[j] = 0;
            if (!o[0]) _snwprintf_s(o, 64, _TRUNCATE, L"(sin salida; código %lu)", g_t.codigo);
            free(g_entorno); g_entorno = o; o = NULL;
        }
        free(wt); free(o);
        free(g_t.salida.p); memset(&g_t.salida, 0, sizeof g_t.salida);
        poner_estado(g_t.codigo == 0 ? C_OK : C_AVISO, L"%s (%lu)", T(T_VER_ENTORNO), g_t.codigo);
        break; }
    default: break;
    }
    g_t.tipo = 0;
    pedir_rehacer();
}

/* ------------------------------------------------------------------ */
/* diagnóstico                                                         */
/* ------------------------------------------------------------------ */
static void diag_var(const char *var, const char *val, int i, void *ctx) { (void)i; buf_printf((Buf *)ctx, "  %s=%s\r\n", var, val); }

static int ft_desc(const void *a, const void *b)
{
    const WIN32_FIND_DATAW *x = (const WIN32_FIND_DATAW *)a, *y = (const WIN32_FIND_DATAW *)b;
    return -CompareFileTime(&x->ftLastWriteTime, &y->ftLastWriteTime);
}

/* Devuelve archivos en el zip, o -1 (err con el error de Windows). */
static int diag_zip(const wchar_t *ruta, DWORD *err)
{
    Buf t = {0}; Zip z; char u[MAX_PATH * 3]; wchar_t w[MAX_PATH * 2], f[32]; int i; Estado *e = &g_est;
    memset(&z, 0, sizeof z);
    calcular_estado();
    fecha_ahora(f, 32);
    wu8(f, u, sizeof u);
    buf_printf(&t, "Configurar Obi-Wan - diagnostico (%s)\r\n\r\n", u);
    {
        typedef LONG (WINAPI *PRtlGetVersion)(OSVERSIONINFOW *);
        OSVERSIONINFOW ov; PRtlGetVersion rv = (PRtlGetVersion)(void *)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
        memset(&ov, 0, sizeof ov); ov.dwOSVersionInfoSize = sizeof ov;
        if (rv && !rv(&ov)) buf_printf(&t, "Windows %lu.%lu build %lu\r\n", ov.dwMajorVersion, ov.dwMinorVersion, ov.dwBuildNumber);
    }
    wu8(g_base, u, sizeof u); buf_printf(&t, "carpeta del port: %s\r\n", u);
    wu8(g_ini, u, sizeof u); buf_printf(&t, "obiwan.ini: %s (estado %d, %d avisos)\r\n", u, g_guardada.estado, g_guardada.navisos);
    if (g_guardada.avisos[0]) buf_printf(&t, "avisos:\r\n%s\r\n", g_guardada.avisos);
    if (g_guardada.desconocidas[0]) buf_printf(&t, "claves desconocidas:\r\n%s\r\n", g_guardada.desconocidas);
    buf_printf(&t, "\r\n[estado]\r\n");
    wu8(e->lanzador, u, sizeof u); buf_printf(&t, "lanzador: %s (%s)\r\n", u, e->hay_lanzador ? "si" : "falta");
    wu8(e->exe, u, sizeof u); buf_printf(&t, "juego compilado: %s (%s, %.1f MB)\r\n", u, e->hay_exe ? "si" : "falta", e->exe_mb);
    buf_printf(&t, "edicion elegida: %s\r\n", ed_actual()->id);
    wu8(e->juego, u, sizeof u); buf_printf(&t, "carpeta del juego: %s (%s)\r\n", u, e->hay_juego ? "si" : "falta");
    buf_printf(&t, "default.xbe: %s sha256=%s edicion=%s\r\n",
               e->xbe == XBE_NO ? "no hay" : e->xbe == XBE_OK ? "ok" : e->xbe == XBE_OTRA ? "otra edicion" : e->xbe == XBE_PAL ? "PAL sin compilar" : "desconocida",
               e->hex[0] ? e->hex : "-", e->ed_xbe ? e->ed_xbe->id : "-");
    buf_printf(&t, "archivos (rapido): %d de %d bien%s\r\n", e->v_ok, e->v_total, e->v_sin ? " (sin EXTRACCION.json)" : "");
    wu8(e->partidas, u, sizeof u); buf_printf(&t, "partidas: %s (%d archivos)\r\n", u, e->npartidas);
    buf_printf(&t, "juego abierto: %s\r\n", e->abierto ? "si" : "no");
    buf_printf(&t, "ajustes distintos del de por defecto: %d\r\n", e->ndif);
    buf_printf(&t, "\r\n[variables que pondria el lanzador con obiwan.ini guardado]\r\n");
    {
        size_t antes = t.n;
        aj_entorno(&g_guardada, diag_var, &t);
        if (t.n == antes) buf_printf(&t, "  (ninguna: todo por defecto; el lanzador solo pone las suyas fijas)\r\n");
    }
    buf_printf(&t, "\r\n[ultima partida con el lanzador 6]\r\n");
    if (!e->u.hay) buf_printf(&t, "ninguna\r\n");
    else {
        wu8(e->u.nombre, u, sizeof u);
        buf_printf(&t, "log: %s\r\ncierre: %s codigo %lu\r\nfps: %.1f (%d medidas)\r\nframetime: %lu de >33 ms, %lu de >50 ms, de %lu (%d lineas)\r\nmodo: %s\r\n",
                   u, e->u.cierre ? "si" : "no", e->u.codigo, e->u.fps_n ? e->u.fps_suma / e->u.fps_n : 0.0, e->u.fps_n,
                   e->u.ft_33, e->u.ft_50, e->u.ft_frames, e->u.ft_n, e->u.modo[0] ? e->u.modo : "-");
    }
    {
        wchar_t m[600]; texto_monitores(m, 600); wu8(m, u, sizeof u);
        buf_printf(&t, "\r\n[monitores]\r\n%s\r\n", u);
    }
    llenar_salidas();
    buf_printf(&t, "\r\n[salidas de sonido]\r\n");
    for (i = 0; i < g_nsalidas; i++) { wu8(g_salidas[i].nombre, u, sizeof u); buf_printf(&t, "  %s\r\n", u); }
    sanear(&t);
    zip_add(&z, "diagnostico.txt", t.p, t.n);
    free(t.p);
    {
        size_t n; char *d = leer_todo(g_ini, &n, 1u << 20);
        if (d) { Buf b = {0}; buf_add(&b, d, n); free(d); sanear(&b); zip_add(&z, "obiwan.ini", b.p ? b.p : "", b.n); free(b.p); }
    }
    {
        static WIN32_FIND_DATAW fd[512]; int nf = 0; HANDLE h; wchar_t pat[MAX_PATH];
        _snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\logs\\*.log", g_local);
        h = FindFirstFileW(pat, &fd[0]);
        if (h != INVALID_HANDLE_VALUE) {
            do { if (!(fd[nf].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) nf++; } while (nf < 512 && FindNextFileW(h, &fd[nf]));
            FindClose(h);
        }
        qsort(fd, (size_t)nf, sizeof fd[0], ft_desc);
        for (i = 0; i < nf && i < 3; i++) {
            size_t n; char *d; Buf nom = {0}, b = {0};
            _snwprintf_s(w, MAX_PATH * 2, _TRUNCATE, L"%s\\logs\\%s", g_local, fd[i].cFileName);
            d = leer_todo(w, &n, 2u << 20);
            if (!d) continue;
            buf_add(&b, d, n); free(d); sanear(&b);
            wu8(fd[i].cFileName, u, sizeof u);
            buf_printf(&nom, "logs/%s", u); buf_add(&nom, "", 1); sanear(&nom);
            zip_add(&z, nom.p, b.p ? b.p : "", b.n);
            free(b.p); free(nom.p);
        }
    }
    i = z.n;
    if (!zip_escribir(&z, ruta)) { *err = GetLastError(); i = -1; }
    free(z.datos.p); free(z.central.p);
    return i;
}

/* ------------------------------------------------------------------ */
/* acciones                                                            */
/* ------------------------------------------------------------------ */
static int juego_cerrado_o_aviso(void)
{
    if (juego_abierto()) { poner_estado(C_AVISO, L"%s", T(T_CIERRA_JUEGO)); return 0; }
    return 1;
}

static void lanzar(int medir)
{
    wchar_t r[MAX_PATH], c[MAX_PATH], s[MAX_PATH + 64], args[MAX_PATH + 16];
    if (g_captura) return;   /* las capturas nunca juegan */
    if (juego_abierto()) { poner_estado(C_AVISO, L"%s", T(T_JUEGO_YA_ABIERTO)); return; }
    if (!guardar()) return;
    if (medir) ruta_medir(r, MAX_PATH); else ruta_lanzador(r, MAX_PATH);
    ruta_corta(r, c, MAX_PATH);
    if (!es_arch(r)) { fmt(s, MAX_PATH + 64, T_NO_ENCUENTRO, c); poner_estado(C_ERROR, L"%s", s); return; }
    _snwprintf_s(args, MAX_PATH + 16, _TRUNCATE, L"--ini \"%s\"", g_ini);
    if ((INT_PTR)ShellExecuteW(g_main, L"open", r, medir ? NULL : args, g_base, SW_SHOWNORMAL) <= 32) {
        fmt(s, MAX_PATH + 64, T_NO_ENCUENTRO, c); poner_estado(C_ERROR, L"%s", s); return;
    }
    fmt(s, MAX_PATH + 64, T_ABIERTO_OK, c); poner_estado(C_OK, L"%s", s);
    g_est.hecho = 0;
}

static void accion_copiar(void)
{
    wchar_t d[MAX_PATH], dst[MAX_PATH], f[32], wid[32], s[1200], c[MAX_PATH]; DWORD err = 0; int n;
    const AjEdicion *ed = g_est.ed_xbe ? g_est.ed_xbe : ed_actual();
    ruta_partidas(ed, g_est.partidas, MAX_PATH);
    if (!es_dir(g_est.partidas) || contar_arbol(g_est.partidas) == 0) { poner_estado(C_AVISO, L"%s", T(T_COPIA_NADA)); return; }
    dir_copias(d, MAX_PATH); fecha_ahora(f, 32); u8w(ed->id, wid, 32);
    _snwprintf_s(dst, MAX_PATH, _TRUNCATE, L"%s\\partidas-%s-%s", d, wid, f);
    n = copiar_arbol(g_est.partidas, dst, &err);
    if (n < 0) { fmt(s, 1200, T_COPIA_ERR, err); poner_estado(C_ERROR, L"%s", s); return; }
    ruta_corta(dst, c, MAX_PATH);
    fmt(s, 1200, T_COPIA_OK, c, n); poner_estado(C_OK, L"%s", s);
    g_copia_sel = 0;
    pedir_rehacer();
}

static void accion_restaurar(void)
{
    wchar_t nom[MAX_PATH], src[MAX_PATH], d[MAX_PATH], antes[MAX_PATH], f[32], s[1200], c[MAX_PATH]; DWORD err = 0; int n;
    const AjEdicion *ed = g_est.ed_xbe ? g_est.ed_xbe : ed_actual();
    if (!g_h_copias || SendMessageW(g_h_copias, CB_GETLBTEXT, (WPARAM)SendMessageW(g_h_copias, CB_GETCURSEL, 0, 0), (LPARAM)nom) <= 0) return;
    if (!juego_cerrado_o_aviso()) return;
    fmt(s, 1200, T_PREG_RESTAURAR, nom);
    if (MessageBoxW(g_main, s, T(T_TITULO), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    dir_copias(d, MAX_PATH);
    _snwprintf_s(src, MAX_PATH, _TRUNCATE, L"%s\\%s", d, nom);
    ruta_partidas(ed, g_est.partidas, MAX_PATH);
    antes[0] = 0;
    if (es_dir(g_est.partidas) && !dir_vacio(g_est.partidas)) {   /* lo que hay se aparta, no se borra */
        fecha_ahora(f, 32);
        _snwprintf_s(antes, MAX_PATH, _TRUNCATE, L"%s-antes-%s", g_est.partidas, f);
        if (!MoveFileExW(g_est.partidas, antes, 0)) { fmt(s, 1200, T_COPIA_ERR, GetLastError()); poner_estado(C_ERROR, L"%s", s); return; }
    }
    n = copiar_arbol(src, g_est.partidas, &err);
    if (n < 0) { fmt(s, 1200, T_COPIA_ERR, err); poner_estado(C_ERROR, L"%s", s); return; }
    if (antes[0]) ruta_corta(antes, c, MAX_PATH); else wcsncpy_s(c, MAX_PATH, L"-", _TRUNCATE);
    fmt(s, 1200, T_RESTAURADO, nom, c); poner_estado(C_OK, L"%s", s);
    g_est.hecho = 0;
}

static void accion_cache(void)
{
    wchar_t exe[MAX_PATH], *sl, dbl[MAX_PATH + 2], s[200]; SHFILEOPSTRUCTW op; int r;
    if (!juego_cerrado_o_aviso()) return;
    ruta_exe(exe, MAX_PATH);
    sl = wcsrchr(exe, L'\\'); if (sl) *sl = 0;
    memset(dbl, 0, sizeof dbl);
    _snwprintf_s(dbl, MAX_PATH, _TRUNCATE, L"%s\\shadercache", exe);
    if (!es_dir(dbl)) { poner_estado(C_GRIS, L"%s", T(T_CACHE_NO)); return; }
    memset(&op, 0, sizeof op);
    op.hwnd = g_main; op.wFunc = FO_DELETE; op.pFrom = dbl;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;   /* a la papelera */
    r = SHFileOperationW(&op);
    if (r || op.fAnyOperationsAborted) { fmt(s, 200, T_CACHE_ERR, r); poner_estado(C_ERROR, L"%s", s); }
    else poner_estado(C_OK, L"%s", T(T_CACHE_OK));
}

static void accion_guardar_perfil(void)
{
    wchar_t r[MAX_PATH], dir[MAX_PATH], s[1200], c[MAX_PATH]; char u[256], out[AJ_VMAX]; DWORD e;
    if (g_h_perfil) GetWindowTextW(g_h_perfil, g_perfil_nombre, 64);
    wu8(g_perfil_nombre, u, sizeof u);
    if (!aj_validar(AJ("Developer", "variante"), u, out, sizeof out)) { poner_estado(C_AVISO, L"%s", T(T_PERFIL_MAL)); return; }
    _snwprintf_s(dir, MAX_PATH, _TRUNCATE, L"%s\\perfiles", g_base);
    crear_dirs(dir);
    _snwprintf_s(r, MAX_PATH, _TRUNCATE, L"%s\\%s.ini", dir, g_perfil_nombre);
    e = aj_guardar(r, &g_conf);
    if (e) {
        _snwprintf_s(dir, MAX_PATH, _TRUNCATE, L"%s\\perfiles", g_local);
        crear_dirs(dir);
        _snwprintf_s(r, MAX_PATH, _TRUNCATE, L"%s\\%s.ini", dir, g_perfil_nombre);
        e = aj_guardar(r, &g_conf);
    }
    if (e) { fmt(s, 1200, T_NO_GUARDADO, e); poner_estado(C_ERROR, L"%s", s); return; }
    ruta_corta(r, c, MAX_PATH);
    fmt(s, 1200, T_PERFIL_OK, c); poner_estado(C_OK, L"%s", s);
    pedir_rehacer();
}

static void accion_cargar_perfil(void)
{
    wchar_t nom[MAX_PATH], rel[MAX_PATH], r[MAX_PATH], s[1200]; static AjConf tmp; int ii = AJ("Language", "interfaz");
    char interfaz[AJ_VMAX];
    if (!g_h_perfiles || SendMessageW(g_h_perfiles, CB_GETLBTEXT, (WPARAM)SendMessageW(g_h_perfiles, CB_GETCURSEL, 0, 0), (LPARAM)nom) <= 0) return;
    _snwprintf_s(rel, MAX_PATH, _TRUNCATE, L"perfiles\\%s.ini", nom);
    if (!aj_ruta_ini(g_base, rel, r, MAX_PATH)) return;
    aj_cargar(r, &tmp);
    limpiar_conf(&tmp);
    strcpy_s(interfaz, AJ_VMAX, g_conf.val[ii]);
    memcpy(g_conf.val, tmp.val, sizeof g_conf.val);
    poner_val(ii, interfaz);
    fmt(s, 1200, T_PERFIL_CARGADO, nom);
    poner_estado(tmp.navisos ? C_AVISO : C_OK, L"%s", s);
    g_est.hecho = 0;
    actualizar_sucio();
    pedir_rehacer();
}

static void accion_restablecer(void)
{
    wchar_t bak[MAX_PATH], f[32], s[1200], c[MAX_PATH]; int ii = AJ("Language", "interfaz"); char interfaz[AJ_VMAX];
    if (MessageBoxW(g_main, T(T_PREG_RESTABLECER), T(T_TITULO), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    c[0] = L'-'; c[1] = 0;
    if (es_arch(g_ini)) {
        fecha_ahora(f, 32);
        _snwprintf_s(bak, MAX_PATH, _TRUNCATE, L"%s.bak-%s", g_ini, f);
        if (!CopyFileW(g_ini, bak, TRUE)) { fmt(s, 1200, T_NO_GUARDADO, GetLastError()); poner_estado(C_ERROR, L"%s", s); return; }
        ruta_corta(bak, c, MAX_PATH);
    }
    strcpy_s(interfaz, AJ_VMAX, g_conf.val[ii]);
    aj_por_defecto(&g_conf);
    limpiar_conf(&g_conf);
    poner_val(ii, interfaz);
    if (!guardar()) return;
    fmt(s, 1200, T_RESTABLECIDO, c);
    poner_estado(C_OK, L"%s", s);
    g_est.hecho = 0;
    pedir_rehacer();
}

static void accion_sonido(void)
{
    int k; char u[900]; wchar_t xid[300];
    const char *v = V("Audio", "dispositivo");
    if (g_t.tipo) { poner_estado(C_AVISO, L"%s", T(T_OCUPADO)); return; }
    u8w(v, g_t.sonido_id, 300);
    g_t.sonido_nombre[0] = 0;
    llenar_salidas();
    for (k = 0; k < g_nsalidas; k++) {
        id_xaudio2(g_salidas[k].id, xid, 300); wu8(xid, u, sizeof u);
        if (!_stricmp(u, v)) wcsncpy_s(g_t.sonido_nombre, 160, g_salidas[k].nombre, _TRUNCATE);
    }
    if (!g_t.sonido_nombre[0]) wcsncpy_s(g_t.sonido_nombre, 160, g_t.sonido_id, _TRUNCATE);
    g_t.volumen = atoi(V("Audio", "volumen"));
    poner_estado(C_GRIS, L"%s", T(T_SONIDO_SONANDO));
    empezar_tarea(TA_SONIDO);
}

static void accion_entorno(void)
{
    wchar_t tmp[MAX_PATH]; DWORD e;
    if (g_t.tipo) { poner_estado(C_AVISO, L"%s", T(T_OCUPADO)); return; }
    ruta_lanzador(g_t.lanz, MAX_PATH);
    if (!es_arch(g_t.lanz)) { wchar_t s[MAX_PATH + 64]; fmt(s, MAX_PATH + 64, T_NO_ENCUENTRO, L"Jugar Obi-Wan 6.exe"); poner_estado(C_ERROR, L"%s", s); return; }
    if (!GetTempPathW(MAX_PATH, tmp)) return;
    _snwprintf_s(g_t.tmpini, MAX_PATH, _TRUNCATE, L"%sobiwan-en-seco-%lu.ini", tmp, GetCurrentProcessId());
    e = aj_guardar(g_t.tmpini, &g_conf);
    if (e) { wchar_t s[200]; fmt(s, 200, T_NO_GUARDADO, e); poner_estado(C_ERROR, L"%s", s); return; }
    memset(&g_t.salida, 0, sizeof g_t.salida);
    poner_estado(C_GRIS, L"%s…", T(T_VER_ENTORNO));
    if (!empezar_tarea(TA_ENTORNO)) DeleteFileW(g_t.tmpini);
}

static void accion(int sub, int aj)
{
    wchar_t r[MAX_PATH], s[1200], c[MAX_PATH];
    switch (sub) {
    case A_ELEGIR:
        if (aj >= 0 && dialogo(0, 1, 0, NULL, NULL, r, MAX_PATH)) {
            int k;
            if (poner_texto(aj, r)) for (k = 0; k < g_nctl; k++) if (g_ctl[k].kind == K_AJ_EDIT && g_ctl[k].aj == aj) {
                g_construyendo = 1; SetWindowTextW(g_ctl[k].h, r); g_construyendo = 0;
            }
        }
        break;
    case A_JUGAR: lanzar(0); break;
    case A_JUGAR_MEDIR: case A_MEDIR: lanzar(1); break;
    case A_COMPROBAR: g_est.hecho = 0; pedir_rehacer(); break;
    case A_ABRIR_LOG: if (g_est.u.hay) abrir(g_est.u.ruta); break;
    case A_ABRIR_LOGS: _snwprintf_s(r, MAX_PATH, _TRUNCATE, L"%s\\logs", g_local); crear_dirs(r); abrir(r); break;
    case A_PROBAR_SONIDO: accion_sonido(); break;
    case A_PROBAR_MANDO:
        if (g_mando_activo) { KillTimer(g_pagina, TIMER_MANDO); g_mando_activo = 0; }
        else if (juego_abierto()) poner_estado(C_AVISO, L"%s", T(T_MANDO_JUEGO));
        else { g_mando_activo = 1; g_mando_ticks = 0; SetTimer(g_pagina, TIMER_MANDO, 50, NULL); }
        pedir_rehacer();
        break;
    case A_IMPORTAR:
        if (g_captura || !juego_cerrado_o_aviso()) break;
        if (g_t.tipo) { poner_estado(C_AVISO, L"%s", T(T_OCUPADO)); break; }
        if (!dialogo(0, 0, T_FILTRO_ISO, L"iso", NULL, r, MAX_PATH)) break;
        memset(&g_t.im, 0, sizeof g_t.im);
        wcsncpy_s(g_t.im.iso, MAX_PATH, r, _TRUNCATE);
        g_t.im_reintento = 0;
        poner_estado(C_GRIS, L"%s", T(T_IMP_LEYENDO));
        empezar_tarea(TA_IMPORTAR);
        break;
    case A_ABRIR_PARTIDAS: {
        const AjEdicion *ed = g_est.ed_xbe ? g_est.ed_xbe : ed_actual();
        ruta_partidas(ed, r, MAX_PATH); crear_dirs(r); abrir(r); break; }
    case A_ABRIR_JUEGO: if (carpeta_del_juego(r, MAX_PATH)) abrir(r); else poner_estado(C_ERROR, L"%s", T(T_SIN_CARPETA)); break;
    case A_COPIAR: accion_copiar(); break;
    case A_RESTAURAR: accion_restaurar(); break;
    case A_DIAG: {
        wchar_t f[32], nom[64]; DWORD err = 0; int n;
        fecha_ahora(f, 32);
        _snwprintf_s(nom, 64, _TRUNCATE, L"diagnostico-obiwan-%s.zip", f);
        if (!dialogo(1, 0, T_FILTRO_ZIP, L"zip", nom, r, MAX_PATH)) break;
        n = diag_zip(r, &err);
        ruta_corta(r, c, MAX_PATH);
        if (n < 0) { fmt(s, 1200, T_DIAG_ERR, err); poner_estado(C_ERROR, L"%s", s); }
        else { fmt(s, 1200, T_DIAG_OK, c, n); poner_estado(C_OK, L"%s", s); }
        pedir_rehacer();
        break; }
    case A_VERIFICAR:
        if (g_t.tipo) { poner_estado(C_AVISO, L"%s", T(T_OCUPADO)); break; }
        if (!carpeta_del_juego(g_t.dir, MAX_PATH)) { poner_estado(C_ERROR, L"%s", T(T_SIN_CARPETA)); break; }
        fmt(s, 1200, T_VERIF_PROGRESO, 0, 0); poner_estado(C_GRIS, L"%s", s);
        empezar_tarea(TA_VERIFICAR);
        break;
    case A_CACHE: accion_cache(); break;
    case A_GUARDAR_PERFIL: accion_guardar_perfil(); break;
    case A_CARGAR_PERFIL: accion_cargar_perfil(); break;
    case A_RESTABLECER: accion_restablecer(); break;
    case A_VER_ENTORNO: accion_entorno(); break;
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/* mensajes de los controles de la página                              */
/* ------------------------------------------------------------------ */
static void comando_pagina(HWND h, int cod)
{
    Ctl *c = ctl_de(h); int i, clic = cod == BN_CLICKED || cod == BN_DOUBLECLICKED;
    if (!c) return;
    i = c->aj;
    switch (c->kind) {
    case K_AJ_COMBO: {
        const AjDef *d = &aj_defs[i]; int sel = (int)SendMessageW(h, CB_GETCURSEL, 0, 0);
        if (!strcmp(d->clave, "variante")) {
            wchar_t t[AJ_VMAX];
            if (cod == CBN_EDITCHANGE) GetWindowTextW(h, t, AJ_VMAX);
            else if (cod == CBN_SELCHANGE && sel >= 0) SendMessageW(h, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)t);
            else if (cod == CBN_KILLFOCUS) {
                char u[AJ_VMAX * 3], out[AJ_VMAX]; GetWindowTextW(h, t, AJ_VMAX); wu8(t, u, sizeof u);
                if (!aj_validar(i, u, out, sizeof out)) { g_construyendo = 1; SetWindowTextW(h, W(g_conf.val[i])); g_construyendo = 0; }
                break;
            } else break;
            poner_texto(i, t);
            break;
        }
        if (cod != CBN_SELCHANGE || sel < 0) break;
        if (d->tipo == AJ_ELEGIR) {
            char id[64];
            if (aj_opcion(d, sel, id, sizeof id, NULL, NULL) && strcmp(g_conf.val[i], id)) { poner_val(i, id); al_cambiar(i); }
        } else if (d->tipo == AJ_TECLA) {
            char v[16]; sprintf_s(v, sizeof v, "%d", (int)SendMessageW(h, CB_GETITEMDATA, (WPARAM)sel, 0));
            if (strcmp(g_conf.val[i], v)) { poner_val(i, v); al_cambiar(i); }
        } else if (!strcmp(d->clave, "dispositivo")) {
            int k = (int)SendMessageW(h, CB_GETITEMDATA, (WPARAM)sel, 0);
            if (k == -1) poner_val(i, "");
            else if (k >= 0 && k < g_nsalidas) { wchar_t xid[300]; char u[900]; id_xaudio2(g_salidas[k].id, xid, 300); wu8(xid, u, sizeof u); if (strlen(u) < AJ_VMAX - 1) poner_val(i, u); }
            al_cambiar(i);
        }
        break; }
    case K_AJ_EDIT:
        if (cod == EN_CHANGE) {
            wchar_t t[AJ_VMAX];
            GetWindowTextW(h, t, AJ_VMAX);
            if (poner_texto(i, t) && g_msg_color == C_AVISO) poner_estado(C_GRIS, L"%s", L"");   /* el aviso ya no vale; la línea de abajo dice si hay cambios */
        } else if (cod == EN_KILLFOCUS) {
            wchar_t t[AJ_VMAX]; char u[AJ_VMAX * 3], out[AJ_VMAX];
            GetWindowTextW(h, t, AJ_VMAX); wu8(t, u, sizeof u);
            if (!aj_validar(i, u, out, sizeof out)) { g_construyendo = 1; SetWindowTextW(h, W(g_conf.val[i])); g_construyendo = 0; }
        }
        break;
    case K_AJ_CHECK:
        if (!clic) break;
        if (!marcado(h)) {
            if (!strcmp(aj_defs[i].clave, "activo") && !g_captura &&
                MessageBoxW(g_main, T(T_DEV_AVISO), T(T_AVISO), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) break;
            poner_val(i, "si");
        } else poner_val(i, "no");
        marcar(h, !marcado(h));
        al_cambiar(i);
        break;
    case K_AJ_LISTA: {
        const char *p = aj_defs[i].valores; char out[AJ_VMAX] = ""; int t = 0, k;
        if (!clic) break;
        marcar(h, !marcado(h));
        while (*p) {
            const char *e = strchr(p, ','); size_t l = e ? (size_t)(e - p) : strlen(p);
            for (k = 0; k < g_nctl; k++)
                if (g_ctl[k].kind == K_AJ_LISTA && g_ctl[k].aj == i && g_ctl[k].sub == t && marcado(g_ctl[k].h)) {
                    if (out[0]) strcat_s(out, sizeof out, ",");
                    strncat_s(out, sizeof out, p, l);
                }
            t++; p += l; if (*p == ',') p++;
        }
        if (strcmp(g_conf.val[i], out)) { poner_val(i, out); al_cambiar(i); }
        break; }
    case K_EDICION:
        if (!clic) break;
        if (c->sub >= 0 && c->sub < AJ_NED && aj_ediciones[c->sub].build && strcmp(g_conf.val[i], aj_ediciones[c->sub].id)) {
            poner_val(i, aj_ediciones[c->sub].id); al_cambiar(i);
        }
        break;
    case K_ACCION:
        if (cod == BN_CLICKED) accion(c->sub, c->aj);
        break;
    case K_OTRO:
        if (c->sub == O_COPIAS && cod == CBN_SELCHANGE) g_copia_sel = (int)SendMessageW(h, CB_GETCURSEL, 0, 0);
        else if (c->sub == O_PERFILES && cod == CBN_SELCHANGE) g_perfil_sel = (int)SendMessageW(h, CB_GETCURSEL, 0, 0);
        else if (c->sub == O_PERFIL && cod == EN_CHANGE) GetWindowTextW(h, g_perfil_nombre, 64);
        else if (c->sub == O_LIC && cod == CBN_SELCHANGE) { g_lic_sel = (int)SendMessageW(h, CB_GETCURSEL, 0, 0); poner_licencia(); }
        break;
    default: break;
    }
}

static void pagina_saliendo(void)
{
    if (g_mando_activo) { KillTimer(g_pagina, TIMER_MANDO); g_mando_activo = 0; }
    g_est.hecho = 0;
}

/* Solo lee el mando: XInputGetState y, si no hay XInput, joyGetPosEx. Nunca vibra. */
static void temporizador_mando(void)
{
    wchar_t s[400], ant[400]; int k; XINPUT_STATE st;
    if (!g_mando_activo || !g_h_mando || !IsWindow(g_h_mando)) return;
    if (++g_mando_ticks % 20 == 0 && juego_abierto()) {
        KillTimer(g_pagina, TIMER_MANDO); g_mando_activo = 0;
        poner_estado(C_AVISO, L"%s", T(T_MANDO_JUEGO));
        pedir_rehacer();
        return;
    }
    wcsncpy_s(s, 400, T(T_MANDO_NINGUNO), _TRUNCATE);
    for (k = 0; k < 4; k++) {
        memset(&st, 0, sizeof st);
        if (XInputGetState((DWORD)k, &st) == ERROR_SUCCESS) {
            static const struct { WORD b; const wchar_t *n; } bs[] = {
                { XINPUT_GAMEPAD_A, L"A" }, { XINPUT_GAMEPAD_B, L"B" }, { XINPUT_GAMEPAD_X, L"X" }, { XINPUT_GAMEPAD_Y, L"Y" },
                { XINPUT_GAMEPAD_LEFT_SHOULDER, L"LB" }, { XINPUT_GAMEPAD_RIGHT_SHOULDER, L"RB" },
                { XINPUT_GAMEPAD_BACK, L"Back" }, { XINPUT_GAMEPAD_START, L"Start" },
                { XINPUT_GAMEPAD_LEFT_THUMB, L"LS" }, { XINPUT_GAMEPAD_RIGHT_THUMB, L"RS" },
                { XINPUT_GAMEPAD_DPAD_UP, L"↑" }, { XINPUT_GAMEPAD_DPAD_DOWN, L"↓" },
                { XINPUT_GAMEPAD_DPAD_LEFT, L"←" }, { XINPUT_GAMEPAD_DPAD_RIGHT, L"→" } };
            wchar_t b[128] = L""; int j;
            for (j = 0; j < (int)(sizeof bs / sizeof bs[0]); j++)
                if (st.Gamepad.wButtons & bs[j].b) { if (b[0]) wcscat_s(b, 128, L" "); wcscat_s(b, 128, bs[j].n); }
            if (!b[0]) wcscpy_s(b, 128, L"-");
            fmt(s, 400, T_MANDO_XINPUT, k + 1, st.Gamepad.sThumbLX, st.Gamepad.sThumbLY, st.Gamepad.sThumbRX, st.Gamepad.sThumbRY,
                st.Gamepad.bLeftTrigger, st.Gamepad.bRightTrigger, b);
            break;
        }
    }
    if (k == 4) {
        UINT j, nj = joyGetNumDevs(); JOYINFOEX ji;
        for (j = 0; j < nj && j < 16; j++) {
            memset(&ji, 0, sizeof ji); ji.dwSize = sizeof ji; ji.dwFlags = JOY_RETURNALL;
            if (joyGetPosEx(j, &ji) == JOYERR_NOERROR) {
                fmt(s, 400, T_MANDO_JOY, (int)j + 1, ji.dwXpos, ji.dwYpos, ji.dwZpos, ji.dwRpos, ji.dwButtons);
                break;
            }
        }
    }
    GetWindowTextW(g_h_mando, ant, 400);
    if (wcscmp(ant, s)) { SetWindowTextW(g_h_mando, s); InvalidateRect(g_h_mando, NULL, TRUE); }
}

/* ------------------------------------------------------------------ */
/* guardar                                                             */
/* ------------------------------------------------------------------ */
static int guardar(void)
{
    DWORD e; wchar_t s[1400], a[MAX_PATH], b[MAX_PATH], alt[MAX_PATH]; int color = C_OK;
    if (g_captura) return 1;   /* las capturas no escriben nada */
    e = aj_guardar(g_ini, &g_conf);
    if (e) {
        DWORD e2 = e;
        _snwprintf_s(alt, MAX_PATH, _TRUNCATE, L"%s\\obiwan.ini", g_local);
        if (_wcsicmp(alt, g_ini)) { crear_dirs(g_local); e2 = aj_guardar(alt, &g_conf); }
        if (e2) { fmt(s, 1400, T_NO_GUARDADO, e2); poner_estado(C_ERROR, L"%s", s); return 0; }
        ruta_corta(g_ini, a, MAX_PATH); ruta_corta(alt, b, MAX_PATH);
        fmt(s, 1400, T_GUARDADO_LOCAL, a, e, b);
        wcsncpy_s(g_ini, MAX_PATH, alt, _TRUNCATE);
        color = C_AVISO;
    } else {
        ruta_corta(g_ini, a, MAX_PATH);
        fmt(s, 1400, T_GUARDADO, a);
    }
    if (juego_abierto()) { wcscat_s(s, 1400, L" "); wcscat_s(s, 1400, T(T_JUEGO_ABIERTO_AVISO)); color = C_AVISO; }
    memcpy(g_guardada.val, g_conf.val, sizeof g_conf.val);
    g_guardada.navisos = 0; g_guardada.avisos[0] = 0;
    poner_estado(color, L"%s", s);
    actualizar_sucio();
    g_est.hecho = 0;
    if (g_pag == AJP_JUGAR) pedir_rehacer();
    return 1;
}

/* ------------------------------------------------------------------ */
/* capturas (--captura <carpeta>): nunca pulsa nada                    */
/* ------------------------------------------------------------------ */
static void bombear(int ms)
{
    MSG m; DWORD t0 = GetTickCount();
    do {
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        Sleep(10);
    } while (GetTickCount() - t0 < (DWORD)ms);
}
static int guardar_png(HBITMAP bmp, int w, int h, const wchar_t *ruta)
{
    IWICImagingFactory *f = NULL; IWICBitmap *b = NULL; IWICStream *s = NULL; IWICBitmapEncoder *e = NULL;
    IWICBitmapFrameEncode *fr = NULL; WICPixelFormatGUID pf = GUID_WICPixelFormat24bppBGR; int ok = 0, paso = 0; HRESULT hr;
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&f);
    if (FAILED(hr) || !f) { sal(L"WIC: fabrica hr=0x%08lX\r\n", (unsigned long)hr); return 0; }
#define WIC_PASO(x) (paso++, SUCCEEDED(hr = (x)))
    if (WIC_PASO(IWICImagingFactory_CreateBitmapFromHBITMAP(f, bmp, NULL, WICBitmapIgnoreAlpha, &b)) &&
        WIC_PASO(IWICImagingFactory_CreateStream(f, &s)) &&
        WIC_PASO(IWICStream_InitializeFromFilename(s, ruta, GENERIC_WRITE)) &&
        WIC_PASO(IWICImagingFactory_CreateEncoder(f, &GUID_ContainerFormatPng, NULL, &e)) &&
        WIC_PASO(IWICBitmapEncoder_Initialize(e, (IStream *)s, WICBitmapEncoderNoCache)) &&
        WIC_PASO(IWICBitmapEncoder_CreateNewFrame(e, &fr, NULL)) &&
        WIC_PASO(IWICBitmapFrameEncode_Initialize(fr, NULL)) &&
        WIC_PASO(IWICBitmapFrameEncode_SetSize(fr, (UINT)w, (UINT)h)) &&
        WIC_PASO(IWICBitmapFrameEncode_SetPixelFormat(fr, &pf)) &&
        WIC_PASO(IWICBitmapFrameEncode_WriteSource(fr, (IWICBitmapSource *)b, NULL)) &&
        WIC_PASO(IWICBitmapFrameEncode_Commit(fr)) &&
        WIC_PASO(IWICBitmapEncoder_Commit(e))) ok = 1;
    else sal(L"WIC: paso %d hr=0x%08lX (%dx%d)\r\n", paso, (unsigned long)hr, w, h);
#undef WIC_PASO
    if (fr) IWICBitmapFrameEncode_Release(fr);
    if (e) IWICBitmapEncoder_Release(e);
    if (s) IWICStream_Release(s);
    if (b) IWICBitmap_Release(b);
    IWICImagingFactory_Release(f);
    return ok;
}
static int capturar(const wchar_t *dir)
{
    static const wchar_t *const slug[AJP_N] = { L"jugar", L"pantalla", L"graficos", L"audio", L"controles", L"idioma",
                                                L"herramientas", L"desarrollador", L"creditos" };
    int p, n = 0, pct = MulDiv(g_dpi, 100, 96);
    if (!crear_dirs(dir)) return 0;
    for (p = 0; p <= AJP_N; p++) {
        int pg = p < AJP_N ? p : AJP_DEV, k;
        RECT wr, cr; HDC dc, mdc; HBITMAP bmp; HGDIOBJ old; int alto, w, h; wchar_t ruta[MAX_PATH];
        if (p == AJP_N) poner_val(AJ("Developer", "activo"), "si");   /* solo en memoria: guardar() no escribe en captura */
        g_pag = -1;
        ir_a_pagina(pg);
        bombear(150);
        for (k = 0; k < 2; k++) {   /* la ventana tan alta como la página */
            GetWindowRect(g_main, &wr); GetClientRect(g_main, &cr);
            alto = (wr.bottom - wr.top) - cr.bottom + hh() + g_alto + bh();
            if (alto < S(760)) alto = S(760);
            if (alto > S(3400)) alto = S(3400);
            SetWindowPos(g_main, NULL, 0, 0, S(1060), alto, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            bombear(150);
        }
        desplazar_a(0);
        SetFocus(g_nav);
        /* la ventana puede ser más alta que el monitor: se repinta todo (también la lista de la izquierda) */
        RedrawWindow(g_main, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
        bombear(100);
        GetClientRect(g_main, &cr); w = cr.right; h = cr.bottom;
        dc = GetDC(g_main); mdc = CreateCompatibleDC(dc); bmp = CreateCompatibleBitmap(dc, w, h); old = SelectObject(mdc, bmp);
        PrintWindow(g_main, mdc, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
        {   /* la lista de la izquierda se repinta encima: lo que queda fuera del monitor puede salir negro */
            RECT rn = { 0, hh(), nw(), h - bh() }; POINT o;
            FillRect(mdc, &rn, g_br[C_PANEL]);
            SetViewportOrgEx(mdc, 0, hh(), &o);
            SendMessageW(g_nav, WM_PRINTCLIENT, (WPARAM)mdc, PRF_CLIENT);
            SetViewportOrgEx(mdc, o.x, o.y, NULL);
        }
        SelectObject(mdc, old);
        _snwprintf_s(ruta, MAX_PATH, _TRUNCATE, L"%s\\%02d-%s%s-%d.png", dir, pg + 1, slug[pg], p == AJP_N ? L"-activo" : L"", pct);
        if (guardar_png(bmp, w, h, ruta)) { n++; sal(L"%s\r\n", ruta); }
        else sal(L"ERROR: %s\r\n", ruta);
        DeleteObject(bmp); DeleteDC(mdc); ReleaseDC(g_main, dc);
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* línea de órdenes                                                    */
/* ------------------------------------------------------------------ */
static int cli_importar(const wchar_t *iso, const wchar_t *destino, int identificar)
{
    static Importacion im; volatile LONG cancelar = 0; wchar_t s[1400], e[1000], c[MAX_PATH]; int ok;
    memset(&im, 0, sizeof im);
    wcsncpy_s(im.iso, MAX_PATH, iso, _TRUNCATE);
    if (destino) wcsncpy_s(im.destino, MAX_PATH, destino, _TRUNCATE);
    im.solo_identificar = identificar;
    ok = importar_iso(&im, NULL, NULL, &cancelar);
    if (identificar) {
        if (im.ed) sal(L"edicion: %s (%s)\r\nsha256: %s\r\narchivos: %d\r\n", W(im.ed->id), W(ed_nombre(im.ed)), W(im.hex), im.narch);
        else { texto_error_imp(&im, e, 1000); sal(L"%s\r\nsha256: %s\r\n", e, W(im.hex[0] ? im.hex : "-")); }
        return im.ed && ok ? 0 : 1;
    }
    if (ok) { ruta_corta(im.donde, c, MAX_PATH); fmt(s, 1400, T_IMP_OK, W(ed_nombre(im.ed)), im.narch, c); }
    else { texto_error_imp(&im, e, 1000); fmt(s, 1400, T_IMP_ERR, e); }
    sal(L"%s\r\n", s);
    return ok ? 0 : 1;
}
static int cli_verificar(const wchar_t *dir)
{
    Verif v; wchar_t s[1400]; int r, mal;
    memset(&v, 0, sizeof v);
    r = verificar(dir, 0, &v, NULL, NULL);
    mal = !r || v.sin_json || v.cambiados || v.faltan || v.sobran;
    if (!r || v.sin_json) wcsncpy_s(s, 1400, T(T_VERIF_SIN), _TRUNCATE);
    else if (!mal) fmt(s, 1400, T_VERIF_OK, v.total);
    else fmt(s, 1400, T_VERIF_MAL, v.cambiados, v.faltan, v.sobran, g_lang == 0 ? L"las líneas de abajo" : L"the lines below");
    sal(L"%s\r\n", s);
    if (v.detalle.p) { buf_add(&v.detalle, "", 1); sal(L"%s", W(v.detalle.p)); }
    free(v.detalle.p);
    return mal ? 1 : 0;
}
static int cli_dispositivos(void)
{
    int k; wchar_t xid[300];
    llenar_salidas();
    sal(L"predeterminada: xaudio2 hr=0x%08lX\r\n", (unsigned long)xa2_probar_id(NULL));
    for (k = 0; k < g_nsalidas; k++) {
        id_xaudio2(g_salidas[k].id, xid, 300);
        sal(L"%d. %s\r\n   mmdevice: %s\r\n   xaudio2:  %s\r\n   hr=0x%08lX\r\n", k + 1, g_salidas[k].nombre, g_salidas[k].id, xid,
            (unsigned long)xa2_probar_id(xid));
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE prev, PWSTR cmd, int show)
{
    int argc = 0, a, r = 0; wchar_t **argv; wchar_t *sl;
    const wchar_t *iso = NULL, *destino = NULL, *verif = NULL, *diag = NULL, *ini = NULL, *idioma = NULL, *captura = NULL;
    int identificar = 0, importar = 0, dispositivos = 0;
    INITCOMMONCONTROLSEX icc;
    (void)prev; (void)cmd;
    g_inst = hi;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    icc.dwSize = sizeof icc; icc.dwICC = ICC_STANDARD_CLASSES | ICC_UPDOWN_CLASS | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);
    InitializeCriticalSection(&g_t.cs);

    GetModuleFileNameW(NULL, g_base, MAX_PATH);
    sl = wcsrchr(g_base, L'\\'); if (sl) *sl = 0;
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", g_local_raiz, MAX_PATH)) wcsncpy_s(g_local_raiz, MAX_PATH, g_base, _TRUNCATE);
    _snwprintf_s(g_local, MAX_PATH, _TRUNCATE, L"%s\\ObiWanRecomp-dev", g_local_raiz);

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (a = 1; argv && a < argc; a++) {
        const wchar_t *o = argv[a], *v = a + 1 < argc ? argv[a + 1] : NULL;
        if (!_wcsicmp(o, L"--importar") && v) { iso = v; importar = 1; a++; }
        else if (!_wcsicmp(o, L"--identificar") && v) { iso = v; identificar = 1; a++; }
        else if (!_wcsicmp(o, L"--destino") && v) { destino = v; a++; }
        else if (!_wcsicmp(o, L"--verificar") && v) { verif = v; a++; }
        else if (!_wcsicmp(o, L"--diagnostico") && v) { diag = v; a++; }
        else if (!_wcsicmp(o, L"--dispositivos")) dispositivos = 1;
        else if (!_wcsicmp(o, L"--ini") && v) { ini = v; a++; }
        else if (!_wcsicmp(o, L"--idioma") && v) { idioma = v; a++; }
        else if (!_wcsicmp(o, L"--tema") && v) { g_tema_forzado = 1; g_oscuro = !_wcsicmp(v, L"oscuro"); a++; }
        else if (!_wcsicmp(o, L"--dpi") && v) { g_dpi = _wtoi(v); if (g_dpi < 72 || g_dpi > 480) g_dpi = 96; g_dpi_forzado = 1; a++; }
        else if (!_wcsicmp(o, L"--captura") && v) { captura = v; g_captura = 1; a++; }
    }

    if (ini) GetFullPathNameW(ini, MAX_PATH, g_ini, NULL);
    else aj_ruta_ini(g_base, L"obiwan.ini", g_ini, MAX_PATH);
    aj_cargar(g_ini, &g_conf);
    limpiar_conf(&g_conf);
    g_guardada = g_conf;
    lang_de_conf();
    if (idioma) {
        int k; for (k = 0; k < N_IDIOMAS; k++) { wchar_t w[8]; u8w(g_idiomas[k], w, 8); if (!_wcsicmp(w, idioma)) g_lang = k; }
    }

    if (importar || identificar || verif || diag || dispositivos) {
        consola();
        if (importar || identificar) r = cli_importar(iso, destino, identificar);
        else if (verif) r = cli_verificar(verif);
        else if (diag) {
            DWORD err = 0; wchar_t full[MAX_PATH], s[1400], c[MAX_PATH]; int n;
            GetFullPathNameW(diag, MAX_PATH, full, NULL);
            n = diag_zip(full, &err);
            ruta_corta(full, c, MAX_PATH);
            if (n < 0) fmt(s, 1400, T_DIAG_ERR, err); else fmt(s, 1400, T_DIAG_OK, c, n);
            sal(L"%s\r\n", s);
            r = n < 0;
        } else r = cli_dispositivos();
        if (argv) LocalFree(argv);
        CoUninitialize();
        return r;
    }

    if (!g_tema_forzado) g_oscuro = tema_del_sistema();
    if (!g_dpi_forzado) { g_dpi = (int)GetDpiForSystem(); if (g_dpi <= 0) g_dpi = 96; }
    modo_oscuro_app();
    colores();
    fuentes();
    if (g_conf.navisos) { wchar_t s[300]; fmt(s, 300, T_INI_AVISOS, g_conf.navisos); poner_estado(C_AVISO, L"%s", s); }
    else poner_estado(C_GRIS, L"%s", T(T_LEIDOS));

    if (captura) {
        wchar_t full[MAX_PATH]; int n;
        consola();
        GetFullPathNameW(captura, MAX_PATH, full, NULL);
        if (!crear_ventana(0, 0, SW_SHOWNOACTIVATE)) { if (argv) LocalFree(argv); return 2; }
        n = capturar(full);
        DestroyWindow(g_main);
        bombear(50);
        sal(L"%d capturas\r\n", n);
        r = n == AJP_N + 1 ? 0 : 1;
    } else {
        if (!crear_ventana(CW_USEDEFAULT, CW_USEDEFAULT, show)) { if (argv) LocalFree(argv); return 2; }
        ir_a_pagina(AJP_JUGAR);
        SetFocus(g_nav);
        r = bucle();
    }
    if (tarea_en_marcha()) cancelar_tarea_y_esperar();
    free(g_entorno);
    if (argv) LocalFree(argv);
    CoUninitialize();
    return r;
}
