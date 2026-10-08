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
    $actualZipName = $ZipName

    # Browser/Windows downloads often rename a second copy to FX(1).zip or
    # SND(1).zip. Accept the newest matching local archive without forcing a
    # rename of private Combat Arms assets.
    $duplicatePattern = $null
    if($ZipName -ieq "FX.zip") {
        $duplicatePattern = "FX*.zip"
    }
    elseif($ZipName -ieq "SND.zip") {
        $duplicatePattern = "SND*.zip"
    }

    if($ZipName -ieq "SND.zip" -and $duplicatePattern) {
        # An older partial SND.zip may live beside a later all-sounds dump
        # such as SND(2).zip. Prefer the largest matching archive, then newest.
        $candidate = Get-ChildItem -LiteralPath $assetRoot -Filter $duplicatePattern -File |
            Sort-Object -Property @{Expression={$_.Length};Descending=$true}, @{Expression={$_.LastWriteTimeUtc};Descending=$true} |
            Select-Object -First 1

        if($candidate) {
            $zipPath = $candidate.FullName
            $actualZipName = $candidate.Name
        }
    }
    elseif (-not (Test-Path -LiteralPath $zipPath) -and $duplicatePattern) {
        $candidate = Get-ChildItem -LiteralPath $assetRoot -Filter $duplicatePattern -File |
            Sort-Object LastWriteTimeUtc -Descending |
            Select-Object -First 1

        if($candidate) {
            $zipPath = $candidate.FullName
            $actualZipName = $candidate.Name
        }
    }

    if (-not (Test-Path -LiteralPath $zipPath)) {
        Write-Host "[SKIP] $ZipName not present"
        return
    }

    $safeName = $actualZipName.Replace(".", "_")
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

    Write-Host "[UPDATE] Expanding $actualZipName..."
    Expand-Archive -LiteralPath $zipPath -DestinationPath $dest -Force
    Set-Content -LiteralPath $stampPath -Value $zipInfo.LastWriteTimeUtc.Ticks
    (Get-Item -LiteralPath $stampPath).LastWriteTimeUtc = $zipInfo.LastWriteTimeUtc
    Write-Host "[OK] $actualZipName -> rez\$RezSubdir"
}

function Stage-WorldDat([string]$SourcePath) {
    if (-not (Test-Path -LiteralPath $SourcePath)) {
        return
    }

    $worlds = Join-Path $rezRoot "Worlds"
    New-Item -ItemType Directory -Force -Path $worlds | Out-Null

    $leaf = [System.IO.Path]::GetFileName($SourcePath).ToUpperInvariant()
    $destMap = Join-Path $worlds $leaf

    if ((-not (Test-Path -LiteralPath $destMap)) -or
        ((Get-Item -LiteralPath $destMap).LastWriteTimeUtc -lt
         (Get-Item -LiteralPath $SourcePath).LastWriteTimeUtc)) {
        Copy-Item -LiteralPath $SourcePath -Destination $destMap -Force
        Write-Host "[OK] $leaf refreshed"
    }
    else {
        Write-Host "[OK] $leaf unchanged"
    }
}

$mapPath = Join-Path $assetRoot "CABINFEVER.DAT"
if (Test-Path -LiteralPath $mapPath) {
    Stage-WorldDat $mapPath
}
else {
    Write-Host "[SKIP] CABINFEVER.DAT not present"
}

# Any additional .DAT at assets-local root is treated as a local world import.
Get-ChildItem -LiteralPath $assetRoot -Filter "*.DAT" -File |
    Where-Object { $_.Name -ine "CABINFEVER.DAT" } |
    ForEach-Object {
        Stage-WorldDat $_.FullName
    }

# The launcher Map Importer persists worlds here so they survive rebuilds.
$mapImportRoot = Join-Path $assetRoot "MapImports"
if (Test-Path -LiteralPath $mapImportRoot) {
    Get-ChildItem -LiteralPath $mapImportRoot -Filter "*.DAT" -File |
        ForEach-Object {
            Stage-WorldDat $_.FullName
        }
}

Expand-OptionalZip "TEXTURES.zip" "Textures"
Expand-OptionalZip "FX.zip" "FX"
Expand-OptionalZip "SND.zip" "Snd"
Expand-OptionalZip "RS.zip" "RenderStyles"
Expand-OptionalZip "SHADERS.zip" "Shaders"
Expand-OptionalZip "CLIENTFX.zip" "ClientFX"

# Local custom render styles can live beside RS.zip without repacking the
# archive.  This is used by FIRETEAM-only effects such as ZombieThroughWall.
$customRenderStyles = Join-Path $assetRoot "RS"
if(Test-Path -LiteralPath $customRenderStyles) {
    $renderStyleDest = Join-Path $rezRoot "RenderStyles"
    New-Item -ItemType Directory -Force -Path $renderStyleDest | Out-Null
    Copy-Item -Path (Join-Path $customRenderStyles "*") -Destination $renderStyleDest -Recurse -Force
    Write-Host "[OK] assets-local\RS -> rez\RenderStyles"
}

# The launcher attachment importer keeps commercial bytes in ignored local
# storage.  Stage its generated tree into the runtime when it exists.
$attachmentImportRoot = Join-Path $assetRoot "AttachmentImports\Attachments\ca"
if(Test-Path -LiteralPath $attachmentImportRoot) {
    $attachmentDest = Join-Path $rezRoot "Attachments\ca"
    New-Item -ItemType Directory -Force -Path $attachmentDest | Out-Null
    Copy-Item -Path (Join-Path $attachmentImportRoot "*") -Destination $attachmentDest -Recurse -Force
    Write-Host "[OK] AttachmentImports -> rez\Attachments\ca"
}

$zombieAmbience = Join-Path $assetRoot "ZombieAmbience.wav"
$zombieAmbienceMp3 = Join-Path $assetRoot "ZombieAmbience.mp3"

# Jupiter's ordinary sound-buffer path expects a RIFF/WAVE container. If the
# user supplied the MP3 and ffmpeg is already installed, create a compact
# IMA-ADPCM WAV locally. Commercial/research audio stays outside Git.
if((-not (Test-Path -LiteralPath $zombieAmbience)) -and
   (Test-Path -LiteralPath $zombieAmbienceMp3)) {
    $ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue

    if($ffmpeg) {
        Write-Host "[UPDATE] Converting ZombieAmbience.mp3 to Jupiter-compatible IMA ADPCM WAV..."

        & $ffmpeg.Source `
            -y `
            -hide_banner `
            -loglevel error `
            -i $zombieAmbienceMp3 `
            -ar 44100 `
            -ac 2 `
            -c:a adpcm_ima_wav `
            $zombieAmbience

        if($LASTEXITCODE -ne 0 -or
           -not (Test-Path -LiteralPath $zombieAmbience)) {
            Write-Host "[WARN] ZombieAmbience conversion failed; ambience loop will be skipped."
        }
    }
    else {
        Write-Host "[INFO] ZombieAmbience.mp3 found, but Jupiter expects RIFF/WAV for this path."
        Write-Host "       Put ZombieAmbience.wav beside it, or install ffmpeg and rebuild."
    }
}

if(Test-Path -LiteralPath $zombieAmbience) {
    $ambientDir = Join-Path $rezRoot "Snd\Fireteam"
    $ambientDest = Join-Path $ambientDir "ZombieAmbience.wav"
    New-Item -ItemType Directory -Force -Path $ambientDir | Out-Null

    if((-not (Test-Path -LiteralPath $ambientDest)) -or
       ((Get-Item -LiteralPath $ambientDest).LastWriteTimeUtc -lt
        (Get-Item -LiteralPath $zombieAmbience).LastWriteTimeUtc)) {
        Copy-Item -LiteralPath $zombieAmbience -Destination $ambientDest -Force
        Write-Host "[OK] ZombieAmbience.wav -> rez\Snd\Fireteam"
    } else {
        Write-Host "[OK] ZombieAmbience.wav unchanged"
    }
}



function Map-WorldTextureReferences([string]$DatPath) {
    if (-not (Test-Path -LiteralPath $DatPath)) {
        return
    }

    $texturesRoot = Join-Path $rezRoot "Textures"
    if (-not (Test-Path -LiteralPath $texturesRoot)) {
        return
    }

    $mapName = [System.IO.Path]::GetFileNameWithoutExtension($DatPath)
    Write-Host "[UPDATE] Mapping world texture/sprite references from $([System.IO.Path]::GetFileName($DatPath))..."

    $textureFiles = @(
        Get-ChildItem -LiteralPath $texturesRoot -Recurse -File
    )

    $byLeaf = @{}
    foreach($file in $textureFiles) {
        $key = $file.Name.ToLowerInvariant()
        if(-not $byLeaf.ContainsKey($key)) {
            $byLeaf[$key] = @()
        }
        $byLeaf[$key] += $file
    }

    $bytes = [System.IO.File]::ReadAllBytes($DatPath)
    $ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
    $matches = [regex]::Matches(
        $ascii,
        '(?i)textures[\\/][A-Za-z0-9_ .\-\\/]+?\.(?:dtx|spr)'
    )

    $refs = @(
        $matches |
            ForEach-Object { $_.Value } |
            Sort-Object -Unique
    )

    $mapped = 0
    $ambiguous = 0
    $missing = New-Object System.Collections.Generic.List[string]

    foreach($ref in $refs) {
        $normalized = $ref.Replace("/", "\")
        $relative = $normalized.Substring($normalized.IndexOf("\") + 1)
        $target = Join-Path $texturesRoot $relative

        if (Test-Path -LiteralPath $target) {
            continue
        }

        $leaf = [System.IO.Path]::GetFileName($relative)
        $leafKey = $leaf.ToLowerInvariant()

        $candidates =
            if($byLeaf.ContainsKey($leafKey)) {
                @($byLeaf[$leafKey])
            }
            else {
                @()
            }

        if ($candidates.Count -eq 0) {
            Write-Host "[MISSING] $normalized"
            $missing.Add($normalized)
            continue
        }

        $segments = $relative -split '\\'
        $targetTop = if($segments.Count -gt 1) { $segments[0] } else { "" }

        $preferred = @(
            $candidates | Where-Object {
                $candidateRelative = $_.FullName.Substring($texturesRoot.Length).TrimStart('\')
                $candidateSegments = $candidateRelative -split '\\'
                $candidateSegments.Count -gt 1 -and
                    $candidateSegments[0] -ieq $targetTop
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
            $missing.Add($normalized)
            continue
        }

        $targetDir = Split-Path -Parent $target
        New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
        Copy-Item -LiteralPath $chosen.FullName -Destination $target -Force
        Write-Host "[MAP] $normalized <- $($chosen.FullName.Substring($texturesRoot.Length).TrimStart('\'))"
        $mapped++
    }

    $reports = Join-Path $assetRoot "Reports"
    New-Item -ItemType Directory -Force -Path $reports | Out-Null
    $reportPath = Join-Path $reports ($mapName + "-missing-map-resources.txt")

    @(
        "# FIRETEAM map dependency audit"
        "map=$([System.IO.Path]::GetFileName($DatPath))"
        "referenced_textures_or_sprites=$($refs.Count)"
        "mapped_aliases=$mapped"
        "ambiguous_or_missing=$($missing.Count)"
        ""
        $missing
    ) | Set-Content -LiteralPath $reportPath

    Write-Host "[OK] World texture/sprite map: $mapped aliases created, $ambiguous ambiguous, $($missing.Count) unresolved"
    Write-Host "[OK] Map dependency report -> assets-local\Reports\$mapName-missing-map-resources.txt"
}

if(Test-Path -LiteralPath $mapPath) {
    Map-WorldTextureReferences $mapPath
}

Get-ChildItem -LiteralPath $assetRoot -Filter "*.DAT" -File |
    Where-Object { $_.Name -ine "CABINFEVER.DAT" } |
    ForEach-Object {
        Map-WorldTextureReferences $_.FullName
    }

if(Test-Path -LiteralPath $mapImportRoot) {
    Get-ChildItem -LiteralPath $mapImportRoot -Filter "*.DAT" -File |
        ForEach-Object {
            Map-WorldTextureReferences $_.FullName
        }
}

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


function Extract-OptionalZipEntry([string]$ZipName, [string]$EntryName, [string]$DestinationRelative) {
    try {
        return Extract-ZipEntry $ZipName $EntryName $DestinationRelative
    }
    catch {
        Write-Host "[INFO] Optional asset not present: $ZipName::$EntryName"
        return $false
    }
}


$uiZip = Join-Path $assetRoot "UI_Items.zip"
if(Test-Path -LiteralPath $uiZip) {
    Write-Host "[UPDATE] Staging Combat Arms combat-feedback HUD textures..."

    foreach($entry in @(
        "UI_HUD_MESSAGE/EFFECT/HEADSHOT.DTX",
        "UI_HUD_MESSAGE/EFFECT/NUTSHOT.DTX",
        "UI_HUD_MESSAGE/EFFECT/FIRSTKILL.DTX",
        "UI_HUD_MESSAGE/EFFECT/DOUBLEKILL.DTX",
        "UI_HUD_MESSAGE/EFFECT/MULTIKILL.DTX",
        "UI_HUD_MESSAGE/EFFECT/ULTRAKILL.DTX",
        "UI_HUD_MESSAGE/EFFECT/FANTASTIC.DTX",
        "UI_HUD_MESSAGE/EFFECT/UNBELIEVABLE.DTX",
        "UI_HUD_MESSAGE/EFFECT/ROUNDSTART.DTX",
        "UI_HUD_WEAPON/CROSSHAIR/HUDEFFECT_HIT.DTX"
    )) {
        Extract-OptionalZipEntry "UI_Items.zip" $entry ($entry.Replace("/", "\")) | Out-Null
    }
} else {
    Write-Host "[SKIP] UI_Items.zip not present - text fallback will be used for combat feedback"
}


$charZipName = $null
foreach($pattern in @(
    "Chars_Files_Updated*.zip",
    "CharModels-Textur*.zip"
)) {
    $candidate = Get-ChildItem -LiteralPath $assetRoot -Filter $pattern -File |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1

    if($candidate) {
        $charZipName = $candidate.Name
        break
    }
}

if($charZipName) {
    Write-Host "[UPDATE] Staging Combat Arms character / infected assets from $charZipName..."

    # Shared CA male character assets used by player/infected content.
    Extract-ZipEntry $charZipName "CHARS_M_BODY/CM_BODY_NM_SPECIAL_BC.LTB" "Characters\male\body\CM_BODY_NM_SPECIAL_BC.LTB" | Out-Null
    Extract-ZipEntry $charZipName "CHARS_T_BODY/CM_BODY_NM_SPECIAL_BC.DTX" "Characters\male\textures\CM_BODY_NM_SPECIAL_BC.DTX" | Out-Null
    Extract-ZipEntry $charZipName "CHARS_M_FACE/CM_FC_NM_SPECIAL_BC.LTB" "Characters\male\face\CM_FC_NM_SPECIAL_BC.LTB" | Out-Null
    Extract-ZipEntry $charZipName "CHARS_T_FACE/CM_FC_NM_SPECIAL_BC.DTX" "Characters\male\textures\CM_FC_NM_SPECIAL_BC.DTX" | Out-Null
    Extract-ZipEntry $charZipName "CHARS_T_HAND/CM_HND_NM_SPECIAL_BC.DTX" "Characters\male\hands\CM_HND_NM_SPECIAL_BC.DTX" | Out-Null

    # Normal_Infecter_Common_D content.
    Extract-ZipEntry $charZipName "CHARS_M_BODY/MT_AR_BODY.LTB" "Characters\infected\body\MT_AR_BODY.LTB" | Out-Null
    Extract-ZipEntry $charZipName "CHARS_M_BODY/ST_M_CHILD.LTB" "Characters\infected\body\ST_M_CHILD.LTB" | Out-Null
    Extract-ZipEntry $charZipName "CHARS_T_BODY/MT_AR_BODY.DTX" "Characters\infected\body\MT_AR_BODY.DTX" | Out-Null
    Extract-ZipEntry $charZipName "CHARS_T_BODY/MT_MG_LEG.DTX" "Characters\infected\body\MT_MG_LEG.DTX" | Out-Null
    Extract-ZipEntry $charZipName "CHARS_M_FACE/CM_FC_NM_VIRUS_HM.LTB" "Characters\infected\face\CM_FC_NM_VIRUS_HM.LTB" | Out-Null
    Extract-ZipEntry $charZipName "CHARS_T_FACE/CM_FC_NM_VIRUS_HM.DTX" "Characters\infected\face\CM_FC_NM_VIRUS_HM.DTX" | Out-Null
    Extract-OptionalZipEntry $charZipName "CHARS_M_HEAD/CM_HLMT_NM_VIRUS_HM.LTB" "Characters\infected\head\CM_HLMT_NM_VIRUS_HM.LTB" | Out-Null
    Extract-OptionalZipEntry $charZipName "CHARS_T_HEAD/CM_HLMT_NM_VIRUS_HM.DTX" "Characters\infected\head\CM_HLMT_NM_VIRUS_HM.DTX" | Out-Null

    # Stage additional known CA infected bodies now so future external config
    # entries can reference them without changing C++ or staging layout.
    Extract-OptionalZipEntry $charZipName "CHARS_M_BODY/MT_BM_BODY.LTB" "Characters\infected\body\MT_BM_BODY.LTB" | Out-Null
    Extract-OptionalZipEntry $charZipName "CHARS_T_BODY/MT_BM_BODY.DTX" "Characters\infected\body\MT_BM_BODY.DTX" | Out-Null
    Extract-OptionalZipEntry $charZipName "CHARS_M_BODY/VIW_F_NM_DF_ASSASSIN_CH.LTB" "Characters\infected\body\VIW_F_NM_DF_ASSASSIN_CH.LTB" | Out-Null
    Extract-OptionalZipEntry $charZipName "CHARS_M_BODY/ANI_VI_ASSASSIN_CH.LTB" "Characters\infected\body\ANI_VI_ASSASSIN_CH.LTB" | Out-Null
    Extract-OptionalZipEntry $charZipName "CHARS_M_BODY/VIM_F_NM_DF_TANKER_SH.LTB" "Characters\infected\body\VIM_F_NM_DF_TANKER_SH.LTB" | Out-Null
    Extract-OptionalZipEntry $charZipName "CHARS_M_BODY/ANI_VI_TANKER_SH.LTB" "Characters\infected\body\ANI_VI_TANKER_SH.LTB" | Out-Null
} else {
    Write-Host "[SKIP] No supported Combat Arms character archive found."
}

# ST_M_CHILD.LTB stores useful model/animation identifiers as printable text.
# Keep a local report so attack/jump animation names can be verified without
# guessing or committing commercial model bytes.
$infectedAnimModel = Join-Path $rezRoot "Characters\infected\body\ST_M_CHILD.LTB"
if(Test-Path -LiteralPath $infectedAnimModel) {
    $reports = Join-Path $assetRoot "Reports"
    New-Item -ItemType Directory -Force -Path $reports | Out-Null
    $reportPath = Join-Path $reports "ST_M_CHILD-strings.txt"

    $bytes = [System.IO.File]::ReadAllBytes($infectedAnimModel)
    $builder = New-Object System.Text.StringBuilder
    $tokens = New-Object System.Collections.Generic.List[string]

    foreach($byte in $bytes) {
        if($byte -ge 32 -and $byte -le 126) {
            [void]$builder.Append([char]$byte)
        }
        else {
            if($builder.Length -ge 2) {
                $token = $builder.ToString()
                if($token -match '^[A-Za-z][A-Za-z0-9_\-]{1,47}$') {
                    $tokens.Add($token)
                }
            }
            [void]$builder.Clear()
        }
    }

    if($builder.Length -ge 2) {
        $token = $builder.ToString()
        if($token -match '^[A-Za-z][A-Za-z0-9_\-]{1,47}$') {
            $tokens.Add($token)
        }
    }

    $uniqueTokens =
        @($tokens |
            Sort-Object -Unique)

    $uniqueTokens |
        Set-Content -LiteralPath $reportPath

    $runtimeReportDir =
        Join-Path $rezRoot "Fireteam"
    New-Item -ItemType Directory -Force -Path $runtimeReportDir | Out-Null

    $runtimeCandidates =
        Join-Path $runtimeReportDir "ST_M_CHILD-strings.txt"

    $uniqueTokens |
        Set-Content -LiteralPath $runtimeCandidates

    Write-Host "[OK] ST_M_CHILD string report -> assets-local\Reports\ST_M_CHILD-strings.txt"
    Write-Host "[OK] ST_M_CHILD animation candidates -> rez\Fireteam\ST_M_CHILD-strings.txt"
}

$gunsZip = Join-Path $assetRoot "Guns.zip"
$gunsHHZip = Join-Path $assetRoot "GunsHH.zip"

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
