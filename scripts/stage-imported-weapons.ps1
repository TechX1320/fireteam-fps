param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

$source = Join-Path $RepoRoot "assets-local\WeaponImports\Weapons\ca"
$built = Join-Path $RepoRoot "BUILT"
$dest = Join-Path $built "rez\Weapons\ca"

if(-not (Test-Path -LiteralPath $source)) {
    Write-Host "[ERROR] No imported weapon asset tree was found:"
    Write-Host "        $source"
    Write-Host "Run the Weapon Editor Combat Arms importer first."
    exit 1
}

if(-not (Test-Path -LiteralPath (Join-Path $built "Lithtech.exe"))) {
    Write-Host "[ERROR] BUILT runtime is missing."
    Write-Host "Run build.cmd once before staging imported assets."
    exit 1
}

New-Item -ItemType Directory -Force -Path $dest | Out-Null

$sourceFiles =
    @(Get-ChildItem -LiteralPath $source -File -Recurse)

if($sourceFiles.Count -eq 0) {
    Write-Host "[ERROR] The importer produced zero Combat Arms asset files."
    Write-Host "        Do not continue with a zero-file stage."
    Write-Host "        Review: assets-local\WeaponImports\last-import.txt"
    Write-Host "        Then rerun Tools -> Combat Arms Importer."
    exit 2
}

Write-Host "[WEAPON] Staging $($sourceFiles.Count) imported Combat Arms asset files..."
Copy-Item -Path (Join-Path $source "*") -Destination $dest -Recurse -Force

# Never overwrite local weapon definitions while staging visual assets.
# Import/Editor writes to BUILT/config already; promotion of tracked source
# templates must use scripts/sync-local-config.ps1 with -ConfirmSync.
Write-Host "[KEEP] BUILT\config\weapons.cfg was not touched."

$destFiles =
    @(Get-ChildItem -LiteralPath $dest -File -Recurse)

Write-Host "[OK] Imported weapon assets staged."
Write-Host "     Source files:  $($sourceFiles.Count)"
Write-Host "     Runtime files: $($destFiles.Count)"
Write-Host "     Runtime path:  $dest"
Write-Host ""
Write-Host "Next: run scan-weapon-imports.cmd to validate config references."
