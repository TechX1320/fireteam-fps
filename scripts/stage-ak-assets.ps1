param(
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,

    [Parameter(Mandatory = $true)]
    [string]$LocalRoot
)

$ErrorActionPreference = "Stop"

$assetRoot = Join-Path $RepoRoot "assets-local"
$rezRoot = Join-Path $LocalRoot "gameassets\rez"

Add-Type -AssemblyName System.IO.Compression.FileSystem

function Copy-ZipEntry {
    param(
        [Parameter(Mandatory = $true)]$Zip,
        [Parameter(Mandatory = $true)]$Entry,
        [Parameter(Mandatory = $true)][string]$DestinationRelative
    )

    $destination = Join-Path $rezRoot $DestinationRelative
    $parent = Split-Path -Parent $destination
    New-Item -ItemType Directory -Force -Path $parent | Out-Null

    [System.IO.Compression.ZipFileExtensions]::ExtractToFile(
        $Entry,
        $destination,
        $true)

    Write-Host "[OK] AK-47: $($Entry.FullName) -> $DestinationRelative"
}

function Find-ArchiveEntryByLeaf {
    param(
        [Parameter(Mandatory = $true)]$Zip,
        [Parameter(Mandatory = $true)][string]$LeafName
    )

    return $Zip.Entries |
        Where-Object { $_.Name -ieq $LeafName } |
        Select-Object -First 1
}

$akPattern = '(?i)AK[-_ ]?47'
$gunsPath = Join-Path $assetRoot "Guns.zip"
$gunsHHPath = Join-Path $assetRoot "GunsHH.zip"

if (Test-Path -LiteralPath $gunsPath) {
    Write-Host "[UPDATE] Discovering Combat Arms AK-47 player-view assets..."

    $zip = [System.IO.Compression.ZipFile]::OpenRead($gunsPath)
    try {
        $pvModel = $zip.Entries |
            Where-Object {
                $_.FullName -match '(?i)GUNS_M_PV' -and
                $_.Name -match $akPattern -and
                $_.Name -match '(?i)\.LTB$' -and
                $_.Name -notmatch '(?i)^ANI_'
            } |
            Sort-Object FullName |
            Select-Object -First 1

        if ($pvModel) {
            $pvRelative = "Weapons\primary_m_pv\AK47_PV.LTB"
            Copy-ZipEntry -Zip $zip -Entry $pvModel -DestinationRelative $pvRelative

            $pvPath = Join-Path $rezRoot $pvRelative
            $modelAscii = [System.Text.Encoding]::ASCII.GetString(
                [System.IO.File]::ReadAllBytes($pvPath))

            $aniEntry = $null
            $aniMatch = [regex]::Match(
                $modelAscii,
                '(?i)ANI_[A-Za-z0-9_.\-]+\.LTB')

            if ($aniMatch.Success) {
                $aniEntry = Find-ArchiveEntryByLeaf -Zip $zip -LeafName $aniMatch.Value
            }

            if (-not $aniEntry) {
                $aniEntry = $zip.Entries |
                    Where-Object {
                        $_.Name -match '(?i)^ANI_.*\.LTB$' -and
                        $_.FullName -match $akPattern
                    } |
                    Sort-Object FullName |
                    Select-Object -First 1
            }

            if ($aniEntry) {
                Copy-ZipEntry -Zip $zip -Entry $aniEntry -DestinationRelative "Weapons\primary_m_pv\AK47_ANI.LTB"
            }
            else {
                Write-Host "[SKIP] AK-47 animation child was not found in Guns.zip"
            }

            $pvTexture = $zip.Entries |
                Where-Object {
                    $_.FullName -match '(?i)GUNS_T_PV' -and
                    $_.Name -match $akPattern -and
                    $_.Name -match '(?i)\.DTX$'
                } |
                Sort-Object FullName |
                Select-Object -First 1

            if (-not $pvTexture) {
                $textureMatch = [regex]::Match(
                    $modelAscii,
                    '(?i)[A-Za-z0-9_.\-]*AK[A-Za-z0-9_.\-]*\.DTX')

                if ($textureMatch.Success) {
                    $pvTexture = Find-ArchiveEntryByLeaf -Zip $zip -LeafName $textureMatch.Value
                }
            }

            if ($pvTexture) {
                Copy-ZipEntry -Zip $zip -Entry $pvTexture -DestinationRelative "Weapons\primary_t\AK47_PV.DTX"
            }
            else {
                Write-Host "[SKIP] AK-47 player-view texture was not found in Guns.zip"
            }

            foreach ($soundName in @("FIRE", "SELECT", "RELOAD")) {
                $soundEntry = $zip.Entries |
                    Where-Object {
                        $_.FullName -match $akPattern -and
                        $_.Name -match ("(?i)^" + $soundName + ".*\.WAV$")
                    } |
                    Sort-Object FullName |
                    Select-Object -First 1

                if ($soundEntry) {
                    Copy-ZipEntry -Zip $zip -Entry $soundEntry -DestinationRelative ("Weapons\primary_snd\AK47\" + $soundName + ".WAV")
                }
                else {
                    Write-Host "[SKIP] AK-47 $soundName sound was not found in Guns.zip"
                }
            }
        }
        else {
            Write-Host "[SKIP] AK-47 player-view model was not found in Guns.zip"
        }
    }
    finally {
        $zip.Dispose()
    }
}
else {
    Write-Host "[SKIP] Guns.zip not present - AK-47 player-view assets unavailable"
}

if (Test-Path -LiteralPath $gunsHHPath) {
    Write-Host "[UPDATE] Discovering Combat Arms AK-47 world assets..."

    $zip = [System.IO.Compression.ZipFile]::OpenRead($gunsHHPath)
    try {
        $hhModel = $zip.Entries |
            Where-Object {
                $_.FullName -match '(?i)GUNS_M_HH' -and
                $_.Name -match $akPattern -and
                $_.Name -match '(?i)\.LTB$'
            } |
            Sort-Object FullName |
            Select-Object -First 1

        if ($hhModel) {
            Copy-ZipEntry -Zip $zip -Entry $hhModel -DestinationRelative "Weapons\primary_m_hh\HH_AK-47.LTB"
        }
        else {
            Write-Host "[SKIP] AK-47 world model was not found in GunsHH.zip"
        }

        $hhTexture = $zip.Entries |
            Where-Object {
                $_.FullName -match '(?i)GUNS_T_HH' -and
                $_.Name -match $akPattern -and
                $_.Name -match '(?i)\.DTX$'
            } |
            Sort-Object FullName |
            Select-Object -First 1

        if ($hhTexture) {
            Copy-ZipEntry -Zip $zip -Entry $hhTexture -DestinationRelative "Weapons\primary_t\HH_AK-47.DTX"
        }
        else {
            Write-Host "[SKIP] AK-47 world texture was not found in GunsHH.zip"
        }
    }
    finally {
        $zip.Dispose()
    }
}
else {
    Write-Host "[SKIP] GunsHH.zip not present - AK-47 world assets unavailable"
}

Write-Host "[OK] AK-47 local asset staging complete."
