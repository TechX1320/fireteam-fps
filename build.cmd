@echo off
setlocal EnableExtensions
cd /d "%~dp0"

title Fireteam FPS - Local Build

echo.
echo ========================================
echo   Fireteam FPS - Local Build
echo ========================================
echo.

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
set "BUILD_DIR=%CD%\out\build"
set "BIN_DIR=%BUILD_DIR%\bin"
set "BUILT_DIR=%CD%\BUILT"
set "RELEASE_DIR=%CD%\.local\imports\release"
set "SEAL_DIR=%CD%\.local\imports\sealhunter"

echo Build configuration: %CONFIG%
echo Project directory:   %CD%
echo.

echo [CHECK] Looking for SealHunter source...
if not exist "%SEAL_DIR%\cshell\src\ltclientshell.cpp" (
  echo [ERROR] Local source is not prepared. Run setup-local.cmd first.
  goto :fail
)
echo [OK] SealHunter source found.

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
echo [1/5] Configuring Visual Studio 2022 Win32 build...
"%CMAKE_EXE%" -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A Win32
if errorlevel 1 (
  echo [ERROR] CMake configuration failed.
  goto :fail
)

echo.
echo [2/5] Building %CONFIG%...
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config %CONFIG% --parallel
if errorlevel 1 (
  echo [ERROR] Compilation failed.
  goto :fail
)

for %%F in (cshell.dll object.lto cres.dll sres.dll) do (
  if not exist "%BIN_DIR%\%%F" (
    echo [ERROR] Build completed but %%F is missing.
    goto :fail
  )
)

echo.
echo [3/5] Staging runtime...
if exist "%BUILT_DIR%" rmdir /s /q "%BUILT_DIR%"
mkdir "%BUILT_DIR%" >nul
mkdir "%BUILT_DIR%\rez" >nul

for %%F in (Lithtech.exe Engine.REZ LTMsg.dll SndDrv.dll server.dll) do (
  if not exist "%RELEASE_DIR%\%%F" (
    echo [ERROR] Runtime file missing: %RELEASE_DIR%\%%F
    goto :fail
  )
  copy /y "%RELEASE_DIR%\%%F" "%BUILT_DIR%\%%F" >nul
  if errorlevel 1 goto :copyfail
)

xcopy "%SEAL_DIR%\rez" "%BUILT_DIR%\rez\" /E /I /Y /Q >nul
if errorlevel 1 goto :copyfail

copy /y "config\autoexec.cfg" "%BUILT_DIR%\autoexec.cfg" >nul
if errorlevel 1 goto :copyfail
copy /y "config\run-normal.cmd" "%BUILT_DIR%\run-normal.cmd" >nul
if errorlevel 1 goto :copyfail

echo.
echo [4/5] Installing freshly built modules...
copy /y "%BIN_DIR%\cshell.dll" "%BUILT_DIR%\rez\cshell.dll" >nul
if errorlevel 1 goto :copyfail
copy /y "%BIN_DIR%\object.lto" "%BUILT_DIR%\rez\object.lto" >nul
if errorlevel 1 goto :copyfail
copy /y "%BIN_DIR%\cres.dll" "%BUILT_DIR%\rez\cres.dll" >nul
if errorlevel 1 goto :copyfail
copy /y "%BIN_DIR%\sres.dll" "%BUILT_DIR%\rez\sres.dll" >nul
if errorlevel 1 goto :copyfail

echo.
echo [5/5] BUILD COMPLETE
echo Output: %BUILT_DIR%
echo Test: BUILT\run-normal.cmd
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
