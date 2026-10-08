using System.Diagnostics;

namespace FireteamLauncher.Infrastructure;

public static class LauncherPaths
{
    public static string SupportRoot
    {
        get
        {
            var baseDir =
                new DirectoryInfo(
                    AppContext.BaseDirectory);

            // Runtime lives at Launcher/App. Keep writable launcher state one
            // level up so App stays a clean dependency folder.
            if(baseDir.Parent is not null &&
               baseDir.Name.Equals(
                   "App",
                   StringComparison.OrdinalIgnoreCase))
            {
                return baseDir.Parent.FullName;
            }

            return baseDir.FullName;
        }
    }

    public static string LogsRoot =>
        Path.Combine(
            SupportRoot,
            "Logs");

    public static string? FindGameDirectory()
    {
        var current = new DirectoryInfo(AppContext.BaseDirectory);

        for(var i = 0; current is not null && i < 8; ++i, current = current.Parent)
        {
            if(File.Exists(Path.Combine(current.FullName, "Lithtech.exe")))
            {
                return current.FullName;
            }

            var built = Path.Combine(current.FullName, "BUILT");
            if(File.Exists(Path.Combine(built, "Lithtech.exe")))
            {
                return built;
            }
        }

        return null;
    }

    public static string? FindRepositoryDirectory()
    {
        var current = new DirectoryInfo(AppContext.BaseDirectory);

        for(var i = 0; current is not null && i < 9; ++i, current = current.Parent)
        {
            if(File.Exists(Path.Combine(current.FullName, "build.cmd")) &&
               Directory.Exists(Path.Combine(current.FullName, "config")))
            {
                return current.FullName;
            }
        }

        return null;
    }

    public static (string? Source, string? Runtime) FindConfigPaths(string fileName)
    {
        var repo = FindRepositoryDirectory();
        var game = FindGameDirectory();

        var source = repo is null
            ? null
            : Path.Combine(repo, "config", fileName);

        var runtime = game is null
            ? null
            : Path.Combine(game, "config", fileName);

        return (
            source is not null && File.Exists(source) ? source : null,
            runtime is not null && File.Exists(runtime) ? runtime : null);
    }

    public static string? FindEditableConfig(string fileName)
    {
        var paths = FindConfigPaths(fileName);
        return paths.Source ?? paths.Runtime;
    }

    public static string? ModsDirectory
    {
        get
        {
            var game = FindGameDirectory();
            return game is null ? null : Path.Combine(game, "Mods");
        }
    }

    public static string? ModToolsDirectory
    {
        get
        {
            var repo = FindRepositoryDirectory();
            if(repo is not null)
            {
                return Path.Combine(repo, "modTools");
            }

            var game = FindGameDirectory();
            return game is null ? null : Path.Combine(game, "modTools");
        }
    }

    public static string? FindMiniTool(
        string fileName)
    {
        if(string.IsNullOrWhiteSpace(fileName))
        {
            return null;
        }

        var candidates =
            new List<string>();

        var repo =
            FindRepositoryDirectory();

        if(repo is not null)
        {
            candidates.Add(
                Path.Combine(
                    repo,
                    "modTools",
                    fileName));
            candidates.Add(
                Path.Combine(
                    repo,
                    "Tools",
                    fileName));
            candidates.Add(
                Path.Combine(
                    repo,
                    "assets-local",
                    "Tools",
                    fileName));
        }

        var game =
            FindGameDirectory();

        if(game is not null)
        {
            candidates.Add(
                Path.Combine(
                    game,
                    "modTools",
                    fileName));
            candidates.Add(
                Path.Combine(
                    game,
                    "Tools",
                    fileName));
        }

        return candidates.FirstOrDefault(
            File.Exists);
    }

    public static bool LaunchMiniTool(
        string fileName)
    {
        var path =
            FindMiniTool(
                fileName);

        if(path is null)
        {
            return false;
        }

        Process.Start(
            new ProcessStartInfo(
                path)
            {
                WorkingDirectory =
                    Path.GetDirectoryName(path),
                UseShellExecute = true
            });

        return true;
    }

    public static void EnsureLayout()
    {
        Directory.CreateDirectory(
            LogsRoot);

        var mods = ModsDirectory;
        if(mods is not null)
        {
            Directory.CreateDirectory(mods);
        }

        var tools = ModToolsDirectory;
        if(tools is not null)
        {
            Directory.CreateDirectory(tools);
        }
    }

    public static bool OpenFolder(string? path)
    {
        if(string.IsNullOrWhiteSpace(path))
        {
            return false;
        }

        Directory.CreateDirectory(path);
        Process.Start(new ProcessStartInfo("explorer.exe", path)
        {
            UseShellExecute = true
        });
        return true;
    }
}
