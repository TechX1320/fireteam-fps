using System.Globalization;
using System.Text.Json;
using FireteamLauncher.Infrastructure;
using FireteamLauncher.Models;

namespace FireteamLauncher.Services;

public sealed class SettingsService
{
    private const double NativeSensitivityBase = 0.004625;

    public LauncherSettings LoadSettings()
    {
        var game = LauncherPaths.FindGameDirectory();
        var path = game is null ? null : Path.Combine(game, "fireteam-settings.cfg");

        if(path is null || !File.Exists(path))
        {
            return Defaults();
        }

        var tokens = File.ReadAllText(path)
            .Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);

        if(tokens.Length < 7 ||
           !double.TryParse(tokens[0], NumberStyles.Float, CultureInfo.InvariantCulture, out var rawSensitivityX) ||
           !double.TryParse(tokens[1], NumberStyles.Float, CultureInfo.InvariantCulture, out var rawSensitivityY) ||
           !int.TryParse(tokens[2], NumberStyles.Integer, CultureInfo.InvariantCulture, out var volume) ||
           !double.TryParse(tokens[3], NumberStyles.Float, CultureInfo.InvariantCulture, out var gamma) ||
           !int.TryParse(tokens[4], NumberStyles.Integer, CultureInfo.InvariantCulture, out var width) ||
           !int.TryParse(tokens[5], NumberStyles.Integer, CultureInfo.InvariantCulture, out var height) ||
           !int.TryParse(tokens[6], NumberStyles.Integer, CultureInfo.InvariantCulture, out var windowed))
        {
            return Defaults();
        }

        return new LauncherSettings(
            width,
            height,
            windowed != 0,
            Math.Clamp(rawSensitivityX / NativeSensitivityBase, 0.01, 4.0),
            Math.Clamp(rawSensitivityY / NativeSensitivityBase, 0.01, 4.0),
            Math.Clamp(volume, 0, 100),
            Math.Clamp(gamma, 0.50, 6.00),
            tokens.Length < 8 || tokens[7] != "0");
    }

    public void SaveSettings(LauncherSettings settings)
    {
        var game = LauncherPaths.FindGameDirectory()
            ?? throw new InvalidOperationException("FIRETEAM BUILT folder was not found.");

        var rawSensitivityX =
            NativeSensitivityBase *
            settings.SensitivityXMultiplier;
        var rawSensitivityY =
            NativeSensitivityBase *
            settings.SensitivityYMultiplier;

        var line = string.Format(
            CultureInfo.InvariantCulture,
            "{0:F6} {1:F6} {2} {3:F3} {4} {5} {6} {7}\n",
            rawSensitivityX,
            rawSensitivityY,
            settings.Volume,
            settings.Gamma,
            settings.Width,
            settings.Height,
            settings.Windowed ? 1 : 0,
            settings.RadarEnabled ? 1 : 0);

        File.WriteAllText(Path.Combine(game, "fireteam-settings.cfg"), line);
    }

    public LauncherProfile LoadProfile()
    {
        var game = LauncherPaths.FindGameDirectory();
        var path = game is null ? null : Path.Combine(game, "fireteam-launcher.json");

        if(path is not null && File.Exists(path))
        {
            try
            {
                var profile = JsonSerializer.Deserialize<LauncherProfile>(File.ReadAllText(path));
                if(profile is not null)
                {
                    return profile;
                }
            }
            catch
            {
            }
        }

        return new LauncherProfile(
            Environment.UserName,
            "Single Player",
            "4",
            "127.0.0.1",
            string.Empty);
    }

    public void SaveProfile(LauncherProfile profile)
    {
        var game = LauncherPaths.FindGameDirectory()
            ?? throw new InvalidOperationException("FIRETEAM BUILT folder was not found.");

        var json = JsonSerializer.Serialize(profile, new JsonSerializerOptions
        {
            WriteIndented = true
        });

        File.WriteAllText(Path.Combine(game, "fireteam-launcher.json"), json);
    }

    private static LauncherSettings Defaults() =>
        new(1920, 1080, true, 0.32, 0.32, 60, 1.00);
}
