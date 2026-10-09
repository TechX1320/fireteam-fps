using FireteamLauncher.Infrastructure;
using FireteamLauncher.Models;

namespace FireteamLauncher.Services;

// Q in Weapon QA writes BUILT/config/weapon-quarantine.txt.
// Preserve a backup outside game staging without sending user choices to Git.
public static class WeaponQaQuarantineStore
{
    private static readonly object Gate = new();
    public static string SavedPath => Path.Combine(
        LauncherPaths.SupportRoot, "weapon-quarantine.saved.txt");

    private static string? RuntimePath
    {
        get
        {
            var game = LauncherPaths.FindGameDirectory();
            return game is null ? null : Path.Combine(game, "config",
                "weapon-quarantine.txt");
        }
    }

    private static string? OriginalSourcePath
    {
        get
        {
            var repo = LauncherPaths.FindRepositoryDirectory();
            return repo is null ? null : Path.Combine(repo, "config",
                "weapon-quarantine.txt");
        }
    }

    private static string? FindLatest()
    {
        var runtime = RuntimePath;
        var backup = SavedPath;
        bool gameExists = runtime is not null && File.Exists(runtime);
        bool backupExists = File.Exists(backup);
        if(gameExists && backupExists)
            return File.GetLastWriteTimeUtc(runtime!) >=
                   File.GetLastWriteTimeUtc(backup) ? runtime : backup;
        if(gameExists) return runtime;
        if(backupExists) return backup;
        var original = OriginalSourcePath;
        return original is not null && File.Exists(original) ? original : null;
    }

    private static void CopyNewer(string from, string? to)
    {
        if(string.IsNullOrWhiteSpace(to) ||
           string.Equals(from, to, StringComparison.OrdinalIgnoreCase))
            return;
        if(File.Exists(to) &&
           File.GetLastWriteTimeUtc(to) >= File.GetLastWriteTimeUtc(from))
            return;
        var parent = Path.GetDirectoryName(to);
        if(parent is not null) Directory.CreateDirectory(parent);
        File.Copy(from, to, true);
    }

    private static string? Synchronize()
    {
        var latest = FindLatest();
        if(latest is null) return null;
        try
        {
            CopyNewer(latest, RuntimePath);
            CopyNewer(latest, SavedPath);
        }
        catch(IOException) { /* Keep working with the original. */ }
        catch(UnauthorizedAccessException) { /* Still read existing data. */ }
        return latest;
    }

    public static HashSet<string> Read()
    {
        lock(Gate)
        {
            var keys = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            var latest = Synchronize();
            if(latest is null) return keys;
            try
            {
                foreach(var line in File.ReadLines(latest))
                {
                    var key = line.Trim();
                    if(key.Length > 0 && !key.StartsWith('#') &&
                       !key.StartsWith(';')) keys.Add(key);
                }
            }
            catch(IOException) { }
            catch(UnauthorizedAccessException) { }
            return keys;
        }
    }

    public static (long WriteTicks, long Length) Signature()
    {
        lock(Gate)
        {
            var path = Synchronize();
            if(path is null || !File.Exists(path)) return (0, 0);
            var info = new FileInfo(path);
            return (info.LastWriteTimeUtc.Ticks, info.Length);
        }
    }

    public static void RestoreBeforeGameLaunch(string gameDir)
    {
        lock(Gate)
        {
            var destination = Path.Combine(gameDir, "config",
                "weapon-quarantine.txt");
            var latest = FindLatest();
            if(latest is not null && !File.Exists(destination))
                CopyNewer(latest, destination);
        }
    }

    // Only explicit ENABLE / Restore actions remove old QA entries.
    public static void Unquarantine(IEnumerable<WeaponDefinition> selections)
    {
        lock(Gate)
        {
            var keys = Read();
            foreach(var w in selections)
            {
                keys.Remove(w.Section);
                keys.Remove("section:" + w.Section);
                keys.Remove("id:" + w.Id);
                if(w.Id.Length > 31) keys.Remove("id:" + w.Id[..31]);
            }
            var text = "# FIRETEAM Weapon QA quarantine.\n" +
                string.Join("\n", keys.OrderBy(k => k,
                    StringComparer.OrdinalIgnoreCase)) + "\n";

            var paths = new List<string> { SavedPath };
            if(RuntimePath is string runtime) paths.Add(runtime);
            if(OriginalSourcePath is string source && File.Exists(source))
                paths.Add(source);

            foreach(var path in paths.Distinct(StringComparer.OrdinalIgnoreCase))
            {
                var parent = Path.GetDirectoryName(path);
                if(parent is not null) Directory.CreateDirectory(parent);
                var temp = path + ".launcher-temp";
                File.WriteAllText(temp, text);
                File.Move(temp, path, true);
            }
        }
    }
}
