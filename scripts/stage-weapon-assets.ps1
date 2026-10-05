param([Parameter(Mandatory=$true)][string]$RepoRoot,[Parameter(Mandatory=$true)][string]$LocalRoot)
$ErrorActionPreference = "Stop"
$assetRoot = Join-Path $RepoRoot "assets-local"
$rezRoot = Join-Path $LocalRoot "gameassets\rez"
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Copy-ZipEntry {
  param($Entry,[string]$DestinationRelative)
  if(-not $Entry) { return $false }
  $destination = Join-Path $rezRoot $DestinationRelative
  $parent = Split-Path -Parent $destination
  New-Item -ItemType Directory -Force -Path $parent | Out-Null
  [System.IO.Compression.ZipFileExtensions]::ExtractToFile($Entry,$destination,$true)
  Write-Host "[OK] $($Entry.FullName) -> $DestinationRelative"
  return $true
}

function Find-Leaf {
  param($Zip,[string[]]$Candidates)
  foreach($candidate in $Candidates) {
    $entry = $Zip.Entries | Where-Object { $_.Name -ieq $candidate } | Select-Object -First 1
    if($entry) { return $entry }
  }
  return $null
}

function Find-TokenAsset {
  param($Zip,[string[]]$Tokens,[string]$PathPattern,[string]$Extension,[string]$ExcludePattern)
  foreach($token in $Tokens) {
    $entry = $Zip.Entries | Where-Object {
      $_.FullName -match $PathPattern -and
      $_.Name -match [regex]::Escape($token) -and
      $_.Name -match ("(?i)\." + [regex]::Escape($Extension) + "$") -and
      (-not $ExcludePattern -or $_.Name -notmatch $ExcludePattern)
    } | Sort-Object FullName | Select-Object -First 1
    if($entry) { return $entry }
  }
  return $null
}

function Add-TextureAliases {
  param([string]$SourceRelative)
  $source = Join-Path $rezRoot $SourceRelative
  if(-not (Test-Path -LiteralPath $source)) { return }

  $leaf = Split-Path -Leaf $source
  Copy-Item -LiteralPath $source -Destination (Join-Path $rezRoot $leaf) -Force

  $modelTextures = Join-Path $rezRoot "ModelTextures"
  New-Item -ItemType Directory -Force -Path $modelTextures | Out-Null
  Copy-Item -LiteralPath $source -Destination (Join-Path $modelTextures $leaf) -Force
}

function Find-AnimationCompanion {
  param($Zip,[string[]]$Tokens)
  foreach($token in $Tokens) {
    $entry = $Zip.Entries | Where-Object { $_.FullName -match "(?i)GUNS_M_PV" -and $_.Name -match [regex]::Escape($token) -and $_.Name -match "(?i)(ANIBASE|^ANI_).*\.LTB$" -and $_.Name -notmatch "(?i)I_INFO|HH_" } | Sort-Object FullName | Select-Object -First 1
    if($entry) { return $entry }
  }
  return $null
}

function Find-Sound {
  param($Zip,[string[]]$Tokens,[string]$Stem)
  foreach($token in $Tokens) {
    $entry = $Zip.Entries | Where-Object { $_.FullName -match "(?i)GUNS_SND" -and $_.FullName -match [regex]::Escape($token) -and $_.Name -match ("(?i)^" + [regex]::Escape($Stem) + ".*\.WAV$") } | Sort-Object FullName | Select-Object -First 1
    if($entry) { return $entry }
  }
  return $null
}

function Stage-PVWeapon {
  param($Zip,[string]$Label,[string[]]$Tokens,[string[]]$ModelLeaves,[string[]]$TextureLeaves,[string[]]$AnimationLeaves,[string]$ModelDest,[string]$TextureDest,[string]$AnimationDest,[string]$SoundDest)
  Write-Host "[WEAPON] $Label player-view assets"

  $model = Find-Leaf $Zip $ModelLeaves
  if(-not $model) {
    $model = Find-TokenAsset $Zip $Tokens "(?i)GUNS_M_PV" "LTB" "(?i)(^ANI_|ANIBASE|I_INFO|HH_)"
    if($model) { Write-Host "[AUTO] $Label PV model fallback: $($model.FullName)" }
  }
  if(-not (Copy-ZipEntry $model $ModelDest)) { Write-Host "[SKIP] $Label PV model not found." }

  $texture = Find-Leaf $Zip $TextureLeaves
  if(-not $texture) {
    $texture = Find-TokenAsset $Zip $Tokens "(?i)GUNS_T_PV" "DTX" "(?i)(I_INFO|HH_)"
    if($texture) { Write-Host "[AUTO] $Label PV texture fallback: $($texture.FullName)" }
  }
  if(Copy-ZipEntry $texture $TextureDest) {
    Add-TextureAliases $TextureDest
  } else {
    Write-Host "[SKIP] $Label PV texture not found."
  }

  $animation = Find-Leaf $Zip $AnimationLeaves
  if(-not $animation) { $animation = Find-AnimationCompanion $Zip $Tokens }
  if(-not (Copy-ZipEntry $animation $AnimationDest)) { Write-Host "[INFO] $Label has no separate animation companion; model animations will be used." }

  foreach($soundName in @("FIRE","SELECT","RELOAD")) {
    $sound = Find-Sound $Zip $Tokens $soundName
    if(-not (Copy-ZipEntry $sound ($SoundDest + "\" + $soundName + ".WAV"))) { Write-Host "[SKIP] $Label $soundName sound not found." }
  }
}

function Stage-HHWeapon {
  param($Zip,[string]$Label,[string[]]$Tokens,[string[]]$ModelLeaves,[string[]]$TextureLeaves,[string]$ModelDest,[string]$TextureDest)
  Write-Host "[WEAPON] $Label handheld/world assets"

  $model = Find-Leaf $Zip $ModelLeaves
  if(-not $model) {
    $model = Find-TokenAsset $Zip $Tokens "(?i)GUNS_M_HH" "LTB" "(?i)(I_INFO|PV_)"
    if($model) { Write-Host "[AUTO] $Label HH model fallback: $($model.FullName)" }
  }
  if(-not (Copy-ZipEntry $model $ModelDest)) { Write-Host "[SKIP] $Label HH model not found." }

  $texture = Find-Leaf $Zip $TextureLeaves
  if(-not $texture) {
    $texture = Find-TokenAsset $Zip $Tokens "(?i)GUNS_T_HH" "DTX" "(?i)(I_INFO|PV_)"
    if($texture) { Write-Host "[AUTO] $Label HH texture fallback: $($texture.FullName)" }
  }
  if(Copy-ZipEntry $texture $TextureDest) {
    Add-TextureAliases $TextureDest
  } else {
    Write-Host "[SKIP] $Label HH texture not found."
  }
}

$gunsPath = Join-Path $assetRoot "Guns.zip"
$gunsHHPath = Join-Path $assetRoot "GunsHH.zip"

if(Test-Path -LiteralPath $gunsPath) {
  $zip = [System.IO.Compression.ZipFile]::OpenRead($gunsPath)
  try {
    Stage-PVWeapon $zip "AK-47" @("AK-47","AK47") @("PV_AR_AK47_SH.LTB") @("PV_AK-47.DTX") @("AK47_ANIBASE.LTB") "Weapons\primary_m_pv\PV_AR_AK47_SH.LTB" "Weapons\primary_t\AK47_PV.DTX" "Weapons\primary_m_pv\AK47_ANIBASE.LTB" "Weapons\primary_snd\AK47"
    Stage-PVWeapon $zip "Beretta M92FS" @("Beretta_M92FS","M92FS") @("PVMLA_BERETTA_M92FS.LTB") @("PV_BERETTA_M92FS.DTX") @("COLT_MEU_ANIBASE-1.LTB") "Weapons\secondary_m_pv\PVMLA_BERETTA_M92FS.LTB" "Weapons\secondary_t\PV_BERETTA_M92FS.DTX" "Weapons\secondary_m_pv\COLT_MEU_ANIBASE-1.LTB" "Weapons\secondary_snd\BERETTA_M92FS"
    Stage-PVWeapon $zip "M67" @("M67") @("PVMLA_M67.LTB") @("PV_M67.DTX") @("M67_ANIBASE-1.LTB") "Weapons\grenade_m_pv\PVMLA_M67.LTB" "Weapons\grenade_t\PV_M67.DTX" "Weapons\grenade_m_pv\M67_ANIBASE-1.LTB" "Weapons\grenade_snd\M67"
    Stage-PVWeapon $zip "LAW" @("LAW") @("PVMLA_LAW.LTB") @("PV_LAW.DTX") @("LAW_ANIBASE.LTB") "Weapons\special_m_pv\PVMLA_LAW.LTB" "Weapons\special_t\PV_LAW.DTX" "Weapons\special_m_pv\LAW_ANIBASE.LTB" "Weapons\special_snd\LAW"
    Write-Host "[WEAPON] CA projectile assets"
    $missileModel = $zip.Entries | Where-Object { $_.FullName -ieq "GUNS_PT/MODELS/MISSILE.LTB" } | Select-Object -First 1
    Copy-ZipEntry $missileModel "Weapons\projectile_m\MISSILE.LTB" | Out-Null

    $missileSkin = $zip.Entries | Where-Object { $_.FullName -ieq "GUNS_PT/SKINS/MISSILE.DTX" } | Select-Object -First 1
    if(Copy-ZipEntry $missileSkin "Weapons\projectile_t\MISSILE.DTX") {
      Add-TextureAliases "Weapons\projectile_t\MISSILE.DTX"
    }

    $projectileSound = $zip.Entries | Where-Object { $_.FullName -ieq "GUNS_PT/SND/GRENADE.WAV" } | Select-Object -First 1
    Copy-ZipEntry $projectileSound "Weapons\projectile_snd\GRENADE.WAV" | Out-Null

    $explosionSound = $zip.Entries | Where-Object { $_.FullName -ieq "GUNS_SND_IMPACTS/EXPLOSIONS/GREN.WAV" } | Select-Object -First 1
    Copy-ZipEntry $explosionSound "Weapons\explosive_snd\GREN.WAV" | Out-Null
  } finally { $zip.Dispose() }
} else { Write-Host "[SKIP] Guns.zip is not present in assets-local." }

if(Test-Path -LiteralPath $gunsHHPath) {
  $zip = [System.IO.Compression.ZipFile]::OpenRead($gunsHHPath)
  try {
    Stage-HHWeapon $zip "AK-47" @("AK-47","AK47") @("HH_AK-47.LTB","HH_AR_AK47_SH.LTB") @("HH_AK-47.DTX","HH_AR_AK47.DTX") "Weapons\primary_m_hh\HH_AK-47.LTB" "Weapons\primary_t\HH_AK-47.DTX"
    Stage-HHWeapon $zip "Beretta M92FS" @("Beretta_M92FS","M92FS") @("HH_PST_M92FS_SH.LTB","HH_BERETTA_M92FS.LTB") @("HH_Beretta_M92FS.DTX","HH_BERETTA_M92FS.DTX") "Weapons\secondary_m_hh\HH_BERETTA_M92FS.LTB" "Weapons\secondary_t\HH_BERETTA_M92FS.DTX"
    Stage-HHWeapon $zip "M67" @("M67") @("HH_Throwing_M67Frag_SH.LTB","HH_M67.LTB") @("HH_M67.DTX") "Weapons\grenade_m_hh\HH_M67.LTB" "Weapons\grenade_t\HH_M67.DTX"
    Stage-HHWeapon $zip "LAW" @("LAW") @("HH_LCH_LAW_SH.LTB","HH_LAW.LTB") @("HH_LAW.DTX") "Weapons\special_m_hh\HH_LAW.LTB" "Weapons\special_t\HH_LAW.DTX"
  } finally { $zip.Dispose() }
} else { Write-Host "[SKIP] GunsHH.zip is not present in assets-local." }

Write-Host "[OK] Fireteam five-slot weapon asset staging complete."