param(
    [string]$RepoRoot,
    [int]$MaxTextEntries = 5000
)

$ErrorActionPreference = "Stop"
if([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Join-Path $PSScriptRoot ".."
}
$repo = (Resolve-Path -LiteralPath $RepoRoot).Path
$assets = Join-Path $repo "assets-local"
$reports = Join-Path $assets "Reports"
New-Item -ItemType Directory -Path $reports -Force | Out-Null
$output = Join-Path $reports "assassin-material-audit.txt"

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$lines = New-Object System.Collections.Generic.List[string]
function Write-Report([string]$Line) {
    $script:lines.Add($Line)
    Write-Host $Line
}

Write-Report "FIRETEAM / Assassin (Combat Arms Striker) material investigation"
Write-Report "No assets are extracted, changed or uploaded by this script."
Write-Report "Note: Printable model tokens are candidates, not verified material slot order."
Write-Report ""

$archives = @(Get-ChildItem -LiteralPath $assets -Filter "*.zip" -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '(?i)char|attribute|infect|zombie|npc' } |
    Sort-Object Name)

if($archives.Count -eq 0) {
    Write-Report "No candidate character/attribute ZIPs in assets-local."
    Write-Report "Place Attributes.zip alongside Chars_Files_Updated.zip, then rerun."
}

foreach($archive in $archives) {
    Write-Report ""
    Write-Report ("ZIP: " + $archive.Name)
    try {
        $zip = [IO.Compression.ZipFile]::OpenRead($archive.FullName)
        try {
            Write-Report ("Total entries: " + $zip.Entries.Count)
            $likely = @($zip.Entries |
                Where-Object { $_.FullName -match '(?i)assassin|assavirus|striker|viw_f_nm_df|ani_vi_assassin|cw_fc_nm_virus|cw_vst_assa|cw_lg_assa' } |
                Sort-Object FullName)
            Write-Report ("Assassin/Striker candidate paths: " + $likely.Count)
            foreach($item in $likely | Select-Object -First 150) {
                Write-Report ("  " + $item.FullName + " (" + $item.Length + " bytes)")
            }
            if($likely.Count -gt 150) {
                Write-Report ("  [TRUNCATED] " + ($likely.Count - 150) + " further matching paths")
            }

            if($archive.Name -match '(?i)attribute|npc|infect') {
                Write-Report "Scanning text entries for references to Assassin/Striker and appearance mappings..."
                $textScanned = 0
                $textMatches = 0
                foreach($entry in $zip.Entries) {
                    if($textScanned -ge $MaxTextEntries) {
                        Write-Report ("Text scan limit reached at " + $MaxTextEntries)
                        break
                    }
                    if($entry.Length -gt 1048576 -or $entry.Length -eq 0 -or
                       $entry.FullName -notmatch '(?i)\.(xml|txt|cfg|ini|json|csv|lua|tbl|attr)$') {
                        continue
                    }
                    $textScanned++
                    $stream = $null
                    $reader = $null
                    try {
                        $stream = $entry.Open()
                        $reader = New-Object System.IO.StreamReader($stream)
                        $body = $reader.ReadToEnd()
                        $found = [regex]::Matches($body,
                            '(?im)^.{0,140}(assassin|striker|assavirus|viw_f_nm_df_assassin|ani_vi_assassin).{0,200}$')
                        if($found.Count -gt 0) {
                            $textMatches++
                            Write-Report ("  REFERENCE IN " + $entry.FullName)
                            foreach($hit in $found | Select-Object -First 4) {
                                $sample = ($hit.Value -replace '[\r\n\t]+', ' ').Trim()
                                Write-Report ("    " + $sample)
                            }
                        }
                    }
                    catch {
                        Write-Report ("  [SKIP] Could not read text in " + $entry.FullName)
                    }
                    finally {
                        if($reader) { $reader.Dispose() }
                        elseif($stream) { $stream.Dispose() }
                    }
                }
                Write-Report ("Scanned text entries: " + $textScanned + "; reference-bearing entries: " + $textMatches)
            }
        }
        finally {
            $zip.Dispose()
        }
    }
    catch {
        Write-Report ("[ERROR] Could not inspect archive: " + $_.Exception.Message)
    }
}

$body = Join-Path $repo "BUILT\rez\Characters\infected\body\VIW_F_NM_DF_ASSASSIN_CH.LTB"
$anim = Join-Path $repo "BUILT\rez\Characters\infected\body\ANI_VI_ASSASSIN_CH.LTB"
foreach($model in @($body, $anim)) {
    Write-Report ""
    if(-not (Test-Path -LiteralPath $model -PathType Leaf)) {
        Write-Report ("[MISSING STAGED MODEL] " + $model)
        continue
    }
    Write-Report ("MODEL: " + [IO.Path]::GetFileName($model))
    $bytes = [IO.File]::ReadAllBytes($model)
    $ascii = [Text.Encoding]::ASCII.GetString($bytes)
    $tokens = @([regex]::Matches($ascii,
        '(?<![A-Za-z0-9_\-])[A-Za-z][A-Za-z0-9_\-]{2,63}(?![A-Za-z0-9_\-])') |
        ForEach-Object { $_.Value } |
        Where-Object { $_ -match '(?i)assa|strik|virus|hair|skin|body|head|upper|lower|face|vest|leg|texture|material|^vl|^va|^vd' } |
        Sort-Object -Unique)
    Write-Report ("Candidate printable material/animation tokens: " + $tokens.Count)
    foreach($token in $tokens | Select-Object -First 160) {
        Write-Report ("  " + $token)
    }
    if($tokens.Count -gt 160) {
        Write-Report ("  [TRUNCATED] " + ($tokens.Count - 160) + " further tokens")
    }
}

Write-Report ""
Write-Report "Current configured slots in config/characters.cfg:"
$characters = Join-Path $repo "config\characters.cfg"
if(Test-Path -LiteralPath $characters) {
    $within = $false
    foreach($line in Get-Content -LiteralPath $characters) {
        if($line.Trim() -eq "[infected_assassin]") { $within = $true; continue }
        if($within -and $line.Trim() -match '^\[') { break }
        if($within -and $line -match '^(body_model|animation_model|body_texture[0-3]|face_mode|idle_anim|walk_anim|run_anim|jump_anim|attack_anim[0-3]|death_anim)=') {
            Write-Report ("  " + $line)
        }
    }
}
Write-Report ""
Write-Report "Compare the LTB names, Attributes references and all material-slot texture names."
Write-Report "Do NOT replace textures based solely on filename resemblance."
$lines | Set-Content -LiteralPath $output -Encoding UTF8
Write-Host ("[OK] Report saved to " + $output)
