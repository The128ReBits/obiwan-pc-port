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

## Play with your own ISO (installer)

You need your own *Star Wars: Obi-Wan* **(USA)** Xbox ISO. Other editions (Europe, Spain) are not supported yet.

### Easy way: `Instalar-Obi-Wan.exe`

Download **`Instalar-Obi-Wan.exe`** from [Releases](https://github.com/The128ReBits/obiwan-pc-port/releases), open it,
pick your ISO and press *Install*. It does not contain the game: it copies the game files from your ISO and compiles
the port on your PC. It brings its own small Python; if Microsoft's C++ compiler (Visual Studio 2022 Build Tools) is
missing it offers to download it from Microsoft, checks Microsoft's signature and installs it (administrator
permission once, several GB, Internet needed). Then it creates Start menu / desktop shortcuts and an entry in
*Installed apps* to uninstall it. About 4 GB free are needed for the game itself.

- Windows SmartScreen will say *unknown publisher* because the exe is not signed: *More info → Run anyway*.
- With *Controlled folder access* on, Documents and Desktop are protected: install to the default folder.
- Uninstalling deletes the install folder; saved games (`%LOCALAPPDATA%\ObiWanRecomp-dev`) and your ISO stay.

To build the installer yourself: `tools\build_setup_obiwan.cmd` (needs MSVC 2022 and Python 3.12 with capstone 5.0.9).

### By hand: `Instalar Obi-Wan.cmd`

1. Install [Python](https://www.python.org/) 3.10+ and then `py -3 -m pip install capstone==5.0.9`.
2. Install [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) with
   *Desktop development with C++*.
3. Download this repository (Code → Download ZIP), unzip it and run **`Instalar Obi-Wan.cmd`**. Pick your ISO.
   It extracts the game files to `private\game\usa`, translates the game code to C and compiles everything
   (about 4 GB of free disk space). Nothing is written to your ISO.
4. Play with **`Jugar Obi-Wan.exe`**; settings in **`Configurar Obi-Wan.exe`**.

Optional: put SDL2 source (tag `release-2.32.10`) in `upstream\SDL2` before step 3 for non-Xbox gamepads; without
it only XInput (Xbox-style) gamepads and the keyboard work.

## Building by hand (Windows, MSVC 2022 Build Tools)

1. `tools\build_sdl2.cmd <SDL2 source, tag release-2.32.10>` (static SDL2 for generic gamepads, optional).
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
tiene que generarlo a partir de su propia copia legal.

### Jugar con tu ISO

Necesitas tu propia ISO de *Star Wars: Obi-Wan* **(USA)**; las ediciones Europa y España todavía no funcionan.

#### Fácil: `Instalar-Obi-Wan.exe`

Descarga **`Instalar-Obi-Wan.exe`** de [Releases](https://github.com/The128ReBits/obiwan-pc-port/releases), ábrelo,
elige tu ISO y pulsa *Instalar*. No lleva el juego dentro: copia los archivos del juego de tu ISO y compila el port en
tu PC. Trae su propio Python pequeño; si falta el compilador de C++ de Microsoft (Visual Studio 2022 Build Tools) te
ofrece descargarlo de Microsoft, comprueba la firma de Microsoft y lo instala (permiso de administrador una vez, varios
GB, hace falta Internet). Al final crea accesos directos en el menú Inicio / escritorio y una entrada en *Aplicaciones
instaladas* para desinstalarlo. Para el juego hacen falta unos 4 GB libres.

- SmartScreen de Windows dirá *editor desconocido* porque el exe no está firmado: *Más información → Ejecutar de todas formas*.
- Con *Acceso controlado a carpetas* activado, Documentos y Escritorio están protegidos: instala en la carpeta que sale por defecto.
- Al desinstalar se borra la carpeta de instalación; las partidas (`%LOCALAPPDATA%\ObiWanRecomp-dev`) y tu ISO se quedan.

#### A mano: `Instalar Obi-Wan.cmd`

1. Instala [Python](https://www.python.org/) 3.10 o más nuevo y luego `py -3 -m pip install capstone==5.0.9`.
2. Instala [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) con
   *Desarrollo para el escritorio con C++*.
3. Descarga este repositorio (Code → Download ZIP), descomprímelo y abre **`Instalar Obi-Wan.cmd`**. Elige tu ISO.
   Extrae el juego en `private\game\usa`, traduce el código del juego a C y lo compila todo (hacen falta unos 4 GB
   libres). Tu ISO no se modifica.
4. Juega con **`Jugar Obi-Wan.exe`**; los ajustes están en **`Configurar Obi-Wan.exe`**.

Opcional: si pones el código de SDL2 (etiqueta `release-2.32.10`) en `upstream\SDL2` antes del paso 3, funcionan
también mandos que no son de Xbox; sin él, solo mandos XInput (tipo Xbox) y teclado.
