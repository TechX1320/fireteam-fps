param([string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path)
$ErrorActionPreference = 'Stop'
$reports = @(
    (Join-Path $RepoRoot 'BUILT/config/infected-animation-valid.txt'),
    (Join-Path $RepoRoot 'assets-local/Reports/ST_M_CHILD-strings.txt'),
    (Join-Path $RepoRoot 'assets-local/Reports/ANI_VI_ASSASSIN_CH-strings.txt'),
    (Join-Path $RepoRoot 'assets-local/Reports/ANI_VI_TANKER_SH-strings.txt')
)
foreach($report in $reports) {
    if(!(Test-Path -LiteralPath $report)) {
        Write-Host "[MISSING] $report"; continue
    }
    $names = @(Get-Content -LiteralPath $report | Where-Object {
        $_ -match '^(?i:[A-Z][A-Z0-9_]{1,62})$' -and
        $_ -match '(?i:ATTACK|ATK|MELEE|^UMFI$)'
    } | Select-Object -Unique)
    Write-Host "[REPORT] $report : $($names.Count) possible attack-name(s)"
    if($names.Count) { $names | ForEach-Object { Write-Host "  $_" } }
}
Write-Host 'Raw model strings are NOT proof of playable animation; use in-game GetAnimIndex audit.'
