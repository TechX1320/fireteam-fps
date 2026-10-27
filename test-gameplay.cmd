@echo off
setlocal EnableExtensions
cd /d "%~dp0"
where cmake >nul 2>nul
if errorlevel 1 (
  echo Use a Visual Studio 2022 Developer Command Prompt with CMake installed.
  exit /b 1
)
cmake -S tests -B out\portable-win32 -G "Visual Studio 17 2022" -A Win32
if errorlevel 1 exit /b 1
cmake --build out\portable-win32 --config Debug
if errorlevel 1 exit /b 1
ctest --test-dir out\portable-win32 -C Debug --output-on-failure
if errorlevel 1 exit /b 1
where dotnet >nul 2>nul
if errorlevel 1 (
  echo [SKIP] .NET settings tests: dotnet not installed.
  exit /b 0
)
dotnet run --project tests\LauncherSettings.Tests\LauncherSettings.Tests.csproj -c Release
exit /b %errorlevel%
