@echo off
rem Play with your own ISO / Jugar con tu ISO: builds the port from your copy of Star Wars: Obi-Wan (USA).
rem Needs Python 3.10+ with capstone and Visual Studio 2022 (Build Tools) with C++. See README.
setlocal
cd /d "%~dp0"
set "PY=py -3"
where py >nul 2>nul || set "PY=python"
%PY% -B tools\instalar_obiwan.py %*
set "RC=%ERRORLEVEL%"
echo.
pause
exit /b %RC%
