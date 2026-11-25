@echo off
setlocal
rem Claude completo-4 2026-11-06: "Configurar Obi-Wan.exe" (Win32 + comctl6, lee y escribe obiwan.ini).
rem Archivo nuevo: no toca ningun Jugar Obi-Wan*.exe.
if not defined VS set "VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 3
cd /d "%~dp0.."
if not exist build\configurador mkdir build\configurador
if not exist tools\configurador_obiwan.ico py -3 -I -B tools\claude4\hacer_icono_configurador.py || exit /b 4
rc /nologo /fo build\configurador\configurador_obiwan.res tools\configurador_obiwan.rc || exit /b 5
cl /nologo /O2 /W4 /WX /utf-8 /DUNICODE /D_UNICODE /Fobuild\configurador\ tools\configurador_obiwan.c build\configurador\configurador_obiwan.res /link /SUBSYSTEM:WINDOWS /MANIFEST:NO comctl32.lib uxtheme.lib dwmapi.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib xinput.lib xaudio2.lib windowscodecs.lib winmm.lib advapi32.lib gdi32.lib bcrypt.lib user32.lib /OUT:"Configurar Obi-Wan.exe" || exit /b 6
