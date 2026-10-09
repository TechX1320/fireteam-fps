@echo off
setlocal EnableExtensions
cd /d "%~dp0"
title FIRETEAM Hub - Windows x64 publish
where dotnet >nul 2>nul
if errorlevel 1 (
  echo [ERROR] .NET 10 SDK missing.
  exit /b 1
)
echo [HUB] Publishing FIRETEAM Hub self-contained for Windows x64...
dotnet publish "hub\FireteamHub\FireteamHub.csproj" -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:PublishTrimmed=false -o "out\hub\win-x64"
if errorlevel 1 exit /b 1
if not exist "out\hub\win-x64\FireteamHub.exe" (
  echo [ERROR] FIRETEAM Hub executable missing.
  exit /b 1
)
echo [OK] out\hub\win-x64\FireteamHub.exe
echo [NOTE] Default listener is localhost only. Read docs\HUB_DEPLOYMENT.md.
exit /b 0
