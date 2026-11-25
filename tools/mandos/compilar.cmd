@echo off
rem Compila tools\mandos\listar_mandos.exe en build\mandos (necesita build\sdl2-win,
rem que deja tools\build_sdl2.cmd).
setlocal
set "RAIZ=%~dp0..\.."
set "SDL=%RAIZ%\build\sdl2-win"
set "OUT=%RAIZ%\build\mandos"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>nul || exit /b 3
if not exist "%OUT%" mkdir "%OUT%"
cl /nologo /W4 /O2 /MD /I"%SDL%\include\SDL2" "%~dp0listar_mandos.c" /Fo"%OUT%\\" /Fe"%OUT%\listar_mandos.exe" ^
   /link /LIBPATH:"%SDL%\lib" SDL2-static.lib kernel32.lib user32.lib gdi32.lib winmm.lib imm32.lib ^
   ole32.lib oleaut32.lib version.lib uuid.lib advapi32.lib setupapi.lib shell32.lib dinput8.lib || exit /b 4
endlocal
