@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "IMPORTS=%CD%\imports"
set "LOCAL=%CD%\.local"
set "RESET=%~1"

if exist "%LOCAL%\imports\sealhunter\cshell\src\ltclientshell.cpp" if /I not "%RESET%"=="--reset" (
  echo Existing local Jupiter dependency workspace found.
  echo Keeping it in place and refreshing engine compatibility/assets only.
  goto :dependencies
)

for %%F in ("release.zip" "sealhunter.zip" "EngineMissing.zip") do (
  if not exist "%IMPORTS%\%%~F" (
    echo [ERROR] Missing imports\%%~F
    pause
    exit /b 1
  )
)

echo [1/4] Preparing local dependency cache...
if exist "%LOCAL%" rmdir /s /q "%LOCAL%"
mkdir "%LOCAL%\imports\release" >nul
mkdir "%LOCAL%\imports\sealhunter" >nul
mkdir "%LOCAL%\imports\engine" >nul

echo [2/4] Extracting original Jupiter files...
powershell -NoProfile -ExecutionPolicy Bypass -Command "Expand-Archive -LiteralPath '%IMPORTS%\release.zip' -DestinationPath '%LOCAL%\imports\release' -Force; Expand-Archive -LiteralPath '%IMPORTS%\sealhunter.zip' -DestinationPath '%LOCAL%\imports\sealhunter' -Force; Expand-Archive -LiteralPath '%IMPORTS%\EngineMissing.zip' -DestinationPath '%LOCAL%\imports\engine' -Force"
if errorlevel 1 goto :fail

if not exist "%LOCAL%\imports\release\Lithtech.exe" (
  echo [ERROR] release.zip layout was not recognized.
  goto :fail
)
if not exist "%LOCAL%\imports\sealhunter\cshell\src\ltclientshell.cpp" (
  echo [ERROR] sealhunter.zip layout was not recognized.
  goto :fail
)
if not exist "%LOCAL%\imports\engine\sdk\inc\iltclient.h" (
  echo [ERROR] EngineMissing.zip layout was not recognized.
  goto :fail
)

:dependencies
echo [3/4] Checking expanded Jupiter engine source...
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\refresh-engine-import.ps1" -RepoRoot "%CD%" -LocalRoot "%LOCAL%"
if errorlevel 1 goto :fail

echo [3/4] Applying local Jupiter engine compatibility...
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\patch-engine-compat.ps1" -LocalRoot "%LOCAL%"
if errorlevel 1 goto :fail

echo [4/4] Refreshing optional local assets...
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\stage-local-assets.ps1" -RepoRoot "%CD%" -LocalRoot "%LOCAL%"
if errorlevel 1 goto :fail
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\stage-weapon-assets.ps1" -RepoRoot "%CD%" -LocalRoot "%LOCAL%"
if errorlevel 1 goto :fail

echo.
echo Local dependency workspace is ready.
echo Fireteam game source is committed under src\ and is not generated here.
echo Future changes normally only require git pull --ff-only and build.cmd.
echo Use setup-local.cmd --reset only if you want a clean dependency re-extraction.
echo.
pause
exit /b 0

:fail
echo.
echo [ERROR] Local setup failed.
echo.
pause
exit /b 1
