@echo off
setlocal EnableExtensions
cd /d "%~dp0"

if not exist "BUILT\Dedicated\FireteamDedicatedServer.exe" (
  echo [ERROR] Dedicated server is not staged.
  echo Run build.cmd and then build-dedicated.cmd.
  exit /b 1
)

cd /d "BUILT\Dedicated"
if "%~1"=="" (
  echo [START] Cabin Fever dedicated host on port 27889 ^(max 24^).
  FireteamDedicatedServer.exe --map CABINFEVER --port 27889 --max-players 24 --name "FIRETEAM Dedicated"
) else (
  echo [START] Custom dedicated host: %*
  FireteamDedicatedServer.exe %*
)
set "RESULT=%ERRORLEVEL%"
echo.
echo Dedicated process exit code: %RESULT%
exit /b %RESULT%
