using System.Diagnostics;
using System.Text.Json;
using FireteamLauncher.Infrastructure;

namespace FireteamLauncher.Services;

// A local hosting preset. No password is persisted: a 4-digit PIN is not
// credentials management and private admission is not yet engine-enforced.
public sealed record DedicatedHostProfile(
    string Name,
    string Map,
    int Difficulty,
    int Port,
    int MaxPlayers,
    bool Private,
    string Pin,
    bool TrackStats,
    int FirstRoundPrepSeconds,
    string[] Mods,
    bool PublishOnline = false,
    string HubUrl = "",
    bool AutoConfigureRouter = false)
{
    public static DedicatedHostProfile Default => new(
        "FIRETEAM Dedicated", "CABINFEVER", 4, 27889, 24,
        false, "", true, 45, []);
}

public sealed class DedicatedServerService
{
    private Process? _process;
    private readonly RouterMappingService _routerMappings = new();

    private static string PresetPath =>
        Path.Combine(LauncherPaths.SupportRoot, "dedicated-host.json");

    public bool IsRunning => _process is not null && !_process.HasExited;

    public DedicatedHostProfile Load()
    {
        try
        {
            if(File.Exists(PresetPath))
            {
                var loaded = JsonSerializer.Deserialize<DedicatedHostProfile>(
                    File.ReadAllText(PresetPath));
                if(loaded is not null)
                    return loaded with
                    {
                        Pin = "",
                        Mods = loaded.Mods ?? [],
                        HubUrl = loaded.HubUrl ?? ""
                    };
            }
        }
        catch (Exception)
        {
            // A malformed old preset must not stop the launcher opening.
        }
        return DedicatedHostProfile.Default;
    }

    public void Save(DedicatedHostProfile profile)
    {
        Validate(profile, forStart: false);
        // The URL is public configuration, not a credential. Saving it
        // locally also configures this launcher's Community Servers page.
        if(!string.IsNullOrWhiteSpace(profile.HubUrl))
            HubAddressService.Save(profile.HubUrl);
        Directory.CreateDirectory(LauncherPaths.SupportRoot);
        var text = JsonSerializer.Serialize(
            profile with { Pin = "" },
            new JsonSerializerOptions { WriteIndented = true });
        File.WriteAllText(PresetPath, text);
    }

    public string Start(DedicatedHostProfile profile)
    {
        Validate(profile, forStart: true);
        if(IsRunning)
            throw new InvalidOperationException(
                "This launcher has already started a dedicated server. Stop it first.");

        var game = LauncherPaths.FindGameDirectory()
            ?? throw new DirectoryNotFoundException(
                "FIRETEAM runtime not found. Run build.cmd first.");
        var hostDir = Path.Combine(game, "Dedicated");
        var exe = Path.Combine(hostDir, "FireteamDedicatedServer.exe");
        if(!File.Exists(exe))
            throw new FileNotFoundException(
                "Dedicated executable not staged. Run build-dedicated.cmd.", exe);

        var map = NormalizeMap(profile.Map);
        var mapFile = Path.Combine(hostDir, "rez", "Worlds", map + ".DAT");
        if(!File.Exists(mapFile))
            throw new FileNotFoundException(
                "Dedicated map is missing. Stage the map and rebuild.", mapFile);

        // The server owns an independent session.cfg. Never rewrite the
        // playable client's BUILT/config/session.cfg.
        var config = Path.Combine(hostDir, "config");
        Directory.CreateDirectory(config);
        var cabinGuard = map == "CABINFEVER" ? 1 : 0;
        File.WriteAllText(Path.Combine(config, "session.cfg"),
            $"difficulty={profile.Difficulty}\n" +
            $"map={map}\n" +
            $"first_round_prep={profile.FirstRoundPrepSeconds}\n" +
            $"cabin_spawn_guard={cabinGuard}\n" +
            $"stats_enabled={(profile.TrackStats ? 1 : 0)}\n");

        var data = Path.Combine(hostDir, "data");
        Directory.CreateDirectory(data);
        var stopFile = Path.Combine(data, "server-stop.request");
        if(File.Exists(stopFile))
            File.Delete(stopFile);

        var start = new ProcessStartInfo(exe)
        {
            WorkingDirectory = hostDir,
            UseShellExecute = false
        };
        start.ArgumentList.Add("--map");
        start.ArgumentList.Add(map);
        start.ArgumentList.Add("--port");
        start.ArgumentList.Add(profile.Port.ToString(
            System.Globalization.CultureInfo.InvariantCulture));
        start.ArgumentList.Add("--max-players");
        start.ArgumentList.Add(profile.MaxPlayers.ToString(
            System.Globalization.CultureInfo.InvariantCulture));
        start.ArgumentList.Add("--name");
        start.ArgumentList.Add(profile.Name.Trim());
        if(profile.PublishOnline)
        {
            start.ArgumentList.Add("--hub-url");
            start.ArgumentList.Add(HubAddressService.Save(profile.HubUrl));
        }

        _process = Process.Start(start)
            ?? throw new InvalidOperationException("Windows did not start the dedicated process.");
        Save(profile);
        return $"Dedicated process started (PID {_process.Id}), {map}, " +
               $"{profile.MaxPlayers} slots, port {profile.Port}. " +
               "Check the server console for the READY message before joining. " +
               (profile.PublishOnline
                   ? "Opt-in public heartbeat enabled. Joinability/NAT is NOT verified."
                   : "LAN/unlisted only; public IP has not been advertised.") +
               (profile.AutoConfigureRouter
                   ? " Requested UPnP mapping will be checked next."
                   : " No automatic router mapping requested.");
    }

    public Task<string> ConfigureRouterAsync(int port)
    {
        if(!IsRunning)
            return Task.FromResult("Dedicated process has stopped; no UPnP changes made.");
        return _routerMappings.TryMapGamePortAsync(
            port, _process?.Id ?? 0, () => IsRunning);
    }

    public Task<string> CleanupRouterAsync() =>
        _routerMappings.RemoveOwnedMappingsAsync();

    public string RequestStop()
    {
        if(!IsRunning)
            return "No dedicated process started by this launcher is currently running.";

        var game = LauncherPaths.FindGameDirectory()
            ?? throw new DirectoryNotFoundException("Game runtime was not found.");
        var data = Path.Combine(game, "Dedicated", "data");
        Directory.CreateDirectory(data);
        File.WriteAllText(Path.Combine(data, "server-stop.request"),
            "graceful-stop\n");
        return "Graceful shutdown requested. Watch the dedicated console for confirmation.";
    }

    public IReadOnlyList<string> DiscoverLocalMods()
    {
        var path = LauncherPaths.ModsDirectory;
        if(path is null || !Directory.Exists(path))
            return [];
        try
        {
            // Visible local packages only; these are not auto-loaded as engine
            // mods until a signed/hashed manifest and mount order exist.
            return Directory.EnumerateFiles(path, "*.ftmod", SearchOption.TopDirectoryOnly)
                .Concat(Directory.EnumerateFiles(path, "*.zip", SearchOption.TopDirectoryOnly))
                .Select(Path.GetFileName)
                .Where(s => s is not null)
                .Select(s => s!)
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(s => s, StringComparer.OrdinalIgnoreCase)
                .Take(200)
                .ToArray();
        }
        catch(IOException) { return []; }
        catch(UnauthorizedAccessException) { return []; }
    }

    private static void Validate(DedicatedHostProfile profile, bool forStart)
    {
        if(string.IsNullOrWhiteSpace(profile.Name) ||
           profile.Name.Length > 64 ||
           profile.Name.IndexOfAny(['\r', '\n']) >= 0)
            throw new ArgumentException("Server name must be 1-64 characters on one line.");
        NormalizeMap(profile.Map);
        if(profile.Difficulty < 1 || profile.Difficulty > 10)
            throw new ArgumentException("Difficulty must be 1-10.");
        if(profile.Port < 1 || profile.Port > 65535)
            throw new ArgumentException("Port must be 1-65535.");
        if(profile.MaxPlayers < 1 || profile.MaxPlayers > 24)
            throw new ArgumentException("Player slots must be 1-24.");
        if(profile.FirstRoundPrepSeconds < 0 ||
           profile.FirstRoundPrepSeconds > 120)
            throw new ArgumentException("First-round preparation must be 0-120 seconds.");
        if(profile.Private &&
           (profile.Pin.Length != 4 || !profile.Pin.All(ch => ch >= '0' && ch <= '9')))
        {
            // Allow saving the intent with no PIN, without persisting secrets.
            if(forStart || profile.Pin.Length > 0)
                throw new ArgumentException("Private access requires exactly four digits.");
        }

        if(!string.IsNullOrWhiteSpace(profile.HubUrl) &&
           !HubAddressService.TryNormalize(profile.HubUrl, out _))
            throw new ArgumentException("Hub URL must be HTTPS (localhost HTTP for tests).");
        if(forStart && profile.PublishOnline &&
           (!HubAddressService.TryNormalize(profile.HubUrl, out _) || profile.Private))
            throw new InvalidOperationException(
                "Online advertising requires an explicit valid Hub URL and non-private server. " +
                "The hub will see your public IP; connectivity is not automatically guaranteed.");

        if(forStart && profile.Private)
            throw new InvalidOperationException(
                "Private PIN admission is not implemented in the Jupiter server yet. " +
                "Refusing to start a falsely protected public socket.");
        if(forStart && profile.Mods.Length > 0)
            throw new InvalidOperationException(
                "Selected mod packages are not mounted by the engine yet. " +
                "Hosting with these selected would silently run an unmodded game.");
    }

    private static string NormalizeMap(string text)
    {
        if(string.IsNullOrWhiteSpace(text) ||
           text.Length > 64 ||
           text.Any(ch => !(char.IsLetterOrDigit(ch) || ch == '_' || ch == '-')))
            throw new ArgumentException("Select a valid staged map.");
        return text.ToUpperInvariant();
    }
}
