@echo off
setlocal EnableExtensions
cd /d "%~dp0"

title Fireteam - One-Time Source Migration

set "LOCAL_SEAL=%CD%\.local\imports\sealhunter"
set "SRC=%CD%\src"

echo.
echo ========================================
echo   Fireteam - One-Time Source Migration
echo ========================================
echo.
echo This copies the current Fireteam/SealHunter C++ source out of .local
echo into the Git-tracked src\ tree, then commits and pushes it.
echo This is a ONE-TIME migration. Future development edits src\ directly.
echo.

if not exist "%LOCAL_SEAL%\cshell\src\ltclientshell.cpp" (
  echo [ERROR] Current local Fireteam source was not found.
  echo Expected: %LOCAL_SEAL%\cshell\src\ltclientshell.cpp
  echo Run this from the same checkout you have been building.
  goto :fail
)

if exist "%SRC%\cshell\ltclientshell.cpp" (
  echo [INFO] src\ already contains Fireteam source.
  echo No migration is necessary.
  goto :done
)

git diff --quiet
if errorlevel 1 (
  echo [ERROR] Tracked working-tree changes are present.
  echo Commit or stash them before running this migration.
  goto :fail
)

git diff --cached --quiet
if errorlevel 1 (
  echo [ERROR] Staged changes are present.
  echo Commit or unstage them before running this migration.
  goto :fail
)

echo [1/4] Copying current source into src\...
mkdir "%SRC%\cshell" >nul 2>nul
mkdir "%SRC%\sshell" >nul 2>nul
mkdir "%SRC%\shared" >nul 2>nul
mkdir "%SRC%\cres" >nul 2>nul
mkdir "%SRC%\sres" >nul 2>nul

xcopy "%LOCAL_SEAL%\cshell\src\*" "%SRC%\cshell\" /E /I /Y /Q >nul
if errorlevel 1 goto :copyfail
xcopy "%LOCAL_SEAL%\sshell\src\*" "%SRC%\sshell\" /E /I /Y /Q >nul
if errorlevel 1 goto :copyfail
xcopy "%LOCAL_SEAL%\shared\src\*" "%SRC%\shared\" /E /I /Y /Q >nul
if errorlevel 1 goto :copyfail
xcopy "%LOCAL_SEAL%\cres\src\*" "%SRC%\cres\" /E /I /Y /Q >nul
if errorlevel 1 goto :copyfail
xcopy "%LOCAL_SEAL%\sres\src\*" "%SRC%\sres\" /E /I /Y /Q >nul
if errorlevel 1 goto :copyfail

rem Diagnostics originally generated this header under the local Jupiter SDK.
rem Promote it into committed shared source so clean future setups do not depend
rem on a generated header.
if exist "%CD%\.local\imports\engine\sdk\inc\fireteamtrace.h" (
  copy /y "%CD%\.local\imports\engine\sdk\inc\fireteamtrace.h" "%SRC%\shared\fireteamtrace.h" >nul
  if errorlevel 1 goto :copyfail
) else (
  echo [ERROR] fireteamtrace.h was not found in the current local SDK.
  echo Run the last working setup/build once or attach the missing header.
  goto :fail
)

del /q "%SRC%\cshell\*.vcproj" >nul 2>nul
del /q "%SRC%\sshell\*.vcproj" >nul 2>nul
del /q "%SRC%\cres\*.vcproj" >nul 2>nul
del /q "%SRC%\sres\*.vcproj" >nul 2>nul

echo [2/4] Removing obsolete game-source patch scripts from the tracked project...
for %%F in (
  patch-diagnostics-source.ps1
  patch-diagnostics-v2.ps1
  patch-fireteam-combat-hud.ps1
  patch-fireteam-gameplay.ps1
  patch-fireteam-loadout.ps1
  patch-fireteam-ui.ps1
  patch-game-source.ps1
  patch-legacy-source.ps1
  patch-systems-source.ps1
  patch-weapon-source.ps1
) do (
  if exist "scripts\%%F" git rm -q "scripts\%%F"
)

echo [3/4] Staging canonical Fireteam source...
git add src
if errorlevel 1 goto :fail

git status --short

echo.
echo [4/4] Committing and pushing source migration...
git commit -m "source: import canonical Fireteam game code"
if errorlevel 1 goto :fail

git push origin main
if errorlevel 1 (
  echo.
  echo [ERROR] Source was committed locally, but git push failed.
  echo Fix the Git authentication/network issue and run:
  echo   git push origin main
  goto :fail
)

echo.
echo [OK] Canonical Fireteam source is now on GitHub under src\.
echo [OK] build.cmd no longer patches game source in .local.
echo.

:done
pause
exit /b 0

:copyfail
echo [ERROR] Failed while copying local source.

:fail
echo.
echo ========================================
echo   SOURCE MIGRATION FAILED
echo ========================================
echo.
pause
exit /b 1
