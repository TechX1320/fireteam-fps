using System.Globalization;
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
