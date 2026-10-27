$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ('ft-config-tests-' + [Guid]::NewGuid().ToString('N'))
$source = Join-Path $testRoot 'source'
$runtime = Join-Path $testRoot 'runtime'
function Check([bool]$Condition,[string]$Message) { if(-not $Condition) { throw $Message } }
try {
    New-Item -ItemType Directory -Path $source,$runtime -Force | Out-Null
    foreach($name in @('infected','characters','difficulties','powerups')) {
        Copy-Item -LiteralPath (Join-Path $root "config/$name.cfg") -Destination (Join-Path $source "$name.cfg")
    }
    $target = Join-Path $runtime 'infected.cfg'
    $old = "[normal_common]`r`nhealth=777`r`nattack_windup_seconds=0`r`n# user choice`r`n"
    [IO.File]::WriteAllText($target,$old,[Text.UTF8Encoding]::new($false))
    $bytesBefore = [Convert]::ToBase64String([IO.File]::ReadAllBytes($target))
    foreach($name in @('characters','difficulties','powerups','weapons','loadouts','player','session')) {
        [IO.File]::WriteAllText((Join-Path $runtime "$name.cfg"),'USER-OWNED-DO-NOT-RESET')
    }
    & (Join-Path $root 'scripts/sync-gameplay-config.ps1') -SourceConfig $source -DestinationConfig $runtime
    $after = [IO.File]::ReadAllText($target)
    Check ($after.Contains('health=777')) 'Custom HP overwritten'
    Check ($after.Contains('attack_windup_seconds=0')) 'Explicit opt-out overwritten'
    Check ($after.Contains('attack_recovery_seconds=0.22')) 'Missing default not migrated'
    Check (-not $after.Contains('[infected_tanker]')) 'Added an unwanted enemy section'
    Check ($after.Contains("`r`n")) 'CRLF not preserved'
    foreach($name in @('characters','difficulties','powerups','weapons','loadouts','player','session')) {
        Check ([IO.File]::ReadAllText((Join-Path $runtime "$name.cfg")) -eq 'USER-OWNED-DO-NOT-RESET') "$name changed"
    }
    $backups = @(Get-ChildItem -LiteralPath (Join-Path $runtime 'backups') -File)
    Check ($backups.Count -eq 1) 'Expected one atomic backup'
    Check ([Convert]::ToBase64String([IO.File]::ReadAllBytes($backups[0].FullName)) -eq $bytesBefore) 'Backup not byte-identical'
    $hash = (Get-FileHash -LiteralPath $target).Hash
    & (Join-Path $root 'scripts/sync-gameplay-config.ps1') -SourceConfig $source -DestinationConfig $runtime
    Check ((Get-FileHash -LiteralPath $target).Hash -eq $hash) 'Repeat build changed config'
    Check (@(Get-ChildItem -LiteralPath (Join-Path $runtime 'backups') -File).Count -eq 1) 'Repeated migration'
    # Fresh install seeds source without creating a migration backup.
    $fresh = Join-Path $testRoot 'fresh'
    & (Join-Path $root 'scripts/sync-gameplay-config.ps1') -SourceConfig $source -DestinationConfig $fresh
    Check ((Get-FileHash (Join-Path $fresh 'infected.cfg')).Hash -eq (Get-FileHash (Join-Path $source 'infected.cfg')).Hash) 'Bad fresh seed'
    # UTF-8 BOM and non-ASCII comments are preserved byte-for-byte.
    $unicodeText = "[normal_common]`nhealth=999`n# " + [char]0x00e9 + "`n"
    [IO.File]::WriteAllText($target,$unicodeText,[Text.UTF8Encoding]::new($true))
    & (Join-Path $root 'scripts/sync-gameplay-config.ps1') -SourceConfig $source -DestinationConfig $runtime
    $bytes = [IO.File]::ReadAllBytes($target)
    Check ($bytes[0] -eq 239 -and $bytes[1] -eq 187 -and $bytes[2] -eq 191) 'BOM lost'
    Check ([IO.File]::ReadAllText($target).Contains([string][char]0x00e9)) 'UTF-8 comment damaged'
    # Unsupported UTF-16 fails closed; game itself cannot parse it.
    [IO.File]::WriteAllText($target,'[normal_common]',[Text.Encoding]::Unicode)
    $beforeBad = (Get-FileHash $target).Hash
    $failed = $false
    try { & (Join-Path $root 'scripts/sync-gameplay-config.ps1') -SourceConfig $source -DestinationConfig $runtime } catch { $failed=$true }
    Check $failed 'Unsupported format was silently rewritten'
    Check ((Get-FileHash $target).Hash -eq $beforeBad) 'Failed migration changed original'
    Write-Host 'PASS: fresh/upgrade/idempotent config staging, opt-outs, exact backups, UTF-8 and invalid format'
} finally {
    if(Test-Path -LiteralPath $testRoot) { Remove-Item -LiteralPath $testRoot -Recurse -Force }
}
