@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "IMPORTS=%CD%\imports"
set "LOCAL=%CD%\.local"
set "RESET=%~1"

if exist "%LOCAL%\imports\sealhunter\cshell\src\ltclientshell.cpp" if /I not "%RESET%"=="--reset" (
  echo Existing local Jupiter workspace found.
  echo Keeping it in place and refreshing patches/assets only.
  goto :patch
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

:patch
echo [3/4] Applying current Fireteam FPS patches...
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\patch-legacy-source.ps1" -LocalRoot "%LOCAL%"
if errorlevel 1 goto :fail
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\patch-game-source.ps1" -LocalRoot "%LOCAL%"
if errorlevel 1 goto :fail

echo [4/4] Refreshing optional local assets...
powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\stage-local-assets.ps1" -RepoRoot "%CD%" -LocalRoot "%LOCAL%"
if errorlevel 1 goto :fail

echo.
echo Local workspace is ready.
echo Future changes normally only require build.cmd.
echo Use setup-local.cmd --reset only if you want a clean re-extraction.
echo.
pause
exit /b 0

:fail
echo.
echo [ERROR] Local setup failed.
echo.
pause
exit /b 1
