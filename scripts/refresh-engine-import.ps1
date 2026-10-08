param(
    [Parameter(Mandatory = $true)]
    [string]$RepoRoot,

    [Parameter(Mandatory = $true)]
    [string]$LocalRoot
)

$ErrorActionPreference = "Stop"

$importsRoot = Join-Path $RepoRoot "imports"
$engineRoot = Join-Path $LocalRoot "imports\engine"
$requiredSource = Join-Path $engineRoot "clientfx\clientfx.cpp"
$markerPath = Join-Path $engineRoot ".fireteam-engine-source.sha256"

$candidates = @(
    Get-ChildItem -LiteralPath $importsRoot -Filter "EngineMissing*.zip" -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending
)

if ($candidates.Count -eq 0) {
    if (Test-Path -LiteralPath $requiredSource) {
        Write-Host "[OK] Full ClientFX source already present in local engine import."
        exit 0
    }

    throw "Full ClientFX source is missing locally and no imports\EngineMissing*.zip archive was found."
}

$archive = $candidates[0]
# Hashing a multi-GB EngineMissing zip on EVERY build is unnecessary.
# First compare file identity, byte length and UTC write time. If any of
# these change, fall back to the original SHA256 integrity check below.
$metadataPath = Join-Path $engineRoot ".fireteam-engine-source.metadata"
$archiveMetadata = "$($archive.FullName)|$($archive.Length)|$($archive.LastWriteTimeUtc.Ticks)"
if ((Test-Path -LiteralPath $requiredSource) -and
    (Test-Path -LiteralPath $markerPath) -and
    (Test-Path -LiteralPath $metadataPath) -and
    ([System.IO.File]::ReadAllText($metadataPath).Trim() -ceq $archiveMetadata)) {
    Write-Host "[FAST] Jupiter engine archive unchanged; skipping multi-GB SHA256 scan."
    exit 0
}

$archiveHash = (Get-FileHash -LiteralPath $archive.FullName -Algorithm SHA256).Hash
$storedHash = ""
if (Test-Path -LiteralPath $markerPath) {
    $storedHash = (Get-Content -LiteralPath $markerPath -Raw).Trim()
}

$needsRefresh = -not (Test-Path -LiteralPath $requiredSource)
if (-not $needsRefresh -and $storedHash -and $storedHash -ne $archiveHash) {
    $needsRefresh = $true
}

if ($needsRefresh) {
    Write-Host "[UPDATE] Refreshing engine source from $($archive.Name)..."
    New-Item -ItemType Directory -Force -Path $engineRoot | Out-Null
    Expand-Archive -LiteralPath $archive.FullName -DestinationPath $engineRoot -Force

    if (-not (Test-Path -LiteralPath $requiredSource)) {
        throw "Engine archive extracted, but clientfx\clientfx.cpp is still missing. Archive layout is not recognized."
    }

    [System.IO.File]::WriteAllText($markerPath, $archiveHash)
    Write-Host "[OK] Full Jupiter ClientFX plugin source refreshed."
}
else {
    if (-not $storedHash) {
        [System.IO.File]::WriteAllText($markerPath, $archiveHash)
    }
    Write-Host "[OK] Full Jupiter ClientFX plugin source already current."
}

# Save only after hash validation and any required extraction succeeded.
[System.IO.File]::WriteAllText($metadataPath, $archiveMetadata)
Write-Host "[INFO] Engine source archive: $($archive.Name)"
