@echo off
cd /d "%~dp0"

if not exist "rez\Worlds\CABINFEVER.DAT" (
  echo [ERROR] Cabin Fever is not staged.
  echo Put CABINFEVER.DAT in assets-local and run build.cmd.
  echo.
  pause
  exit /b 1
)

if exist "cabinfever-safe-error.log" del /q "cabinfever-safe-error.log"
if exist "fireteam-trace.log" del /q "fireteam-trace.log"
for %%D in (term-*.dmp) do del /q "%%D" 2>nul

echo Starting Cabin Fever with ClientFX disabled...
echo This is a diagnostic launch path, not the final game configuration.
echo.

Lithtech.exe -rez Engine.REZ -rez rez -config autoexec.cfg +runworld "Worlds/CABINFEVER" +autostart 1 +disableclientfx 1 +errorlog 1 +alwaysflushlog 1 +errorlogfile "cabinfever-safe-error.log" +consoleenable 1 +numconsolelines 12
