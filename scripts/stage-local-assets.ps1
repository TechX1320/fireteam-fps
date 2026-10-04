param(
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,

    [Parameter(Mandatory = $true)]
    [string]$LocalRoot
)

$ErrorActionPreference = "Stop"

$assetRoot = Join-Path $RepoRoot "assets-local"
$rezRoot = Join-Path $LocalRoot "gameassets\rez"
$stampRoot = Join-Path $LocalRoot "gameassets\stamps"

New-Item -ItemType Directory -Force -Path $rezRoot | Out-Null
New-Item -ItemType Directory -Force -Path $stampRoot | Out-Null

function Expand-OptionalZip([string]$ZipName, [string]$RezSubdir) {
    $zipPath = Join-Path $assetRoot $ZipName
    if (-not (Test-Path -LiteralPath $zipPath)) {
        Write-Host "[SKIP] $ZipName not present"
        return
    }

    $safeName = $ZipName.Replace(".", "_")
    $stampPath = Join-Path $stampRoot ($safeName + ".stamp")
    $zipInfo = Get-Item -LiteralPath $zipPath

    if (Test-Path -LiteralPath $stampPath) {
        $stampInfo = Get-Item -LiteralPath $stampPath
        if ($stampInfo.LastWriteTimeUtc -ge $zipInfo.LastWriteTimeUtc) {
            Write-Host "[OK] $ZipName unchanged"
            return
        }
    }

    $dest = Join-Path $rezRoot $RezSubdir
    New-Item -ItemType Directory -Force -Path $dest | Out-Null

    Write-Host "[UPDATE] Expanding $ZipName..."
    Expand-Archive -LiteralPath $zipPath -DestinationPath $dest -Force
    Set-Content -LiteralPath $stampPath -Value $zipInfo.LastWriteTimeUtc.Ticks
    (Get-Item -LiteralPath $stampPath).LastWriteTimeUtc = $zipInfo.LastWriteTimeUtc
    Write-Host "[OK] $ZipName -> rez\$RezSubdir"
}

$mapPath = Join-Path $assetRoot "CABINFEVER.DAT"
if (Test-Path -LiteralPath $mapPath) {
    $worlds = Join-Path $rezRoot "Worlds"
    $destMap = Join-Path $worlds "CABINFEVER.DAT"
    New-Item -ItemType Directory -Force -Path $worlds | Out-Null

    if ((-not (Test-Path -LiteralPath $destMap)) -or
        ((Get-Item -LiteralPath $destMap).LastWriteTimeUtc -lt (Get-Item -LiteralPath $mapPath).LastWriteTimeUtc)) {
        Copy-Item -LiteralPath $mapPath -Destination $destMap -Force
        Write-Host "[OK] CABINFEVER.DAT refreshed"
    } else {
        Write-Host "[OK] CABINFEVER.DAT unchanged"
    }
} else {
    Write-Host "[SKIP] CABINFEVER.DAT not present"
}

Expand-OptionalZip "TEXTURES.zip" "Textures"
Expand-OptionalZip "FX.zip" "FX"
Expand-OptionalZip "RS.zip" "RenderStyles"

Write-Host "[OK] Local game asset staging complete."
