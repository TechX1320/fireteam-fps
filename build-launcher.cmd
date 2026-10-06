@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "NO_PAUSE=0"
if /I "%~1"=="nopause" set "NO_PAUSE=1"

where dotnet >nul 2>nul
if errorlevel 1 (
  echo [ERROR] .NET 8 SDK was not found.
  echo Install the .NET 8 SDK, then run this file again.
  if "%NO_PAUSE%"=="0" pause
  exit /b 1
)

if not exist "BUILT" mkdir "BUILT"
if not exist "BUILT\Launcher" mkdir "BUILT\Launcher"

echo Building FIRETEAM Launcher...
dotnet publish "launcher\FireteamLauncher\FireteamLauncher.csproj" -c Release -o "BUILT\Launcher"
if errorlevel 1 (
  echo [ERROR] Launcher build failed.
  if "%NO_PAUSE%"=="0" pause
  exit /b 1
)

echo.
echo [OK] Launcher: BUILT\Launcher\FireteamLauncher.exe
echo.
if "%NO_PAUSE%"=="0" pause
exit /b 0
