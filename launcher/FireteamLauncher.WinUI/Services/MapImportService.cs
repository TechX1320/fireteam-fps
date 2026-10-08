using System.Diagnostics;
using FireteamLauncher.Infrastructure;

namespace FireteamLauncher.Services;

public sealed record MapImportResult(
    string MapName,
    string SourcePath,
    string LocalPath,
    string? RuntimePath,
    string Summary);

public sealed class MapImportService
{
    public MapImportResult Import(string sourcePath)
    {
        if(string.IsNullOrWhiteSpace(sourcePath) ||
           !File.Exists(sourcePath))
        {
            throw new FileNotFoundException(
                "Select a Combat Arms .DAT world file first.",
                sourcePath);
        }

        if(!string.Equals(
               Path.GetExtension(sourcePath),
               ".dat",
               StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidDataException(
                "Map Importer only accepts LithTech .DAT world files.");
        }

        var rawName =
            Path.GetFileNameWithoutExtension(
                sourcePath);

        var safeName =
            new string(
                rawName
                    .Where(
                        ch =>
                            char.IsLetterOrDigit(ch) ||
                            ch == '_' ||
                            ch == '-')
                    .ToArray())
                .Trim();

        if(string.IsNullOrWhiteSpace(safeName))
        {
            throw new InvalidDataException(
                "The selected map filename does not contain a usable world name.");
        }

        var mapName =
            safeName.ToUpperInvariant();

        var repo =
            LauncherPaths.FindRepositoryDirectory()
            ?? throw new InvalidOperationException(
                "FIRETEAM repository was not found. Run the launcher from a development build to import Combat Arms content.");

        var importRoot =
            Path.Combine(
                repo,
                "assets-local",
                "MapImports");

        Directory.CreateDirectory(
            importRoot);

        var localPath =
            Path.Combine(
                importRoot,
                mapName + ".DAT");

        File.Copy(
            sourcePath,
            localPath,
            true);

        var localRoot =
            Path.Combine(
                repo,
                ".local");

        RunAssetStaging(
            repo,
            localRoot);

        string? runtimePath = null;

        var game =
            LauncherPaths.FindGameDirectory();

        if(game is not null)
        {
            var stagedRez =
                Path.Combine(
                    localRoot,
                    "gameassets",
                    "rez");

            var runtimeRez =
                Path.Combine(
                    game,
                    "rez");

            CopyTree(
                stagedRez,
                runtimeRez);

            runtimePath =
                Path.Combine(
                    runtimeRez,
                    "Worlds",
                    mapName + ".DAT");
        }

        return new MapImportResult(
            mapName,
            sourcePath,
            localPath,
            runtimePath,
            runtimePath is null
                ? $"Imported {mapName}.DAT and refreshed local map resources. Run build.cmd to create a runtime."
                : $"Imported {mapName}.DAT and refreshed map textures/sounds/resources from assets-local.");
    }

    private static void RunAssetStaging(
        string repo,
        string localRoot)
    {
        var script =
            Path.Combine(
                repo,
                "scripts",
                "stage-local-assets.ps1");

        if(!File.Exists(script))
        {
            throw new FileNotFoundException(
                "Map asset staging script was not found.",
                script);
        }

        var start =
            new ProcessStartInfo(
                "powershell.exe")
            {
                WorkingDirectory =
                    repo,
                UseShellExecute =
                    false,
                CreateNoWindow =
                    true,
                RedirectStandardOutput =
                    true,
                RedirectStandardError =
                    true
            };

        start.ArgumentList.Add(
            "-NoProfile");
        start.ArgumentList.Add(
            "-ExecutionPolicy");
        start.ArgumentList.Add(
            "Bypass");
        start.ArgumentList.Add(
            "-File");
        start.ArgumentList.Add(
            script);
        start.ArgumentList.Add(
            "-RepoRoot");
        start.ArgumentList.Add(
            repo);
        start.ArgumentList.Add(
            "-LocalRoot");
        start.ArgumentList.Add(
            localRoot);

        using var process =
            Process.Start(
                start)
            ?? throw new InvalidOperationException(
                "Could not start PowerShell asset staging.");

        var stdout =
            process
                .StandardOutput
                .ReadToEndAsync();

        var stderr =
            process
                .StandardError
                .ReadToEndAsync();

        process.WaitForExit();

        var output =
            stdout
                .GetAwaiter()
                .GetResult();

        var error =
            stderr
                .GetAwaiter()
                .GetResult();

        if(process.ExitCode != 0)
        {
            throw new InvalidOperationException(
                "Map resource staging failed. " +
                (string.IsNullOrWhiteSpace(error)
                    ? output
                    : error));
        }
    }

    private static void CopyTree(
        string sourceRoot,
        string destinationRoot)
    {
        if(!Directory.Exists(
               sourceRoot))
        {
            return;
        }

        foreach(var source in
                Directory.EnumerateFiles(
                    sourceRoot,
                    "*",
                    SearchOption.AllDirectories))
        {
            var relative =
                Path.GetRelativePath(
                    sourceRoot,
                    source);

            var destination =
                Path.Combine(
                    destinationRoot,
                    relative);

            var parent =
                Path.GetDirectoryName(
                    destination);

            if(parent is not null)
            {
                Directory.CreateDirectory(
                    parent);
            }

            File.Copy(
                source,
                destination,
                true);
        }
    }
}
