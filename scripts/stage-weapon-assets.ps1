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
    Stage-PVWeapon $zip "AK-47" @("AK-47","AK47") @("PVMLA_AK-47.LTB") @("PV_AK-47.DTX") @("AK47_ANIBASE.LTB") "Weapons\primary_m_pv\PVMLA_AK-47.LTB" "Weapons\primary_t\PV_AK-47.DTX" "Weapons\primary_m_pv\AK47_ANIBASE.LTB" "Weapons\primary_snd\AK47"
    Stage-PVWeapon $zip "Beretta M92FS" @("BERETTA_M92FS","M92FS","BERETTA") @("PVMLA_BERETTA_M92FS.LTB") @("PV_BERETTA_M92FS.DTX") @("BERETTA_ANIBASE.LTB") "Weapons\secondary_m_pv\PVMLA_BERETTA_M92FS.LTB" "Weapons\secondary_t\PV_BERETTA_M92FS.DTX" "Weapons\secondary_m_pv\BERETTA_ANIBASE.LTB" "Weapons\secondary_snd\BERETTA_M92FS"
    Stage-PVWeapon $zip "Colt 1911A1 MEU" @("COLT_1911A1_MEU") @("PVMLA_COLT_1911A1_MEU.LTB") @("PV_COLT_1911A1_MEU.DTX") @("COLT_MEU_ANIBASE-1.LTB","COLT_MEU_ANIBASE.LTB") "Weapons\sidearm2_m_pv\PVMLA_COLT_1911A1_MEU.LTB" "Weapons\sidearm2_t\PV_COLT_1911A1_MEU.DTX" "Weapons\sidearm2_m_pv\COLT_MEU_ANIBASE-1.LTB" "Weapons\sidearm2_snd\COLT_1911A1_MEU"
    Stage-PVWeapon $zip "L96A1" @("L96A1") @("PVMLA_L96A1.LTB") @("PV_L96A1.DTX") @("L96A1_ANIBASE.LTB") "Weapons\sniper_m_pv\PVMLA_L96A1.LTB" "Weapons\sniper_t\PV_L96A1.DTX" "Weapons\sniper_m_pv\L96A1_ANIBASE.LTB" "Weapons\sniper_snd\L96A1"
  } finally { $zip.Dispose() }
} else { Write-Host "[SKIP] Guns.zip is not present in assets-local." }

if(Test-Path -LiteralPath $gunsHHPath) {
  $zip = [System.IO.Compression.ZipFile]::OpenRead($gunsHHPath)
  try {
    Stage-HHWeapon $zip "AK-47" @("AK-47","AK47") @("HH_AK-47.LTB","HH_AR_AK47_SH.LTB") @("HH_AK-47.DTX","HH_AR_AK47.DTX") "Weapons\primary_m_hh\HH_AK-47.LTB" "Weapons\primary_t\HH_AK-47.DTX"
    Stage-HHWeapon $zip "Beretta M92FS" @("Beretta_M92FS","M92FS") @("HH_PST_M92FS_SH.LTB","HH_BERETTA_M92FS.LTB") @("HH_Beretta_M92FS.DTX","HH_BERETTA_M92FS.DTX") "Weapons\secondary_m_hh\HH_BERETTA_M92FS.LTB" "Weapons\secondary_t\HH_BERETTA_M92FS.DTX"
    Stage-HHWeapon $zip "Colt 1911A1 MEU" @("COLT_1911A1_MEU") @("HH_COLT_1911A1_MEU.LTB") @("HH_COLT_1911A1_MEU.DTX") "Weapons\sidearm2_m_hh\HH_COLT_1911A1_MEU.LTB" "Weapons\sidearm2_t\HH_COLT_1911A1_MEU.DTX"
    Stage-HHWeapon $zip "L96A1" @("L96A1") @("HH_L96A1.LTB") @("HH_L96A1.DTX") "Weapons\sniper_m_hh\HH_L96A1.LTB" "Weapons\sniper_t\HH_L96A1.DTX"
  } finally { $zip.Dispose() }
} else { Write-Host "[SKIP] GunsHH.zip is not present in assets-local." }

$weaponImports = Join-Path $assetRoot "WeaponImports"
if(Test-Path -LiteralPath $weaponImports) {
  Write-Host "[WEAPON] Staging enabled launcher/catalog imports..."

  $weaponsCfg = Join-Path $RepoRoot "config\weapons.cfg"
  $catalogEntries = @{}
  $currentCatalog = $null

  if(Test-Path -LiteralPath $weaponsCfg) {
    foreach($rawLine in Get-Content -LiteralPath $weaponsCfg) {
      $line = $rawLine.Trim()

      if($line -match '^\[catalog\.(.+)\]$') {
        $currentCatalog = $Matches[1]
        if(-not $catalogEntries.ContainsKey($currentCatalog)) {
          $catalogEntries[$currentCatalog] = @{
            id = $currentCatalog
            enabled = $false
          }
        }
        continue
      }

      if($line -match '^\[') {
        $currentCatalog = $null
        continue
      }

      if(-not $currentCatalog -or
         -not $line -or
         $line.StartsWith('#') -or
         $line.StartsWith(';')) {
        continue
      }

      if($line -match '^id\s*=\s*(.+)$') {
        $catalogEntries[$currentCatalog].id = $Matches[1].Trim()
      } elseif($line -match '^enabled\s*=\s*(1|true|yes|on)\s*$') {
        $catalogEntries[$currentCatalog].enabled = $true
      } elseif($line -match '^enabled\s*=') {
        $catalogEntries[$currentCatalog].enabled = $false
      }
    }
  }

  $catalogImportRoot = Join-Path $weaponImports "Weapons\imported"
  $runtimeImportRoot = Join-Path $rezRoot "Weapons\imported"

  # BUILT preserves unrelated files, but imported weapon enable/disable needs
  # deterministic output. Only this launcher-owned subtree is rebuilt cleanly.
  if(Test-Path -LiteralPath $runtimeImportRoot) {
    Remove-Item -LiteralPath $runtimeImportRoot -Recurse -Force
  }

  if($catalogEntries.Count -gt 0) {
    foreach($catalogName in $catalogEntries.Keys) {
      $entry = $catalogEntries[$catalogName]
      if(-not $entry.enabled) {
        continue
      }

      $weaponId = [string]$entry.id
      $sourceWeapon = Join-Path $catalogImportRoot $weaponId
      if(-not (Test-Path -LiteralPath $sourceWeapon)) {
        Write-Host "[SKIP] Enabled catalog weapon has no imported assets: $weaponId"
        continue
      }

      $destWeapon = Join-Path $runtimeImportRoot $weaponId
      New-Item -ItemType Directory -Force -Path $destWeapon | Out-Null
      Copy-Item -Path (Join-Path $sourceWeapon "*") -Destination $destWeapon -Recurse -Force
      Write-Host "[OK] Catalog weapon -> Weapons\imported\$weaponId"
    }
  } else {
    # Compatibility with imports created by older launcher builds.
    Copy-Item -Path (Join-Path $weaponImports "*") -Destination $rezRoot -Recurse -Force
  }
}

$sharedCaRoot = Join-Path $weaponImports "Weapons\ca"
if(Test-Path -LiteralPath $sharedCaRoot) {
  Write-Host "[WEAPON] Staging shared Combat Arms importer assets..."
  $sharedCaDest = Join-Path $rezRoot "Weapons\ca"
  New-Item -ItemType Directory -Force -Path $sharedCaDest | Out-Null
  Copy-Item -Path (Join-Path $sharedCaRoot "*") -Destination $sharedCaDest -Recurse -Force
  Write-Host "[OK] Shared Combat Arms importer assets staged."
}

Write-Host "[OK] Fireteam stock/custom firearm asset staging complete."