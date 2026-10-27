param(
    [Parameter(Mandatory = $true)][string]$SourceConfig,
    [Parameter(Mandatory = $true)][string]$DestinationConfig
)
# Routine builds preserve user values. The ONLY additive migration is the two
# new melee timing keys, and only in an existing matching infected section.
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $SourceConfig).Path
[IO.Directory]::CreateDirectory($DestinationConfig) | Out-Null
$destination = (Resolve-Path -LiteralPath $DestinationConfig).Path
$encoding = [Text.Encoding]::GetEncoding(28591) # byte-preserving for UTF-8/legacy text

function Read-ConfigBytes([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if($bytes.Length -ge 2 -and (($bytes[0] -eq 255 -and $bytes[1] -eq 254) -or
       ($bytes[0] -eq 254 -and $bytes[1] -eq 255))) {
        throw "UTF-16 config is not supported by the game; left untouched: $Path"
    }
    return $encoding.GetString($bytes)
}
function Parse-Sections([string]$Text) {
    $sections = @{}
    $section = ''
    foreach($line in [regex]::Split($Text, '\r\n|\n|\r')) {
        $clean = $line.Trim()
        # Ignore a UTF-8 BOM for parsing, without stripping it from saved bytes.
        if($clean.StartsWith([string][char]239 + [char]187 + [char]191)) { $clean = $clean.Substring(3) }
        if($clean -match '^\[([^\]]+)\]') {
            $section = $Matches[1].Trim()
            if(-not $sections.ContainsKey($section)) { $sections[$section] = @{} }
        } elseif($section -and $clean -match '^([^#;=]+)=(.*)$') {
            $sections[$section][$Matches[1].Trim()] = $Matches[2].Trim()
        }
    }
    return $sections
}

foreach($name in @('infected.cfg','characters.cfg','difficulties.cfg','powerups.cfg')) {
    $template = Join-Path $source $name
    $target = Join-Path $destination $name
    if(-not (Test-Path -LiteralPath $template -PathType Leaf)) { throw "Missing template: $template" }
    if(Test-Path -LiteralPath $target) {
        $item = Get-Item -LiteralPath $target -Force
        if($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing to replace a directory/link config: $target"
        }
    } else {
        [IO.File]::Copy($template, $target, $false)
        Write-Host "[SEED] $name (new runtime only)"
        continue
    }
    if($name -ne 'infected.cfg') {
        Write-Host "[KEEP] Local $name unchanged"
        continue
    }
    $original = Read-ConfigBytes $target
    $existing = Parse-Sections $original
    $defaults = Parse-Sections (Read-ConfigBytes $template)
    $additions = @{}
    foreach($section in $defaults.Keys) {
        if(-not $existing.ContainsKey($section)) { continue }
        foreach($key in @('attack_windup_seconds','attack_recovery_seconds')) {
            if($defaults[$section].ContainsKey($key) -and -not $existing[$section].ContainsKey($key)) {
                if(-not $additions.ContainsKey($section)) { $additions[$section] = [Collections.Generic.List[string]]::new() }
                $additions[$section].Add($key + '=' + $defaults[$section][$key])
            }
        }
    }
    if($additions.Count -eq 0) { Write-Host '[KEEP] Local infected.cfg unchanged'; continue }
    $newline = if($original.Contains("`r`n")) { "`r`n" } else { "`n" }
    $lines = [Collections.Generic.List[string]]::new()
    $section = ''
    foreach($line in [regex]::Split($original, '\r\n|\n|\r')) {
        $clean = $line.Trim()
        if($clean.StartsWith([string][char]239 + [char]187 + [char]191)) { $clean = $clean.Substring(3) }
        if($clean -match '^\[([^\]]+)\]') {
            $nextSection = $Matches[1].Trim()
            if($additions.ContainsKey($section)) {
                foreach($entry in $additions[$section]) { $lines.Add($entry) }
                $additions.Remove($section)
            }
            $section = $nextSection
        }
        $lines.Add($line)
    }
    if($additions.ContainsKey($section)) {
        if($lines.Count -and $lines[$lines.Count-1] -eq '') { $lines.RemoveAt($lines.Count-1) }
        foreach($entry in $additions[$section]) { $lines.Add($entry) }
        $lines.Add('')
    }
    $updated = [string]::Join($newline, $lines)
    $backupDir = Join-Path $destination 'backups'
    [IO.Directory]::CreateDirectory($backupDir) | Out-Null
    $suffix = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N')
    $backup = Join-Path $backupDir ($name + '.' + $suffix + '.bak')
    $temp = $target + '.' + $suffix + '.tmp'
    try {
        [IO.File]::WriteAllBytes($temp, $encoding.GetBytes($updated))
        # Atomic replacement keeps the old file as an exact backup. A failure
        # does not fall back to deleting/overwriting the original.
        [IO.File]::Replace($temp, $target, $backup)
    } finally {
        if(Test-Path -LiteralPath $temp -PathType Leaf) { Remove-Item -LiteralPath $temp }
    }
    Write-Host "[ADD] Missing melee timing keys only; all existing values preserved. Backup: $backup"
}
