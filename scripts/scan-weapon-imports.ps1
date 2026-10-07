param(
    [Parameter(Mandatory=$true)]
    [string]$RepoRoot
)

$ErrorActionPreference = "Stop"

$cfg = Join-Path $RepoRoot "config\weapons.cfg"
$sourceRez = Join-Path $RepoRoot "assets-local\WeaponImports"
$runtimeRez = Join-Path $RepoRoot "BUILT\rez"

if(-not (Test-Path -LiteralPath $cfg)) {
    Write-Host "[ERROR] Missing config\weapons.cfg"
    exit 1
}

$sections = @{}
$current = $null

foreach($raw in Get-Content -LiteralPath $cfg) {
    $line = $raw.Trim()

    if($line -match '^\[([^\]]+)\]$') {
        $current = $Matches[1]
        if(-not $sections.ContainsKey($current)) {
            $sections[$current] = @{}
        }
        continue
    }

    if(-not $current -or
       -not $line -or
       $line.StartsWith('#') -or
       $line.StartsWith(';') -or
       $line -notmatch '^([^=]+)=(.*)$') {
        continue
    }

    $sections[$current][$Matches[1].Trim()] =
        $Matches[2].Trim()
}

$failures = 0
$warnings = 0
$checked = 0

function Test-Asset {
    param(
        [string]$Weapon,
        [string]$Label,
        [string]$Relative,
        [bool]$Required,
        [bool]$RuntimeRequired
    )

    if([string]::IsNullOrWhiteSpace($Relative)) {
        if($Required) {
            Write-Host "[MISSING] $Weapon $Label is not configured."
            $script:failures++
        }
        return
    }

    $clean = $Relative.Replace('/', '\')
    $source = Join-Path $sourceRez $clean

    if(Test-Path -LiteralPath $source) {
        Write-Host "[OK] $Weapon $Label source -> $Relative"
    } elseif($Required) {
        Write-Host "[MISSING] $Weapon $Label source -> $Relative"
        $script:failures++
    } else {
        Write-Host "[WARN] $Weapon optional $Label source -> $Relative"
        $script:warnings++
    }

    if($RuntimeRequired) {
        $runtime = Join-Path $runtimeRez $clean

        if(Test-Path -LiteralPath $runtime) {
            Write-Host "[OK] $Weapon $Label runtime -> $Relative"
        } else {
            Write-Host "[MISSING] $Weapon enabled runtime $Label -> $Relative"
            $script:failures++
        }
    }
}

foreach($sectionName in $sections.Keys | Sort-Object) {
    if($sectionName -notlike 'catalog.*') {
        continue
    }

    $section = $sections[$sectionName]

    if($section['source'] -ne 'COMBAT ARMS AUTO IMPORT') {
        continue
    }

    $checked++
    $name = $section['name']
    if(-not $name) { $name = $sectionName }

    $enabled =
        $section['enabled'] -match '^(1|true|yes|on)$'

    Write-Host ""
    Write-Host "[WEAPON] $name enabled=$enabled supported=$($section['supported'])"

    Test-Asset $name "PV model" $section['pv_model'] $true $enabled
    Test-Asset $name "PV texture" $section['pv_texture'] $true $enabled
    Test-Asset $name "PV animation" $section['pv_anim'] $false $enabled
    Test-Asset $name "HH model" $section['hh_model'] $false $enabled
    Test-Asset $name "HH texture" $section['hh_texture'] $false $enabled

    $soundDir = $section['sound_dir']
    if($soundDir) {
        Test-Asset $name "fire sound" ($soundDir.TrimEnd('/','\') + '\FIRE.WAV') $true $enabled
        Test-Asset $name "select sound" ($soundDir.TrimEnd('/','\') + '\SELECT.WAV') $false $enabled
        Test-Asset $name "reload sound" ($soundDir.TrimEnd('/','\') + '\RELOAD.WAV') $false $enabled
    } else {
        Write-Host "[WARN] $name has no sound_dir."
        $warnings++
    }

    if($section['ca_timing_verified'] -ne '1') {
        Write-Host "[INFO] $name uses FIRETEAM DEV timing defaults; gameplay timing still needs verification."
    }
}

Write-Host ""
Write-Host "========================================"
Write-Host "  FIRETEAM WEAPON IMPORT SCAN"
Write-Host "========================================"
Write-Host "Imported definitions checked: $checked"
Write-Host "Warnings: $warnings"
Write-Host "Failures: $failures"

if($checked -eq 0) {
    Write-Host "[INFO] No Combat Arms auto-import catalog entries found."
}

if($failures -gt 0) {
    exit 1
}

exit 0
