@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title FIRETEAM Dedicated Server - Experimental Build

echo ============================================
echo   FIRETEAM Dedicated Server (experimental)
echo ============================================
echo.

if not exist ".local\imports\engine\sdk\inc\server_interface.h" (
  echo [ERROR] Missing Jupiter server_interface.h.
  echo Run setup-local.cmd first.
  goto :fail
)

if not exist "out\build\CMakeCache.txt" (
  echo [CHECK] Configuring Win32 game workspace...
  cmake -S . -B "out\build" -G "Visual Studio 17 2022" -A Win32
  if errorlevel 1 goto :fail
)

echo [BUILD] Compiling headless server...
cmake --build "out\build" --config Release --target fireteam_dedicated --parallel
if errorlevel 1 goto :fail

if not exist "out\build\bin\FireteamDedicatedServer.exe" (
  echo [ERROR] Dedicated binary was not produced.
  goto :fail
)

if not exist "BUILT\Engine.REZ" (
  echo [ERROR] Run build.cmd once to stage the FIRETEAM runtime.
  goto :fail
)

copy /y "out\build\bin\FireteamDedicatedServer.exe" "BUILT\FireteamDedicatedServer.exe" >nul
if errorlevel 1 goto :fail

echo [OK] BUILT\FireteamDedicatedServer.exe
echo [INFO] Experimental binary: verify map load and network joins before public hosting.
exit /b 0

:fail
echo [ERROR] Dedicated build failed. Your regular game build is unaffected.
exit /b 1
