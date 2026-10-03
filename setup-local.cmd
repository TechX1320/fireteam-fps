@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "IMPORTS=%CD%\imports"
set "LOCAL=%CD%\.local"

for %%F in ("release.zip" "sealhunter.zip" "EngineMissing.zip") do (
  if not exist "%IMPORTS%\%%~F" (
    echo [ERROR] Missing imports\%%~F
    exit /b 1
  )
)

echo [1/3] Resetting local dependency cache...
if exist "%LOCAL%" rmdir /s /q "%LOCAL%"
mkdir "%LOCAL%\imports\release" >nul
mkdir "%LOCAL%\imports\sealhunter" >nul
mkdir "%LOCAL%\imports\engine" >nul

echo [2/3] Extracting original Jupiter files...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -LiteralPath '%IMPORTS%\release.zip' -DestinationPath '%LOCAL%\imports\release' -Force; Expand-Archive -LiteralPath '%IMPORTS%\sealhunter.zip' -DestinationPath '%LOCAL%\imports\sealhunter' -Force; Expand-Archive -LiteralPath '%IMPORTS%\EngineMissing.zip' -DestinationPath '%LOCAL%\imports\engine' -Force"
if errorlevel 1 exit /b 1

if not exist "%LOCAL%\imports\release\Lithtech.exe" (
  echo [ERROR] release.zip layout was not recognized.
  exit /b 1
)
if not exist "%LOCAL%\imports\sealhunter\cshell\src\ltclientshell.cpp" (
  echo [ERROR] sealhunter.zip layout was not recognized.
  exit /b 1
)
if not exist "%LOCAL%\imports\engine\sdk\inc\iltclient.h" (
  echo [ERROR] EngineMissing.zip layout was not recognized.
  exit /b 1
)

echo [3/3] Local source/runtime is ready.
echo Run build.cmd next.
exit /b 0
