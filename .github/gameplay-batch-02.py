edit('launcher/FireteamLauncher.WinUI/MainWindow.cs', '6a279efb28e2d88d9e33fb0c0175e2cdd42ac40f07bc25088f95b9beb0b872bd', [
    (5654, 5654, r"""    private readonly NumberBox RadarScaleBox = new();
    private readonly ToggleSwitch DamageFeedbackToggle = new();
    private readonly ToggleSwitch ReducedEffectsToggle = new();
"""),
    (13285, 13285, r"""        RadarScaleBox.Value = settings.RadarScale;
        DamageFeedbackToggle.IsOn = settings.DamageFeedback;
        ReducedEffectsToggle.IsOn = settings.ReducedEffects;
"""),
    (66294, 66329, r"""                RadarToggle.IsOn,
                double.IsFinite(RadarScaleBox.Value) ? Math.Clamp(RadarScaleBox.Value, 0.75, 1.75) : 1.0,
                DamageFeedbackToggle.IsOn,
                ReducedEffectsToggle.IsOn);
"""),
])
edit('launcher/FireteamLauncher.WinUI/Models/LauncherSettings.cs', '33bd104ecf96cf2375f6eef823c20c0779d84fc5c4d7b38d40e0fa8a899b5506', [
    (229, 260, r"""    bool RadarEnabled = true,
    double RadarScale = 1.0,
    bool DamageFeedback = true,
    bool ReducedEffects = false);
"""),
])
edit('launcher/FireteamLauncher.WinUI/Services/GameLaunchService.cs', 'c95250bee8f75edadc5f556d929c7b4a2e0088b4e40d7517928eef8a279ac513', [
    (2618, 2618, r"""        AddArg(start, "+ftradarscale", (double.IsFinite(settings.RadarScale)
            ? Math.Clamp(settings.RadarScale, 0.75, 1.75) : 1.0).ToString(CultureInfo.InvariantCulture));
        AddArg(start, "+ftdamagefeedback", settings.DamageFeedback ? "1" : "0");
        AddArg(start, "+ftreducedfx", settings.ReducedEffects ? "1" : "0");
"""),
])
edit('launcher/FireteamLauncher.WinUI/Services/SettingsService.cs', 'c3665e71d5ca962421a88bceda00b2d0f33341c668db2477736273b3f441b611', [
    (1360, 1467, r"""           !int.TryParse(tokens[6], NumberStyles.Integer, CultureInfo.InvariantCulture, out var windowed) ||
           !double.IsFinite(rawSensitivityX) || !double.IsFinite(rawSensitivityY) ||
           !double.IsFinite(gamma) || width < 640 || height < 480)
"""),
    (1857, 1909, r"""            tokens.Length < 8 || tokens[7] != "0",
            tokens.Length > 8 && double.TryParse(tokens[8], NumberStyles.Float,
                CultureInfo.InvariantCulture, out var radarScale) && double.IsFinite(radarScale)
                ? Math.Clamp(radarScale, 0.75, 1.75) : 1.0,
            tokens.Length < 10 || tokens[9] != "0",
            tokens.Length > 10 && tokens[10] == "1");
"""),
    (2124, 2124, r"""        if(!double.IsFinite(settings.SensitivityXMultiplier) ||
           !double.IsFinite(settings.SensitivityYMultiplier) || !double.IsFinite(settings.Gamma) ||
           settings.Width < 640 || settings.Height < 480)
            throw new ArgumentException("Settings require finite sensitivity/gamma and a valid resolution.", nameof(settings));

"""),
    (2190, 2235, r"""            Math.Clamp(settings.SensitivityXMultiplier, 0.01, 4.0);
"""),
    (2301, 2346, r"""            Math.Clamp(settings.SensitivityYMultiplier, 0.01, 4.0);
"""),
    (2423, 2481, r"""            "{0:F6} {1:F6} {2} {3:F3} {4} {5} {6} {7} {8:F3} {9} {10}\n",
"""),
    (2539, 2596, r"""            Math.Clamp(settings.Volume, 0, 100),
            Math.Clamp(settings.Gamma, 0.50, 6.00),
"""),
    (2692, 2736, r"""            settings.RadarEnabled ? 1 : 0,
            double.IsFinite(settings.RadarScale) ? Math.Clamp(settings.RadarScale, 0.75, 1.75) : 1.0,
            settings.DamageFeedback ? 1 : 0,
            settings.ReducedEffects ? 1 : 0);
"""),
    (2737, 2815, r"""        var path = Path.Combine(game, "fireteam-settings.cfg");
        var temporary = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try
        {
            File.WriteAllText(temporary, line);
            File.Move(temporary, path, overwrite: true);
        }
        finally
        {
            if(File.Exists(temporary)) File.Delete(temporary);
        }
"""),
])
edit('scripts/stage-dedicated-server.ps1', '37eb8e63c18dcf1cc79df59dffa8da891d2b1c35e0f3826278a90d935d1acf0f', [
    (1796, 1940, r"""    "weapons.cfg", "player.cfg", "loadouts.cfg", "weapon-library.cfg"
"""),
    (2444, 2444, r"""
# Seed from the live host's choices once, then preserve independent server
# values. Apply additive timing defaults from source, never copy over tuning.
foreach($name in @('infected.cfg','characters.cfg','difficulties.cfg','powerups.cfg')) {
    $dest = Join-Path $configDir $name
    if(-not (Test-Path -LiteralPath $dest)) {
        Copy-Item -LiteralPath (Join-Path $built ("config\" + $name)) -Destination $dest
    }
}
& (Join-Path $repo 'scripts\sync-gameplay-config.ps1') -SourceConfig (Join-Path $repo 'config') -DestinationConfig $configDir
"""),
])
new('scripts/sync-gameplay-config.ps1', r"""param(
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
""")
edit('src/cshell/FireteamCombatFeedback.cpp', 'af69ebae1f358fdf17b75e02a8a1132b0903b95ff674d544de415f2b7ab80c93', [
    (86, 86, r"""#include "FireteamFeedbackQueue.h"
"""),
    (379, 445, r"""    FT_FEEDBACK_ROUNDSTART = 100
"""),
    (1043, 1153, r"""static FTFeedbackRules::RecentQueue s_Queue;
"""),
    (1471, 1471, r"""    LTVector vRight, vUp, vBack;
    bool bBasisReady;
"""),
    (1684, 1705, r"""    if(!g_pLTClient || !FTCombatRules::Finite(vImpact.x) ||
       !FTCombatRules::Finite(vImpact.y) || !FTCombatRules::Finite(vImpact.z))
"""),
    (1841, 1841, r"""    burst.bBasisReady = false;
"""),
    (2512, 2512, r"""    HCONSOLEVAR hReduced = g_pLTClient->GetConsoleVar("ftreducedfx");
    const bool bReduced = hReduced && g_pLTClient->GetVarValueFloat(hReduced) >= 0.5f;
"""),
    (3708, 3708, r"""        if(!burst.bBasisReady)
        {
            burst.vRight = vRight; burst.vUp = vUp;
            burst.vBack = vCameraForward * -1.0f; burst.bBasisReady = true;
        }
"""),
    (3817, 3871, r"""        for(uint32 j = 0; j < (bReduced ? 3u : (uint32)FT_BLOOD_DROPLETS); ++j)
"""),
    (4290, 4352, r"""                burst.vRight * x + burst.vUp * y + burst.vBack * 3.0f;
"""),
    (8930, 8981, r"""static void FT_QueueFeedback(uint8 nFeedback)
"""),
    (8983, 9046, r"""    if(!nFeedback) return;
    if(nFeedback == FT_FEEDBACK_ROUNDSTART)
"""),
    (9052, 9105, r"""        // A new wave must not wait behind 30 seconds of old kill accolades.
        s_Queue.Clear(); FT_StartFeedback(nFeedback); return;
"""),
    (9111, 9387, r"""    if(!s_nActiveFeedback) { FT_StartFeedback(nFeedback); return; }
    s_Queue.Push(nFeedback, g_pLTClient->GetTime());
"""),
    (9389, 9390, r""""""),
    (9425, 9753, r"""    const uint8 next = s_Queue.Pop(g_pLTClient->GetTime());
    if(next) FT_StartFeedback(next);
    else { s_nActiveFeedback = 0; s_fActiveStart = 0; }
"""),
    (14918, 14963, r"""    s_Queue.Clear();
"""),
    (15788, 15833, r"""    s_Queue.Clear();
"""),
    (17731, 17759, r"""    HCONSOLEVAR hReduced = g_pLTClient->GetConsoleVar("ftreducedfx");
    const bool bReduced = hReduced && g_pLTClient->GetVarValueFloat(hReduced) >= 0.5f;
    if(fAge < kZoomSeconds && !bReduced)
"""),
])
