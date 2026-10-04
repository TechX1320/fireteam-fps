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


function Extract-ZipEntry([string]$ZipName, [string]$EntryName, [string]$DestinationRelative) {
    $zipPath = Join-Path $assetRoot $ZipName
    if (-not (Test-Path -LiteralPath $zipPath)) {
        return $false
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($zipPath)
    try {
        $entry = $zip.Entries | Where-Object { $_.FullName -ieq $EntryName } | Select-Object -First 1
        if (-not $entry) {
            throw "Missing $EntryName inside $ZipName"
        }

        $dest = Join-Path $rezRoot $DestinationRelative
        $destDir = Split-Path -Parent $dest
        New-Item -ItemType Directory -Force -Path $destDir | Out-Null

        if ((-not (Test-Path -LiteralPath $dest)) -or
            ((Get-Item -LiteralPath $dest).LastWriteTimeUtc -lt (Get-Item -LiteralPath $zipPath).LastWriteTimeUtc)) {
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dest, $true)
            Write-Host "[OK] $ZipName::$EntryName"
        } else {
            Write-Host "[OK] Bowie asset unchanged: $DestinationRelative"
        }
    }
    finally {
        $zip.Dispose()
    }

    return $true
}

$gunsZip = Join-Path $assetRoot "Guns.zip"
$gunsHHZip = Join-Path $assetRoot "GunsHH.zip"

if (Test-Path -LiteralPath $gunsZip) {
    Write-Host "[UPDATE] Staging Combat Arms Bowie knife player-view assets..."
    Extract-ZipEntry "Guns.zip" "GUNS_M_PV_MELEE/CM_HND_NM_DF_BOWIEKNIFE_CH.LTB" "Models\Weapons\Bowie\CM_HND_NM_DF_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_M_PV_MELEE/ANI_G_BOWIEKNIFE_CH.LTB" "Models\Weapons\Bowie\ANI_G_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_T_PV_MELEE/PV_ML_DF_BOWIEKNIFE_BC.DTX" "ModelTextures\Weapons\Bowie\PV_ML_DF_BOWIEKNIFE_BC.DTX" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_SND_MELEE/BOWIE_KNIFE/FIRE.WAV" "Sounds\Weapons\Bowie\FIRE.WAV" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_SND_MELEE/BOWIE_KNIFE/SELECT.WAV" "Sounds\Weapons\Bowie\SELECT.WAV" | Out-Null
} else {
    Write-Host "[SKIP] Guns.zip not present - Bowie player-view model will be unavailable"
}

if (Test-Path -LiteralPath $gunsHHZip) {
    Write-Host "[UPDATE] Staging Combat Arms Bowie knife world assets..."
    Extract-ZipEntry "GunsHH.zip" "GUNS_M_HH/HH_ML_DF_BOWIEKNIFE_CH.LTB" "Models\Weapons\Bowie\HH_ML_DF_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "GunsHH.zip" "GUNS_T_HH/HH_ML_DF_BOWIEKNIFE_BC.DTX" "ModelTextures\Weapons\Bowie\HH_ML_DF_BOWIEKNIFE_BC.DTX" | Out-Null
} else {
    Write-Host "[SKIP] GunsHH.zip not present - Bowie world model will be unavailable"
}


$bowieSoundZip = Join-Path $assetRoot "BOWIE_KNIFE.zip"
if (Test-Path -LiteralPath $bowieSoundZip) {
    Write-Host "[UPDATE] Staging dedicated Bowie sound archive..."
    Extract-ZipEntry "BOWIE_KNIFE.zip" "BOWIE_KNIFE/FIRE.WAV" "Sounds\Weapons\Bowie\FIRE.WAV" | Out-Null
    Extract-ZipEntry "BOWIE_KNIFE.zip" "BOWIE_KNIFE/SELECT.WAV" "Sounds\Weapons\Bowie\SELECT.WAV" | Out-Null
}

# Combat Arms LTBs also carry bare texture names. Keep aliases at the resource
# root and ModelTextures root in addition to our organized Bowie directory.
$pvTex = Join-Path $rezRoot "ModelTextures\Weapons\Bowie\PV_ML_DF_BOWIEKNIFE_BC.DTX"
if (Test-Path -LiteralPath $pvTex) {
    Copy-Item -LiteralPath $pvTex -Destination (Join-Path $rezRoot "PV_ML_DF_BowieKnife_BC.dtx") -Force
    $modelTexturesRoot = Join-Path $rezRoot "ModelTextures"
    New-Item -ItemType Directory -Force -Path $modelTexturesRoot | Out-Null
    Copy-Item -LiteralPath $pvTex -Destination (Join-Path $modelTexturesRoot "PV_ML_DF_BowieKnife_BC.dtx") -Force
}

$hhTex = Join-Path $rezRoot "ModelTextures\Weapons\Bowie\HH_ML_DF_BOWIEKNIFE_BC.DTX"
if (Test-Path -LiteralPath $hhTex) {
    Copy-Item -LiteralPath $hhTex -Destination (Join-Path $rezRoot "HH_ML_DF_BowieKnife_BC.dtx") -Force
    $modelTexturesRoot = Join-Path $rezRoot "ModelTextures"
    New-Item -ItemType Directory -Force -Path $modelTexturesRoot | Out-Null
    Copy-Item -LiteralPath $hhTex -Destination (Join-Path $modelTexturesRoot "HH_ML_DF_BowieKnife_BC.dtx") -Force
}

Write-Host "[OK] Local game asset staging complete."
