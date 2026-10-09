param(
    [string]$ArchiveRoot = "",
    [string]$RepoRoot = ""
)

$ErrorActionPreference = "Stop"
if([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
}
if([string]::IsNullOrWhiteSpace($ArchiveRoot)) {
    $ArchiveRoot = Join-Path $RepoRoot "assets-local"
}
$reports = Join-Path $RepoRoot "assets-local\Reports"
New-Item -ItemType Directory -Path $reports -Force | Out-Null
$output = Join-Path $reports "radar-asset-audit.txt"

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$lines = New-Object 'System.Collections.Generic.List[string]'
function Print([string]$line) {
    $script:lines.Add($line)
    Write-Host $line
}
Print "FIRETEAM Combat Arms HUD/Radar archive inspection"
Print "Source: $ArchiveRoot"
Print "Archive contents only: no extraction, copying, modifications or uploads."
Print ""

if(-not (Test-Path -LiteralPath $ArchiveRoot -PathType Container)) {
    Print "Folder not found. Place local ZIPs in assets-local or use -ArchiveRoot."
}
else {
    $zips = @(Get-ChildItem -LiteralPath $ArchiveRoot -Filter "*.zip" -File |
        Where-Object { $_.Name -match '(?i)textures|texture|tex|fx|ui|hud|attr|gms|cabinf|ca' })
    if($zips.Count -eq 0) {
        Print "No likely HUD/Texture/FX archives found."
    }
    foreach($file in $zips) {
        Print ("Archive: " + $file.Name)
        $zip = $null
        try {
            $zip = [IO.Compression.ZipFile]::OpenRead($file.FullName)
            $names = @($zip.Entries | Where-Object {
                $_.FullName -match '(?i)(radar|mini.?map|compass|mapicon|map_icon|team.?icon|blip|enemy.?mark|player.?mark|cabinf)'
            } | Select-Object -ExpandProperty FullName | Sort-Object -Unique)
            Print ("  Total entries: " + $zip.Entries.Count)
            Print ("  Candidate path matches: " + $names.Count)
            foreach($name in $names | Select-Object -First 160) {
                Print ("    " + $name)
            }
            if($names.Count -gt 160) {
                Print ("    [Truncated] Further candidate names: " + ($names.Count - 160))
            }
            if($names.Count -eq 0) {
                Print "    No obvious candidate filenames; original radar may be procedural or use generic HUD assets."
            }
        }
        catch {
            Print ("  [ERROR] " + $_.Exception.Message)
        }
        finally {
            if($zip) { $zip.Dispose() }
        }
        Print ""
    }
}
Print "A matching DTX name is only a candidate; material format/layout still requires validation."
$lines | Set-Content -LiteralPath $output -Encoding UTF8
Print ("Report: " + $output)
