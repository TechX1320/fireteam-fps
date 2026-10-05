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
  if(-not (Copy-ZipEntry $model $ModelDest)) { Write-Host "[SKIP] $Label PV model not found." }
  $texture = Find-Leaf $Zip $TextureLeaves
  if(-not (Copy-ZipEntry $texture $TextureDest)) { Write-Host "[SKIP] $Label PV texture not found." }
  $animation = Find-Leaf $Zip $AnimationLeaves
  if(-not $animation) { $animation = Find-AnimationCompanion $Zip $Tokens }
  if(-not (Copy-ZipEntry $animation $AnimationDest)) { Write-Host "[INFO] $Label has no separate animation companion; model animations will be used." }
  foreach($soundName in @("FIRE","SELECT","RELOAD")) {
    $sound = Find-Sound $Zip $Tokens $soundName
    if(-not (Copy-ZipEntry $sound ($SoundDest + "\" + $soundName + ".WAV"))) { Write-Host "[SKIP] $Label $soundName sound not found." }
  }
}

function Stage-HHWeapon {
  param($Zip,[string]$Label,[string[]]$ModelLeaves,[string[]]$TextureLeaves,[string]$ModelDest,[string]$TextureDest)
  Write-Host "[WEAPON] $Label handheld/world assets"
  $model = Find-Leaf $Zip $ModelLeaves
  if(-not (Copy-ZipEntry $model $ModelDest)) { Write-Host "[SKIP] $Label HH model not found." }
  $texture = Find-Leaf $Zip $TextureLeaves
  if(-not (Copy-ZipEntry $texture $TextureDest)) { Write-Host "[SKIP] $Label HH texture not found." }
}

$gunsPath = Join-Path $assetRoot "Guns.zip"
$gunsHHPath = Join-Path $assetRoot "GunsHH.zip"

if(Test-Path -LiteralPath $gunsPath) {
  $zip = [System.IO.Compression.ZipFile]::OpenRead($gunsPath)
  try {
    Stage-PVWeapon $zip "AK-47" @("AK-47","AK47") @("PV_AR_AK47_SH.LTB") @("PV_AK-47.DTX") @("AK47_ANIBASE.LTB") "Weapons\primary_m_pv\PV_AR_AK47_SH.LTB" "Weapons\primary_t\AK47_PV.DTX" "Weapons\primary_m_pv\AK47_ANIBASE.LTB" "Weapons\primary_snd\AK47"
    Stage-PVWeapon $zip "Beretta M92FS" @("Beretta_M92FS","M92FS") @("PV_PST_M92FS_SH.LTB") @("PV_Beretta_M92FS.DTX") $null "Weapons\secondary_m_pv\BERETTA_M92FS_PV.LTB" "Weapons\secondary_t\BERETTA_M92FS_PV.DTX" "Weapons\secondary_m_pv\BERETTA_M92FS_ANI.LTB" "Weapons\secondary_snd\BERETTA_M92FS"
    Stage-PVWeapon $zip "M67" @("M67") @("PV_Throwing_M67Frag_SH.LTB") @("PV_M67.DTX") $null "Weapons\grenade_m_pv\M67_PV.LTB" "Weapons\grenade_t\M67_PV.DTX" "Weapons\grenade_m_pv\M67_ANI.LTB" "Weapons\grenade_snd\M67"
    Stage-PVWeapon $zip "LAW" @("LAW") @("PV_LCH_LAW_SH.LTB") @("PV_LAW.DTX") $null "Weapons\special_m_pv\LAW_PV.LTB" "Weapons\special_t\LAW_PV.DTX" "Weapons\special_m_pv\LAW_ANI.LTB" "Weapons\special_snd\LAW"
  } finally { $zip.Dispose() }
} else { Write-Host "[SKIP] Guns.zip is not present in assets-local." }

if(Test-Path -LiteralPath $gunsHHPath) {
  $zip = [System.IO.Compression.ZipFile]::OpenRead($gunsHHPath)
  try {
    Stage-HHWeapon $zip "AK-47" @("HH_AK-47.LTB","HH_AR_AK47_SH.LTB") @("HH_AK-47.DTX","HH_AR_AK47.DTX") "Weapons\primary_m_hh\HH_AK-47.LTB" "Weapons\primary_t\HH_AK-47.DTX"
    Stage-HHWeapon $zip "Beretta M92FS" @("HH_PST_M92FS_SH.LTB","HH_BERETTA_M92FS.LTB") @("HH_Beretta_M92FS.DTX","HH_BERETTA_M92FS.DTX") "Weapons\secondary_m_hh\HH_BERETTA_M92FS.LTB" "Weapons\secondary_t\HH_BERETTA_M92FS.DTX"
    Stage-HHWeapon $zip "M67" @("HH_Throwing_M67Frag_SH.LTB","HH_M67.LTB") @("HH_M67.DTX") "Weapons\grenade_m_hh\HH_M67.LTB" "Weapons\grenade_t\HH_M67.DTX"
    Stage-HHWeapon $zip "LAW" @("HH_LCH_LAW_SH.LTB","HH_LAW.LTB") @("HH_LAW.DTX") "Weapons\special_m_hh\HH_LAW.LTB" "Weapons\special_t\HH_LAW.DTX"
  } finally { $zip.Dispose() }
} else { Write-Host "[SKIP] GunsHH.zip is not present in assets-local." }

Write-Host "[OK] Fireteam five-slot weapon asset staging complete."