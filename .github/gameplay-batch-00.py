edit('.github/workflows/gameplay-rules.yml', '9cdc42ccaa93954051541f26b06ac7b1dff84b1088756feac4223b1cad762485', [
    (87, 87, r"""      - "launcher/**"
"""),
    (238, 238, r"""      - "launcher/**"
"""),
    (616, 745, r"""      - name: Configure portable tests (no proprietary SDK, native Win32 on Windows)
        shell: pwsh
        run: |
          if ($IsWindows) { cmake -S tests -B out/portable -A Win32 }
          else { cmake -S tests -B out/portable -DCMAKE_BUILD_TYPE=Debug }
          if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
"""),
    (1523, 1523, r"""
  launcher:
    name: Windows launcher and settings
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-dotnet@v4
        with:
          dotnet-version: '10.0.x'
      - name: Test actual settings service without UI
        run: dotnet run --project tests/LauncherSettings.Tests/LauncherSettings.Tests.csproj -c Release
      - name: Publish actual WinUI launcher (no proprietary game SDK)
        run: dotnet publish launcher/FireteamLauncher.WinUI/FireteamLauncher.WinUI.csproj -c Release -r win-x64 -p:Platform=x64 -o out/launcher-ci
      - name: Verify launch resources
        shell: pwsh
        run: |
          foreach ($file in @('FireteamLauncher.exe', 'FireteamLauncher.pri', 'FireteamLauncher.dll')) {
            if (!(Test-Path "out/launcher-ci/$file")) { throw "Missing launcher output: $file" }
          }
"""),
])
edit('build.cmd', 'b59df36970067e651df721e55433c9850517581971e3fab5b79a63847323518c', [
    (4766, 4859, r"""if not exist "%BUILT_DIR%\autoexec.cfg" (
  copy /y "config\autoexec.cfg" "%BUILT_DIR%\autoexec.cfg" >nul
  if errorlevel 1 goto :copyfail
) else (
  echo [KEEP] Local autoexec.cfg unchanged.
)
"""),
    (5127, 5127, r"""powershell -NoProfile -ExecutionPolicy Bypass -File "%CD%\scripts\sync-gameplay-config.ps1" -SourceConfig "%CD%\config" -DestinationConfig "%BUILT_DIR%\config"
if errorlevel 1 goto :fail
"""),
    (5701, 5801, r""""""),
    (6087, 6299, r""""""),
    (6595, 6695, r""""""),
])
edit('config/infected.cfg', '4547e0d5885582d3dee25751f3ce236e90a59a9798294fd32256a3216378c775', [
    (762, 762, r"""# Gameplay test timings, not original CA animation measurements.
# Wind-up = dodge window; recovery = committed pause after contact or miss.
"""),
    (789, 789, r"""attack_recovery_seconds=0.22
"""),
    (2347, 2347, r"""attack_recovery_seconds=0.12
"""),
    (3190, 3190, r"""attack_recovery_seconds=0.65
"""),
])
