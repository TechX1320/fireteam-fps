# Explicit user action only. Never called by any build.
param(
    [ValidateSet("ToRuntime","ToSource","ToDedicated")]
    [string]$Direction = "ToRuntime",
    [ValidateSet("weapons.cfg","weapon-library.cfg","loadouts.cfg","player.cfg","session.cfg")]
    [string[]]$Files = @("weapons.cfg","weapon-library.cfg","loadouts.cfg","player.cfg"),
    [switch]$ConfirmSync
)
$ErrorActionPreference = "Stop"
if(-not $ConfirmSync) {
    Write-Host "[SAFE] No changes. Specify -ConfirmSync to overwrite config."
    Write-Host "Example: -Direction ToRuntime -Files weapons.cfg -ConfirmSync"
    exit 2
}
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$repoConfig = Join-Path $root "config"
$runtime = Join-Path $root "BUILT\config"
$dedicated = Join-Path $root "BUILT\Dedicated\config"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
foreach($name in ($Files | Select-Object -Unique)) {
    switch($Direction) {
        "ToRuntime" { $from = Join-Path $repoConfig $name; $to = Join-Path $runtime $name }
        "ToSource" { $from = Join-Path $runtime $name; $to = Join-Path $repoConfig $name }
        "ToDedicated" { $from = Join-Path $runtime $name; $to = Join-Path $dedicated $name }
    }
    if(-not (Test-Path -LiteralPath $from -PathType Leaf)) {
        throw "Missing source: $from"
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $to) -Force | Out-Null
    if(Test-Path -LiteralPath $to -PathType Leaf) {
        $backup = "$to.$stamp.bak"
        Copy-Item -LiteralPath $to -Destination $backup
        Write-Host "[BACKUP] $backup"
    }
    Copy-Item -LiteralPath $from -Destination $to -Force
    Write-Host "[MANUAL SYNC] $name ($Direction)"
}
Write-Host "Completed explicit config sync. Private stats and profiles were never touched."
