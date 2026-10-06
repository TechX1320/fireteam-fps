@echo off
setlocal EnableExtensions
cd /d "%~dp0"

where dotnet >nul 2>nul
if errorlevel 1 (
  echo [ERROR] .NET 8 SDK was not found.
  echo Install the .NET 8 SDK, then run this file again.
  pause
  exit /b 1
)

if not exist "BUILT" mkdir "BUILT"
if not exist "BUILT\Launcher" mkdir "BUILT\Launcher"

echo Building FIRETEAM Launcher...
dotnet publish "launcher\FireteamLauncher\FireteamLauncher.csproj" -c Release -o "BUILT\Launcher"
if errorlevel 1 (
  echo [ERROR] Launcher build failed.
  pause
  exit /b 1
)

echo.
echo [OK] Launcher: BUILT\Launcher\FireteamLauncher.exe
echo.
pause
