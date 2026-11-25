@echo off
setlocal
rem Claude completo-9 2026-11-12: "Jugar Obi-Wan 11.exe" (variante obiwan-claude-completo-9, lee obiwan.ini).
rem Archivo nuevo: no toca Jugar Obi-Wan.exe ni 2, 3, 4, 5, 6, 7, 8, 9 o 10.
if not defined VS set "VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 3
cd /d "%~dp0.."
if not exist build\lanzador mkdir build\lanzador
cl /nologo /O2 /W4 /utf-8 /DUNICODE /D_UNICODE /Fobuild\lanzador\ tools\lanzador_obiwan_completo9.c /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib bcrypt.lib /OUT:"Jugar Obi-Wan.exe" || exit /b 4
