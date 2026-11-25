@echo off
rem Variante Claude completo-9 2026-11-12 (Jugar Obi-Wan 11): copia de completo-8 sin cambios funcionales (work\xboxrecomp-claude-completo9).
rem Copia nueva: no toca revision 13, causal-1, gpuwait-1, completo-1 a completo-8. El C del juego es el de work\obiwan-claude-completo (sin cambios).
setlocal
set "VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 3
set "PATH=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
cd /d "%~dp0.."
cmake -S work\obiwan-claude-completo -B build\obiwan-claude-completo9-build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DXBOXRECOMP_DIR="%CD%\work\xboxrecomp-claude-completo9" -DCMAKE_PREFIX_PATH="%CD%\build\sdl2-win" -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="%CD%\build\obiwan-claude-completo-9" || exit /b 4
cmake --build build\obiwan-claude-completo9-build --target obiwan_recomp --parallel 4
