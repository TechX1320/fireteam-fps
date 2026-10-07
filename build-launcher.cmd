@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set "NO_PAUSE=0"
if /I "%~1"=="nopause" set "NO_PAUSE=1"

set "PROJECT=%CD%\launcher\FireteamLauncher.WinUI\FireteamLauncher.WinUI.csproj"
set "APP_XAML=%CD%\launcher\FireteamLauncher.WinUI\App.xaml"
set "BUILT_DIR=%CD%\BUILT"
set "SUPPORT_DIR=%BUILT_DIR%\Launcher"
set "APP_DIR=%SUPPORT_DIR%\App"
set "ROOT_EXE=%BUILT_DIR%\FireteamLauncher.exe"
set "BOOTSTRAP_EXE=%CD%\out\build\bin\FireteamLauncher.exe"
set "SHORT_BUILD=%TEMP%\FireteamLauncher\WinUI"
set "SHORT_OUTPUT=%SHORT_BUILD%\out"

where dotnet >nul 2>nul
if errorlevel 1 (
  echo [ERROR] .NET SDK was not found.
  echo Install the .NET 10 SDK, then run build.cmd again.
  goto :fail
)

for /f "tokens=*" %%V in ('dotnet --version') do set "DOTNET_VERSION=%%V"
for /f "tokens=1 delims=." %%M in ("!DOTNET_VERSION!") do set "DOTNET_MAJOR=%%M"

if not defined DOTNET_MAJOR (
  echo [ERROR] Could not determine the installed .NET SDK version.
  goto :fail
)

if !DOTNET_MAJOR! LSS 10 (
  echo [ERROR] .NET SDK 10 or newer is required. Found: !DOTNET_VERSION!
  goto :fail
)

if not exist "%PROJECT%" (
  echo [ERROR] WinUI launcher project is missing:
  echo         %PROJECT%
  goto :fail
)

if not exist "%APP_XAML%" (
  echo [ERROR] App.xaml is missing.
  goto :fail
)

findstr /C:"XamlControlsResources" "%APP_XAML%" >nul
if errorlevel 1 (
  echo [ERROR] App.xaml does not merge XamlControlsResources.
  goto :fail
)

if not exist "%BOOTSTRAP_EXE%" (
  echo [ERROR] Native FIRETEAM launcher bootstrap is missing:
  echo         %BOOTSTRAP_EXE%
  echo Run build.cmd so the Win32 game/bootstrap target is compiled first.
  goto :fail
)

echo Building FIRETEAM WinUI 3 Launcher...
echo [INFO] .NET SDK: !DOTNET_VERSION!
echo [INFO] Runtime dependencies will be staged under:
echo        BUILT\Launcher\App
echo [INFO] Player-facing executable will be:
echo        BUILT\FireteamLauncher.exe

if exist "%SHORT_BUILD%" rmdir /s /q "%SHORT_BUILD%"
mkdir "%SHORT_OUTPUT%" >nul 2>nul

rem WinUI/XAML must keep its normal project-local obj tree. Redirecting
rem BaseIntermediateOutputPath/MSBuildProjectExtensionsPath caused the markup
rem compiler to emit App.g.i.cs and MainWindow.g.i.cs twice.
if exist "%CD%\launcher\FireteamLauncher.WinUI\obj" rmdir /s /q "%CD%\launcher\FireteamLauncher.WinUI\obj"
if exist "%CD%\launcher\FireteamLauncher.WinUI\bin" rmdir /s /q "%CD%\launcher\FireteamLauncher.WinUI\bin"

echo.
echo [LAUNCHER 1/4] Restoring WinUI launcher...
dotnet restore "%PROJECT%" -r win-x64 -p:Platform=x64
if errorlevel 1 (
  echo [ERROR] Launcher dependency restore failed.
  goto :fail
)

echo.
echo [LAUNCHER 2/4] Publishing Release x64...
dotnet publish "%PROJECT%" ^
  -c Release ^
  -r win-x64 ^
  -p:Platform=x64 ^
  -p:SelfContained=true ^
  -p:WindowsAppSDKSelfContained=true ^
  --no-restore ^
  -o "%SHORT_OUTPUT%"
if errorlevel 1 (
  echo [ERROR] Launcher build failed.
  goto :fail
)

if not exist "%SHORT_OUTPUT%\FireteamLauncher.exe" (
  echo [ERROR] Build succeeded but FireteamLauncher.exe is missing from:
  echo         %SHORT_OUTPUT%
  goto :fail
)

echo.
echo [LAUNCHER 3/4] Staging clean launcher runtime...
if not exist "%BUILT_DIR%" mkdir "%BUILT_DIR%" >nul

rem A previously launched bootstrap or WinUI child can keep the self-contained
rem runtime DLLs locked. Stop both copies before replacing BUILT\Launcher\App.
taskkill /IM FireteamLauncher.exe /T /F >nul 2>nul
timeout /t 1 /nobreak >nul 2>nul

rem Remove the previous raw WinUI publish completely. This cleans the old
rem DLL/language-folder sprawl that earlier FIRETEAM launcher builds left
rem directly under BUILT\Launcher.
if exist "%SUPPORT_DIR%" rmdir /s /q "%SUPPORT_DIR%"

if exist "%SUPPORT_DIR%" (
  echo [ERROR] Could not remove the previous launcher runtime.
  echo         Close FireteamLauncher.exe and run build.cmd again.
  goto :fail
)

mkdir "%APP_DIR%" >nul 2>nul
mkdir "%SUPPORT_DIR%\Logs" >nul 2>nul

xcopy "%SHORT_OUTPUT%\*" "%APP_DIR%\" /E /I /Y /Q >nul
if errorlevel 1 (
  echo [ERROR] Could not stage the WinUI runtime.
  goto :fail
)

if not exist "%SUPPORT_DIR%\Logs" mkdir "%SUPPORT_DIR%\Logs" >nul
if not exist "%BUILT_DIR%\Mods" mkdir "%BUILT_DIR%\Mods" >nul
if not exist "%BUILT_DIR%\modTools" mkdir "%BUILT_DIR%\modTools" >nul

echo.
echo [LAUNCHER 4/4] Installing clean root bootstrap...
copy /y "%BOOTSTRAP_EXE%" "%ROOT_EXE%" >nul
if errorlevel 1 (
  echo [ERROR] Could not install BUILT\FireteamLauncher.exe.
  goto :fail
)

if not exist "%ROOT_EXE%" (
  echo [ERROR] Root launcher bootstrap was not created.
  goto :fail
)

if not exist "%APP_DIR%\FireteamLauncher.exe" (
  echo [ERROR] Launcher runtime was not staged correctly.
  goto :fail
)

echo.
echo [OK] FIRETEAM Launcher:
echo      BUILT\FireteamLauncher.exe
echo [OK] WinUI/.NET runtime:
echo      BUILT\Launcher\App\
echo [OK] Startup diagnostics:
echo      BUILT\Launcher\Logs\startup.log
echo.
if "%NO_PAUSE%"=="0" pause
exit /b 0

:fail
echo.
if "%NO_PAUSE%"=="0" pause
exit /b 1
