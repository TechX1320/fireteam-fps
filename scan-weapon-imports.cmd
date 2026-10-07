@echo off
setlocal EnableExtensions
cd /d "%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\scan-weapon-imports.ps1" -RepoRoot "%CD%"
set "RESULT=%ERRORLEVEL%"

echo.
if not "%RESULT%"=="0" (
  echo [ERROR] Weapon import scan found missing required files.
) else (
  echo [OK] Weapon import scan completed.
)

pause
exit /b %RESULT%
