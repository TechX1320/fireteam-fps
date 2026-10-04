@echo off
cd /d "%~dp0"

if not exist "rez\Worlds\CABINFEVER.DAT" (
  echo [ERROR] Cabin Fever is not staged.
  echo Put CABINFEVER.DAT in assets-local and run build.cmd.
  echo.
  pause
  exit /b 1
)

if exist "cabinfever-error.log" del /q "cabinfever-error.log"

echo Starting Cabin Fever directly...
echo If LithTech crashes, send cabinfever-error.log with the result.
echo.

Lithtech.exe -rez Engine.REZ -rez rez -config autoexec.cfg +runworld "Worlds/CABINFEVER" +autostart 1 +errorlog 1 +alwaysflushlog 1 +errorlogfile "cabinfever-error.log" +consoleenable 1 +numconsolelines 12
