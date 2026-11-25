# Star Wars: Obi-Wan — PC port (static recompilation)

*English below · Español más abajo*

A Windows port of the Original Xbox game *Star Wars: Obi-Wan* (2001) built by **static recompilation**: the game's
own code is translated to C and compiled for x86-64. Graphics are low-level: the game's Direct3D 8 library runs as
recompiled code and writes real NV2A pushbuffer commands, which a runtime thread executes on Direct3D 11. Audio runs
through the emulated MCPX APU.

**Status:** playable. Measured on one PC: about 100–157 fps in a combat test run of the first level (combat is limited
by the game's own CPU code) and about 320 fps in a quieter section of another level. Reducing GPU fence waits gave +9 %
in combat. Work in progress: measurements and notes in `docs/` (Spanish).

## No game data here

This repository contains **only our source code and notes**. It does **not** include the game, its XBE, any extracted
data, saves, or the generated C translation of the game (`work/obiwan-claude-completo/src/recomp/gen/`). You must own
the game and generate that code locally from your own legally obtained copy.

## Layout

| Path | What it is | License |
|---|---|---|
| `work/obiwan-claude-completo/` | Title project: launcher `main.c`, title fixes, generator tools | GPL-3.0-only (based on [soltros/obiwan-recomp](https://github.com/soltros/obiwan-recomp)) |
| `work/xboxrecomp-claude-completo9/` | Runtime: Xbox kernel, NV2A → D3D11, APU, input | MIT ([xboxrecomp](work/xboxrecomp-claude-completo9/README.md) by sp00nz); xemu-derived APU/NV2A files are LGPL-2.1+, see its `NOTICE` |
| `tools/` | Windows build scripts, launcher (`Jugar Obi-Wan`) and settings tool (`Configurar Obi-Wan`) | GPL-3.0-only |
| `docs/` | Investigation notes (Spanish) | — |

Runtime switches are environment variables `RECOMP_FIX_<NAME>` (on by default, `=0` turns a fix off) plus opt-in
diagnostics (`RECOMP_PROF=1`, `RECOMP_EXC_DIAG=1`, ...). See `docs/CLAUDE_rendimiento_y_fallos_2026-11-25.md`.

## Building (Windows, MSVC 2022 Build Tools)

1. `tools\build_sdl2.cmd <SDL2 source, tag release-2.32.10>` (static SDL2 for generic gamepads).
2. Generate the title C into `work/obiwan-claude-completo/src/recomp/gen/` from your own `default.xbe` with
   `work/obiwan-claude-completo/tools/generate_obiwan.py` (see that folder's README).
3. `tools\build_claude_completo9.cmd` → `build\obiwan-claude-completo-9\obiwan_recomp.exe`.
4. Optional: `tools\build_lanzador_obiwan_completo9.cmd` and `tools\build_configurador_obiwan.cmd`.

## Credits

- [xboxrecomp](work/xboxrecomp-claude-completo9/README.md) (MIT, sp00nz and contributors) — the recompilation toolkit
  and runtime this port is built on.
- [soltros/obiwan-recomp](https://github.com/soltros/obiwan-recomp) (GPL-3.0) — the original title project.
- [xemu](https://github.com/xemu-project/xemu) (LGPL-2.1+) — APU and NV2A register code, see `NOTICE`.
- [Star Wars: Obi Comp](https://github.com/onedeck-launcher/star-wars-obi-comp) — another Obi-Wan PC port (HLE,
  D3D12); several ideas in `docs/` came from reading it.

*Star Wars: Obi-Wan* is © Lucasfilm / LucasArts. This is an unofficial fan project, not affiliated with them.

---

## Español

Port a Windows de *Star Wars: Obi-Wan* (Xbox, 2001) por **recompilación estática**: el código del juego se traduce a C
y se compila para PC. Los gráficos son de bajo nivel: la biblioteca Direct3D 8 del propio juego escribe órdenes reales
de la GPU NV2A y un hilo del runtime las ejecuta sobre Direct3D 11. El sonido pasa por el APU emulado.

**Estado:** jugable. Medido en un PC: unos 100-157 fps en una prueba de combate del primer nivel y unos 320 fps en un
tramo tranquilo de otro nivel. Sigue en desarrollo; las notas están en `docs/`.

Aquí **solo** hay nuestro código y nuestras notas: ni el juego, ni el XBE, ni el código traducido del juego. Cada uno
tiene que generarlo a partir de su propia copia legal (pasos de arriba).
