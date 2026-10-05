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
Expand-OptionalZip "CLIENTFX.zip" "ClientFX"



function Map-CabinFeverTextureReferences([string]$DatPath) {
    if (-not (Test-Path -LiteralPath $DatPath)) {
        return
    }

    $texturesRoot = Join-Path $rezRoot "Textures"
    if (-not (Test-Path -LiteralPath $texturesRoot)) {
        return
    }

    Write-Host "[UPDATE] Mapping Cabin Fever texture references from DAT..."

    $bytes = [System.IO.File]::ReadAllBytes($DatPath)
    $ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
    $matches = [regex]::Matches(
        $ascii,
        '(?i)textures[\\/][A-Za-z0-9_ .\-\\\/]+?\.dtx'
    )

    $refs = @($matches | ForEach-Object { $_.Value } | Sort-Object -Unique)
    $mapped = 0
    $ambiguous = 0

    foreach($ref in $refs) {
        $normalized = $ref.Replace("/", "\")
        $relative = $normalized.Substring($normalized.IndexOf("\") + 1)
        $target = Join-Path $texturesRoot $relative

        if (Test-Path -LiteralPath $target) {
            continue
        }

        $leaf = [System.IO.Path]::GetFileName($relative)
        $candidates = @(
            Get-ChildItem -LiteralPath $texturesRoot -Recurse -File |
                Where-Object { $_.Name -ieq $leaf }
        )

        if ($candidates.Count -eq 0) {
            continue
        }

        $segments = $relative -split '\\'
        $targetTop = if($segments.Count -gt 1) { $segments[0] } else { "" }

        $preferred = @(
            $candidates | Where-Object {
                $candidateRelative = $_.FullName.Substring($texturesRoot.Length).TrimStart('\')
                $candidateSegments = $candidateRelative -split '\\'
                $candidateSegments.Count -gt 1 -and $candidateSegments[0] -ieq $targetTop
            }
        )

        if ($preferred.Count -gt 0) {
            $candidates = $preferred
        }

        $chosen = $null

        if ($candidates.Count -eq 1) {
            $chosen = $candidates[0]
        }
        else {
            # If duplicate category copies are byte-identical, either one is safe.
            $hashGroups = @{}
            foreach($candidate in $candidates) {
                $hash = (Get-FileHash -LiteralPath $candidate.FullName -Algorithm SHA256).Hash
                if (-not $hashGroups.ContainsKey($hash)) {
                    $hashGroups[$hash] = @()
                }
                $hashGroups[$hash] += $candidate
            }

            if ($hashGroups.Keys.Count -eq 1) {
                $chosen = @($candidates | Sort-Object FullName)[0]
            }
        }

        if (-not $chosen) {
            Write-Host "[AMBIGUOUS] $normalized"
            foreach($candidate in $candidates) {
                Write-Host "            $($candidate.FullName)"
            }
            $ambiguous++
            continue
        }

        $targetDir = Split-Path -Parent $target
        New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
        Copy-Item -LiteralPath $chosen.FullName -Destination $target -Force
        Write-Host "[MAP] $normalized <- $($chosen.FullName.Substring($texturesRoot.Length).TrimStart('\'))"
        $mapped++
    }

    Write-Host "[OK] Cabin Fever texture map: $mapped aliases created, $ambiguous ambiguous"
}

Map-CabinFeverTextureReferences $mapPath

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


$charZip = Join-Path $assetRoot "CharModels-Textur.zip"
if (Test-Path -LiteralPath $charZip) {
    Write-Host "[UPDATE] Staging Combat Arms Specialist character test assets..."
    Extract-ZipEntry "CharModels-Textur.zip" "CHARS_M_BODY/CM_BODY_NM_SPECIAL_BC.LTB" "Characters\male\body\CM_BODY_NM_SPECIAL_BC.LTB" | Out-Null
    Extract-ZipEntry "CharModels-Textur.zip" "CHARS_T_BODY/CM_BODY_NM_SPECIAL_BC.DTX" "Characters\male\textures\CM_BODY_NM_SPECIAL_BC.DTX" | Out-Null
    Extract-ZipEntry "CharModels-Textur.zip" "CHARS_M_FACE/CM_FC_NM_SPECIAL_BC.LTB" "Characters\male\face\CM_FC_NM_SPECIAL_BC.LTB" | Out-Null
    Extract-ZipEntry "CharModels-Textur.zip" "CHARS_T_FACE/CM_FC_NM_SPECIAL_BC.DTX" "Characters\male\textures\CM_FC_NM_SPECIAL_BC.DTX" | Out-Null
    Extract-ZipEntry "CharModels-Textur.zip" "CHARS_T_HAND/CM_HND_NM_SPECIAL_BC.DTX" "Characters\male\hands\CM_HND_NM_SPECIAL_BC.DTX" | Out-Null
}

$gunsZip = Join-Path $assetRoot "Guns.zip"
$gunsHHZip = Join-Path $assetRoot "GunsHH.zip"

function Stage-CombatArmsAK47([string]$ZipPath) {
    if (-not (Test-Path -LiteralPath $ZipPath)) {
        return $false
    }

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($ZipPath)

    try {
        $akPattern = '(?i)AK[-_ ]?47'

        $pvModels = @(
            $zip.Entries |
                Where-Object {
                    $_.FullName -match '(?i)GUNS_M_PV' -and
                    $_.Name -match $akPattern -and
                    $_.Name -match '(?i)\.LTB
    Write-Host "[UPDATE] Staging Combat Arms Bowie knife player-view assets..."
    Extract-ZipEntry "Guns.zip" "GUNS_M_PV_MELEE/CM_HND_NM_DF_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_pv\CM_HND_NM_DF_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_M_PV_MELEE/ANI_G_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_pv\ANI_G_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_T_PV_MELEE/PV_ML_DF_BOWIEKNIFE_BC.DTX" "Weapons\melee_t\PV_ML_DF_BOWIEKNIFE_BC.DTX" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_SND_MELEE/BOWIE_KNIFE/FIRE.WAV" "Weapons\melee_snd\BOWIE_KNIFE\FIRE.WAV" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_SND_MELEE/BOWIE_KNIFE/SELECT.WAV" "Weapons\melee_snd\BOWIE_KNIFE\SELECT.WAV" | Out-Null

    Write-Host "[UPDATE] Discovering/staging Combat Arms AK-47 player-view assets..."
    Stage-CombatArmsAK47 $gunsZip | Out-Null
} else {
    Write-Host "[SKIP] Guns.zip not present - Bowie player-view model will be unavailable"
}

if (Test-Path -LiteralPath $gunsHHZip) {
    Write-Host "[UPDATE] Staging Combat Arms Bowie knife world assets..."
    Extract-ZipEntry "GunsHH.zip" "GUNS_M_HH/HH_ML_DF_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_hh\HH_ML_DF_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "GunsHH.zip" "GUNS_T_HH/HH_ML_DF_BOWIEKNIFE_BC.DTX" "Weapons\melee_t\HH_ML_DF_BOWIEKNIFE_BC.DTX" | Out-Null

    Write-Host "[UPDATE] Staging Combat Arms AK-47 world assets..."
    Extract-ZipEntry "GunsHH.zip" "GUNS_M_HH/HH_AK-47.LTB" "Weapons\primary_m_hh\HH_AK-47.LTB" | Out-Null
    Extract-ZipEntry "GunsHH.zip" "GUNS_T_HH/HH_AK-47.DTX" "Weapons\primary_t\HH_AK-47.DTX" | Out-Null
} else {
    Write-Host "[SKIP] GunsHH.zip not present - Bowie world model will be unavailable"
}


$bowieSoundZip = Join-Path $assetRoot "BOWIE_KNIFE.zip"
if (Test-Path -LiteralPath $bowieSoundZip) {
    Write-Host "[UPDATE] Staging dedicated Bowie sound archive..."
    Extract-ZipEntry "BOWIE_KNIFE.zip" "BOWIE_KNIFE/FIRE.WAV" "Weapons\melee_snd\BOWIE_KNIFE\FIRE.WAV" | Out-Null
    Extract-ZipEntry "BOWIE_KNIFE.zip" "BOWIE_KNIFE/SELECT.WAV" "Weapons\melee_snd\BOWIE_KNIFE\SELECT.WAV" | Out-Null
}

# Combat Arms LTBs also carry bare texture names. Keep aliases at the resource
# root and ModelTextures root in addition to our organized Bowie directory.
$pvTex = Join-Path $rezRoot "Weapons\melee_t\PV_ML_DF_BOWIEKNIFE_BC.DTX"
if (Test-Path -LiteralPath $pvTex) {
    Copy-Item -LiteralPath $pvTex -Destination (Join-Path $rezRoot "PV_ML_DF_BowieKnife_BC.dtx") -Force
    $modelTexturesRoot = Join-Path $rezRoot "ModelTextures"
    New-Item -ItemType Directory -Force -Path $modelTexturesRoot | Out-Null
    Copy-Item -LiteralPath $pvTex -Destination (Join-Path $modelTexturesRoot "PV_ML_DF_BowieKnife_BC.dtx") -Force
}

$hhTex = Join-Path $rezRoot "Weapons\melee_t\HH_ML_DF_BOWIEKNIFE_BC.DTX"
if (Test-Path -LiteralPath $hhTex) {
    Copy-Item -LiteralPath $hhTex -Destination (Join-Path $rezRoot "HH_ML_DF_BowieKnife_BC.dtx") -Force
    $modelTexturesRoot = Join-Path $rezRoot "ModelTextures"
    New-Item -ItemType Directory -Force -Path $modelTexturesRoot | Out-Null
    Copy-Item -LiteralPath $hhTex -Destination (Join-Path $modelTexturesRoot "HH_ML_DF_BowieKnife_BC.dtx") -Force
}


$clientFxDll = Join-Path $assetRoot "ClientFx.fxd"
$stagedClientFxDll = Join-Path $rezRoot "ClientFx.fxd"

# Combat Arms' ClientFx.fxd is NOT ABI-safe to drop into the SealHunter/Jupiter
# runtime. Keep it in assets-local as reverse-engineering/reference material only.
if (Test-Path -LiteralPath $stagedClientFxDll) {
    Remove-Item -LiteralPath $stagedClientFxDll -Force
    Write-Host "[CLEAN] Removed previously staged Combat Arms ClientFx.fxd"
}

if (Test-Path -LiteralPath $clientFxDll) {
    Write-Host "[INFO] ClientFx.fxd found - reference only; using Fireteam/Jupiter ClientFX code"
}

Write-Host "[OK] Local game asset staging complete."

                } |
                Sort-Object FullName
        )

        $pvModel = @(
            $pvModels |
                Where-Object { $_.Name -notmatch '(?i)^ANI_' }
        ) | Select-Object -First 1

        if (-not $pvModel) {
            Write-Host "[SKIP] Could not locate an AK-47 PV model inside Guns.zip"
            return $false
        }

        function Copy-AKEntry($entry, [string]$DestinationRelative) {
            if (-not $entry) {
                return $false
            }

            $dest = Join-Path $rezRoot $DestinationRelative
            $destDir = Split-Path -Parent $dest
            New-Item -ItemType Directory -Force -Path $destDir | Out-Null
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $dest, $true)
            Write-Host "[OK] AK-47: $($entry.FullName) -> $DestinationRelative"
            return $true
        }

        $pvDestRel = "Weapons\primary_m_pv\AK47_PV.LTB"
        Copy-AKEntry $pvModel $pvDestRel | Out-Null

        # Find the animation child referenced by the PV model itself. CA weapon
        # filenames vary by era, so reading the actual LTB is safer than guessing.
        $pvDest = Join-Path $rezRoot $pvDestRel
        $modelAscii = [System.Text.Encoding]::ASCII.GetString(
            [System.IO.File]::ReadAllBytes($pvDest))

        $aniMatch = [regex]::Match(
            $modelAscii,
            '(?i)ANI_[A-Za-z0-9_\-.]+\.LTB')

        if ($aniMatch.Success) {
            $aniLeaf = [System.IO.Path]::GetFileName($aniMatch.Value)
            $aniEntry = $zip.Entries |
                Where-Object { $_.Name -ieq $aniLeaf } |
                Select-Object -First 1

            if ($aniEntry) {
                Copy-AKEntry $aniEntry "Weapons\primary_m_pv\AK47_ANI.LTB" | Out-Null
            }
        }

        $pvTexture = $zip.Entries |
            Where-Object {
                $_.FullName -match '(?i)GUNS_T_PV' -and
                $_.Name -match $akPattern -and
                $_.Name -match '(?i)\.DTX
    Write-Host "[UPDATE] Staging Combat Arms Bowie knife player-view assets..."
    Extract-ZipEntry "Guns.zip" "GUNS_M_PV_MELEE/CM_HND_NM_DF_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_pv\CM_HND_NM_DF_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_M_PV_MELEE/ANI_G_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_pv\ANI_G_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_T_PV_MELEE/PV_ML_DF_BOWIEKNIFE_BC.DTX" "Weapons\melee_t\PV_ML_DF_BOWIEKNIFE_BC.DTX" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_SND_MELEE/BOWIE_KNIFE/FIRE.WAV" "Weapons\melee_snd\BOWIE_KNIFE\FIRE.WAV" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_SND_MELEE/BOWIE_KNIFE/SELECT.WAV" "Weapons\melee_snd\BOWIE_KNIFE\SELECT.WAV" | Out-Null
} else {
    Write-Host "[SKIP] Guns.zip not present - Bowie player-view model will be unavailable"
}

if (Test-Path -LiteralPath $gunsHHZip) {
    Write-Host "[UPDATE] Staging Combat Arms Bowie knife world assets..."
    Extract-ZipEntry "GunsHH.zip" "GUNS_M_HH/HH_ML_DF_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_hh\HH_ML_DF_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "GunsHH.zip" "GUNS_T_HH/HH_ML_DF_BOWIEKNIFE_BC.DTX" "Weapons\melee_t\HH_ML_DF_BOWIEKNIFE_BC.DTX" | Out-Null
} else {
    Write-Host "[SKIP] GunsHH.zip not present - Bowie world model will be unavailable"
}


$bowieSoundZip = Join-Path $assetRoot "BOWIE_KNIFE.zip"
if (Test-Path -LiteralPath $bowieSoundZip) {
    Write-Host "[UPDATE] Staging dedicated Bowie sound archive..."
    Extract-ZipEntry "BOWIE_KNIFE.zip" "BOWIE_KNIFE/FIRE.WAV" "Weapons\melee_snd\BOWIE_KNIFE\FIRE.WAV" | Out-Null
    Extract-ZipEntry "BOWIE_KNIFE.zip" "BOWIE_KNIFE/SELECT.WAV" "Weapons\melee_snd\BOWIE_KNIFE\SELECT.WAV" | Out-Null
}

# Combat Arms LTBs also carry bare texture names. Keep aliases at the resource
# root and ModelTextures root in addition to our organized Bowie directory.
$pvTex = Join-Path $rezRoot "Weapons\melee_t\PV_ML_DF_BOWIEKNIFE_BC.DTX"
if (Test-Path -LiteralPath $pvTex) {
    Copy-Item -LiteralPath $pvTex -Destination (Join-Path $rezRoot "PV_ML_DF_BowieKnife_BC.dtx") -Force
    $modelTexturesRoot = Join-Path $rezRoot "ModelTextures"
    New-Item -ItemType Directory -Force -Path $modelTexturesRoot | Out-Null
    Copy-Item -LiteralPath $pvTex -Destination (Join-Path $modelTexturesRoot "PV_ML_DF_BowieKnife_BC.dtx") -Force
}

$hhTex = Join-Path $rezRoot "Weapons\melee_t\HH_ML_DF_BOWIEKNIFE_BC.DTX"
if (Test-Path -LiteralPath $hhTex) {
    Copy-Item -LiteralPath $hhTex -Destination (Join-Path $rezRoot "HH_ML_DF_BowieKnife_BC.dtx") -Force
    $modelTexturesRoot = Join-Path $rezRoot "ModelTextures"
    New-Item -ItemType Directory -Force -Path $modelTexturesRoot | Out-Null
    Copy-Item -LiteralPath $hhTex -Destination (Join-Path $modelTexturesRoot "HH_ML_DF_BowieKnife_BC.dtx") -Force
}


$clientFxDll = Join-Path $assetRoot "ClientFx.fxd"
$stagedClientFxDll = Join-Path $rezRoot "ClientFx.fxd"

# Combat Arms' ClientFx.fxd is NOT ABI-safe to drop into the SealHunter/Jupiter
# runtime. Keep it in assets-local as reverse-engineering/reference material only.
if (Test-Path -LiteralPath $stagedClientFxDll) {
    Remove-Item -LiteralPath $stagedClientFxDll -Force
    Write-Host "[CLEAN] Removed previously staged Combat Arms ClientFx.fxd"
}

if (Test-Path -LiteralPath $clientFxDll) {
    Write-Host "[INFO] ClientFx.fxd found - reference only; using Fireteam/Jupiter ClientFX code"
}

Write-Host "[OK] Local game asset staging complete."

            } |
            Sort-Object FullName |
            Select-Object -First 1

        if ($pvTexture) {
            Copy-AKEntry $pvTexture "Weapons\primary_t\AK47_PV.DTX" | Out-Null
        }

        foreach($soundName in @("FIRE", "SELECT", "RELOAD")) {
            $soundEntry = $zip.Entries |
                Where-Object {
                    $_.FullName -match $akPattern -and
                    $_.Name -match ('(?i)^' + $soundName + '.*\.WAV
    Write-Host "[UPDATE] Staging Combat Arms Bowie knife player-view assets..."
    Extract-ZipEntry "Guns.zip" "GUNS_M_PV_MELEE/CM_HND_NM_DF_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_pv\CM_HND_NM_DF_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_M_PV_MELEE/ANI_G_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_pv\ANI_G_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_T_PV_MELEE/PV_ML_DF_BOWIEKNIFE_BC.DTX" "Weapons\melee_t\PV_ML_DF_BOWIEKNIFE_BC.DTX" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_SND_MELEE/BOWIE_KNIFE/FIRE.WAV" "Weapons\melee_snd\BOWIE_KNIFE\FIRE.WAV" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_SND_MELEE/BOWIE_KNIFE/SELECT.WAV" "Weapons\melee_snd\BOWIE_KNIFE\SELECT.WAV" | Out-Null
} else {
    Write-Host "[SKIP] Guns.zip not present - Bowie player-view model will be unavailable"
}

if (Test-Path -LiteralPath $gunsHHZip) {
    Write-Host "[UPDATE] Staging Combat Arms Bowie knife world assets..."
    Extract-ZipEntry "GunsHH.zip" "GUNS_M_HH/HH_ML_DF_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_hh\HH_ML_DF_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "GunsHH.zip" "GUNS_T_HH/HH_ML_DF_BOWIEKNIFE_BC.DTX" "Weapons\melee_t\HH_ML_DF_BOWIEKNIFE_BC.DTX" | Out-Null
} else {
    Write-Host "[SKIP] GunsHH.zip not present - Bowie world model will be unavailable"
}


$bowieSoundZip = Join-Path $assetRoot "BOWIE_KNIFE.zip"
if (Test-Path -LiteralPath $bowieSoundZip) {
    Write-Host "[UPDATE] Staging dedicated Bowie sound archive..."
    Extract-ZipEntry "BOWIE_KNIFE.zip" "BOWIE_KNIFE/FIRE.WAV" "Weapons\melee_snd\BOWIE_KNIFE\FIRE.WAV" | Out-Null
    Extract-ZipEntry "BOWIE_KNIFE.zip" "BOWIE_KNIFE/SELECT.WAV" "Weapons\melee_snd\BOWIE_KNIFE\SELECT.WAV" | Out-Null
}

# Combat Arms LTBs also carry bare texture names. Keep aliases at the resource
# root and ModelTextures root in addition to our organized Bowie directory.
$pvTex = Join-Path $rezRoot "Weapons\melee_t\PV_ML_DF_BOWIEKNIFE_BC.DTX"
if (Test-Path -LiteralPath $pvTex) {
    Copy-Item -LiteralPath $pvTex -Destination (Join-Path $rezRoot "PV_ML_DF_BowieKnife_BC.dtx") -Force
    $modelTexturesRoot = Join-Path $rezRoot "ModelTextures"
    New-Item -ItemType Directory -Force -Path $modelTexturesRoot | Out-Null
    Copy-Item -LiteralPath $pvTex -Destination (Join-Path $modelTexturesRoot "PV_ML_DF_BowieKnife_BC.dtx") -Force
}

$hhTex = Join-Path $rezRoot "Weapons\melee_t\HH_ML_DF_BOWIEKNIFE_BC.DTX"
if (Test-Path -LiteralPath $hhTex) {
    Copy-Item -LiteralPath $hhTex -Destination (Join-Path $rezRoot "HH_ML_DF_BowieKnife_BC.dtx") -Force
    $modelTexturesRoot = Join-Path $rezRoot "ModelTextures"
    New-Item -ItemType Directory -Force -Path $modelTexturesRoot | Out-Null
    Copy-Item -LiteralPath $hhTex -Destination (Join-Path $modelTexturesRoot "HH_ML_DF_BowieKnife_BC.dtx") -Force
}


$clientFxDll = Join-Path $assetRoot "ClientFx.fxd"
$stagedClientFxDll = Join-Path $rezRoot "ClientFx.fxd"

# Combat Arms' ClientFx.fxd is NOT ABI-safe to drop into the SealHunter/Jupiter
# runtime. Keep it in assets-local as reverse-engineering/reference material only.
if (Test-Path -LiteralPath $stagedClientFxDll) {
    Remove-Item -LiteralPath $stagedClientFxDll -Force
    Write-Host "[CLEAN] Removed previously staged Combat Arms ClientFx.fxd"
}

if (Test-Path -LiteralPath $clientFxDll) {
    Write-Host "[INFO] ClientFx.fxd found - reference only; using Fireteam/Jupiter ClientFX code"
}

Write-Host "[OK] Local game asset staging complete."
)
                } |
                Sort-Object FullName |
                Select-Object -First 1

            if ($soundEntry) {
                Copy-AKEntry $soundEntry ("Weapons\primary_snd\AK47\" + $soundName + ".WAV") | Out-Null
            }
        }

        return $true
    }
    finally {
        $zip.Dispose()
    }
}

if (Test-Path -LiteralPath $gunsZip) {
    Write-Host "[UPDATE] Staging Combat Arms Bowie knife player-view assets..."
    Extract-ZipEntry "Guns.zip" "GUNS_M_PV_MELEE/CM_HND_NM_DF_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_pv\CM_HND_NM_DF_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_M_PV_MELEE/ANI_G_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_pv\ANI_G_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_T_PV_MELEE/PV_ML_DF_BOWIEKNIFE_BC.DTX" "Weapons\melee_t\PV_ML_DF_BOWIEKNIFE_BC.DTX" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_SND_MELEE/BOWIE_KNIFE/FIRE.WAV" "Weapons\melee_snd\BOWIE_KNIFE\FIRE.WAV" | Out-Null
    Extract-ZipEntry "Guns.zip" "GUNS_SND_MELEE/BOWIE_KNIFE/SELECT.WAV" "Weapons\melee_snd\BOWIE_KNIFE\SELECT.WAV" | Out-Null
} else {
    Write-Host "[SKIP] Guns.zip not present - Bowie player-view model will be unavailable"
}

if (Test-Path -LiteralPath $gunsHHZip) {
    Write-Host "[UPDATE] Staging Combat Arms Bowie knife world assets..."
    Extract-ZipEntry "GunsHH.zip" "GUNS_M_HH/HH_ML_DF_BOWIEKNIFE_CH.LTB" "Weapons\melee_m_hh\HH_ML_DF_BOWIEKNIFE_CH.LTB" | Out-Null
    Extract-ZipEntry "GunsHH.zip" "GUNS_T_HH/HH_ML_DF_BOWIEKNIFE_BC.DTX" "Weapons\melee_t\HH_ML_DF_BOWIEKNIFE_BC.DTX" | Out-Null
} else {
    Write-Host "[SKIP] GunsHH.zip not present - Bowie world model will be unavailable"
}


$bowieSoundZip = Join-Path $assetRoot "BOWIE_KNIFE.zip"
if (Test-Path -LiteralPath $bowieSoundZip) {
    Write-Host "[UPDATE] Staging dedicated Bowie sound archive..."
    Extract-ZipEntry "BOWIE_KNIFE.zip" "BOWIE_KNIFE/FIRE.WAV" "Weapons\melee_snd\BOWIE_KNIFE\FIRE.WAV" | Out-Null
    Extract-ZipEntry "BOWIE_KNIFE.zip" "BOWIE_KNIFE/SELECT.WAV" "Weapons\melee_snd\BOWIE_KNIFE\SELECT.WAV" | Out-Null
}

# Combat Arms LTBs also carry bare texture names. Keep aliases at the resource
# root and ModelTextures root in addition to our organized Bowie directory.
$pvTex = Join-Path $rezRoot "Weapons\melee_t\PV_ML_DF_BOWIEKNIFE_BC.DTX"
if (Test-Path -LiteralPath $pvTex) {
    Copy-Item -LiteralPath $pvTex -Destination (Join-Path $rezRoot "PV_ML_DF_BowieKnife_BC.dtx") -Force
    $modelTexturesRoot = Join-Path $rezRoot "ModelTextures"
    New-Item -ItemType Directory -Force -Path $modelTexturesRoot | Out-Null
    Copy-Item -LiteralPath $pvTex -Destination (Join-Path $modelTexturesRoot "PV_ML_DF_BowieKnife_BC.dtx") -Force
}

$hhTex = Join-Path $rezRoot "Weapons\melee_t\HH_ML_DF_BOWIEKNIFE_BC.DTX"
if (Test-Path -LiteralPath $hhTex) {
    Copy-Item -LiteralPath $hhTex -Destination (Join-Path $rezRoot "HH_ML_DF_BowieKnife_BC.dtx") -Force
    $modelTexturesRoot = Join-Path $rezRoot "ModelTextures"
    New-Item -ItemType Directory -Force -Path $modelTexturesRoot | Out-Null
    Copy-Item -LiteralPath $hhTex -Destination (Join-Path $modelTexturesRoot "HH_ML_DF_BowieKnife_BC.dtx") -Force
}


$clientFxDll = Join-Path $assetRoot "ClientFx.fxd"
$stagedClientFxDll = Join-Path $rezRoot "ClientFx.fxd"

# Combat Arms' ClientFx.fxd is NOT ABI-safe to drop into the SealHunter/Jupiter
# runtime. Keep it in assets-local as reverse-engineering/reference material only.
if (Test-Path -LiteralPath $stagedClientFxDll) {
    Remove-Item -LiteralPath $stagedClientFxDll -Force
    Write-Host "[CLEAN] Removed previously staged Combat Arms ClientFx.fxd"
}

if (Test-Path -LiteralPath $clientFxDll) {
    Write-Host "[INFO] ClientFx.fxd found - reference only; using Fireteam/Jupiter ClientFX code"
}

Write-Host "[OK] Local game asset staging complete."
