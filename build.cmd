@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
set "BUILD_DIR=%CD%\out\build"
set "BIN_DIR=%BUILD_DIR%\bin"
set "BUILT_DIR=%CD%\BUILT"
set "RELEASE_DIR=%CD%\.local\imports\release"
set "SEAL_DIR=%CD%\.local\imports\sealhunter"

if not exist "%SEAL_DIR%\cshell\src\ltclientshell.cpp" (
  echo [ERROR] Local source is not prepared. Run setup-local.cmd first.
  exit /b 1
)

where cmake >nul 2>nul
if errorlevel 1 (
  echo [ERROR] CMake was not found.
  echo Install Visual Studio 2022 Desktop development with C++ including CMake tools.
  exit /b 1
)

echo [1/5] Configuring Visual Studio 2022 Win32 build...
cmake -S . -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A Win32
if errorlevel 1 exit /b 1

echo [2/5] Building %CONFIG%...
cmake --build "%BUILD_DIR%" --config %CONFIG% --parallel
if errorlevel 1 exit /b 1

for %%F in (cshell.dll object.lto cres.dll sres.dll) do (
  if not exist "%BIN_DIR%\%%F" (
    echo [ERROR] Build completed but %%F is missing.
    exit /b 1
  )
)

echo [3/5] Staging runtime...
if exist "%BUILT_DIR%" rmdir /s /q "%BUILT_DIR%"
mkdir "%BUILT_DIR%" >nul
mkdir "%BUILT_DIR%\rez" >nul
for %%F in (Lithtech.exe Engine.REZ LTMsg.dll SndDrv.dll server.dll) do copy /y "%RELEASE_DIR%\%%F" "%BUILT_DIR%\%%F" >nul
xcopy "%SEAL_DIR%\rez" "%BUILT_DIR%\rez\" /E /I /Y /Q >nul
copy /y "config\autoexec.cfg" "%BUILT_DIR%\autoexec.cfg" >nul
copy /y "config\run-normal.cmd" "%BUILT_DIR%\run-normal.cmd" >nul

echo [4/5] Installing freshly built modules...
copy /y "%BIN_DIR%\cshell.dll" "%BUILT_DIR%\rez\cshell.dll" >nul
copy /y "%BIN_DIR%\object.lto" "%BUILT_DIR%\rez\object.lto" >nul
copy /y "%BIN_DIR%\cres.dll" "%BUILT_DIR%\rez\cres.dll" >nul
copy /y "%BIN_DIR%\sres.dll" "%BUILT_DIR%\rez\sres.dll" >nul

echo [5/5] Done.
echo Output: %BUILT_DIR%
echo Test: BUILT\run-normal.cmd
exit /b 0
