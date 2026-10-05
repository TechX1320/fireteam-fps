param([Parameter(Mandatory=$true)][string]$RepoRoot,[Parameter(Mandatory=$true)][string]$LocalRoot)
$ErrorActionPreference = "Stop"
$assetRoot = Join-Path $RepoRoot "assets-local"
$rezRoot = Join-Path $LocalRoot "gameassets\\rez"
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Copy-ZipEntry {
  param($Zip,$Entry,[string]$DestinationRelative)
  $destination = Join-Path $rezRoot $DestinationRelative
  $parent = Split-Path -Parent $destination
  New-Item -ItemType Directory -Force -Path $parent | Out-Null
  [System.IO.Compression.ZipFileExtensions]::ExtractToFile($Entry,$destination,$true)
  Write-Host "[OK] Weapon asset: $($Entry.FullName) -> $DestinationRelative"
}

function Find-BestEntry {
  param($Zip,[string[]]$Tokens,[string]$Extension,[string]$PathPattern,[string]$PreferPattern,[string]$ExcludePattern)
  foreach($token in $Tokens) {
    $matches = @($Zip.Entries | Where-Object { $_.Name -match [regex]::Escape($token) -and $_.Name -match ("(?i)\\." + [regex]::Escape($Extension) + "$") -and (-not $PathPattern -or $_.FullName -match $PathPattern) -and (-not $ExcludePattern -or $_.Name -notmatch $ExcludePattern) } | Sort-Object FullName)
    if($matches.Count -eq 0) { continue }
    if($PreferPattern) { $preferred = $matches | Where-Object { $_.Name -match $PreferPattern } | Select-Object -First 1; if($preferred) { return $preferred } }
    return $matches[0]
  }
  return $null
}

function Find-SoundEntry {
  param($Zip,[string[]]$Tokens,[string]$Stem)
  foreach($token in $Tokens) {
    $entry = $Zip.Entries | Where-Object { $_.FullName -match [regex]::Escape($token) -and $_.Name -match ("(?i)^" + [regex]::Escape($Stem) + ".*\\.WAV$") } | Sort-Object FullName | Select-Object -First 1
    if($entry) { return $entry }
  }
  return $null
}

function Stage-PlayerViewWeapon {
  param($Zip,[string[]]$Tokens,[string]$ModelDest,[string]$AnimDest,[string]$TextureDest,[string]$SoundDest,[string]$ExactModelLeaf="",[string]$ExactAnimLeaf="",[string]$ExactTextureLeaf="")
  $model = $null
  if($ExactModelLeaf) { $model = $Zip.Entries | Where-Object { $_.Name -ieq $ExactModelLeaf } | Select-Object -First 1 }
  if(-not $model) { $model = Find-BestEntry $Zip $Tokens "LTB" "(?i)GUNS_M_PV" "(?i)^PV_" "(?i)ANI|ANIBASE|I_INFO|HH_" }
  if($model) { Copy-ZipEntry $Zip $model $ModelDest } else { Write-Host "[SKIP] No PV model found for $($Tokens -join '/')" }
  $anim = $null
  if($ExactAnimLeaf) { $anim = $Zip.Entries | Where-Object { $_.Name -ieq $ExactAnimLeaf } | Select-Object -First 1 }
  if(-not $anim) { $anim = Find-BestEntry $Zip $Tokens "LTB" "(?i)GUNS_M_PV" "(?i)ANIBASE|^ANI_" "(?i)I_INFO|HH_" }
  if($anim) { Copy-ZipEntry $Zip $anim $AnimDest } else { Write-Host "[SKIP] No PV animation companion found for $($Tokens -join '/')" }
  $texture = $null
  if($ExactTextureLeaf) { $texture = $Zip.Entries | Where-Object { $_.Name -ieq $ExactTextureLeaf } | Select-Object -First 1 }
  if(-not $texture) { $texture = Find-BestEntry $Zip $Tokens "DTX" "(?i)GUNS_T_PV" "(?i)^PV_" "" }
  if($texture) { Copy-ZipEntry $Zip $texture $TextureDest } else { Write-Host "[SKIP] No PV texture found for $($Tokens -join '/')" }
  foreach($sound in @("FIRE","SELECT","RELOAD")) { $entry = Find-SoundEntry $Zip $Tokens $sound; if($entry) { Copy-ZipEntry $Zip $entry ($SoundDest + "\\" + $sound + ".WAV") } else { Write-Host "[SKIP] No $sound sound found for $($Tokens -join '/')" } }
}

function Stage-WorldWeapon {
  param($Zip,[string]$ModelLeaf,[string]$TextureLeaf,[string]$ModelDest,[string]$TextureDest)
  $model = $Zip.Entries | Where-Object { $_.Name -ieq $ModelLeaf } | Select-Object -First 1
  if($model) { Copy-ZipEntry $Zip $model $ModelDest } else { Write-Host "[SKIP] Missing world model $ModelLeaf" }
  $texture = $Zip.Entries | Where-Object { $_.Name -ieq $TextureLeaf } | Select-Object -First 1
  if($texture) { Copy-ZipEntry $Zip $texture $TextureDest } else { Write-Host "[SKIP] Missing world texture $TextureLeaf" }
}

$gunsPath = Join-Path $assetRoot "Guns.zip"
$gunsHHPath = Join-Path $assetRoot "GunsHH.zip"
if(Test-Path -LiteralPath $gunsPath) {
  Write-Host "[UPDATE] Staging Combat Arms player-view weapon assets..."
  $zip = [System.IO.Compression.ZipFile]::OpenRead($gunsPath)
  try {
    Stage-PlayerViewWeapon $zip @("AK-47","AK47") "Weapons\\primary_m_pv\\PV_AR_AK47_SH.LTB" "Weapons\\primary_m_pv\\AK47_ANIBASE.LTB" "Weapons\\primary_t\\AK47_PV.DTX" "Weapons\\primary_snd\\AK47" "PV_AR_AK47_SH.LTB" "AK47_ANIBASE.LTB" "PV_AK-47.DTX"
    Stage-PlayerViewWeapon $zip @("BERETTA_M92FS","M92FS","BERETTA") "Weapons\\secondary_m_pv\\BERETTA_M92FS_PV.LTB" "Weapons\\secondary_m_pv\\BERETTA_M92FS_ANI.LTB" "Weapons\\secondary_t\\BERETTA_M92FS_PV.DTX" "Weapons\\secondary_snd\\BERETTA_M92FS"
    Stage-PlayerViewWeapon $zip @("M67") "Weapons\\grenade_m_pv\\M67_PV.LTB" "Weapons\\grenade_m_pv\\M67_ANI.LTB" "Weapons\\grenade_t\\M67_PV.DTX" "Weapons\\grenade_snd\\M67"
    Stage-PlayerViewWeapon $zip @("LAW") "Weapons\\special_m_pv\\LAW_PV.LTB" "Weapons\\special_m_pv\\LAW_ANI.LTB" "Weapons\\special_t\\LAW_PV.DTX" "Weapons\\special_snd\\LAW"
  } finally { $zip.Dispose() }
} else { Write-Host "[SKIP] Guns.zip not present - player-view firearm assets unavailable" }

if(Test-Path -LiteralPath $gunsHHPath) {
  Write-Host "[UPDATE] Staging Combat Arms world/hand weapon assets..."
  $zip = [System.IO.Compression.ZipFile]::OpenRead($gunsHHPath)
  try {
    Stage-WorldWeapon $zip "HH_AK-47.LTB" "HH_AK-47.DTX" "Weapons\\primary_m_hh\\HH_AK-47.LTB" "Weapons\\primary_t\\HH_AK-47.DTX"
    Stage-WorldWeapon $zip "HH_BERETTA_M92FS.LTB" "HH_BERETTA_M92FS.DTX" "Weapons\\secondary_m_hh\\HH_BERETTA_M92FS.LTB" "Weapons\\secondary_t\\HH_BERETTA_M92FS.DTX"
    Stage-WorldWeapon $zip "HH_M67.LTB" "HH_M67.DTX" "Weapons\\grenade_m_hh\\HH_M67.LTB" "Weapons\\grenade_t\\HH_M67.DTX"
    Stage-WorldWeapon $zip "HH_LAW.LTB" "HH_LAW.DTX" "Weapons\\special_m_hh\\HH_LAW.LTB" "Weapons\\special_t\\HH_LAW.DTX"
  } finally { $zip.Dispose() }
} else { Write-Host "[SKIP] GunsHH.zip not present - world weapon assets unavailable" }
Write-Host "[OK] Fireteam local weapon asset staging complete."