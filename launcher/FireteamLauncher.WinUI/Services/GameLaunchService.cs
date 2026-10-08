using System.Diagnostics;
using System.Globalization;
using FireteamLauncher.Infrastructure;
using FireteamLauncher.Models;

namespace FireteamLauncher.Services;

public sealed class GameLaunchService
{
    private readonly SettingsService _settings;

    public GameLaunchService(SettingsService settings)
    {
        _settings = settings;
    }

    public string Launch(LauncherProfile profile, LauncherSettings settings)
    {
        var gameDir = LauncherPaths.FindGameDirectory()
            ?? throw new InvalidOperationException(
                "Could not locate Lithtech.exe. Build FIRETEAM first or run the launcher from BUILT\\Launcher.");

        var mapName =
            NormalizeMapName(
                profile.Map);

        var mapFile =
            Path.Combine(
                gameDir,
                "rez",
                "Worlds",
                mapName + ".DAT");

        if(!File.Exists(mapFile))
        {
            throw new FileNotFoundException(
                $"{mapName}.DAT is not staged in BUILT.",
                mapFile);
        }

        _settings.SaveSettings(settings);
        _settings.SaveProfile(profile);
        WriteSession(gameDir, profile.Difficulty);

        var exe = Path.Combine(gameDir, "Lithtech.exe");
        var start = new ProcessStartInfo(exe)
        {
            WorkingDirectory = gameDir,
            UseShellExecute = false
        };

        AddArg(start, "-rez", "Engine.REZ");
        AddArg(start, "-rez", "rez");
        AddArg(start, "-config", "autoexec.cfg");
        AddArg(
            start,
            "+runworld",
            "Worlds/" + mapName);
        AddArg(start, "+autostart", "1");

        var mode = profile.Mode switch
        {
            "Host Multiplayer" => "host",
            "Join Multiplayer" => "join",
            _ => "single"
        };

        AddArg(start, "+fireteammode", mode);
        AddArg(start, "+playername", string.IsNullOrWhiteSpace(profile.PlayerName) ? "Player" : profile.PlayerName.Trim());

        if(mode == "join")
        {
            AddArg(start, "+joinip", string.IsNullOrWhiteSpace(profile.JoinIp) ? "127.0.0.1" : profile.JoinIp.Trim());
        }

        AddArg(start, "+screenwidth", settings.Width.ToString(CultureInfo.InvariantCulture));
        AddArg(start, "+screenheight", settings.Height.ToString(CultureInfo.InvariantCulture));
        AddArg(start, "+windowed", settings.Windowed ? "1" : "0");
        var errorLog =
            mapName.Equals(
                "CABINFEVER",
                StringComparison.OrdinalIgnoreCase)
                ? "cabinfever-error.log"
                : mapName.ToLowerInvariant() +
                    "-error.log";

        AddArg(start, "+errorlog", "1");
        AddArg(start, "+alwaysflushlog", "1");
        AddArg(start, "+errorlogfile", errorLog);
        AddArg(start, "+consoleenable", "1");
        AddArg(start, "+numconsolelines", "0");

        foreach(var token in TokenizeCommandLine(profile.CustomCommands))
        {
            start.ArgumentList.Add(token);
        }

        Process.Start(start);

        return mode switch
        {
            "host" => "Starting FIRETEAM host...",
            "join" => "Connecting to FIRETEAM server...",
            _ => $"Starting {mapName}..."
        };
    }

    private static string NormalizeMapName(
        string? map)
    {
        var value =
            string.IsNullOrWhiteSpace(map)
            ? "CABINFEVER"
            : Path.GetFileNameWithoutExtension(
                map.Trim());

        var safe =
            new string(
                value
                    .Where(
                        ch =>
                            char.IsLetterOrDigit(ch) ||
                            ch == '_' ||
                            ch == '-')
                    .ToArray());

        return string.IsNullOrWhiteSpace(safe)
            ? "CABINFEVER"
            : safe.ToUpperInvariant();
    }

    private static void WriteSession(string gameDir, string difficulty)
    {
        var configDir = Path.Combine(gameDir, "config");
        Directory.CreateDirectory(configDir);

        var value = string.IsNullOrWhiteSpace(difficulty)
            ? "normal"
            : difficulty.Trim().ToLowerInvariant();

        File.WriteAllText(Path.Combine(configDir, "session.cfg"), $"difficulty={value}\n");
    }

    private static void AddArg(ProcessStartInfo start, string name, string value)
    {
        start.ArgumentList.Add(name);
        start.ArgumentList.Add(value);
    }

    private static IEnumerable<string> TokenizeCommandLine(string? text)
    {
        if(string.IsNullOrWhiteSpace(text))
        {
            yield break;
        }

        var current = new System.Text.StringBuilder();
        var quoted = false;

        foreach(var ch in text)
        {
            if(ch == '"')
            {
                quoted = !quoted;
                continue;
            }

            if(char.IsWhiteSpace(ch) && !quoted)
            {
                if(current.Length > 0)
                {
                    yield return current.ToString();
                    current.Clear();
                }
                continue;
            }

            current.Append(ch);
        }

        if(current.Length > 0)
        {
            yield return current.ToString();
        }
    }
}
