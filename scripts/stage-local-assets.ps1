param(
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,

    [Parameter(Mandatory = $true)]
    [string]$LocalRoot
)

$ErrorActionPreference = "Stop"

$assetRoot = Join-Path $RepoRoot "assets-local"
$rezRoot = Join-Path $LocalRoot "gameassets\rez"

if (Test-Path -LiteralPath $rezRoot) {
    Remove-Item -LiteralPath $rezRoot -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $rezRoot | Out-Null

function Expand-OptionalZip([string]$ZipName, [string]$RezSubdir) {
    $zipPath = Join-Path $assetRoot $ZipName
    if (-not (Test-Path -LiteralPath $zipPath)) {
        Write-Host "[SKIP] $ZipName not present"
        return
    }

    $dest = Join-Path $rezRoot $RezSubdir
    New-Item -ItemType Directory -Force -Path $dest | Out-Null
    Expand-Archive -LiteralPath $zipPath -DestinationPath $dest -Force
    Write-Host "[OK] $ZipName -> rez\$RezSubdir"
}

$mapPath = Join-Path $assetRoot "CABINFEVER.DAT"
if (Test-Path -LiteralPath $mapPath) {
    $worlds = Join-Path $rezRoot "Worlds"
    New-Item -ItemType Directory -Force -Path $worlds | Out-Null
    Copy-Item -LiteralPath $mapPath -Destination (Join-Path $worlds "CABINFEVER.DAT") -Force
    Write-Host "[OK] CABINFEVER.DAT -> rez\Worlds"
} else {
    Write-Host "[SKIP] CABINFEVER.DAT not present"
}

Expand-OptionalZip "TEXTURES.zip" "Textures"
Expand-OptionalZip "FX.zip" "FX"
Expand-OptionalZip "RS.zip" "RenderStyles"

Write-Host "[OK] Local game asset staging complete."
