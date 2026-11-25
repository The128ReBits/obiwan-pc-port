# Claude (completo-9), 2026-11-25: verificación de las tareas de Codex, fallos de niveles y rendimiento

Leyenda: **[H]** hecho comprobado con medida o log · **[Hip]** hipótesis razonada, sin comprobar · **[P]** propuesta.
Build: `work/xboxrecomp-claude-completo9` → `build/obiwan-claude-completo-9/obiwan_recomp.exe`.
Logs y guiones: `build/claude9/` (runs/, pads/, ab_walk.sh, nivel.sh, fpsjuego.py, fence_sum.py).
Port de otros: ver `docs/CLAUDE_otros_ports_obiwan_2026-11-25.md`.

## 1. Tarea 10 de Codex (mejoras del recorrido del pushbuffer): VERIFICADA [H]

Nivel 1, combate (`pads/l01_juego.pad`), FF_GPU=1, 2 rondas alternas, `ab_walk.sh w`:

| Recorrido | fps medios | recorrido ms/frame |
|---|---|---|
| mejoras activadas | 120,1 / 121,7 | ~3,55 |
| mejoras desactivadas | 111,4 / 111,0 | 4,97 / 4,62 |

+9 % de fps, imagen correcta en los PNG, 0 FATAL. La tarea 3 ya estaba verificada; SA_05 no se reproduce con completo-9.

## 2. Fallos de niveles

- **SA_05 y L05_7** [H]: 90 s cada uno con completo-9 → 0 FATAL. Antes morían al leer host 0x84010000 (primer byte después de
  la vista contigua de 64 MB) dentro de una DLL del sistema.
  - **RECOMP_FIX_GPU_SURFACE_BOUNDS** (Codex tarea 11, aplicado): toda copia de superficie GPU↔RAM comprueba que cabe en la
    vista; si no, se salta y avisa una vez con `[SURF-BOUNDS] skipped`. En las pruebas no saltó nunca (0 avisos), así que es
    **preventivo**: [Hip] la causa original era una copia de superficie fuera de la vista. `=0` vuelve a copiar sin mirar.
  - **RECOMP_FIX_CRASH_DIAG** (Codex tarea 13, aplicado con cambio mío): si el juego muere, el log dice excepción, hilo,
    dirección, módulo+offset del rip y 16 retornos (`[CRASH] ...`). El diff de Codex lo instalaba antes que main.c, que lo
    pisaba; ahora se instala al crear el primer hilo del juego (`bridge_spawn_thread` → `xbox_CrashDiagInstall`).
    Comprobado de punta a punta con `RECOMP_CRASH_DIAG_TEST=1` (lee 0x84010000 a los 3 s) [H].
- **L05_4a** [H]: no hay fallo. Los "31 fps" de la tabla de la tarea 3 medían la pantalla "Grounded!" (objetivos/pausa, 32 fps
  con el recorrido a 95 ms/s). Jugando (`pads/l24_juego.pad`) va a ~322 fps.

## 3. Parpadeo y sombras

Codex (tarea 12) concluyó [H] que nuestra ruta F16 es correcta; su diff de borde de RAM queda como propuesta, sin aplicar.

## 4. Rendimiento: dónde se va el tiempo en combate

- **Esperas a la GPU** (`RECOMP_FENCE_DIAG=1`, Codex tarea 14, run cx_f01, 146 fps) [H]: en partida casi no hay esperas de
  BlockOnTime (algún make_space de 0,77 ms; los locks de buffers/texturas con 0 fences pendientes). El 66 % de espera que se
  vio antes era de las **cargas**, no del combate. Partir el recorrido en dos etapas daría poco en combate.
- **Perfil limpio** (`RECOMP_PROF=1`, run cx_p03, 125 fps) [H]:
  - title-main ocupado ~100 % con código del propio juego: sub_0010BD40 12-15 %, sub_003AED12 8-10 %, KickOff
    (sub_00370460, espera el ack de 0x100410) ~7 %, KiUserExceptionDispatcher 5,5-7 %, sub_0005DAF0 5,5 %. Esperas: 0,1 %.
  - hilo pushbuffer: 45 % en ZwDelayExecution (ocioso).
  - Conclusión: en combate el límite es **la CPU del juego** (~6,8-8 ms/frame); el recorrido va en paralelo y le sobra.
- **Excepciones** (`RECOMP_EXC_DIAG=1`, nuevo, apagado por defecto, run cx_e01) [H]: en combate saltan
  **40 000-135 000 excepciones por segundo en registros del APU (audio)** y ~1 400/s en USB OHCI; 0 en AC97 y 0 de otro tipo.
  Ese es el ~6 % de KiUserExceptionDispatcher. **No se ha tocado** (regla: el audio no se toca).
  [P] La mejora más clara que queda: responder los registros más leídos del APU sin excepción (mapearlos como memoria
  normal y actualizar su valor desde el hilo del APU, o un fast path). Necesita permiso del usuario por ser audio. Ganancia
  [Hip]: hasta ~6 % en title-main, más lo que pierdan los hilos de audio.
- **PB_HOTSPIN** (espera activa del recorrido, opción ya existente, opt-in): A/B en combate más abajo (§5).

### ¿240 fps?

Siendo honestos [Hip, apoyado en el perfil]: 240 fps son 4,17 ms por frame y hoy title-main gasta ~7-8 ms en combate
haciendo trabajo del propio juego (código recompilado), sin esperar a la GPU. Quitar las excepciones del APU daría ~6 %;
el resto exige acelerar el código recompilado (optimizaciones del generador, funciones calientes como sub_0010BD40 y
sub_003AED12, o paralelizar). No es alcanzable con ajustes del runtime de dibujo. En menús y escenas ligeras ya se pasa de
240 (L05_4a ~322 fps).

## 5. A/B de PB_HOTSPIN en combate

`RECOMP_PB_HOTSPIN_US=100` (on) frente a `=0` (off), nivel 1 combate, FF_GPU=1, `ab_walk.sh hs 2` + `ab_walk.sh hz 3` [H]:

| | fps medios por partida | media |
|---|---|---|
| on | 143,9 · 138,9 · 127,8 · 100,5 · 156,6 | 133,5 |
| off | 120,4 · 143,2 · 116,9 · 105,8 (cx_hz1_off se quedó en el menú: descartada) | 121,6 |

Diferencia +10 %, pero la dispersión entre partidas (100-157 fps con el mismo exe) es igual de grande: Welch t ≈ 1,
**no concluyente**. Se queda opt-in como estaba. [Hip] el guion de combate no es determinista (segs/frame varía de 6,7 a
12,8), así que para medir mejoras de <10 % hace falta un banco fijo (cámara quieta o repetición) o muchas más rondas.

Partida de control con el exe final (sin el enganche FENCE, cx_k01): 136,8 fps, 0 FATAL/[CRASH]/[SURF-BOUNDS], imagen correcta.

## 6. Interruptores nuevos o tocados

| Variable | Por defecto | Qué hace |
|---|---|---|
| RECOMP_FIX_GPU_SURFACE_BOUNDS | on | no copia superficies que salgan de la vista contigua |
| RECOMP_FIX_CRASH_DIAG | on | informe `[CRASH]` detallado en un cierre por excepción |
| RECOMP_CRASH_DIAG_TEST=1 | off | prueba: provoca el fallo de 0x84010000 a los 3 s |
| RECOMP_EXC_DIAG=1 | off | cuenta excepciones por tipo, una línea `[EXC]` por segundo |
| RECOMP_FENCE_DIAG=1 | off | líneas `[FENCE]` de esperas de BlockOnTime; **solo si se reaplica** el enganche (abajo) |

Diffs de esta sesión: `docs/parches-claude-completo9/`. El enganche de CRASH_DIAG en `kernel_bridge.c`
(`bridge_spawn_thread`) son 2 líneas: `extern void xbox_CrashDiagInstall(void);` y su llamada.

**Enganche FENCE retirado del build**: obligaba a compilar 3 chunks del gen (0038/0257/0258) con `/FI` del
recomp_types.h del runtime. Cumplido su papel, se devolvieron `CMakeLists.txt` y `templates/runtime/recomp_types.h` a su
estado anterior (copias con el enganche en `build/claude9/con_t14/`, diff completo en
`docs/parches-claude-completo9/FENCE_DIAG_tarea14_completo.diff`). La parte de `kernel_bridge.c` sigue compilada pero no
se llama.
