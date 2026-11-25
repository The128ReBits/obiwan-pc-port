/* obiwan_ajustes.h -- tabla UNICA de ajustes de obiwan.ini (Claude completo-4, 2026-11-06).
 *
 * La usan el lanzador ("Jugar Obi-Wan 6.exe", tools/lanzador_obiwan_completo4.c)
 * y el configurador ("Configurar Obi-Wan.exe", tools/configurador_obiwan.c).
 * Cada ajuste dice: seccion y clave del ini, variable de entorno que lee el
 * juego, valores permitidos, valor por defecto y la linea del runtime que la
 * lee (work/xboxrecomp-claude-completo4/src salvo que diga otra cosa).
 *
 * Reglas:
 *  - Un valor igual al de por defecto NO pone ninguna variable: sin tocar nada,
 *    el juego arranca igual que "Jugar Obi-Wan 6.exe" sin obiwan.ini.
 *  - Ninguna clave toca las variables fijas del lanzador (k_set) salvo
 *    [Developer] extra, que solo se aplica con [Developer] activo = si.
 *  - Valor raro: se usa el de por defecto y se apunta un aviso.
 *  - Archivo que falta o roto: valores por defecto (el juego arranca igual).
 *
 * Compilar con /utf-8: los textos son UTF-8. Solo Win32 (sin CRT extra). */
#ifndef OBIWAN_AJUSTES_H
#define OBIWAN_AJUSTES_H

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#pragma warning(push)
#pragma warning(disable: 4505)   /* funciones static que una de las dos herramientas no usa */

#define AJ_INI_VERSION 1
#define AJ_VMAX 256

typedef enum { AJ_ELEGIR, AJ_NUM, AJ_TEXTO, AJ_TECLA, AJ_LISTA } AjTipo;

enum { AJP_JUGAR, AJP_PANTALLA, AJP_GRAFICOS, AJP_AUDIO, AJP_CONTROLES,
       AJP_IDIOMA, AJP_HERRAMIENTAS, AJP_DEV, AJP_CREDITOS, AJP_N };

typedef struct {
    const char *seccion, *clave;
    AjTipo tipo;
    /* ELEGIR: "id;id>VAR=VAL;id>VAR=VAL&VAR2=VAL2" (un id sin '>' no pone nada).
     * NUM: "min..max" (con '.' admite decimales). LISTA: fichas permitidas
     * separadas por ','. TECLA: nombre de la tecla en mkb_input.c. TEXTO: NULL
     * o "nombre" (solo letras, numeros, . _ -) o "extra" (VAR=VAL;...). */
    const char *valores;
    const char *defecto;
    const char *var;        /* variable (NUM/TEXTO/LISTA/TECLA); NULL si no hay */
    const char *runtime;    /* linea que la lee */
    int pestana, dev;
    const char *es, *en;            /* nombre */
    const char *ayuda_es, *ayuda_en;/* explicacion llana */
    const char *opc_es, *opc_en;    /* textos de cada opcion de ELEGIR, separados por ';' */
    const char *recomendado;        /* valor recomendado */
    const char *por_que;            /* por que ese valor por defecto (informe) */
} AjDef;

#define FB  "src/video/fb_present.c"
#define D3D "src/kernel/gpu_tt/kernel/nv2a_d3d11.inc"
#define MKB "src/input/mkb_input.c"
#define XIN "src/input/xinput_device.c"
#define XA2 "src/apu/apu_xaudio2.c"

static const AjDef aj_defs[] = {
/* ---------------- [Paths] ---------------- */
{ "Paths", "carpeta_juego", AJ_TEXTO, NULL, "", NULL,
  "lanzador: find_game_dir (como OBIWAN_JUEGO); el juego la recibe como argv[1]", AJP_HERRAMIENTAS, 0,
  "Carpeta del juego", "Game folder",
  "Vacío: private\\game\\<edición>. Solo cámbiala si tienes los archivos del juego en otro sitio.",
  "Empty: private\\game\\<edition>. Change it only if the game files are somewhere else.",
  NULL, NULL, "", "Vacío = la carpeta que ya usaban los lanzadores 1-5." },
{ "Paths", "carpeta_partidas", AJ_TEXTO, NULL, "", NULL,
  "lanzador: argv[2] del juego; src/kernel/kernel_path.c:537", AJP_HERRAMIENTAS, 0,
  "Carpeta de partidas", "Saves folder",
  "Vacío: %LOCALAPPDATA%\\ObiWanRecomp-dev\\saves\\jugar-codex-audio (USA), la misma de siempre.",
  "Empty: %LOCALAPPDATA%\\ObiWanRecomp-dev\\saves\\jugar-codex-audio (USA), the usual one.",
  NULL, NULL, "", "Vacío = las partidas de siempre; no se pierde ninguna." },
/* ---------------- [Language] ---------------- */
{ "Language", "interfaz", AJ_ELEGIR, "es;en", "es", NULL,
  "solo el configurador", AJP_IDIOMA, 0,
  "Idioma del configurador", "Configurator language",
  "Idioma de esta ventana. El idioma del juego depende de la edición.",
  "Language of this window. The game's language depends on the edition.",
  "Español;English", "Español;English", "es", "El usuario escribe en español." },
{ "Language", "edicion", AJ_ELEGIR, "usa;europe;spain", "usa", NULL,
  "obiwan-claude-completo/src/main.c:156 (comprueba el SHA-256 del XBE contra obiwan_ediciones.h)", AJP_IDIOMA, 0,
  "Edición del juego", "Game edition",
  "Cada idioma es una edición distinta del juego, con su propio XBE. Hoy solo la de EE. UU. está compilada.",
  "Each language is a different edition of the game, with its own XBE. Today only the USA one is built.",
  "EE. UU. (inglés);Europa (inglés, PAL);España (español, PAL)",
  "USA (English);Europe (English, PAL);Spain (Spanish, PAL)", "usa",
  "La única edición con build: las PAL necesitan su propio C generado." },
/* ---------------- [Display] ---------------- */
{ "Display", "modo", AJ_ELEGIR, "ventana;sin_bordes>RECOMP_FULLSCREEN=1", "ventana", NULL,
  FB ":598", AJP_PANTALLA, 0,
  "Modo de pantalla", "Display mode",
  "Ventana o pantalla completa sin bordes (cubre el monitor). No existe pantalla completa exclusiva. F11 o Alt+Intro cambian en el juego.",
  "Window or borderless fullscreen (covers the monitor). There is no exclusive fullscreen. F11 or Alt+Enter switch in game.",
  "Ventana;Pantalla completa sin bordes", "Window;Borderless fullscreen", "ventana",
  "Igual que los lanzadores 1-5." },
{ "Display", "monitor", AJ_NUM, "0..8", "0", "RECOMP_MONITOR",
  FB ":529", AJP_PANTALLA, 0,
  "Monitor", "Monitor",
  "0 = el principal. 1, 2... = el monitor número N según Windows. Si no existe, usa el principal.",
  "0 = the main one. 1, 2... = monitor number N as Windows counts them. If missing, the main one.",
  NULL, NULL, "0", "Sin la variable el juego abre en el principal, como antes." },
{ "Display", "escala_ventana", AJ_NUM, "1..8", "2", "RECOMP_WINDOW_SCALE",
  FB ":568", AJP_PANTALLA, 0,
  "Tamaño de la ventana", "Window size",
  "Veces 640x480. 2 = 1280x960. Si no cabe, se ajusta al escritorio.",
  "Times 640x480. 2 = 1280x960. If it does not fit, it shrinks to the desktop.",
  NULL, NULL, "2", "Es el valor que ya usa el juego." },
{ "Display", "aspecto", AJ_ELEGIR,
  "4:3;16:10>RECOMP_ASPECT=1.6;16:9>RECOMP_ASPECT=1.7778;21:9>RECOMP_ASPECT=2.3333", "4:3", NULL,
  D3D ":1398 (atof; acepta 1.34-6)", AJP_PANTALLA, 0,
  "Forma de la imagen", "Picture shape",
  "4:3 es la original. Las panorámicas enseñan más a los lados; algún menú o efecto puede verse raro.",
  "4:3 is the original. Widescreen shows more at the sides; some menu or effect may look odd.",
  "4:3 (original);16:10;16:9;21:9", "4:3 (original);16:10;16:9;21:9", "4:3",
  "Original. Se pasa como número porque la línea 1398 usa atof (\"16:9\" daría 16 y se ignoraría)." },
{ "Display", "estirar", AJ_ELEGIR, "no;si>RECOMP_PRESENT_STRETCH=1", "no", NULL,
  D3D ":3935", AJP_PANTALLA, 0,
  "Estirar la imagen", "Stretch the picture",
  "Sí: llena la ventana aunque deforme. No: mantiene la forma con bandas negras.",
  "Yes: fills the window even if distorted. No: keeps the shape with black bars.",
  "No;Sí", "No;Yes", "no", "Mantener la forma es lo que hace hoy." },
{ "Display", "hud", AJ_ELEGIR, "4:3;estirado>RECOMP_HUD_STRETCH=1", "4:3", NULL,
  D3D ":3003 y :3056", AJP_PANTALLA, 0,
  "Marcador (HUD) en panorámica", "HUD in widescreen",
  "Solo importa con forma panorámica. 4:3: el marcador queda en su sitio. Estirado: se ensancha con la imagen.",
  "Only matters in widescreen. 4:3: the HUD stays in place. Stretched: it widens with the picture.",
  "4:3;Estirado", "4:3;Stretched", "4:3", "Comportamiento actual del runtime." },
{ "Display", "ritmo", AJ_ELEGIR,
  "libre;xbox>RECOMP_FLIP_PACING=1;xbox_borde>RECOMP_FLIP_PACING=edge;vsync>RECOMP_VSYNC=1", "libre", NULL,
  "src/kernel/nv2a_pb_exec.c:3825 (FLIP_PACING), " D3D ":4042 (VSYNC)", AJP_PANTALLA, 0,
  "Ritmo de imágenes", "Frame pacing",
  "Libre: como hasta ahora (el juego marca su propio ritmo). Xbox: espera al vblank emulado como la consola. Vsync: espera al monitor.",
  "Free: as until now (the game sets its own pace). Xbox: waits for the emulated vblank like the console. Vsync: waits for the monitor.",
  "Libre (como hasta ahora);Ritmo Xbox;Ritmo Xbox (borde);Sincronía vertical",
  "Free (as until now);Xbox pacing;Xbox pacing (edge);Vertical sync", "libre",
  "Ritmo original del juego mientras no se demuestre otra cosa: FLIP_PACING no se ha probado en partida [P]." },
{ "Display", "limite_fps", AJ_NUM, "0..240", "0", "RECOMP_FRAME_CAP",
  "src/kernel/gpu_tt/kernel/nv2a_pb_exec.c:2190", AJP_PANTALLA, 0,
  "Límite de imágenes por segundo", "Frame rate limit",
  "0 = sin límite propio (el del juego). 30 o 60 fuerzan ese máximo.",
  "0 = no extra limit (the game's own). 30 or 60 force that maximum.",
  NULL, NULL, "0", "El título no pone límite (xbox_frame_cap_hz = 0)." },
{ "Display", "tearing", AJ_ELEGIR, "si;no>RECOMP_NO_TEARING=1", "si", NULL,
  D3D ":3831", AJP_PANTALLA, 0,
  "Permitir cortes (tearing) sin vsync", "Allow tearing without vsync",
  "Sí: menos retraso, puede verse una línea de corte. No: sin cortes, algo más de retraso.",
  "Yes: less lag, a tear line may show. No: no tearing, a bit more lag.",
  "Sí;No", "Yes;No", "si", "Es lo que hace hoy." },
/* ---------------- [Graphics] ---------------- */
{ "Graphics", "preset", AJ_ELEGIR, "original;rendimiento;calidad;personalizado", "original", NULL,
  "solo el configurador (rellena las claves de abajo)", AJP_GRAFICOS, 0,
  "Preajuste", "Preset",
  "Original: como la consola. Rendimiento: igual que Original (no hay nada más barato). Calidad: el doble de resolución, MSAA 4x y anisotrópico 16x.",
  "Original: like the console. Performance: same as Original (nothing cheaper exists). Quality: double resolution, MSAA 4x and 16x anisotropic.",
  "Original;Rendimiento;Calidad;Personalizado", "Original;Performance;Quality;Custom", "original",
  "Original no pone ninguna variable." },
{ "Graphics", "escala_interna", AJ_ELEGIR,
  "1;1.5>RECOMP_RENDER_SCALE=1.5;2>RECOMP_RENDER_SCALE=2;3>RECOMP_RENDER_SCALE=3;4>RECOMP_RENDER_SCALE=4", "1", NULL,
  D3D ":1390", AJP_GRAFICOS, 0,
  "Resolución interna", "Internal resolution",
  "Veces la resolución de la consola (640x480). Más es más nítido y pide más a la tarjeta.",
  "Times the console's resolution (640x480). Higher is sharper and harder on the GPU.",
  "1x (original);1,5x;2x;3x;4x", "1x (original);1.5x;2x;3x;4x", "1", "Original." },
{ "Graphics", "antialias", AJ_ELEGIR,
  "no;fxaa>RECOMP_AA=1;msaa2>RECOMP_AA=2;msaa4>RECOMP_AA=3;msaa8>RECOMP_AA=4;msaa4_fxaa>RECOMP_AA=5;msaa8_fxaa>RECOMP_AA=6",
  "no", NULL, "src/kernel/nv2a_gpu_tt_shims.c:33 (usado en " D3D ":1391)", AJP_GRAFICOS, 0,
  "Suavizado de bordes", "Anti-aliasing",
  "Quita los dientes de sierra. MSAA es más fiel; FXAA es más barato y algo borroso.",
  "Removes jagged edges. MSAA is more faithful; FXAA is cheaper and a bit blurry.",
  "No;FXAA;MSAA 2x;MSAA 4x;MSAA 8x;MSAA 4x + FXAA;MSAA 8x + FXAA",
  "Off;FXAA;MSAA 2x;MSAA 4x;MSAA 8x;MSAA 4x + FXAA;MSAA 8x + FXAA", "no", "Original." },
{ "Graphics", "anisotropico", AJ_ELEGIR,
  "1;2>RECOMP_ANISO=2;4>RECOMP_ANISO=4;8>RECOMP_ANISO=8;16>RECOMP_ANISO=16", "1", NULL,
  D3D ":174", AJP_GRAFICOS, 0,
  "Filtrado anisotrópico", "Anisotropic filtering",
  "Suelos y paredes lejanas más nítidos. Cuesta poco.",
  "Sharper far floors and walls. Cheap.",
  "1x (original);2x;4x;8x;16x", "1x (original);2x;4x;8x;16x", "1", "Original." },
{ "Graphics", "mipmaps", AJ_ELEGIR, "si;no>RECOMP_MIPMAPS=0", "si", NULL,
  D3D ":174", AJP_GRAFICOS, 0,
  "Mipmaps", "Mipmaps",
  "Sí: las texturas lejanas no parpadean. No: solo para pruebas.",
  "Yes: far textures do not shimmer. No: only for tests.",
  "Sí;No", "Yes;No", "si", "Es lo que hace hoy." },
/* ---------------- [Compat] ---------------- */
{ "Compat", "motor", AJ_ELEGIR, "d3d11;software>RECOMP_RENDERER=software;vulkan>RECOMP_RENDERER=vulkan", "d3d11", NULL,
  "src/kernel/nv2a_gpu_tt.c:72; src/kernel/gpu_tt/platform/d3d11_dyn.h:34", AJP_GRAFICOS, 0,
  "Motor de dibujo", "Renderer",
  "Direct3D 11 es el normal. Software (v0.16) es lento y solo para comparar. Vulkan solo si hay una carpeta dxvk junto al juego.",
  "Direct3D 11 is the normal one. Software (v0.16) is slow and only for comparing. Vulkan only if there is a dxvk folder next to the game.",
  "Direct3D 11;Software (v0.16);Vulkan (DXVK)", "Direct3D 11;Software (v0.16);Vulkan (DXVK)", "d3d11",
  "El motor de todas las pruebas." },
{ "Compat", "cache_shaders", AJ_ELEGIR, "si;no>RECOMP_SHADER_CACHE=0", "si", NULL,
  "src/kernel/gpu_tt/platform/d3dcompile_dyn.h:61", AJP_GRAFICOS, 0,
  "Guardar shaders compilados", "Shader cache",
  "Sí: el segundo arranque va más fluido (carpeta shadercache junto al juego).",
  "Yes: the second start is smoother (shadercache folder next to the game).",
  "Sí;No", "Yes;No", "si", "Es lo que hace hoy." },
/* ---------------- [Audio] ---------------- */
{ "Audio", "volumen", AJ_NUM, "0..100", "100", "RECOMP_AUDIO_VOLUME",
  XA2 ":126", AJP_AUDIO, 0,
  "Volumen general", "Master volume",
  "Todo el sonido del juego. Música, voces y efectos por separado no se puede: el juego los mezcla antes de que lleguen al PC.",
  "All game sound. Separate music, voice and effects is not possible: the game mixes them before they reach the PC.",
  NULL, NULL, "100", "Sin la variable no se toca el volumen." },
{ "Audio", "dispositivo", AJ_TEXTO, NULL, "", "RECOMP_AUDIO_DEVICE",
  XA2 ":103", AJP_AUDIO, 0,
  "Salida de sonido", "Sound output",
  "Vacío: la salida predeterminada de Windows. Si la elegida no existe, usa la predeterminada.",
  "Empty: the Windows default output. If the chosen one is missing, the default.",
  NULL, NULL, "", "La salida de siempre." },
{ "Audio", "buferes", AJ_NUM, "2..8", "6", "RECOMP_AUDIO_BUF_COUNT",
  XA2 ":82 (el lanzador pone 6 si no hay otro)", AJP_AUDIO, 0,
  "Búferes de sonido", "Sound buffers",
  "Más búferes: menos cortes y algo más de retraso. Menos: al revés.",
  "More buffers: fewer dropouts and a bit more lag. Fewer: the opposite.",
  NULL, NULL, "6", "Valor fijado por los lanzadores 1-5 (k_default)." },
{ "Audio", "muestras_bufer", AJ_ELEGIR,
  "512;128>RECOMP_AUDIO_BUF_SAMPLES=128;256>RECOMP_AUDIO_BUF_SAMPLES=256;1024>RECOMP_AUDIO_BUF_SAMPLES=1024;2048>RECOMP_AUDIO_BUF_SAMPLES=2048;4096>RECOMP_AUDIO_BUF_SAMPLES=4096",
  "512", NULL, XA2 ":79", AJP_AUDIO, 0,
  "Tamaño de cada búfer", "Buffer size",
  "Muestras por búfer (512 = 10,7 ms). Súbelo si oyes chasquidos.",
  "Samples per buffer (512 = 10.7 ms). Raise it if you hear crackles.",
  "512;128;256;1024;2048;4096", "512;128;256;1024;2048;4096", "512", "Valor por defecto del runtime." },
/* ---------------- [Input] ---------------- */
{ "Input", "vibracion", AJ_ELEGIR, "si;no>RECOMP_RUMBLE=0", "si", NULL,
  "src/usb/ohci.c:679", AJP_CONTROLES, 0,
  "Vibración del mando", "Controller rumble",
  "Sí: el mando vibra cuando el juego lo pide.", "Yes: the controller rumbles when the game asks.",
  "Sí;No", "Yes;No", "si", "Es lo que hace hoy." },
{ "Input", "mando_zona_muerta", AJ_NUM, "0..50", "0", "RECOMP_PAD_DEADZONE",
  XIN ":373", AJP_CONTROLES, 0,
  "Zona muerta de los sticks (%)", "Stick dead zone (%)",
  "0 = sin cambios. Súbela si el personaje se mueve solo con el stick suelto.",
  "0 = unchanged. Raise it if the character moves with the stick released.",
  NULL, NULL, "0", "Sin la variable el mando va igual que antes." },
{ "Input", "mando_invertir", AJ_LISTA, "lx,ly,rx,ry", "", "RECOMP_PAD_INVERT",
  XIN ":377", AJP_CONTROLES, 0,
  "Invertir ejes del mando", "Invert controller axes",
  "lx/ly = stick izquierdo horizontal/vertical; rx/ry = stick derecho.",
  "lx/ly = left stick horizontal/vertical; rx/ry = right stick.",
  NULL, NULL, "", "Ninguno invertido, como antes." },
{ "Input", "mando_cambiar_sticks", AJ_ELEGIR, "no;si>RECOMP_PAD_SWAP_STICKS=1", "no", NULL,
  XIN ":385", AJP_CONTROLES, 0,
  "Intercambiar sticks del mando", "Swap controller sticks",
  "Sí: el stick derecho mueve y el izquierdo maneja el sable.",
  "Yes: the right stick moves and the left one aims the saber.",
  "No;Sí", "No;Yes", "no", "Como antes." },
{ "Input", "raton_sensibilidad", AJ_NUM, "0.1..10", "1.0", "RECOMP_MKB_SENS",
  MKB ":109 (sens)", AJP_CONTROLES, 0,
  "Sensibilidad del ratón", "Mouse sensitivity",
  "El ratón hace de stick derecho (el sable). 1 = la de siempre.",
  "The mouse acts as the right stick (the saber). 1 = the usual.",
  NULL, NULL, "1.0", "Valor de mkb_input.c:51." },
{ "Input", "raton_aceleracion", AJ_NUM, "0.1..9.9", "1.0", "RECOMP_MKB_ACC",
  MKB ":111 (acc)", AJP_CONTROLES, 0,
  "Aceleración del ratón", "Mouse acceleration",
  "1 = sin aceleración. Más: los movimientos rápidos llegan más lejos.",
  "1 = no acceleration. Higher: fast moves go further.",
  NULL, NULL, "1.0", "Valor de mkb_input.c:51." },
{ "Input", "raton_zona_muerta", AJ_NUM, "0..0.89", "0.18", "RECOMP_MKB_DZ",
  MKB ":110 (dz)", AJP_CONTROLES, 0,
  "Empuje mínimo del ratón", "Mouse minimum push",
  "Fracción del recorrido que da un movimiento pequeño del ratón (para que el juego lo note).",
  "Share of the stick throw a small mouse move gives (so the game notices it).",
  NULL, NULL, "0.18", "Valor de mkb_input.c:51." },
{ "Input", "raton_recorrido", AJ_NUM, "1..999", "8", "RECOMP_MKB_FULL",
  MKB ":112 (full)", AJP_CONTROLES, 0,
  "Recorrido del ratón", "Mouse throw",
  "Píxeles de ratón por sondeo que llevan el stick al tope. Menos = más sensible.",
  "Mouse pixels per poll that push the stick fully. Lower = more sensitive.",
  NULL, NULL, "8", "Valor de mkb_input.c:51." },
{ "Input", "raton_invertir_y", AJ_ELEGIR, "no;si>RECOMP_MKB_INVERT_Y=1", "no", NULL,
  MKB ":113 (invert_y)", AJP_CONTROLES, 0,
  "Invertir eje vertical del ratón", "Invert mouse Y",
  "Sí: subir el ratón baja el sable.", "Yes: moving the mouse up moves the saber down.",
  "No;Sí", "No;Yes", "no", "Como antes." },
{ "Input", "teclado_cambiar_sticks", AJ_ELEGIR, "no;si>RECOMP_MKB_SWAP_STICKS=1", "no", NULL,
  MKB ":114 (swap_sticks)", AJP_CONTROLES, 0,
  "Ratón como stick izquierdo", "Mouse as left stick",
  "Sí: el ratón mueve al personaje y WASD el sable.", "Yes: the mouse moves the character and WASD the saber.",
  "No;Sí", "No;Yes", "no", "Como antes." },
#define AJ_TECLA_DEF(k, rt, d, es, en) \
  { "Input", "tecla_" k, AJ_TECLA, "key_" k, d, "RECOMP_MKB_KEYS", MKB ":130 (RECOMP_MKB_KEYS, key_" k "; por defecto :" rt ")", AJP_CONTROLES, 0, \
    es, en, "Tecla para esta acción. F9, F11 y F12 están reservadas.", "Key for this action. F9, F11 and F12 are reserved.", \
    NULL, NULL, d, "Tecla fija de los lanzadores 1-5 (mkb_input.c:62)." }
AJ_TECLA_DEF("up",    "62", "87",  "Mover arriba (stick izq.)", "Move up (left stick)"),
AJ_TECLA_DEF("down",  "62", "83",  "Mover abajo",               "Move down"),
AJ_TECLA_DEF("left",  "62", "65",  "Mover izquierda",           "Move left"),
AJ_TECLA_DEF("right", "62", "68",  "Mover derecha",             "Move right"),
AJ_TECLA_DEF("a",     "62", "32",  "Botón A",                   "A button"),
AJ_TECLA_DEF("b",     "62", "69",  "Botón B",                   "B button"),
AJ_TECLA_DEF("x",     "62", "82",  "Botón X",                   "X button"),
AJ_TECLA_DEF("y",     "62", "0",   "Botón Y (sin tecla)",       "Y button (no key)"),
AJ_TECLA_DEF("white", "62", "81",  "Botón blanco",              "White button"),
AJ_TECLA_DEF("black", "62", "70",  "Botón negro",               "Black button"),
AJ_TECLA_DEF("start", "62", "13",  "Start",                     "Start"),
AJ_TECLA_DEF("start2","62", "27",  "Start (segunda tecla)",     "Start (second key)"),
AJ_TECLA_DEF("back",  "62", "9",   "Back",                      "Back"),
AJ_TECLA_DEF("lthumb","62", "160", "Pulsar stick izquierdo",    "Left stick click"),
AJ_TECLA_DEF("rthumb","62", "162", "Pulsar stick derecho",      "Right stick click"),
AJ_TECLA_DEF("dup",   "62", "38",  "Cruceta arriba",            "D-pad up"),
AJ_TECLA_DEF("ddown", "62", "40",  "Cruceta abajo",             "D-pad down"),
AJ_TECLA_DEF("dleft", "62", "37",  "Cruceta izquierda",         "D-pad left"),
AJ_TECLA_DEF("dright","62", "39",  "Cruceta derecha",           "D-pad right"),
#undef AJ_TECLA_DEF
/* ---------------- [Developer] (solo con activo = si) ---------------- */
{ "Developer", "activo", AJ_ELEGIR, "no;si", "no", NULL,
  "lanzador: sin esto se ignora toda la sección", AJP_DEV, 0,
  "Modo desarrollador", "Developer mode",
  "Enseña interruptores de prueba. Pueden romper el juego: úsalos solo para medir o comparar.",
  "Shows test switches. They can break the game: use them only to measure or compare.",
  "No;Sí", "No;Yes", "no", "Apagado." },
#define AJ_FIX(n, nv, rt, es, en) \
  { "Developer", n, AJ_ELEGIR, "si;no>RECOMP_FIX_" nv "=0" , "si", NULL, rt, AJP_DEV, 1, \
    "Arreglo " es, "Fix " en, "Sí = arreglo encendido (por defecto). No = comportamiento anterior, para comparar.", \
    "Yes = fix on (default). No = previous behaviour, to compare.", "Sí;No", "Yes;No", "si", "Los arreglos van encendidos." }
AJ_FIX("fix_tex_lod",      "TEX_LOD",      D3D ":1193",                       "TEX_LOD (nivel de mipmap)", "TEX_LOD (mip level)"),
AJ_FIX("fix_shadow_pcf",   "SHADOW_PCF",   D3D ":2070",                       "SHADOW_PCF (sombras)",      "SHADOW_PCF (shadows)"),
AJ_FIX("fix_apu_panic",    "APU_PANIC",    "src/apu/apu_core.c:1152",         "APU_PANIC (audio)",         "APU_PANIC (audio)"),
AJ_FIX("fix_stream_stale", "STREAM_STALE", "src/apu/apu_vp.c:67",             "STREAM_STALE (voces)",      "STREAM_STALE (voices)"),
AJ_FIX("fix_last_sample",  "LAST_SAMPLE",  "src/apu/apu_vp.c:139",            "LAST_SAMPLE (voces)",       "LAST_SAMPLE (voices)"),
AJ_FIX("fix_read_bounce",  "READ_BOUNCE",  "src/kernel/kernel_file.c:371",    "READ_BOUNCE (lecturas)",    "READ_BOUNCE (reads)"),
AJ_FIX("fix_gpu_wait",     "GPU_WAIT",     "src/kernel/kernel_bridge.c:2175", "GPU_WAIT (espera GPU)",     "GPU_WAIT (GPU wait)"),
AJ_FIX("fix_sw_methods",   "SW_METHODS",   "src/kernel/kernel_bridge.c:2227", "SW_METHODS (métodos NV2A)", "SW_METHODS (NV2A methods)"),
AJ_FIX("fix_mkb_lr",       "MKB_LR",       "src/input/mkb_input.c:347 (usado en :264)", "MKB_LR (Mayús/Ctrl izquierdos)", "MKB_LR (left Shift/Ctrl)"),
#undef AJ_FIX
#define AJ_MED(n, spec, rt, es, en) \
  { "Developer", n, AJ_ELEGIR, "no;si>" spec, "no", NULL, rt, AJP_DEV, 1, \
    "Medida: " es, "Measure: " en, "Apagada por defecto. Escribe más en el log; no cambia el juego.", \
    "Off by default. Writes more to the log; does not change the game.", "No;Sí", "No;Yes", "no", "Las medidas van apagadas." }
AJ_MED("medir",          "RECOMP_MEDIR=1",          "obiwan-claude-completo/src/main.c:186", "todas las de Medir (FPS, GPU, audio)", "all Measure ones (FPS, GPU, audio)"),
AJ_MED("medir_frametime","RECOMP_FRAMETIME=1",      "src/kernel/nv2a_gpu_tt.c:161",          "tiempo de cada imagen (CSV)",  "time of every frame (CSV)"),
AJ_MED("medir_captura",  "RECOMP_CAPTURA_FRAME=1",  "src/kernel/nv2a_gpu_tt.c:49",           "captura de imagen con F12",    "frame capture on F12"),
AJ_MED("medir_voces",    "RECOMP_APU_STREAMS=dump", "src/apu/apu_vp.c:160",                  "voces en streaming (guarda el audio)", "streamed voices (saves the audio)"),
AJ_MED("medir_lecturas", "RECOMP_TRACE_FILE_READS=1","src/kernel/kernel_bridge.c:11531", "lecturas de archivos",   "file reads"),
AJ_MED("medir_metodos",  "RECOMP_PB_UNHANDLED_ALL=1","src/kernel/nv2a_pb_exec.c:4735",       "todos los métodos NV2A sin manejar", "every unhandled NV2A method"),
#undef AJ_MED
{ "Developer", "variante", AJ_TEXTO, "nombre", "obiwan-claude-completo-4", NULL,
  "lanzador: build\\<variante>\\obiwan_recomp.exe", AJP_DEV, 1,
  "Variante del juego", "Game build",
  "Carpeta dentro de build\\ con obiwan_recomp.exe. Sirve para comparar con versiones anteriores.",
  "Folder inside build\\ with obiwan_recomp.exe. Useful to compare with earlier builds.",
  NULL, NULL, "obiwan-claude-completo-4", "La variante de este lanzador." },
{ "Developer", "extra", AJ_TEXTO, "extra", "", NULL,
  "lanzador: se aplica al final; puede cambiar las variables fijas (k_set)", AJP_DEV, 1,
  "Variables extra", "Extra variables",
  "VAR=VALOR separadas por ';' (solo RECOMP_, OBIWAN_ o DXVK_). Pueden cambiar las variables fijas del lanzador: el juego puede no arrancar.",
  "VAR=VALUE separated by ';' (only RECOMP_, OBIWAN_ or DXVK_). They can change the launcher's fixed variables: the game may not start.",
  NULL, NULL, "", "Vacío." },
};
#define AJ_N ((int)(sizeof aj_defs / sizeof aj_defs[0]))

#undef FB
#undef D3D
#undef MKB
#undef XIN
#undef XA2

/* ---------------- ediciones (firma del XBE) ---------------- */
typedef struct {
    const char *id, *nombre_es, *nombre_en, *idioma_es, *idioma_en, *sha256, *title_id;
    unsigned size;
    const char *build;      /* NULL: falta compilar esa edicion */
    const char *partidas;   /* subcarpeta de saves */
} AjEdicion;

static const AjEdicion aj_ediciones[] = {
    { "usa", "EE. UU. (NTSC)", "USA (NTSC)", "Inglés", "English",
      "3511048a815085f78a8f1c298355b0c887a9d1040e211f888f39376cf101da53", "4C410001", 4710400u,
      "obiwan-claude-completo-4", "jugar-codex-audio" },
    { "europe", "Europa (PAL)", "Europe (PAL)", "Inglés", "English",
      "fedbc1c6576a7163303eb9ea79a70cd83fcff2e369995329413fc5fe06ba7e55", "4156000F", 4714496u,
      NULL, "europe" },
    { "spain", "España (PAL)", "Spain (PAL)", "Español", "Spanish",
      "1876dad88e70597fad75a8fb95599ae07e7c84a9f5a657dcd30be0b1bbc20cd2", "45410022", 4714496u,
      NULL, "spain" },
};
#define AJ_NED ((int)(sizeof aj_ediciones / sizeof aj_ediciones[0]))

static const AjEdicion *aj_edicion(const char *id)
{
    int i;
    for (i = 0; i < AJ_NED; i++)
        if (!_stricmp(aj_ediciones[i].id, id)) return &aj_ediciones[i];
    return NULL;
}

/* SHA-256 (hex en minusculas) de un archivo con BCrypt. 1 si bien. */
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
static int aj_sha256_archivo(const wchar_t *ruta, char hex[65], unsigned long long *tam)
{
    BCRYPT_ALG_HANDLE alg = NULL; BCRYPT_HASH_HANDLE h = NULL;
    HANDLE f; static unsigned char buf[1 << 20]; unsigned char md[32];
    DWORD n; int ok = 0, i; unsigned long long t = 0;
    f = CreateFileW(ruta, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0 &&
        BCryptCreateHash(alg, &h, NULL, 0, NULL, 0, 0) == 0) {
        ok = 1;
        while (ReadFile(f, buf, sizeof buf, &n, NULL) && n) {
            if (BCryptHashData(h, buf, n, 0) != 0) { ok = 0; break; }
            t += n;
        }
        if (ok && BCryptFinishHash(h, md, 32, 0) != 0) ok = 0;
    }
    if (h) BCryptDestroyHash(h);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    CloseHandle(f);
    if (!ok) return 0;
    for (i = 0; i < 32; i++) sprintf_s(hex + 2 * i, 3, "%02x", md[i]);
    if (tam) *tam = t;
    return 1;
}

/* Edicion de un default.xbe por su firma, o NULL. */
static const AjEdicion *aj_edicion_xbe(const wchar_t *xbe, char hex[65])
{
    unsigned long long t = 0; int i;
    if (!aj_sha256_archivo(xbe, hex, &t)) { hex[0] = 0; return NULL; }
    for (i = 0; i < AJ_NED; i++)
        if (!strcmp(hex, aj_ediciones[i].sha256) && t == aj_ediciones[i].size) return &aj_ediciones[i];
    return NULL;
}

/* ---------------- configuracion en memoria ---------------- */
#define AJ_AVISOS 4096
typedef struct {
    char val[AJ_N][AJ_VMAX];
    int version;            /* ini_version leido (0 si no habia) */
    int estado;             /* 0 falta, 1 bien, 2 con avisos, -1 roto */
    char avisos[AJ_AVISOS]; /* lineas "aviso\n" */
    int navisos;
    char desconocidas[2048];/* "seccion.clave = valor\n" que no estan en la tabla */
} AjConf;

static void aj_aviso(AjConf *c, const char *fmt, ...)
{
    va_list ap; size_t l = strlen(c->avisos);
    if (l + 2 >= AJ_AVISOS) return;
    va_start(ap, fmt);
    _vsnprintf_s(c->avisos + l, AJ_AVISOS - l - 1, _TRUNCATE, fmt, ap);
    va_end(ap);
    l = strlen(c->avisos);
    if (l + 1 < AJ_AVISOS) { c->avisos[l] = '\n'; c->avisos[l + 1] = 0; }
    c->navisos++;
}

static int aj_indice(const char *sec, const char *clave)
{
    int i;
    for (i = 0; i < AJ_N; i++)
        if (!_stricmp(aj_defs[i].seccion, sec) && !_stricmp(aj_defs[i].clave, clave)) return i;
    return -1;
}

static void aj_por_defecto(AjConf *c)
{
    int i;
    memset(c, 0, sizeof *c);
    for (i = 0; i < AJ_N; i++) strcpy_s(c->val[i], AJ_VMAX, aj_defs[i].defecto);
}

/* n-esimo id de una lista ELEGIR. Devuelve 0 si no hay tantos. */
static int aj_opcion(const AjDef *d, int n, char *id, size_t idn, const char **asig, size_t *asign)
{
    const char *p = d->valores, *e, *g; int k = 0;
    while (p && *p) {
        e = strchr(p, ';'); if (!e) e = p + strlen(p);
        if (k == n) {
            g = memchr(p, '>', (size_t)(e - p));
            if (id) { size_t l = (size_t)((g ? g : e) - p); if (l >= idn) l = idn - 1; memcpy(id, p, l); id[l] = 0; }
            if (asig) { *asig = g ? g + 1 : NULL; if (asign) *asign = g ? (size_t)(e - g - 1) : 0; }
            return 1;
        }
        k++; p = *e ? e + 1 : e;
    }
    return 0;
}

static int aj_opcion_idx(const AjDef *d, const char *v)
{
    char id[64]; int n;
    for (n = 0; aj_opcion(d, n, id, sizeof id, NULL, NULL); n++)
        if (!_stricmp(id, v)) return n;
    return -1;
}

static int aj_rango(const AjDef *d, double *lo, double *hi, int *dec)
{
    const char *p = d->valores;
    if (!p) return 0;
    *lo = strtod(p, NULL);
    p = strstr(p, "..");
    if (!p) return 0;
    *hi = strtod(p + 2, NULL);
    *dec = memchr(d->valores, '.', (size_t)(p - d->valores)) != NULL || strchr(p + 2, '.') != NULL;
    return 1;
}

static int aj_es_var(const char *s, size_t n)
{
    size_t i;
    if (!(n > 7 && !_strnicmp(s, "RECOMP_", 7)) && !(n > 7 && !_strnicmp(s, "OBIWAN_", 7)) &&
        !(n > 5 && !_strnicmp(s, "DXVK_", 5))) return 0;
    for (i = 0; i < n; i++)
        if (!((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z') || (s[i] >= '0' && s[i] <= '9') || s[i] == '_'))
            return 0;
    return 1;
}

/* Comprueba y normaliza v para el ajuste i. 1 si vale (out lleno). */
static int aj_validar(int i, const char *v, char *out, size_t outn)
{
    const AjDef *d = &aj_defs[i];
    const unsigned char *u;
    char id[64];
    switch (d->tipo) {
    case AJ_ELEGIR: {
        int n = aj_opcion_idx(d, v);
        if (n < 0) return 0;
        aj_opcion(d, n, id, sizeof id, NULL, NULL);
        strcpy_s(out, outn, id);
        return 1; }
    case AJ_NUM: {
        double lo, hi, x; int dec; char *end, nv[64], *q;
        if (!aj_rango(d, &lo, &hi, &dec) || !*v || strlen(v) >= sizeof nv) return 0;
        strcpy_s(nv, sizeof nv, v);
        for (q = nv; *q; q++) if (*q == ',') *q = '.';   /* "1,5" tambien vale */
        x = strtod(nv, &end);
        while (*end == ' ') end++;
        if (*end || x < lo || x > hi) return 0;
        if (!dec && x != (double)(long)x) return 0;
        if (dec) sprintf_s(out, outn, "%g", x); else sprintf_s(out, outn, "%ld", (long)x);
        if (dec && !strchr(out, '.') && !strchr(out, 'e')) strcat_s(out, outn, ".0");
        return 1; }
    case AJ_TECLA: {
        char *end; long k = strtol(v, &end, 0);
        if (!*v || *end || k < 0 || k > 254 || k == VK_F9 || k == VK_F11 || k == VK_F12) return 0;
        sprintf_s(out, outn, "%ld", k);
        return 1; }
    case AJ_LISTA: {
        /* fichas validas en el orden de la tabla, sin repetir */
        const char *p = d->valores; char tok[16], low[AJ_VMAX]; size_t l; out[0] = 0;
        if (strlen(v) >= sizeof low) return 0;
        strcpy_s(low, sizeof low, v); _strlwr_s(low, sizeof low); v = low;
        { /* cada ficha de v debe estar en la lista */
            const char *q = v;
            while (*q) {
                const char *e = q; while (*e && *e != ',' && *e != ' ' && *e != ';') e++;
                if (e > q) {
                    char t[16]; const char *r = d->valores; int ok = 0; size_t tl = (size_t)(e - q);
                    if (tl >= sizeof t) return 0;
                    memcpy(t, q, tl); t[tl] = 0;
                    while (*r) { const char *re = strchr(r, ','); size_t rl = re ? (size_t)(re - r) : strlen(r);
                        if (rl == tl && !_strnicmp(r, t, tl)) { ok = 1; break; }
                        r += rl; if (*r) r++; }
                    if (!ok) return 0;
                }
                q = *e ? e + 1 : e;
            }
        }
        while (*p) {
            const char *e = strchr(p, ','); l = e ? (size_t)(e - p) : strlen(p);
            memcpy(tok, p, l); tok[l] = 0;
            { const char *q = v; while ((q = strstr(q, tok)) != NULL) {
                if ((q == v || q[-1] == ',' || q[-1] == ' ' || q[-1] == ';') &&
                    (q[l] == 0 || q[l] == ',' || q[l] == ' ' || q[l] == ';')) {
                    if (out[0]) strcat_s(out, outn, ",");
                    strcat_s(out, outn, tok); break; }
                q += l; } }
            p += l; if (*p) p++;
        }
        return 1; }
    case AJ_TEXTO:
        if (strlen(v) >= AJ_VMAX - 1) return 0;
        for (u = (const unsigned char *)v; *u; u++) if (*u < 32) return 0;
        if (d->valores && !strcmp(d->valores, "nombre")) {
            const char *q;
            if (!*v) return 0;
            for (q = v; *q; q++)
                if (!((*q >= 'A' && *q <= 'Z') || (*q >= 'a' && *q <= 'z') || (*q >= '0' && *q <= '9') ||
                      *q == '-' || *q == '_' || *q == '.')) return 0;
            if (!strcmp(v, ".") || !strcmp(v, "..")) return 0;
        } else if (d->valores && !strcmp(d->valores, "extra")) {
            const char *q = v;
            while (*q) {
                const char *e = strchr(q, ';'), *eq; size_t l = e ? (size_t)(e - q) : strlen(q);
                while (l && *q == ' ') { q++; l--; }
                if (l) {
                    eq = memchr(q, '=', l);
                    if (!eq || !aj_es_var(q, (size_t)(eq - q))) return 0;
                }
                q += l; if (*q == ';') q++;
            }
        }
        strcpy_s(out, outn, v);
        return 1;
    }
    return 0;
}

static int aj_es_defecto(const AjConf *c, int i)
{
    const AjDef *d = &aj_defs[i];
    if (d->tipo == AJ_NUM) return strtod(c->val[i], NULL) == strtod(d->defecto, NULL);
    if (d->tipo == AJ_TECLA) return strtol(c->val[i], NULL, 0) == strtol(d->defecto, NULL, 0);
    return !_stricmp(c->val[i], d->defecto);
}

static void aj_poner(AjConf *c, int i, const char *v, int linea)
{
    char tmp[AJ_VMAX];
    if (aj_validar(i, v, tmp, sizeof tmp)) { strcpy_s(c->val[i], AJ_VMAX, tmp); return; }
    aj_aviso(c, "linea %d: [%s] %s = \"%s\" no vale; uso \"%s\" (por defecto)", linea,
             aj_defs[i].seccion, aj_defs[i].clave, v, aj_defs[i].defecto);
    strcpy_s(c->val[i], AJ_VMAX, aj_defs[i].defecto);
}

static char *aj_recortar(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t') s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    if (e - s >= 2 && s[0] == '"' && e[-1] == '"') { e[-1] = 0; s++; }
    return s;
}

static int aj_utf8_valido(const unsigned char *p, size_t n)
{
    size_t i = 0;
    while (i < n) {
        unsigned char b = p[i]; int k;
        if (b < 0x80) { i++; continue; }
        if ((b & 0xE0) == 0xC0 && b >= 0xC2) k = 1;
        else if ((b & 0xF0) == 0xE0) k = 2;
        else if ((b & 0xF8) == 0xF0 && b <= 0xF4) k = 3;
        else return 0;
        { int j; for (j = 1; j <= k; j++) if (i + (size_t)j >= n || (p[i + (size_t)j] & 0xC0) != 0x80) return 0; }
        i += (size_t)k + 1;
    }
    return 1;
}

/* Lee un ini. Siempre deja valores usables en c (por defecto donde falte).
 * Devuelve c->estado: 0 falta, 1 bien, 2 con avisos, -1 roto (todo por defecto). */
static int aj_cargar(const wchar_t *ruta, AjConf *c)
{
    HANDLE f; LARGE_INTEGER sz; char *buf, *p, *line, sec[64] = ""; DWORD n; int linea = 0;
    aj_por_defecto(c);
    f = CreateFileW(ruta, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { c->estado = 0; return 0; }
    if (!GetFileSizeEx(f, &sz) || sz.QuadPart > 1024 * 1024) {
        CloseHandle(f); c->estado = -1;
        aj_aviso(c, "obiwan.ini es demasiado grande o no se puede leer: uso los valores por defecto");
        return -1;
    }
    buf = (char *)malloc((size_t)sz.QuadPart + 1);
    if (!buf) { CloseHandle(f); c->estado = -1; return -1; }
    if (!ReadFile(f, buf, (DWORD)sz.QuadPart, &n, NULL) || n != (DWORD)sz.QuadPart) {
        free(buf); CloseHandle(f); c->estado = -1;
        aj_aviso(c, "no pude leer obiwan.ini entero: uso los valores por defecto");
        return -1;
    }
    CloseHandle(f);
    buf[n] = 0;
    if (memchr(buf, 0, n) || !aj_utf8_valido((unsigned char *)buf, n)) {
        free(buf); c->estado = -1;
        aj_aviso(c, "obiwan.ini esta roto (bytes nulos o texto que no es UTF-8): uso los valores por defecto");
        return -1;
    }
    p = buf;
    if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;
    while (*p) {
        char *eol = strpbrk(p, "\r\n"), *s, *eq;
        if (eol) { char save = *eol; *eol = 0; line = p; p = eol + 1; if (save == '\r' && *p == '\n') p++; }
        else { line = p; p += strlen(p); }
        linea++;
        s = aj_recortar(line);
        if (!*s || *s == ';' || *s == '#') continue;
        if (*s == '[') {
            char *e = strchr(s, ']');
            if (!e) { aj_aviso(c, "linea %d: seccion sin ']': la ignoro", linea); sec[0] = 0; continue; }
            *e = 0; strncpy_s(sec, sizeof sec, aj_recortar(s + 1), _TRUNCATE);
            continue;
        }
        eq = strchr(s, '=');
        if (!eq) { aj_aviso(c, "linea %d: \"%.40s\" no es clave = valor: la ignoro", linea, s); continue; }
        *eq = 0;
        {
            char *k = aj_recortar(s), *v = aj_recortar(eq + 1); int i;
            if (!_stricmp(k, "ini_version")) { c->version = atoi(v); continue; }
            i = aj_indice(sec, k);
            if (i < 0) {
                size_t l = strlen(c->desconocidas);
                aj_aviso(c, "linea %d: [%s] %s no existe: la ignoro", linea, sec, k);
                _snprintf_s(c->desconocidas + l, sizeof c->desconocidas - l, _TRUNCATE, "%s.%s = %s\n", sec, k, v);
                continue;
            }
            aj_poner(c, i, v, linea);
        }
    }
    free(buf);
    c->estado = c->navisos ? 2 : 1;
    return c->estado;
}

static int aj_dev_activo(const AjConf *c)
{
    int i = aj_indice("Developer", "activo");
    return i >= 0 && !_stricmp(c->val[i], "si");
}

/* Llama a fn(var, valor, i, ctx) por cada variable que hay que poner.
 * Solo valores distintos del de por defecto; [Developer] solo si activo = si.
 * i = -1 para RECOMP_MKB_KEYS (junta las teclas) y para [Developer] extra. */
typedef void (*AjFn)(const char *var, const char *val, int i, void *ctx);
static void aj_entorno(const AjConf *c, AjFn fn, void *ctx)
{
    char keys[512] = "", var[64], val[AJ_VMAX];
    int i, dev = aj_dev_activo(c), extra = -1;
    for (i = 0; i < AJ_N; i++) {
        const AjDef *d = &aj_defs[i];
        if (d->dev && !dev) continue;
        if (!_stricmp(d->seccion, "Developer") && !_stricmp(d->clave, "extra")) { extra = i; continue; }
        if (aj_es_defecto(c, i)) continue;
        switch (d->tipo) {
        case AJ_ELEGIR: {
            const char *a; size_t an; int n = aj_opcion_idx(d, c->val[i]);
            if (n < 0 || !aj_opcion(d, n, NULL, 0, &a, &an) || !a) break;
            while (an) {   /* VAR=VAL&VAR=VAL */
                const char *amp = memchr(a, '&', an), *eq; size_t l = amp ? (size_t)(amp - a) : an;
                eq = memchr(a, '=', l);
                if (eq) {
                    _snprintf_s(var, sizeof var, _TRUNCATE, "%.*s", (int)(eq - a), a);
                    _snprintf_s(val, sizeof val, _TRUNCATE, "%.*s", (int)(l - (size_t)(eq - a) - 1), eq + 1);
                    fn(var, val, i, ctx);
                }
                a += l; an -= l; if (an) { a++; an--; }
            }
            break; }
        case AJ_TECLA:
            if (strlen(keys)) strcat_s(keys, sizeof keys, ";");
            { char t[48]; sprintf_s(t, sizeof t, "%s=%s", d->valores, c->val[i]); strcat_s(keys, sizeof keys, t); }
            break;
        default:
            if (d->var && c->val[i][0]) fn(d->var, c->val[i], i, ctx);
            break;
        }
    }
    if (keys[0]) fn("RECOMP_MKB_KEYS", keys, -1, ctx);
    if (dev && extra >= 0 && c->val[extra][0]) {
        const char *q = c->val[extra];
        while (*q) {
            const char *e = strchr(q, ';'), *eq; size_t l = e ? (size_t)(e - q) : strlen(q);
            while (l && *q == ' ') { q++; l--; }
            eq = l ? memchr(q, '=', l) : NULL;
            if (eq) {
                _snprintf_s(var, sizeof var, _TRUNCATE, "%.*s", (int)(eq - q), q);
                _snprintf_s(val, sizeof val, _TRUNCATE, "%.*s", (int)(l - (size_t)(eq - q) - 1), eq + 1);
                fn(var, val, -1, ctx);
            }
            q += l; if (*q == ';') q++;
        }
    }
}

/* Escribe el ini entero (plantilla comentada + valores) de forma atomica:
 * ruta.tmp y luego MoveFileEx. Devuelve 0 si bien, o el GetLastError. */
static DWORD aj_guardar(const wchar_t *ruta, const AjConf *c)
{
    static char out[96 * 1024];
    wchar_t tmp[MAX_PATH + 8];
    size_t l = 0; int i; const char *sec = ""; HANDLE f; DWORD wr, err;
#define AJ_W(...) do { int _r = _snprintf_s(out + l, sizeof out - l, _TRUNCATE, __VA_ARGS__); if (_r > 0) l += (size_t)_r; } while (0)
    AJ_W("; obiwan.ini - ajustes de Star Wars: Obi-Wan (port de PC)\r\n");
    AJ_W("; Lo escribe \"Configurar Obi-Wan.exe\" y lo lee \"Jugar Obi-Wan 6.exe\".\r\n");
    AJ_W("; Se puede editar a mano. Un valor que no vale se cambia por el de por defecto (con aviso).\r\n");
    AJ_W("; Si borras este archivo, el juego arranca con los valores por defecto.\r\n");
    AJ_W("; Un valor igual al de por defecto no cambia nada del juego.\r\n");
    AJ_W("ini_version = %d\r\n", AJ_INI_VERSION);
    for (i = 0; i < AJ_N; i++) {
        const AjDef *d = &aj_defs[i];
        if (_stricmp(sec, d->seccion)) { sec = d->seccion; AJ_W("\r\n[%s]\r\n", sec); }
        AJ_W("\r\n; %s. %s\r\n", d->es, d->ayuda_es);
        switch (d->tipo) {
        case AJ_ELEGIR: {
            char id[64]; int n; AJ_W("; valores:");
            for (n = 0; aj_opcion(d, n, id, sizeof id, NULL, NULL); n++) AJ_W(" %s", id);
            AJ_W("\r\n"); break; }
        case AJ_NUM: AJ_W("; valores: %s\r\n", d->valores); break;
        case AJ_TECLA: AJ_W("; valor: codigo de tecla virtual de Windows (0 = ninguna)\r\n"); break;
        case AJ_LISTA: AJ_W("; valores: ninguno o varios de %s, separados por comas\r\n", d->valores); break;
        default: break;
        }
        AJ_W("; por defecto: %s%s%s | lo lee: %s\r\n", d->defecto[0] ? d->defecto : "(vacio)",
             d->var ? " | variable: " : "", d->var ? d->var : "", d->runtime);
        AJ_W("%s = %s\r\n", d->clave, c->val[i]);
    }
    if (c->desconocidas[0]) {
        const char *q = c->desconocidas;
        AJ_W("\r\n; Claves que no conozco, conservadas como comentario:\r\n");
        while (*q) { const char *e = strchr(q, '\n'); size_t n = e ? (size_t)(e - q) : strlen(q);
            AJ_W("; %.*s\r\n", (int)n, q); q += n; if (*q) q++; }
    }
#undef AJ_W
    if (l >= sizeof out - 1) return ERROR_INSUFFICIENT_BUFFER;
    _snwprintf_s(tmp, MAX_PATH + 8, _TRUNCATE, L"%s.tmp", ruta);
    f = CreateFileW(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return GetLastError();
    if (!WriteFile(f, out, (DWORD)l, &wr, NULL) || wr != (DWORD)l || !FlushFileBuffers(f)) {
        err = GetLastError(); CloseHandle(f); DeleteFileW(tmp); return err ? err : ERROR_WRITE_FAULT;
    }
    CloseHandle(f);
    if (!MoveFileExW(tmp, ruta, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        err = GetLastError(); DeleteFileW(tmp); return err;
    }
    return 0;
}

/* Rutas del ini: la de la carpeta del juego y la de LOCALAPPDATA (por si
 * Windows no deja escribir en Documentos: Acceso controlado a carpetas).
 * Elige la mas nueva por fecha de escritura. Devuelve 0 si no hay ninguna
 * (deja en out la de la carpeta del juego). */
static int aj_ruta_ini(const wchar_t *base, const wchar_t *nombre, wchar_t *out, size_t n)
{
    wchar_t a[MAX_PATH], b[MAX_PATH], la[MAX_PATH];
    WIN32_FILE_ATTRIBUTE_DATA fa, fb; int ha, hb;
    _snwprintf_s(a, MAX_PATH, _TRUNCATE, L"%s\\%s", base, nombre);
    b[0] = 0;
    if (GetEnvironmentVariableW(L"LOCALAPPDATA", la, MAX_PATH))
        _snwprintf_s(b, MAX_PATH, _TRUNCATE, L"%s\\ObiWanRecomp-dev\\%s", la, nombre);
    ha = GetFileAttributesExW(a, GetFileExInfoStandard, &fa) && !(fa.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    hb = b[0] && GetFileAttributesExW(b, GetFileExInfoStandard, &fb) && !(fb.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    if (hb && (!ha || CompareFileTime(&fb.ftLastWriteTime, &fa.ftLastWriteTime) > 0)) {
        wcsncpy_s(out, n, b, _TRUNCATE); return 2;
    }
    wcsncpy_s(out, n, a, _TRUNCATE);
    return ha ? 1 : 0;
}

#pragma warning(pop)
#endif
