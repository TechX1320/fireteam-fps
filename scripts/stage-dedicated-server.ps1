param(
    [Parameter(Mandatory = $true)][string]$RepoRoot
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path -LiteralPath $RepoRoot).Path
$built = Join-Path $repo "BUILT"
$dedicated = Join-Path $built "Dedicated"
$rezSource = Join-Path $built "rez"
$rezLink = Join-Path $dedicated "rez"

$files = @(
    "Engine.REZ",
    "server.dll",
    "LTMsg.dll",
    "SndDrv.dll",
    "FireteamDedicatedServer.exe"
)

foreach($file in $files) {
    $source = Join-Path $built $file
    if(-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing $source. Run build.cmd then build-dedicated.cmd."
    }
}
if(-not (Test-Path -LiteralPath (Join-Path $rezSource "object.lto") -PathType Leaf)) {
    throw "Missing BUILT\rez\object.lto. Run build.cmd first."
}

New-Item -ItemType Directory -Path $dedicated -Force | Out-Null
foreach($file in $files) {
    Copy-Item -LiteralPath (Join-Path $built $file) -Destination (Join-Path $dedicated $file) -Force
}

# The dedicated process needs its own config directory, but the (potentially
# multi-gigabyte) resources should not be copied a second time.
# An NTFS junction is a directory reference, not a file download or clone.
if(Test-Path -LiteralPath $rezLink) {
    $entry = Get-Item -LiteralPath $rezLink -Force
    if($entry.LinkType -ne "Junction") {
        throw "$rezLink already exists and is not an NTFS junction. Refusing to modify it."
    }
} else {
    New-Item -ItemType Junction -Path $rezLink -Target $rezSource | Out-Null
}
if(-not (Test-Path -LiteralPath (Join-Path $rezLink "object.lto") -PathType Leaf)) {
    throw "Dedicated rez junction does not resolve to the built object module."
}

$configDir = Join-Path $dedicated "config"
New-Item -ItemType Directory -Path $configDir -Force | Out-Null
$configFiles = @(
    "weapons.cfg", "infected.cfg", "player.cfg", "characters.cfg",
    "difficulties.cfg", "powerups.cfg", "loadouts.cfg", "weapon-library.cfg"
)
foreach($name in $configFiles) {
    $src = Join-Path $built ("config\" + $name)
    if(Test-Path -LiteralPath $src -PathType Leaf) {
        Copy-Item -LiteralPath $src -Destination (Join-Path $configDir $name) -Force
    }
}

# The server's gameplay settings MUST NOT change when a player launches
# Lithtech.exe from BUILT (GameLaunchService writes BUILT/config/session.cfg).
$sessionFile = Join-Path $configDir "session.cfg"
if(-not (Test-Path -LiteralPath $sessionFile -PathType Leaf)) {
    @(
        "difficulty=4"
        "first_round_prep=45"
        "cabin_spawn_guard=1"
    ) | Set-Content -LiteralPath $sessionFile -Encoding ASCII
}

Write-Host "[OK] Isolated dedicated server staged at $dedicated"
Write-Host "[OK] Shared resources: $rezLink (junction to $rezSource)"
Write-Host "[OK] Independent gameplay config: $sessionFile"
Write-Host "[INFO] No commercial assets copied into git or a second asset directory."
