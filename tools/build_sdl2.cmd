@echo off
rem Compila SDL2 para los mandos genericos en Windows: biblioteca estatica x64
rem con MSVC, sin audio ni render. El video queda dentro solo porque crea la
rem ventana auxiliar que DirectInput necesita para abrir un mando
rem (SDL_HelperWindow); el juego no inicia el video de SDL.
rem Uso: tools\build_sdl2.cmd [FUENTE] [INSTALAR]
rem   FUENTE    clon de https://github.com/libsdl-org/SDL, etiqueta release-2.32.10
rem             (por defecto upstream\SDL2)
rem   INSTALAR  carpeta donde queda SDL2 instalado (por defecto build\sdl2-win)
rem Licencia de SDL2: zlib (upstream\SDL2\LICENSE.txt).
setlocal
set "SRC=%~f1"
if "%~1"=="" set "SRC=%~dp0..\upstream\SDL2"
set "DEST=%~f2"
if "%~2"=="" set "DEST=%~dp0..\build\sdl2-win"
if not defined VS set "VS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 3
set "PATH=%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
cmake -S "%SRC%" -B "%DEST%-obj" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX="%DEST%" ^
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF -DSDL2_DISABLE_SDL2MAIN=ON ^
    -DSDL_LIBC=ON -DSDL_VIDEO=ON -DSDL_AUDIO=OFF -DSDL_RENDER=OFF ^
    -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_VULKAN=OFF ^
    -DSDL_POWER=OFF -DSDL_FILESYSTEM=OFF -DSDL_LOCALE=OFF -DSDL_MISC=OFF ^
    -DSDL_JOYSTICK=ON -DSDL_HAPTIC=ON -DSDL_HIDAPI=ON -DSDL_SENSOR=ON ^
    -DSDL_EVENTS=ON -DSDL_TIMERS=ON -DSDL_THREADS=ON || exit /b 4
cmake --build "%DEST%-obj" --parallel || exit /b 5
cmake --install "%DEST%-obj" || exit /b 6
endlocal
