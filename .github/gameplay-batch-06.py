new('tests/LauncherSettings.Tests/LauncherPaths.cs', r"""namespace FireteamLauncher.Infrastructure;
// Test-only path boundary; the actual settings service and models are linked.
public static class LauncherPaths
{
    public static string? TestDirectory { get; set; }
    public static string? FindGameDirectory() => TestDirectory;
}
""")
new('tests/LauncherSettings.Tests/LauncherSettings.Tests.csproj', r"""<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>net10.0</TargetFramework>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
    <TreatWarningsAsErrors>true</TreatWarningsAsErrors>
  </PropertyGroup>
  <ItemGroup>
    <Compile Include="../../launcher/FireteamLauncher.WinUI/Services/SettingsService.cs" Link="SettingsService.cs" />
    <Compile Include="../../launcher/FireteamLauncher.WinUI/Models/LauncherSettings.cs" Link="LauncherSettings.cs" />
  </ItemGroup>
</Project>
""")
new('tests/LauncherSettings.Tests/Program.cs', r"""using System.Globalization;
using FireteamLauncher.Infrastructure;
using FireteamLauncher.Models;
using FireteamLauncher.Services;

var root = Path.Combine(Path.GetTempPath(), "fireteam-settings-test-" + Guid.NewGuid().ToString("N"));
Directory.CreateDirectory(root);
LauncherPaths.TestDirectory = root;
var path = Path.Combine(root, "fireteam-settings.cfg");
var service = new SettingsService();
var checks = 0;
void Check(bool pass, string name)
{
    if(!pass) throw new InvalidOperationException("FAIL: " + name);
    ++checks;
}
var culture = CultureInfo.CurrentCulture;
try
{
    Check(service.LoadSettings().RadarScale == 1, "missing file defaults");
    File.WriteAllText(path, "0.001480 0.001480 60 1.000 1920 1080 1\n");
    var settings = service.LoadSettings();
    Check(settings.RadarEnabled && settings.DamageFeedback && !settings.ReducedEffects, "seven-field legacy defaults");
    File.AppendAllText(path, "0\n");
    Check(!service.LoadSettings().RadarEnabled, "eight-field legacy disabled radar");
    settings = settings with { RadarEnabled = false, RadarScale = 1.45, DamageFeedback = false, ReducedEffects = true };
    CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("fr-FR");
    service.SaveSettings(settings);
    var loaded = service.LoadSettings();
    Check(!loaded.RadarEnabled && !loaded.DamageFeedback && loaded.ReducedEffects, "toggle round trip");
    Check(Math.Abs(loaded.RadarScale - 1.45) < 0.0001, "scale round trip under comma-decimal culture");
    Check(File.ReadAllText(path).Contains("1.450"), "invariant file representation");
    Check(Directory.GetFiles(root, "*.tmp").Length == 0, "no temporary residue");
    var good = File.ReadAllText(path);
    bool rejected = false;
    try { service.SaveSettings(settings with { Gamma = double.NaN }); }
    catch(ArgumentException) { rejected = true; }
    Check(rejected && File.ReadAllText(path) == good, "invalid save preserves last good file");
    service.SaveSettings(settings with { RadarScale = 999, Volume = 999 });
    Check(service.LoadSettings().RadarScale == 1.75 && service.LoadSettings().Volume == 100, "bounds");
    service.SaveSettings(settings with { RadarScale = double.NaN });
    Check(service.LoadSettings().RadarScale == 1, "invalid scale fallback");
    File.WriteAllText(path, "NaN 0.001480 60 1.0 1920 1080 1");
    Check(service.LoadSettings().SensitivityXMultiplier == 0.32, "NaN legacy sensitivity rejected");
    File.WriteAllText(path, "garbage");
    Check(service.LoadSettings().Width == 1920, "corrupt input defaults");
    var unrelated = Path.Combine(root, "custom-weapons.cfg");
    File.WriteAllText(unrelated, "keep me");
    service.SaveSettings(settings);
    Check(File.ReadAllText(unrelated) == "keep me", "unrelated settings untouched");
    LauncherPaths.TestDirectory = null;
    Check(service.LoadSettings().Width == 1920, "missing game defaults");
    Console.WriteLine($"PASS: {checks} production settings checks (legacy, culture, atomic save, invalid input)");
}
finally
{
    CultureInfo.CurrentCulture = culture;
    LauncherPaths.TestDirectory = null;
    Directory.Delete(root, recursive: true);
}
""")
new('tests/test-config-preservation.ps1', r"""$ErrorActionPreference = 'Stop'
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
""")
new('tests/test-feedback-queue.cpp', r"""#include "../src/shared/FireteamFeedbackQueue.h"
#include <assert.h>
#include <limits>
#include <stdio.h>
int main()
{
    FTFeedbackRules::RecentQueue q;
    assert(q.Pop(0)==0);
    q.Push(1,0); q.Push(2,0.2f); q.Push(1,0.4f);
    assert(q.Pop(0.5f)==1); assert(q.Pop(0.5f)==2); assert(q.Pop(0.5f)==0);
    for(unsigned char i=1;i<=8;++i) q.Push(i,1);
    for(unsigned char i=5;i<=8;++i) assert(q.Pop(2)==i);
    assert(q.Pop(2)==0);
    q.Push(1,0); q.Push(2,3); assert(q.Pop(3)==2); assert(q.Pop(3)==0);
    q.Push(1,10); assert(q.Pop(1)==0); // map clock rolled back
    q.Push(0,1); q.Push(1,std::numeric_limits<float>::quiet_NaN()); assert(q.Pop(1)==0);
    q.Push(3,1); q.Clear(); assert(q.Pop(1)==0); // round start preempts kill backlog
    q.Push(3,1); assert(q.Pop(std::numeric_limits<float>::infinity())==0);
    puts("PASS: bounded/coalesced recent feedback, expiry, map reset, round priority");
}
""")
new('tests/test-melee-timeline.cpp', r"""#include "../src/sshell/FireteamMeleeRules.h"
#include <assert.h>
#include <limits>
#include <stdio.h>
int main()
{
    using namespace FTMeleeRules;
    StrikeClock clock;
    assert(!clock.TakeImpact(0) && !clock.Locked(0));
    clock.Begin(10,0.55f,0.65f);
    assert(clock.Locked(10));
    assert(!clock.TakeImpact(10.54f));
    assert(clock.TakeImpact(10.56f));
    assert(!clock.TakeImpact(10.56f));
    assert(clock.Locked(11.19f));
    assert(!clock.Locked(11.21f));
    clock.Begin(20,0.28f,0.22f); clock.Cancel();
    assert(!clock.TakeImpact(21) && !clock.Locked(21));
    clock.Begin(30,0.18f,0.12f);
    assert(clock.TakeImpact(45)); // hitch: one impact, no catch-up damage burst
    assert(!clock.TakeImpact(45) && !clock.Locked(45));
    clock.Begin(50,0,0); assert(clock.TakeImpact(50)); assert(!clock.Locked(50));
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float inf=std::numeric_limits<float>::infinity();
    assert(WindupSeconds(inf,1)==0 && WindupSeconds(0.2f,inf)==0);
    assert(RecoverySeconds(nan,1,0.2f)==0);
    assert(RecoverySeconds(3,1,0.5f)==0.5f);
    assert(!CanConnect(0,0,0,inf,70));
    assert(!CanConnect(0,inf,0,50,70));
    assert(CanConnect(0,70,0,50,70));
    assert(!CanConnect(0,70.1f,0,50,70));
    assert(!CanConnect(40,0,40,50,70));
    assert(SameLife(17,0,17,0,true));
    assert(!SameLife(17,0,18,0,true)); // recycled object handle
    assert(!SameLife(17,0,17,1,true)); // same player, another life
    assert(!SameLife(17,0,17,0,false)); // dead/spectating/unverified
    assert(!SameLife(0,0,0,0,true));
    assert(KeepTarget(100,90)); assert(!KeepTarget(100,40));
    assert(!KeepTarget(-1,1)); assert(!KeepTarget(inf,1));
    clock.Begin(nan,1,1); assert(!clock.TakeImpact(80));
    clock.Begin(80,1,1); assert(!clock.TakeImpact(nan)); assert(clock.TakeImpact(81));
    const int rates[]={10,20,30,60,120};
    for(unsigned rate=0;rate<5;++rate)
    {
        for(unsigned variant=0;variant<3;++variant)
        {
            const float windups[]={0.28f,0.18f,0.55f};
            const float recovery[]={0.22f,0.12f,0.65f};
            clock.Begin(0,windups[variant],recovery[variant]);
            unsigned impacts=0;
            for(int tick=0;tick<rates[rate]*3;++tick)
            {
                const float now=(float)tick/rates[rate];
                if(clock.TakeImpact(now)) { assert(now>=windups[variant]); ++impacts; }
            }
            assert(impacts==1);
        }
    }
    puts("PASS: melee one-shot timing, recovery, cancellation, identity/life guards, five tick rates");
}
""")
