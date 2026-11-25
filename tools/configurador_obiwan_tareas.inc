/* configurador_obiwan_tareas.inc -- lo que hace el configurador sin ventana
 * (Claude completo-4, 2026-11-06). Lo incluye tools/configurador_obiwan.c.
 *
 *  - rutas, lectura de archivos, procesos abiertos
 *  - EXTRACCION.json (leer y escribir) y verificación de la carpeta del juego
 *  - lector XDVDFS e importación de ISO (la ISO solo se lee)
 *  - último log del lanzador 6 (resultado, FPS, tirones)
 *  - copias de partidas, zip de diagnóstico sin datos personales
 *  - salidas de sonido (MMDevice) y tono de prueba (XAudio2)
 *
 * Usa g_base (carpeta del configurador) y g_local (%LOCALAPPDATA%\ObiWanRecomp-dev). */

/* Propio, para no depender de initguid/uuid.lib en C. */
static const GUID kCLSID_MMDeviceEnumerator = {0xBCDE0395,0xE52F,0x467C,{0x8E,0x3D,0xC4,0x57,0x92,0x91,0x69,0x2E}};
static const GUID kIID_IMMDeviceEnumerator  = {0xA95664D2,0x9614,0x4F35,{0xA7,0x46,0xDE,0x8D,0xB6,0x36,0x17,0xE6}};
static const PROPERTYKEY kPKEY_FriendlyName = {{0xa45c254e,0xdf1c,0x4efd,{0x80,0x20,0x67,0xd1,0x46,0xa8,0x50,0xe0}},14};

typedef int (*Progreso)(void *ctx, int hecho, int total, const wchar_t *que);

/* ------------------------------------------------------------------ */
/* utilidades                                                          */
/* ------------------------------------------------------------------ */
static void u8w(const char *s, wchar_t *o, int n) { if (!MultiByteToWideChar(CP_UTF8, 0, s, -1, o, n)) o[0] = 0; }
static void wu8(const wchar_t *w, char *o, int n) { if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, o, n, NULL, NULL)) o[0] = 0; }

static int es_dir(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
static int es_arch(const wchar_t *p)
{
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static unsigned long long tam_arch(const wchar_t *p)
{
    WIN32_FILE_ATTRIBUTE_DATA fa;
    if (!GetFileAttributesExW(p, GetFileExInfoStandard, &fa)) return 0;
    return ((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow;
}
static int crear_dirs(const wchar_t *p)
{
    int r = SHCreateDirectoryExW(NULL, p, NULL);
    if (r == ERROR_SUCCESS || r == ERROR_ALREADY_EXISTS) return 1;
    SetLastError((DWORD)r);   /* para que quien llama pueda decir el error */
    return 0;
}
/* Carpeta vacía o que no existe. */
static int dir_vacio(const wchar_t *p)
{
    wchar_t pat[MAX_PATH]; WIN32_FIND_DATAW fd; HANDLE h; int vacio = 1;
    _snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\*", p);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 1;
    do { if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) { vacio = 0; break; } } while (FindNextFileW(h, &fd));
    FindClose(h);
    return vacio;
}

/* Lee un archivo entero (como mucho max bytes; si es más grande, los últimos max). */
static char *leer_todo(const wchar_t *ruta, size_t *n, size_t max)
{
    HANDLE f; LARGE_INTEGER sz; char *b; DWORD rd; size_t want, got = 0;
    f = CreateFileW(ruta, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    if (!GetFileSizeEx(f, &sz)) { CloseHandle(f); return NULL; }
    want = (size_t)sz.QuadPart;
    if (want > max) {
        LARGE_INTEGER off; off.QuadPart = sz.QuadPart - (LONGLONG)max;
        SetFilePointerEx(f, off, NULL, FILE_BEGIN);
        want = max;
    }
    b = (char *)malloc(want + 1);
    if (!b) { CloseHandle(f); return NULL; }
    while (got < want && ReadFile(f, b + got, (DWORD)min(want - got, 1u << 24), &rd, NULL) && rd) got += rd;
    CloseHandle(f);
    b[got] = 0;
    if (n) *n = got;
    return b;
}

static int escribir_todo(const wchar_t *ruta, const void *d, size_t n)
{
    HANDLE f = CreateFileW(ruta, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD wr; BOOL ok;
    if (f == INVALID_HANDLE_VALUE) return 0;
    ok = WriteFile(f, d, (DWORD)n, &wr, NULL) && wr == (DWORD)n;
    CloseHandle(f);
    return ok;
}

/* Texto que crece (UTF-8). */
typedef struct { char *p; size_t n, cap; } Buf;
static void buf_add(Buf *b, const char *s, size_t n)
{
    if (b->n + n + 1 > b->cap) {
        size_t c = b->cap ? b->cap * 2 : 4096; char *q;
        while (c < b->n + n + 1) c *= 2;
        q = (char *)realloc(b->p, c);
        if (!q) return;
        b->p = q; b->cap = c;
    }
    memcpy(b->p + b->n, s, n); b->n += n; b->p[b->n] = 0;
}
static void buf_printf(Buf *b, const char *fmt, ...)
{
    char t[4096]; va_list ap; int r;
    va_start(ap, fmt); r = _vsnprintf_s(t, sizeof t, _TRUNCATE, fmt, ap); va_end(ap);
    buf_add(b, t, r < 0 ? strlen(t) : (size_t)r);
}

/* Ruta para enseñar: relativa a la carpeta del port o con %LOCALAPPDATA%.
 * Así ni la ventana ni las capturas llevan el nombre de usuario. */
static void ruta_corta(const wchar_t *p, wchar_t *o, size_t n)
{
    size_t lb = wcslen(g_base), ll = wcslen(g_local_raiz);
    if (lb && !_wcsnicmp(p, g_base, lb) && (p[lb] == L'\\' || !p[lb])) {
        _snwprintf_s(o, n, _TRUNCATE, L"%s", p[lb] ? p + lb + 1 : L".");
        return;
    }
    if (ll && !_wcsnicmp(p, g_local_raiz, ll) && (p[ll] == L'\\' || !p[ll])) {
        _snwprintf_s(o, n, _TRUNCATE, L"%%LOCALAPPDATA%%%s", p + ll);
        return;
    }
    {
        wchar_t up[MAX_PATH]; size_t lu;
        if (GetEnvironmentVariableW(L"USERPROFILE", up, MAX_PATH) && (lu = wcslen(up)) > 3 &&
            !_wcsnicmp(p, up, lu) && (p[lu] == L'\\' || !p[lu])) {
            _snwprintf_s(o, n, _TRUNCATE, L"%%USERPROFILE%%%s", p + lu);
            return;
        }
    }
    wcsncpy_s(o, n, p, _TRUNCATE);
}

static int juego_abierto(void)
{
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); PROCESSENTRY32W pe; int n = 0;
    if (s == INVALID_HANDLE_VALUE) return 0;
    pe.dwSize = sizeof pe;
    if (Process32FirstW(s, &pe))
        do { if (!_wcsicmp(pe.szExeFile, L"obiwan_recomp.exe")) n++; } while (Process32NextW(s, &pe));
    CloseHandle(s);
    return n;
}

static void fecha_ahora(wchar_t *o, size_t n)
{
    SYSTEMTIME st; GetLocalTime(&st);
    _snwprintf_s(o, n, _TRUNCATE, L"%04u%02u%02u-%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
}

/* SHA-256 de un trozo de memoria o de un archivo que se va leyendo. */
typedef struct { BCRYPT_ALG_HANDLE alg; BCRYPT_HASH_HANDLE h; } Sha;
static int sha_ini(Sha *s)
{
    s->alg = NULL; s->h = NULL;
    if (BCryptOpenAlgorithmProvider(&s->alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) != 0) return 0;
    if (BCryptCreateHash(s->alg, &s->h, NULL, 0, NULL, 0, 0) != 0) { BCryptCloseAlgorithmProvider(s->alg, 0); return 0; }
    return 1;
}
static void sha_add(Sha *s, const void *d, size_t n) { BCryptHashData(s->h, (PUCHAR)d, (ULONG)n, 0); }
static void sha_fin(Sha *s, char hex[65])
{
    unsigned char md[32]; int i;
    BCryptFinishHash(s->h, md, 32, 0);
    BCryptDestroyHash(s->h); BCryptCloseAlgorithmProvider(s->alg, 0);
    for (i = 0; i < 32; i++) sprintf_s(hex + 2 * i, 3, "%02x", md[i]);
}

/* ------------------------------------------------------------------ */
/* EXTRACCION.json                                                     */
/* ------------------------------------------------------------------ */
typedef struct { char path[600]; char clave[600]; unsigned long long size; char sha[65]; } ArchEsp;
typedef struct { ArchEsp *a; int n; char iso[260]; } Extraccion;

static void json_ws(const char **p) { while (**p == ' ' || **p == '\t' || **p == '\r' || **p == '\n') (*p)++; }
static int json_str(const char **p, char *o, size_t on)
{
    const char *q = *p; size_t l = 0;
    if (*q != '"') return 0;
    q++;
    while (*q && *q != '"') {
        unsigned c = (unsigned char)*q++;
        if (c == '\\') {
            char e = *q++;
            switch (e) {
            case 'n': c = '\n'; break; case 't': c = '\t'; break; case 'r': c = '\r'; break;
            case 'b': c = '\b'; break; case 'f': c = '\f'; break;
            case 'u': {
                unsigned v = 0; int i;
                for (i = 0; i < 4; i++) {
                    char h = *q++; v <<= 4;
                    if (h >= '0' && h <= '9') v |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') v |= (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') v |= (unsigned)(h - 'A' + 10);
                    else return 0;
                }
                if (v >= 0xD800 && v < 0xE000) v = '?';   /* sin pares sustitutos */
                if (v < 0x80) c = v;
                else {   /* a UTF-8 */
                    char t[4]; int k;
                    if (v < 0x800) { t[0] = (char)(0xC0 | (v >> 6)); t[1] = (char)(0x80 | (v & 63)); k = 2; }
                    else { t[0] = (char)(0xE0 | (v >> 12)); t[1] = (char)(0x80 | ((v >> 6) & 63)); t[2] = (char)(0x80 | (v & 63)); k = 3; }
                    if (l + (size_t)k < on) { memcpy(o + l, t, (size_t)k); l += (size_t)k; }
                    continue;
                }
                break; }
            default: c = (unsigned char)e; break;
            }
        }
        if (l + 1 < on) o[l++] = (char)c;
    }
    if (*q != '"') return 0;
    o[l] = 0;
    *p = q + 1;
    return 1;
}
static int json_saltar(const char **p)
{
    int prof = 0; char t[8];
    json_ws(p);
    if (**p == '"') return json_str(p, t, sizeof t) || 1;
    do {
        char c = **p;
        if (!c) return 0;
        if (c == '"') { char x[2]; const char *q = *p; size_t l; /* cadena larga: saltarla sin copiar */
            q++; while (*q && *q != '"') { if (*q == '\\' && q[1]) q++; q++; } if (!*q) return 0; *p = q + 1; (void)x; (void)l; continue; }
        if (c == '{' || c == '[') prof++;
        else if (c == '}' || c == ']') { if (!prof) return 1; prof--; if (!prof) { (*p)++; return 1; } }
        else if (!prof && (c == ',')) return 1;
        (*p)++;
    } while (prof || (**p != ',' && **p != '}' && **p != ']' && **p));
    return 1;
}
static void clave_ruta(const char *path, char *o, size_t n)
{
    size_t i;
    for (i = 0; path[i] && i + 1 < n; i++) {
        char c = path[i];
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        o[i] = c;
    }
    o[i] = 0;
}
static int cmp_arch(const void *a, const void *b) { return strcmp(((const ArchEsp *)a)->clave, ((const ArchEsp *)b)->clave); }

/* 1 si leído. */
static int extraccion_leer(const wchar_t *dir, Extraccion *x)
{
    wchar_t r[MAX_PATH]; char *t, k[64]; const char *p; size_t n; int cap = 0;
    memset(x, 0, sizeof *x);
    _snwprintf_s(r, MAX_PATH, _TRUNCATE, L"%s\\EXTRACCION.json", dir);
    t = leer_todo(r, &n, 256u << 20);
    if (!t) return 0;
    p = strstr(t, "\"iso_name\"");
    if (p) { p += 10; json_ws(&p); if (*p == ':') { p++; json_ws(&p); json_str(&p, x->iso, sizeof x->iso); } }
    p = strstr(t, "\"files\"");
    if (!p) { free(t); return 0; }
    p += 7; json_ws(&p); if (*p != ':') { free(t); return 0; }
    p++; json_ws(&p); if (*p != '[') { free(t); return 0; }
    p++;
    for (;;) {
        ArchEsp e; memset(&e, 0, sizeof e);
        json_ws(&p);
        if (*p == ']') break;
        if (*p == ',') { p++; continue; }
        if (*p != '{') { free(t); free(x->a); x->a = NULL; return 0; }
        p++;
        for (;;) {
            json_ws(&p);
            if (*p == '}') { p++; break; }
            if (*p == ',') { p++; continue; }
            if (!json_str(&p, k, sizeof k)) { free(t); free(x->a); x->a = NULL; return 0; }
            json_ws(&p); if (*p != ':') { free(t); free(x->a); x->a = NULL; return 0; }
            p++; json_ws(&p);
            if (!strcmp(k, "path")) json_str(&p, e.path, sizeof e.path);
            else if (!strcmp(k, "sha256")) json_str(&p, e.sha, sizeof e.sha);
            else if (!strcmp(k, "size")) { e.size = _strtoui64(p, (char **)&p, 10); }
            else json_saltar(&p);
        }
        if (e.path[0]) {
            if (x->n == cap) {
                ArchEsp *q; cap = cap ? cap * 2 : 256;
                q = (ArchEsp *)realloc(x->a, (size_t)cap * sizeof *q);
                if (!q) break;
                x->a = q;
            }
            clave_ruta(e.path, e.clave, sizeof e.clave);
            x->a[x->n++] = e;
        }
    }
    free(t);
    if (x->n) qsort(x->a, (size_t)x->n, sizeof *x->a, cmp_arch);
    return 1;
}

static void json_esc(Buf *b, const char *s)
{
    buf_add(b, "\"", 1);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"') buf_add(b, "\\\"", 2);
        else if (c == '\\') buf_add(b, "\\\\", 2);
        else if (c == '\n') buf_add(b, "\\n", 2);
        else if (c == '\r') buf_add(b, "\\r", 2);
        else if (c == '\t') buf_add(b, "\\t", 2);
        else if (c < 32) buf_printf(b, "\\u%04x", c);
        else buf_add(b, (const char *)&c, 1);
    }
    buf_add(b, "\"", 1);
}

/* Igual que tools/analisis_regional.py extraer (json.dumps indent=2, ensure_ascii=False). */
static int extraccion_escribir(const wchar_t *dir, const char *iso, const ArchEsp *a, int n)
{
    Buf b = {0}; wchar_t r[MAX_PATH], tmp[MAX_PATH]; SYSTEMTIME st; int i, ok;
    GetSystemTime(&st);
    buf_add(&b, "{\n  \"iso_name\": ", 16); json_esc(&b, iso);
    if (!n) buf_add(&b, ",\n  \"files\": []", 15);
    else {
        buf_add(&b, ",\n  \"files\": [\n", 14);
        for (i = 0; i < n; i++) {
            buf_add(&b, "    {\n      \"path\": ", 20); json_esc(&b, a[i].path);
            buf_printf(&b, ",\n      \"size\": %llu,\n      \"sha256\": \"%s\"\n    }%s\n", a[i].size, a[i].sha, i + 1 < n ? "," : "");
        }
        buf_add(&b, "  ]", 3);
    }
    buf_printf(&b, ",\n  \"generated_utc\": \"%04u-%02u-%02uT%02u:%02u:%02u+00:00\",\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    buf_add(&b, "  \"note\": ", 10);
    json_esc(&b, "Derivado privado de la copia del usuario. No versionar ni distribuir.");
    buf_add(&b, "\n}\n", 3);
    _snwprintf_s(r, MAX_PATH, _TRUNCATE, L"%s\\EXTRACCION.json", dir);
    _snwprintf_s(tmp, MAX_PATH, _TRUNCATE, L"%s\\EXTRACCION.json.tmp", dir);
    ok = b.p && escribir_todo(tmp, b.p, b.n) && MoveFileExW(tmp, r, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    free(b.p);
    return ok;
}

/* ------------------------------------------------------------------ */
/* verificación de la carpeta del juego                                */
/* ------------------------------------------------------------------ */
typedef struct {
    int total, ok, cambiados, faltan, sobran, sin_json, cancelado;
    Buf detalle;   /* una línea por problema (UTF-8) */
} Verif;

typedef struct { char **v; int n, cap; } ListaRutas;
static void lista_add(ListaRutas *l, const char *s)
{
    if (l->n == l->cap) { char **q; l->cap = l->cap ? l->cap * 2 : 256; q = (char **)realloc(l->v, (size_t)l->cap * sizeof *q); if (!q) return; l->v = q; }
    l->v[l->n++] = _strdup(s);
}
static void lista_libre(ListaRutas *l) { int i; for (i = 0; i < l->n; i++) free(l->v[i]); free(l->v); memset(l, 0, sizeof *l); }

/* Todos los archivos bajo dir, como rutas relativas con '/' (UTF-8). */
static void listar_arbol(const wchar_t *dir, const wchar_t *rel, ListaRutas *out)
{
    wchar_t pat[MAX_PATH * 2], sub[MAX_PATH * 2]; WIN32_FIND_DATAW fd; HANDLE h;
    _snwprintf_s(pat, MAX_PATH * 2, _TRUNCATE, rel[0] ? L"%s\\%s\\*" : L"%s%s\\*", dir, rel);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        if (rel[0]) _snwprintf_s(sub, MAX_PATH * 2, _TRUNCATE, L"%s\\%s", rel, fd.cFileName);
        else wcsncpy_s(sub, MAX_PATH * 2, fd.cFileName, _TRUNCATE);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) listar_arbol(dir, sub, out);
        } else {
            char u[MAX_PATH * 3]; wchar_t *c;
            for (c = sub; *c; c++) if (*c == L'\\') *c = L'/';
            wu8(sub, u, sizeof u);
            for (c = sub; *c; c++) if (*c == L'/') *c = L'\\';
            lista_add(out, u);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

/* rapido: solo tamaños. Devuelve 0 si no hay EXTRACCION.json. */
static int verificar(const wchar_t *dir, int rapido, Verif *v, Progreso pr, void *ctx)
{
    Extraccion x; ListaRutas todos = {0}; int i;
    memset(v, 0, sizeof *v);
    if (!extraccion_leer(dir, &x)) { v->sin_json = 1; return 0; }
    v->total = x.n;
    for (i = 0; i < x.n; i++) {
        wchar_t w[MAX_PATH * 2], rel[MAX_PATH], *c; unsigned long long t; char hex[65];
        u8w(x.a[i].path, rel, MAX_PATH);
        for (c = rel; *c; c++) if (*c == L'/') *c = L'\\';
        _snwprintf_s(w, MAX_PATH * 2, _TRUNCATE, L"%s\\%s", dir, rel);
        if (pr && !rapido && (i % 8 == 0 || i + 1 == x.n) && pr(ctx, i, x.n, rel)) { v->cancelado = 1; break; }
        if (!es_arch(w)) { v->faltan++; buf_printf(&v->detalle, "falta     %s\r\n", x.a[i].path); continue; }
        t = tam_arch(w);
        if (t != x.a[i].size) { v->cambiados++; buf_printf(&v->detalle, "tamano    %s (%llu, esperado %llu)\r\n", x.a[i].path, t, x.a[i].size); continue; }
        if (!rapido) {
            if (!aj_sha256_archivo(w, hex, NULL)) { v->cambiados++; buf_printf(&v->detalle, "no leido  %s\r\n", x.a[i].path); continue; }
            if (_stricmp(hex, x.a[i].sha)) { v->cambiados++; buf_printf(&v->detalle, "cambiado  %s\r\n", x.a[i].path); continue; }
        }
        v->ok++;
    }
    if (!v->cancelado && !rapido) {
        listar_arbol(dir, L"", &todos);
        for (i = 0; i < todos.n; i++) {
            ArchEsp k;
            clave_ruta(todos.v[i], k.clave, sizeof k.clave);
            if (!strcmp(k.clave, "extraccion.json")) continue;
            if (!x.n || !bsearch(&k, x.a, (size_t)x.n, sizeof *x.a, cmp_arch)) {
                v->sobran++; buf_printf(&v->detalle, "de mas    %s\r\n", todos.v[i]);
            }
        }
        lista_libre(&todos);
    }
    free(x.a);
    return 1;
}

/* ------------------------------------------------------------------ */
/* XDVDFS (como tools/analisis_regional.py)                            */
/* ------------------------------------------------------------------ */
#define XS 2048ull
typedef struct { HANDLE f; unsigned long long tam, base; FILETIME mt; unsigned root_sec, root_tam; char err[96]; } Iso;
typedef struct { char path[600]; unsigned sector; unsigned tam; } IsoArch;

static int iso_leer(Iso *s, unsigned long long off, void *b, size_t n)
{
    LARGE_INTEGER o; DWORD rd; size_t got = 0;
    if (off + n > s->tam || off + n < off) return 0;
    o.QuadPart = (LONGLONG)off;
    if (!SetFilePointerEx(s->f, o, NULL, FILE_BEGIN)) return 0;
    while (got < n) {
        if (!ReadFile(s->f, (char *)b + got, (DWORD)min(n - got, 1u << 24), &rd, NULL) || !rd) return 0;
        got += rd;
    }
    return 1;
}

/* 0 bien; si no, T_E_* */
static int iso_abrir(Iso *s, const wchar_t *ruta)
{
    static const unsigned long long parts[] = { 0, 0x18300000ull, 0x0FD90000ull, 0x02080000ull };
    BY_HANDLE_FILE_INFORMATION bi; unsigned char sec[2048]; int i;
    memset(s, 0, sizeof *s);
    s->f = CreateFileW(ruta, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (s->f == INVALID_HANDLE_VALUE) return T_E_ABRIR;
    if (!GetFileInformationByHandle(s->f, &bi)) { CloseHandle(s->f); s->f = INVALID_HANDLE_VALUE; return T_E_ABRIR; }
    s->tam = ((unsigned long long)bi.nFileSizeHigh << 32) | bi.nFileSizeLow;
    s->mt = bi.ftLastWriteTime;
    for (i = 0; i < 4; i++) {
        unsigned long long off = parts[i] + 32 * XS;
        if (off + XS > s->tam) continue;
        if (!iso_leer(s, off, sec, sizeof sec)) continue;
        if (!memcmp(sec, "MICROSOFT*XBOX*MEDIA", 20) && !memcmp(sec + 0x7EC, "MICROSOFT*XBOX*MEDIA", 20)) {
            s->base = parts[i];
            memcpy(&s->root_sec, sec + 20, 4);
            memcpy(&s->root_tam, sec + 24, 4);
            return 0;
        }
    }
    CloseHandle(s->f); s->f = INVALID_HANDLE_VALUE;
    return T_E_NO_XDVDFS;
}

/* 1 si la ISO no cambió desde que se abrió. */
static int iso_cerrar(Iso *s, const wchar_t *ruta)
{
    WIN32_FILE_ATTRIBUTE_DATA fa; int ok;
    if (s->f != INVALID_HANDLE_VALUE) CloseHandle(s->f);
    s->f = INVALID_HANDLE_VALUE;
    if (!GetFileAttributesExW(ruta, GetFileExInfoStandard, &fa)) return 0;
    ok = ((((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow) == s->tam) &&
         !CompareFileTime(&fa.ftLastWriteTime, &s->mt);
    return ok;
}

static int nombre_seguro(const char *n)
{
    const unsigned char *p;
    if (!*n || !strcmp(n, ".") || !strcmp(n, "..")) return 0;
    for (p = (const unsigned char *)n; *p; p++)
        if (*p < 32 || strchr("/\\:*?\"<>|", *p)) return 0;
    if (n[strlen(n) - 1] == ' ' || n[strlen(n) - 1] == '.') return 0;
    return 1;
}

/* Lista todos los archivos. 0 bien; si no, T_E_ESTRUCTURA con s->err. */
static int iso_listar(Iso *s, IsoArch **out, int *nout)
{
    typedef struct { char pre[600]; unsigned sec, tam; int prof; } Pend;
    Pend *pila = NULL; int np = 0, cp = 0, n = 0, cap = 0, ndirs = 0; IsoArch *a = NULL;
    unsigned char *tab = NULL;
#define ISO_ERR(m) do { strcpy_s(s->err, sizeof s->err, m); goto mal; } while (0)
    pila = (Pend *)malloc(sizeof *pila * 64); cp = 64;
    if (!pila) ISO_ERR("sin memoria");
    pila[np].pre[0] = 0; pila[np].sec = s->root_sec; pila[np].tam = s->root_tam; pila[np].prof = 0; np++;
    while (np) {
        Pend d = pila[--np]; unsigned *vistos; unsigned *pend; int npend = 0;
        if (++ndirs > 100000) ISO_ERR("demasiados directorios");
        if (d.prof > 32) ISO_ERR("demasiada profundidad");
        if (!d.tam) continue;
        if (d.tam > 64u * 1024 * 1024) ISO_ERR("directorio demasiado grande");
        tab = (unsigned char *)malloc(d.tam);
        vistos = (unsigned *)calloc(d.tam / 4 + 1, sizeof *vistos);
        pend = (unsigned *)malloc((d.tam / 4 + 1) * sizeof *pend);
        if (!tab || !vistos || !pend) { free(vistos); free(pend); ISO_ERR("sin memoria"); }
        if (!iso_leer(s, s->base + (unsigned long long)d.sec * XS, tab, d.tam)) { free(vistos); free(pend); ISO_ERR("directorio fuera de la ISO"); }
        pend[npend++] = 0;
        while (npend) {
            unsigned off = pend[--npend]; unsigned short l, r; unsigned first, len; unsigned char attr, nl;
            char nombre[600]; int j, k;
            if (vistos[off / 4]) { free(vistos); free(pend); ISO_ERR("ciclo en un directorio"); }
            vistos[off / 4] = 1;
            if (off + 14 > d.tam) { free(vistos); free(pend); ISO_ERR("nodo fuera de la tabla"); }
            memcpy(&l, tab + off, 2); memcpy(&r, tab + off + 2, 2);
            memcpy(&first, tab + off + 4, 4); memcpy(&len, tab + off + 8, 4);
            attr = tab[off + 12]; nl = tab[off + 13];
            if (l == 0xFFFF && r == 0xFFFF) continue;
            if (off + 14 + nl > d.tam) { free(vistos); free(pend); ISO_ERR("nombre fuera de la tabla"); }
            for (j = 0, k = 0; j < nl && k < 250; j++) {   /* Latin-1 a UTF-8 */
                unsigned char c = tab[off + 14 + j];
                if (c < 0x80) nombre[k++] = (char)c;
                else { nombre[k++] = (char)(0xC0 | (c >> 6)); nombre[k++] = (char)(0x80 | (c & 63)); }
            }
            nombre[k] = 0;
            if (!nombre_seguro(nombre)) { free(vistos); free(pend); ISO_ERR("nombre de archivo inseguro"); }
            if (attr & 0x10) {
                if (np == cp) { Pend *q; cp *= 2; q = (Pend *)realloc(pila, sizeof *pila * (size_t)cp); if (!q) { free(vistos); free(pend); ISO_ERR("sin memoria"); } pila = q; }
                _snprintf_s(pila[np].pre, sizeof pila[np].pre, _TRUNCATE, d.pre[0] ? "%s/%s" : "%s%s", d.pre, nombre);
                pila[np].sec = first; pila[np].tam = len; pila[np].prof = d.prof + 1; np++;
            } else {
                if (n == cap) { IsoArch *q; cap = cap ? cap * 2 : 512; q = (IsoArch *)realloc(a, sizeof *a * (size_t)cap); if (!q) { free(vistos); free(pend); ISO_ERR("sin memoria"); } a = q; }
                _snprintf_s(a[n].path, sizeof a[n].path, _TRUNCATE, d.pre[0] ? "%s/%s" : "%s%s", d.pre, nombre);
                a[n].sector = first; a[n].tam = len;
                if (s->base + (unsigned long long)first * XS + len > s->tam) { free(vistos); free(pend); ISO_ERR("archivo fuera de la ISO"); }
                n++;
            }
            if (l) pend[npend++] = (unsigned)l * 4;
            if (r) pend[npend++] = (unsigned)r * 4;
        }
        free(vistos); free(pend); free(tab); tab = NULL;
    }
    free(pila);
    *out = a; *nout = n;
    return 0;
mal:
    free(tab); free(pila); free(a);
    return T_E_ESTRUCTURA;
#undef ISO_ERR
}

static int cmp_isoarch(const void *a, const void *b) { return strcmp(((const IsoArch *)a)->path, ((const IsoArch *)b)->path); }

/* Copia un archivo de la ISO a dst (.partial y luego renombrar). 0 bien o el error de Windows. */
static DWORD iso_copiar(Iso *s, const IsoArch *e, const wchar_t *dst, char hex[65], volatile LONG *cancelar)
{
    static unsigned char buf[8 << 20];
    wchar_t tmp[MAX_PATH * 2]; HANDLE f; Sha sh; unsigned long long off = s->base + (unsigned long long)e->sector * XS; unsigned left = e->tam;
    DWORD wr, err = 0;
    _snwprintf_s(tmp, MAX_PATH * 2, _TRUNCATE, L"%s.partial", dst);
    f = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return GetLastError();
    if (!sha_ini(&sh)) { CloseHandle(f); DeleteFileW(tmp); return ERROR_INTERNAL_ERROR; }
    while (left) {
        unsigned n = left < sizeof buf ? left : (unsigned)sizeof buf;
        if (cancelar && *cancelar) { err = ERROR_CANCELLED; break; }
        if (!iso_leer(s, off, buf, n)) { err = ERROR_READ_FAULT; break; }
        sha_add(&sh, buf, n);
        if (!WriteFile(f, buf, n, &wr, NULL) || wr != n) { err = GetLastError(); if (!err) err = ERROR_WRITE_FAULT; break; }
        off += n; left -= n;
    }
    sha_fin(&sh, hex);
    if (!err && !FlushFileBuffers(f)) err = GetLastError();
    CloseHandle(f);
    if (!err && !MoveFileExW(tmp, dst, MOVEFILE_WRITE_THROUGH)) err = GetLastError();   /* sin REPLACE: nunca pisa */
    if (err) DeleteFileW(tmp);
    return err;
}

/* SHA-256 de un archivo de la ISO sin copiarlo. */
static int iso_sha(Iso *s, const IsoArch *e, char hex[65])
{
    static unsigned char buf[1 << 20];
    Sha sh; unsigned long long off = s->base + (unsigned long long)e->sector * XS; unsigned left = e->tam;
    if (!sha_ini(&sh)) return 0;
    while (left) {
        unsigned n = left < sizeof buf ? left : (unsigned)sizeof buf;
        if (!iso_leer(s, off, buf, n)) { sha_fin(&sh, hex); return 0; }
        sha_add(&sh, buf, n); off += n; left -= n;
    }
    sha_fin(&sh, hex);
    return 1;
}

typedef struct {
    wchar_t iso[MAX_PATH], destino[MAX_PATH];   /* destino vacío: private\game\<edicion> */
    int solo_identificar;
    /* salida */
    const AjEdicion *ed;
    int narch, error;        /* error: T_E_* o 0 */
    DWORD winerr;
    wchar_t donde[MAX_PATH], detalle[MAX_PATH];
    char hex[65];
} Importacion;

/* Mueve a la papelera una carpeta que creó esta misma importación. */
static void a_papelera(const wchar_t *dir)
{
    wchar_t dbl[MAX_PATH + 2]; SHFILEOPSTRUCTW op;
    memset(dbl, 0, sizeof dbl);
    wcsncpy_s(dbl, MAX_PATH, dir, _TRUNCATE);
    memset(&op, 0, sizeof op);
    op.wFunc = FO_DELETE; op.pFrom = dbl;
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    SHFileOperationW(&op);
}

static int importar_iso(Importacion *im, Progreso pr, void *ctx, volatile LONG *cancelar)
{
    Iso s; IsoArch *a = NULL; ArchEsp *hechos = NULL; int n = 0, i, ixbe = -1, r;
    wchar_t tmpdir[MAX_PATH], wid[32]; unsigned long long total = 0; char hex[65];
    ULARGE_INTEGER libre; Verif v;
    im->error = 0; im->winerr = 0; im->narch = 0; im->ed = NULL; im->donde[0] = 0; im->detalle[0] = 0; im->hex[0] = 0;
    if (pr) pr(ctx, 0, 1, NULL);
    r = iso_abrir(&s, im->iso);
    if (r) { im->error = r; return 0; }
    r = iso_listar(&s, &a, &n);
    if (r) { u8w(s.err, im->detalle, MAX_PATH); im->error = r; iso_cerrar(&s, im->iso); return 0; }
    for (i = 0; i < n; i++) { total += a[i].tam; if (!_stricmp(a[i].path, "default.xbe")) ixbe = i; }
    if (ixbe < 0) { im->error = T_E_SIN_XBE; goto fin; }
    if (!iso_sha(&s, &a[ixbe], hex)) { im->error = T_E_LECTURA; goto fin; }
    strcpy_s(im->hex, sizeof im->hex, hex);
    for (i = 0; i < AJ_NED; i++)
        if (!strcmp(hex, aj_ediciones[i].sha256) && a[ixbe].tam == aj_ediciones[i].size) im->ed = &aj_ediciones[i];
    if (!im->ed) { im->error = T_E_DESCONOCIDA; goto fin; }
    im->narch = n;
    if (im->solo_identificar) goto fin;

    u8w(im->ed->id, wid, 32);
    if (!im->destino[0]) _snwprintf_s(im->destino, MAX_PATH, _TRUNCATE, L"%s\\private\\game\\%s", g_base, wid);
    wcsncpy_s(im->donde, MAX_PATH, im->destino, _TRUNCATE);
    {
        wchar_t j[MAX_PATH];
        _snwprintf_s(j, MAX_PATH, _TRUNCATE, L"%s\\EXTRACCION.json", im->destino);
        if (es_arch(j)) { im->error = T_E_YA_IMPORTADA; goto fin; }
        if (!dir_vacio(im->destino) || es_arch(im->destino)) { im->error = T_E_DESTINO; goto fin; }
    }
    _snwprintf_s(tmpdir, MAX_PATH, _TRUNCATE, L"%s.importando", im->destino);
    if (es_dir(tmpdir) || es_arch(tmpdir)) { wcsncpy_s(im->donde, MAX_PATH, tmpdir, _TRUNCATE); im->error = T_E_TEMPORAL; goto fin; }
    {   /* sitio libre */
        wchar_t padre[MAX_PATH], *sl; wcsncpy_s(padre, MAX_PATH, im->destino, _TRUNCATE);
        while (!es_dir(padre) && (sl = wcsrchr(padre, L'\\')) != NULL) *sl = 0;
        if (GetDiskFreeSpaceExW(padre, &libre, NULL, NULL) && libre.QuadPart < total + (64ull << 20)) {
            _snwprintf_s(im->detalle, MAX_PATH, _TRUNCATE, L"%.1f|%.1f", (total + (64ull << 20)) / 1073741824.0, libre.QuadPart / 1073741824.0);
            im->error = T_E_ESPACIO; goto fin;
        }
    }
    if (!crear_dirs(tmpdir)) { im->winerr = GetLastError(); wcsncpy_s(im->detalle, MAX_PATH, tmpdir, _TRUNCATE); im->error = T_E_ESCRITURA; goto fin; }
    qsort(a, (size_t)n, sizeof *a, cmp_isoarch);
    hechos = (ArchEsp *)calloc((size_t)(n ? n : 1), sizeof *hechos);
    if (!hechos) { im->error = T_E_ESCRITURA; im->winerr = ERROR_OUTOFMEMORY; goto quitar; }
    for (i = 0; i < n; i++) {
        wchar_t rel[MAX_PATH], dst[MAX_PATH * 2], dirp[MAX_PATH * 2], *c, *sl; DWORD e;
        u8w(a[i].path, rel, MAX_PATH);
        for (c = rel; *c; c++) if (*c == L'/') *c = L'\\';
        if (pr && pr(ctx, i, n, rel)) { im->error = T_CANCELADO; goto quitar; }
        _snwprintf_s(dst, MAX_PATH * 2, _TRUNCATE, L"%s\\%s", tmpdir, rel);
        wcsncpy_s(dirp, MAX_PATH * 2, dst, _TRUNCATE);
        sl = wcsrchr(dirp, L'\\'); if (sl) *sl = 0;
        if (!crear_dirs(dirp)) { im->winerr = GetLastError(); ruta_corta(dst, im->detalle, MAX_PATH); im->error = T_E_ESCRITURA; goto quitar; }
        e = iso_copiar(&s, &a[i], dst, hex, cancelar);
        if (e == ERROR_CANCELLED) { im->error = T_CANCELADO; goto quitar; }
        if (e) { im->winerr = e; ruta_corta(dst, im->detalle, MAX_PATH); im->error = e == ERROR_READ_FAULT ? T_E_LECTURA : T_E_ESCRITURA; goto quitar; }
        strcpy_s(hechos[i].path, sizeof hechos[i].path, a[i].path);
        hechos[i].size = a[i].tam;
        strcpy_s(hechos[i].sha, sizeof hechos[i].sha, hex);
    }
    {
        wchar_t nombre[MAX_PATH]; char u[MAX_PATH * 3]; const wchar_t *b = wcsrchr(im->iso, L'\\');
        wcsncpy_s(nombre, MAX_PATH, b ? b + 1 : im->iso, _TRUNCATE);
        wu8(nombre, u, sizeof u);
        if (!extraccion_escribir(tmpdir, u, hechos, n)) { im->winerr = GetLastError(); wcsncpy_s(im->detalle, MAX_PATH, L"EXTRACCION.json", _TRUNCATE); im->error = T_E_ESCRITURA; goto quitar; }
    }
    if (pr) pr(ctx, n, n, NULL);
    /* comprobar lo copiado leyendo otra vez del disco */
    verificar(tmpdir, 0, &v, NULL, NULL);
    free(v.detalle.p);
    if (v.sin_json || v.cambiados || v.faltan || v.sobran || v.ok != n) { im->error = T_E_VERIF; goto quitar; }
    if (!iso_cerrar(&s, im->iso)) { im->error = T_E_ISO_CAMBIO; goto quitar_cerrada; }
    if (!dir_vacio(im->destino) || es_arch(im->destino)) { im->error = T_E_DESTINO; goto quitar_cerrada; }
    RemoveDirectoryW(im->destino);   /* solo si está vacía */
    if (!MoveFileExW(tmpdir, im->destino, 0)) { im->winerr = GetLastError(); wcsncpy_s(im->detalle, MAX_PATH, im->destino, _TRUNCATE); im->error = T_E_ESCRITURA; goto quitar_cerrada; }
    free(a); free(hechos);
    return 1;
quitar:
    iso_cerrar(&s, im->iso);
quitar_cerrada:
    a_papelera(tmpdir);   /* solo lo que creó esta importación */
    free(a); free(hechos);
    return 0;
fin:
    if (!iso_cerrar(&s, im->iso) && !im->error) im->error = T_E_ISO_CAMBIO;
    free(a); free(hechos);
    return im->error == 0;
}

/* ------------------------------------------------------------------ */
/* último log del lanzador 6                                           */
/* ------------------------------------------------------------------ */
typedef struct {
    int hay; wchar_t ruta[MAX_PATH], nombre[MAX_PATH];
    SYSTEMTIME fecha;
    int cierre;              /* 0 sin línea, 1 con código */
    unsigned long codigo;
    double fps_suma; int fps_n;
    unsigned long ft_frames, ft_33, ft_50; int ft_n;
    char modo[16];
} UltimoLog;

/* El más nuevo de la carpeta que empiece por prefijo (por fecha de escritura). */
static int mas_nuevo(const wchar_t *dir, const wchar_t *patron, wchar_t *out, size_t n, FILETIME *ft)
{
    wchar_t pat[MAX_PATH]; WIN32_FIND_DATAW fd; HANDLE h; FILETIME best = {0, 0}; int hay = 0;
    _snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\%s", dir, patron);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!hay || CompareFileTime(&fd.ftLastWriteTime, &best) > 0) {
            best = fd.ftLastWriteTime; hay = 1;
            _snwprintf_s(out, n, _TRUNCATE, L"%s\\%s", dir, fd.cFileName);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (ft) *ft = best;
    return hay;
}

static void leer_ultimo_log(UltimoLog *u)
{
    wchar_t dir[MAX_PATH]; FILETIME ft, lft; HANDLE f; static char buf[1 << 20]; char linea[2048]; size_t ll = 0; DWORD rd;
    memset(u, 0, sizeof *u);
    _snwprintf_s(dir, MAX_PATH, _TRUNCATE, L"%s\\logs", g_local);
    if (!mas_nuevo(dir, L"claude-completo4-normal-*.log", u->ruta, MAX_PATH, &ft)) return;
    u->hay = 1;
    { const wchar_t *b = wcsrchr(u->ruta, L'\\'); wcsncpy_s(u->nombre, MAX_PATH, b ? b + 1 : u->ruta, _TRUNCATE); }
    FileTimeToLocalFileTime(&ft, &lft); FileTimeToSystemTime(&lft, &u->fecha);
    f = CreateFileW(u->ruta, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    while (ReadFile(f, buf, sizeof buf, &rd, NULL) && rd) {
        DWORD i;
        for (i = 0; i < rd; i++) {
            char c = buf[i];
            if (c != '\n') { if (ll + 1 < sizeof linea) linea[ll++] = c; continue; }
            linea[ll] = 0; ll = 0;
            {
                const char *p;
                /* "[GPU] %.2f fps (%u flips)" nv2a_pb_exec.c:4621 */
                if (!strncmp(linea, "[GPU] ", 6) && strstr(linea, " fps (")) { u->fps_suma += atof(linea + 6); u->fps_n++; }
                /* "[FRAMETIME] pid .. N frames (..), ... over 33 ms N, over 50 ms N;" nv2a_gpu_tt.c:322 */
                else if ((p = strstr(linea, "[FRAMETIME] pid ")) != NULL) {
                    const char *q = strstr(p, " s: "), *o33 = strstr(p, "over 33 ms "), *o50 = strstr(p, "over 50 ms ");
                    if (q && o33 && o50) {
                        u->ft_frames += strtoul(q + 4, NULL, 10);
                        u->ft_33 += strtoul(o33 + 11, NULL, 10);
                        u->ft_50 += strtoul(o50 + 11, NULL, 10);
                        u->ft_n++;
                    }
                }
                /* lanzador 6: líneas de cierre */
                else if ((p = strstr(linea, "[LAUNCH] todos los procesos del juego han terminado; codigo del ultimo: ")) != NULL) {
                    u->cierre = 1; u->codigo = strtoul(p + 72, NULL, 10);
                } else if ((p = strstr(linea, "[LAUNCH] primer proceso finalizado: ")) != NULL) {
                    u->cierre = 1; u->codigo = strtoul(p + 36, NULL, 10);
                } else if (!strncmp(linea, "[LAUNCH] ", 9) && (p = strstr(linea, " modo=")) != NULL) {
                    sscanf_s(p + 6, "%15s", u->modo, (unsigned)sizeof u->modo);
                }
            }
        }
    }
    CloseHandle(f);
}

/* ------------------------------------------------------------------ */
/* copias de partidas                                                  */
/* ------------------------------------------------------------------ */
/* Copia recursiva sin sobrescribir. Devuelve archivos copiados o -1. */
static int copiar_arbol(const wchar_t *de, const wchar_t *a, DWORD *err)
{
    wchar_t pat[MAX_PATH], s[MAX_PATH], d[MAX_PATH]; WIN32_FIND_DATAW fd; HANDLE h; int n = 0, k;
    if (!crear_dirs(a)) { *err = GetLastError(); return -1; }
    _snwprintf_s(pat, MAX_PATH, _TRUNCATE, L"%s\\*", de);
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        _snwprintf_s(s, MAX_PATH, _TRUNCATE, L"%s\\%s", de, fd.cFileName);
        _snwprintf_s(d, MAX_PATH, _TRUNCATE, L"%s\\%s", a, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
            k = copiar_arbol(s, d, err);
            if (k < 0) { FindClose(h); return -1; }
            n += k;
        } else {
            if (!CopyFileW(s, d, TRUE)) { *err = GetLastError(); FindClose(h); return -1; }
            n++;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}
static int contar_arbol(const wchar_t *de)
{
    ListaRutas l = {0}; int n;
    listar_arbol(de, L"", &l); n = l.n; lista_libre(&l);
    return n;
}

/* ------------------------------------------------------------------ */
/* zip (solo guardado, CRC-32, nombres UTF-8)                          */
/* ------------------------------------------------------------------ */
static unsigned long crc32_de(const unsigned char *p, size_t n)
{
    static unsigned long t[256]; static int hecha; unsigned long c; size_t i;
    if (!hecha) { unsigned k; int j; for (k = 0; k < 256; k++) { c = k; for (j = 0; j < 8; j++) c = (c & 1) ? 0xEDB88320ul ^ (c >> 1) : c >> 1; t[k] = c; } hecha = 1; }
    c = 0xFFFFFFFFul;
    for (i = 0; i < n; i++) c = t[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFul;
}
typedef struct { Buf datos, central; int n; } Zip;
static void le16(Buf *b, unsigned v) { unsigned char t[2] = { (unsigned char)v, (unsigned char)(v >> 8) }; buf_add(b, (char *)t, 2); }
static void le32(Buf *b, unsigned long v) { unsigned char t[4] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) }; buf_add(b, (char *)t, 4); }
static void zip_add(Zip *z, const char *nombre, const void *d, size_t n)
{
    SYSTEMTIME st; unsigned dt, dd; unsigned long crc = crc32_de((const unsigned char *)d, n), off = (unsigned long)z->datos.n;
    size_t nl = strlen(nombre);
    GetLocalTime(&st);
    dt = (unsigned)((st.wHour << 11) | (st.wMinute << 5) | (st.wSecond / 2));
    dd = (unsigned)(((st.wYear - 1980) << 9) | (st.wMonth << 5) | st.wDay);
    le32(&z->datos, 0x04034b50ul); le16(&z->datos, 20); le16(&z->datos, 0x0800); le16(&z->datos, 0);
    le16(&z->datos, dt); le16(&z->datos, dd); le32(&z->datos, crc); le32(&z->datos, (unsigned long)n); le32(&z->datos, (unsigned long)n);
    le16(&z->datos, (unsigned)nl); le16(&z->datos, 0); buf_add(&z->datos, nombre, nl); buf_add(&z->datos, (const char *)d, n);
    le32(&z->central, 0x02014b50ul); le16(&z->central, 20); le16(&z->central, 20); le16(&z->central, 0x0800); le16(&z->central, 0);
    le16(&z->central, dt); le16(&z->central, dd); le32(&z->central, crc); le32(&z->central, (unsigned long)n); le32(&z->central, (unsigned long)n);
    le16(&z->central, (unsigned)nl); le16(&z->central, 0); le16(&z->central, 0); le16(&z->central, 0); le16(&z->central, 0);
    le32(&z->central, 0); le32(&z->central, off); buf_add(&z->central, nombre, nl);
    z->n++;
}
static int zip_escribir(Zip *z, const wchar_t *ruta)
{
    Buf fin = {0}; unsigned long off = (unsigned long)z->datos.n; int ok; HANDLE f; DWORD wr;
    le32(&fin, 0x06054b50ul); le16(&fin, 0); le16(&fin, 0); le16(&fin, (unsigned)z->n); le16(&fin, (unsigned)z->n);
    le32(&fin, (unsigned long)z->central.n); le32(&fin, off); le16(&fin, 0);
    f = CreateFileW(ruta, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { free(fin.p); return 0; }
    ok = WriteFile(f, z->datos.p, (DWORD)z->datos.n, &wr, NULL) && wr == z->datos.n &&
         WriteFile(f, z->central.p, (DWORD)z->central.n, &wr, NULL) && wr == z->central.n &&
         WriteFile(f, fin.p, (DWORD)fin.n, &wr, NULL) && wr == fin.n;
    CloseHandle(f);
    free(fin.p);
    return ok;
}

/* Quita datos personales de un texto UTF-8: carpeta de usuario, nombre de
 * usuario y nombre del equipo (sin mirar mayúsculas). Devuelve un texto nuevo. */
static void reemplazar_ci(Buf *b, const char *busca, const char *pon)
{
    Buf o = {0}; size_t lb = strlen(busca), i = 0;
    if (!lb || !b->p) return;
    while (i < b->n) {
        if (i + lb <= b->n && !_strnicmp(b->p + i, busca, lb)) { buf_add(&o, pon, strlen(pon)); i += lb; }
        else { buf_add(&o, b->p + i, 1); i++; }
    }
    if (!o.p) buf_add(&o, "", 0);
    free(b->p); *b = o;
}
static void barras(char *s, char de, char a) { for (; *s; s++) if (*s == de) *s = a; }
static void sanear(Buf *b)
{
    wchar_t w[MAX_PATH]; char u[MAX_PATH * 3], v[MAX_PATH * 3]; DWORD n;
    if (GetEnvironmentVariableW(L"USERPROFILE", w, MAX_PATH)) {
        wu8(w, u, sizeof u); reemplazar_ci(b, u, "%USERPROFILE%");
        strcpy_s(v, sizeof v, u); barras(v, '\\', '/'); reemplazar_ci(b, v, "%USERPROFILE%");
    }
    /* cualquier \Users\<nombre> que quede (otras unidades, rutas cortas) */
    {
        Buf o = {0}; size_t i = 0;
        while (b->p && i < b->n) {
            if (i + 7 <= b->n && (!_strnicmp(b->p + i, "\\Users\\", 7) || !_strnicmp(b->p + i, "/Users/", 7))) {
                size_t j = i + 7;
                buf_add(&o, b->p + i, 7);
                while (j < b->n && b->p[j] != '\\' && b->p[j] != '/' && b->p[j] != '\r' && b->p[j] != '\n' && b->p[j] != '"' && b->p[j] != ' ') j++;
                buf_add(&o, "<usuario>", 9);
                i = j;
            } else { buf_add(&o, b->p + i, 1); i++; }
        }
        if (o.p) { free(b->p); *b = o; }
    }
    n = MAX_PATH;
    if (GetUserNameW(w, &n) && wcslen(w) >= 2) { wu8(w, u, sizeof u); reemplazar_ci(b, u, "<usuario>"); }
    if (GetEnvironmentVariableW(L"USERNAME", w, MAX_PATH) && wcslen(w) >= 2) { wu8(w, u, sizeof u); reemplazar_ci(b, u, "<usuario>"); }
    n = MAX_PATH;
    if (GetComputerNameW(w, &n) && wcslen(w) >= 2) { wu8(w, u, sizeof u); reemplazar_ci(b, u, "<equipo>"); }
    n = MAX_PATH;
    if (GetComputerNameExW(ComputerNameDnsHostname, w, &n) && wcslen(w) >= 2) { wu8(w, u, sizeof u); reemplazar_ci(b, u, "<equipo>"); }
}

/* ------------------------------------------------------------------ */
/* salidas de sonido                                                   */
/* ------------------------------------------------------------------ */
typedef struct { wchar_t id[256], nombre[160]; } SalidaAudio;

/* Id que entiende XAudio2 2.9 para CreateMasteringVoice (szDeviceId): la
 * ruta de interfaz SWD del endpoint. Se comprueba con --dispositivos. */
static void id_xaudio2(const wchar_t *mmid, wchar_t *o, size_t n)
{
    _snwprintf_s(o, n, _TRUNCATE, L"\\\\?\\SWD#MMDEVAPI#%s#{e6327cad-dcec-4949-ae8a-991e976a79d2}", mmid);
}

static int listar_salidas(SalidaAudio *s, int max)
{
    IMMDeviceEnumerator *en = NULL; IMMDeviceCollection *col = NULL; UINT c = 0, i; int n = 0; HRESULT hr;
    hr = CoCreateInstance(&kCLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &kIID_IMMDeviceEnumerator, (void **)&en);
    if (FAILED(hr) || !en) return 0;
    if (SUCCEEDED(IMMDeviceEnumerator_EnumAudioEndpoints(en, eRender, DEVICE_STATE_ACTIVE, &col)) && col) {
        IMMDeviceCollection_GetCount(col, &c);
        for (i = 0; i < c && n < max; i++) {
            IMMDevice *d = NULL; LPWSTR id = NULL; IPropertyStore *ps = NULL; PROPVARIANT pv;
            if (FAILED(IMMDeviceCollection_Item(col, i, &d)) || !d) continue;
            if (SUCCEEDED(IMMDevice_GetId(d, &id)) && id) {
                wcsncpy_s(s[n].id, 256, id, _TRUNCATE);
                s[n].nombre[0] = 0;
                if (SUCCEEDED(IMMDevice_OpenPropertyStore(d, STGM_READ, &ps)) && ps) {
                    PropVariantInit(&pv);
                    if (SUCCEEDED(IPropertyStore_GetValue(ps, &kPKEY_FriendlyName, &pv)) && pv.vt == VT_LPWSTR)
                        wcsncpy_s(s[n].nombre, 160, pv.pwszVal, _TRUNCATE);
                    PropVariantClear(&pv);
                    IPropertyStore_Release(ps);
                }
                if (!s[n].nombre[0]) wcsncpy_s(s[n].nombre, 160, id, _TRUNCATE);
                n++;
                CoTaskMemFree(id);
            }
            IMMDevice_Release(d);
        }
        IMMDeviceCollection_Release(col);
    }
    IMMDeviceEnumerator_Release(en);
    return n;
}

/* Prueba si XAudio2 abre una salida con ese id (sin tocar nada). */
static HRESULT xa2_probar_id(const wchar_t *id)
{
    IXAudio2 *x = NULL; IXAudio2MasteringVoice *m = NULL; HRESULT hr;
    hr = XAudio2Create(&x, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) return hr;
    hr = x->lpVtbl->CreateMasteringVoice(x, &m, 2, 48000, 0, id, NULL, 0);
    if (SUCCEEDED(hr) && m) m->lpVtbl->DestroyVoice(m);
    x->lpVtbl->Release(x);
    return hr;
}

/* Tono corto (660 y 880 Hz) en la salida id (vacía: la predeterminada) al volumen 0-100. */
static HRESULT tocar_tono(const wchar_t *id, int volumen)
{
    IXAudio2 *x = NULL; IXAudio2MasteringVoice *m = NULL; IXAudio2SourceVoice *v = NULL; HRESULT hr;
    WAVEFORMATEX wf; XAUDIO2_BUFFER b; XAUDIO2_VOICE_STATE st; static short pcm[48000];
    int i, nmuestras = 48000 * 6 / 10, t;
    hr = XAudio2Create(&x, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) return hr;
    hr = E_FAIL;
    if (id && id[0]) hr = x->lpVtbl->CreateMasteringVoice(x, &m, 2, 48000, 0, id, NULL, 0);
    if (FAILED(hr)) hr = x->lpVtbl->CreateMasteringVoice(x, &m, 2, 48000, 0, NULL, NULL, 0);
    if (FAILED(hr)) { x->lpVtbl->Release(x); return hr; }
    m->lpVtbl->SetVolume(m, volumen / 100.0f, XAUDIO2_COMMIT_NOW);
    for (i = 0; i < nmuestras; i++) {
        double tt = i / 48000.0, f = tt < 0.25 ? 660.0 : 880.0, env, a;
        double t0 = tt < 0.25 ? tt : tt - 0.25, dur = tt < 0.25 ? 0.25 : 0.35;
        env = (t0 < 0.01 ? t0 / 0.01 : 1.0) * (t0 > dur - 0.08 ? (dur - t0) / 0.08 : 1.0);
        if (env < 0) env = 0;
        a = sin(2 * 3.14159265358979 * f * tt) * env * 0.35;
        pcm[i] = (short)(a * 32767);
    }
    memset(&wf, 0, sizeof wf);
    wf.wFormatTag = WAVE_FORMAT_PCM; wf.nChannels = 1; wf.nSamplesPerSec = 48000; wf.wBitsPerSample = 16;
    wf.nBlockAlign = 2; wf.nAvgBytesPerSec = 96000;
    hr = x->lpVtbl->CreateSourceVoice(x, &v, &wf, 0, 2.0f, NULL, NULL, NULL);
    if (SUCCEEDED(hr)) {
        memset(&b, 0, sizeof b);
        b.Flags = XAUDIO2_END_OF_STREAM; b.AudioBytes = (UINT32)(nmuestras * 2); b.pAudioData = (const BYTE *)pcm;
        hr = v->lpVtbl->SubmitSourceBuffer(v, &b, NULL);
        if (SUCCEEDED(hr)) hr = v->lpVtbl->Start(v, 0, XAUDIO2_COMMIT_NOW);
        for (t = 0; SUCCEEDED(hr) && t < 60; t++) {
            v->lpVtbl->GetState(v, &st, 0);
            if (!st.BuffersQueued) break;
            Sleep(25);
        }
        Sleep(60);
        v->lpVtbl->DestroyVoice(v);
    }
    m->lpVtbl->DestroyVoice(m);
    x->lpVtbl->Release(x);
    return hr;
}
