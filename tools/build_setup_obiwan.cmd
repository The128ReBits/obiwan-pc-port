@echo off
setlocal
rem Claude 2026-11-26: builds build\setup\Instalar-Obi-Wan.exe (the installer for players).
rem Needs MSVC 2022 and a Python 3.12 with capstone 5.0.9 (set PY312 to its python.exe if "py -3.12" is not it).
if not defined VS set "VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
rem vcvars64.bat calls vswhere.exe from PATH
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 3
cd /d "%~dp0.."
if not exist build\setup mkdir build\setup
if defined PY312 ("%PY312%" -I -B tools\hacer_payload_setup.py build\setup\payload.zip || exit /b 4) else (py -3.12 -I -B tools\hacer_payload_setup.py build\setup\payload.zip || exit /b 4)
rc /nologo /fo build\setup\setup_obiwan.res tools\setup_obiwan.rc || exit /b 5
cl /nologo /O2 /W4 /WX /utf-8 /DUNICODE /D_UNICODE /Fobuild\setup\ tools\setup_obiwan.c build\setup\setup_obiwan.res /link /SUBSYSTEM:WINDOWS /MANIFEST:NO comctl32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib urlmon.lib wintrust.lib crypt32.lib advapi32.lib user32.lib gdi32.lib comdlg32.lib /OUT:build\setup\Instalar-Obi-Wan.exe || exit /b 6
