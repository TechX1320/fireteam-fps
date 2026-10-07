@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "NO_PAUSE=0"
if /I "%~1"=="nopause" set "NO_PAUSE=1"

where dotnet >nul 2>nul
if errorlevel 1 (
  echo [ERROR] .NET SDK was not found.
  echo FIRETEAM's launcher now uses the same WinUI 3 stack as the supplied Open1320 launcher.
  echo Install the .NET 10 SDK, then run this file again.
  if "%NO_PAUSE%"=="0" pause
  exit /b 1
)

dotnet --list-sdks | findstr /B /C:"10." >nul
if errorlevel 1 (
  echo [ERROR] .NET 10 SDK is required for the WinUI launcher.
  echo Install .NET 10 SDK and run build.cmd again.
  if "%NO_PAUSE%"=="0" pause
  exit /b 1
)

if not exist "BUILT" mkdir "BUILT"
if exist "BUILT\Launcher" rmdir /s /q "BUILT\Launcher"
mkdir "BUILT\Launcher"

echo Building FIRETEAM WinUI 3 Launcher...
dotnet publish "launcher\FireteamLauncher.WinUI\FireteamLauncher.WinUI.csproj" -c Release -r win-x64 -o "BUILT\Launcher"
if errorlevel 1 (
  echo [ERROR] Launcher build failed.
  if "%NO_PAUSE%"=="0" pause
  exit /b 1
)

if not exist "BUILT\Launcher\FireteamLauncher.exe" (
  echo [ERROR] WinUI launcher publish completed but FireteamLauncher.exe is missing.
  if "%NO_PAUSE%"=="0" pause
  exit /b 1
)

echo.
echo [OK] FIRETEAM WinUI launcher: BUILT\Launcher\FireteamLauncher.exe
echo.
if "%NO_PAUSE%"=="0" pause
exit /b 0
