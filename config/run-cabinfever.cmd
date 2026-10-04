@echo off
cd /d "%~dp0"

if not exist "rez\Worlds\CABINFEVER.DAT" (
  echo [ERROR] Cabin Fever is not staged.
  echo Put CABINFEVER.DAT in assets-local and rerun setup-local.cmd then build.cmd.
  echo.
  pause
  exit /b 1
)

Lithtech.exe -rez Engine.REZ -rez rez -config autoexec.cfg +runworld Worlds/CABINFEVER
