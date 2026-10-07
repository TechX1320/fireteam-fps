@echo off
setlocal EnableExtensions
cd /d "%~dp0"

echo.
echo ========================================
echo   FIRETEAM - Stage Imported Weapons
echo ========================================
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\stage-imported-weapons.ps1" -RepoRoot "%CD%"
set "RESULT=%ERRORLEVEL%"

echo.
if not "%RESULT%"=="0" (
  echo [ERROR] Imported weapon asset staging failed.
) else (
  echo [OK] Imported weapon asset staging complete.
)

pause
exit /b %RESULT%
