@echo off
setlocal EnableExtensions
cd /d "%~dp0"

title Fireteam - Local Build

echo.
echo ========================================
echo   Fireteam - Local Build
echo ========================================
echo.

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
set "BUILD_SCOPE=%~2"
if "%BUILD_SCOPE%"=="" set "BUILD_SCOPE=All"
set "BUILD_DIR=%CD%\out\build"
set "BIN_DIR=%BUILD_DIR%\bin"
set "BUILT_DIR=%CD%\BUILT"
set "RELEASE_DIR=%CD%\.local\imports\release"
set "SEAL_DIR=%CD%\.local\imports\sealhunter"
set "SOURCE_DIR=%CD%\src"
set "ASSET_REZ=%CD%\.local\gameassets\rez"

echo Build configuration: %CONFIG%
echo Project directory:   %CD%
echo.

echo [CHECK] Looking for committed Fireteam source...
if not exist "%SOURCE_DIR%\cshell\ltclientshell.cpp" (
  echo [ERROR] Committed Fireteam source is not present yet.
  echo Run migrate-source.cmd once, then run build.cmd again.
  goto :fail
)
echo [OK] Fireteam source found in src\.

echo.
echo [UPDATE] Checking expanded Jupiter engine source...
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\refresh-engine-import.ps1" -RepoRoot "%CD%" -LocalRoot "%CD%\.local"
if errorlevel 1 goto :fail

echo.
echo [UPDATE] Applying local Jupiter engine compatibility...
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\patch-engine-compat.ps1" -LocalRoot "%CD%\.local"
if errorlevel 1 goto :fail

echo.
echo [UPDATE] Refreshing local optional game assets...
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\stage-local-assets.ps1" -RepoRoot "%CD%" -LocalRoot "%CD%\.local"
if errorlevel 1 goto :fail
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\stage-weapon-assets.ps1" -RepoRoot "%CD%" -LocalRoot "%CD%\.local"
if errorlevel 1 goto :fail

echo.
echo [CHECK] Looking for CMake...
set "CMAKE_EXE="

where cmake >nul 2>nul
if not errorlevel 1 set "CMAKE_EXE=cmake"

if not defined CMAKE_EXE (
  for %%E in (
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    "C:\Program Files\CMake\bin\cmake.exe"
  ) do (
    if exist %%E set "CMAKE_EXE=%%~E"
  )
)

if not defined CMAKE_EXE (
  echo [ERROR] CMake could not be found.
  echo Install "C++ CMake tools for Windows" through Visual Studio Installer.
  goto :fail
)

echo [OK] CMake found:
echo %CMAKE_EXE%

echo.
echo [CHECK] Verifying CMake cache matches this folder...
if exist "%BUILD_DIR%\CMakeCache.txt" (
  powershell -NoProfile -ExecutionPolicy Bypass -Command "$cache = Get-Content -LiteralPath '%BUILD_DIR%\CMakeCache.txt' -ErrorAction SilentlyContinue | Where-Object { $_ -like 'CMAKE_HOME_DIRECTORY:INTERNAL=*' } | Select-Object -First 1; if ($cache) { $old = $cache.Substring($cache.IndexOf('=') + 1); $now = (Resolve-Path -LiteralPath '%CD%').Path; if ([System.IO.Path]::GetFullPath($old).TrimEnd('\') -ne [System.IO.Path]::GetFullPath($now).TrimEnd('\')) { exit 10 } }"
  if errorlevel 10 (
    echo [INFO] Project folder moved or renamed. Resetting generated CMake files only...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
  )
)

echo.
echo [1/6] Configuring Visual Studio 2022 Win32 build...
"%CMAKE_EXE%" -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A Win32
if errorlevel 1 (
  echo [ERROR] CMake configuration failed.
  goto :fail
)

echo.
echo [2/6] Building %CONFIG%...
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config %CONFIG% --parallel
if errorlevel 1 (
  echo [ERROR] Compilation failed.
  goto :fail
)

for %%F in (cshell.dll object.lto cres.dll sres.dll ClientFx.fxd) do (
  if not exist "%BIN_DIR%\%%F" (
    echo [ERROR] Build completed but %%F is missing.
    goto :fail
  )
)

echo.
echo [3/6] Staging runtime...
if not exist "%BUILT_DIR%" mkdir "%BUILT_DIR%" >nul
if not exist "%BUILT_DIR%\rez" mkdir "%BUILT_DIR%\rez" >nul

if exist "%BUILT_DIR%\rez\ClientFx.fxd" (
  echo [CLEAN] Removing stale Combat Arms ClientFx.fxd override...
  del /q "%BUILT_DIR%\rez\ClientFx.fxd"
)

for %%F in (Lithtech.exe Engine.REZ LTMsg.dll SndDrv.dll server.dll) do (
  if not exist "%RELEASE_DIR%\%%F" (
    echo [ERROR] Runtime file missing: %RELEASE_DIR%\%%F
    goto :fail
  )
  copy /y "%RELEASE_DIR%\%%F" "%BUILT_DIR%\%%F" >nul
  if errorlevel 1 goto :copyfail
)

xcopy "%SEAL_DIR%\rez" "%BUILT_DIR%\rez\" /D /E /I /Y /Q >nul
if errorlevel 1 goto :copyfail

if exist "%ASSET_REZ%" (
  echo Staging local Cabin Fever assets...
  xcopy "%ASSET_REZ%" "%BUILT_DIR%\rez\" /D /E /I /Y /Q >nul
  if errorlevel 1 goto :copyfail
)

copy /y "config\autoexec.cfg" "%BUILT_DIR%\autoexec.cfg" >nul
if errorlevel 1 goto :copyfail
copy /y "config\run-normal.cmd" "%BUILT_DIR%\run-normal.cmd" >nul
if errorlevel 1 goto :copyfail
copy /y "config\run-cabinfever.cmd" "%BUILT_DIR%\run-cabinfever.cmd" >nul
if errorlevel 1 goto :copyfail
if not exist "%BUILT_DIR%\config" mkdir "%BUILT_DIR%\config" >nul
copy /y "config\weapons.cfg" "%BUILT_DIR%\config\weapons.cfg" >nul
if errorlevel 1 goto :copyfail
copy /y "config\infected.cfg" "%BUILT_DIR%\config\infected.cfg" >nul
if errorlevel 1 goto :copyfail
copy /y "config\player.cfg" "%BUILT_DIR%\config\player.cfg" >nul
if errorlevel 1 goto :copyfail
copy /y "config\difficulties.cfg" "%BUILT_DIR%\config\difficulties.cfg" >nul
if errorlevel 1 goto :copyfail
copy /y "config\session.cfg" "%BUILT_DIR%\config\session.cfg" >nul
if errorlevel 1 goto :copyfail

echo.
echo [4/6] Installing freshly built modules...
copy /y "%BIN_DIR%\cshell.dll" "%BUILT_DIR%\rez\cshell.dll" >nul
if errorlevel 1 goto :copyfail
copy /y "%BIN_DIR%\object.lto" "%BUILT_DIR%\rez\object.lto" >nul
if errorlevel 1 goto :copyfail
copy /y "%BIN_DIR%\cres.dll" "%BUILT_DIR%\rez\cres.dll" >nul
if errorlevel 1 goto :copyfail
copy /y "%BIN_DIR%\sres.dll" "%BUILT_DIR%\rez\sres.dll" >nul
if errorlevel 1 goto :copyfail
copy /y "%BIN_DIR%\ClientFx.fxd" "%BUILT_DIR%\rez\ClientFx.fxd" >nul
if errorlevel 1 goto :copyfail

echo.
if /I "%BUILD_SCOPE%"=="GameOnly" (
  echo [5/6] Launcher build skipped ^(GameOnly^).
) else (
  echo [5/6] Building FIRETEAM Launcher...
  call "%CD%\build-launcher.cmd" nopause
  if errorlevel 1 (
    echo [ERROR] Game compiled, but launcher build failed.
    goto :fail
  )
)

echo.
echo [6/6] BUILD COMPLETE
echo Output: %BUILT_DIR%
echo Existing extra files in BUILT were preserved.
echo Launcher: BUILT\Launcher\FireteamLauncher.exe
echo Stock test: BUILT\run-normal.cmd
echo Cabin Fever test: BUILT\run-cabinfever.cmd
echo.
pause
exit /b 0

:copyfail
echo [ERROR] A file copy operation failed.

:fail
echo.
echo ========================================
echo   BUILD FAILED
echo ========================================
echo.
pause
exit /b 1
