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

            // BUILT/Launcher/App is replaced on EVERY launcher rebuild.
            // Save user-owned presets, favorites and logs outside that
            // disposable tree: BUILT/data/launcher survives build.cmd.
            if(baseDir.Parent?.Parent is not null &&
               baseDir.Name.Equals("App", StringComparison.OrdinalIgnoreCase) &&
               baseDir.Parent.Name.Equals("Launcher", StringComparison.OrdinalIgnoreCase))
            {
                return Path.Combine(
                    baseDir.Parent.Parent.FullName, "data", "launcher");
            }

            // Developer/debug builds likewise keep user state separate
            // from bin/ and obj/ which are routinely cleaned by MSBuild.
            return Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "FIRETEAM", "Launcher");
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

        var searchRoots =
            new List<string>();

        var repo =
            FindRepositoryDirectory();

        if(repo is not null)
        {
            searchRoots.Add(
                Path.Combine(
                    repo,
                    "modTools"));
            searchRoots.Add(
                Path.Combine(
                    repo,
                    "Tools"));
            searchRoots.Add(
                Path.Combine(
                    repo,
                    "assets-local",
                    "Tools"));
        }

        var game =
            FindGameDirectory();

        if(game is not null)
        {
            searchRoots.Add(
                Path.Combine(
                    game,
                    "modTools"));
            searchRoots.Add(
                Path.Combine(
                    game,
                    "Tools"));
        }

        foreach(var root in searchRoots)
        {
            if(!Directory.Exists(root))
            {
                continue;
            }

            var direct =
                Path.Combine(
                    root,
                    fileName);

            if(File.Exists(direct))
            {
                return direct;
            }

            try
            {
                var nested =
                    Directory
                        .EnumerateFiles(
                            root,
                            fileName,
                            SearchOption.AllDirectories)
                        .FirstOrDefault();

                if(nested is not null)
                {
                    return nested;
                }
            }
            catch
            {
                // Keep looking in the remaining local tool roots.
            }
        }

        return null;
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
                    Path.GetDirectoryName(path) ??
                    AppContext.BaseDirectory,
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
