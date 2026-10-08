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

        string? runtimePath = null;

        var game =
            LauncherPaths.FindGameDirectory();

        if(game is not null)
        {
            var worlds =
                Path.Combine(
                    game,
                    "rez",
                    "Worlds");

            Directory.CreateDirectory(
                worlds);

            runtimePath =
                Path.Combine(
                    worlds,
                    mapName + ".DAT");

            File.Copy(
                sourcePath,
                runtimePath,
                true);
        }

        return new MapImportResult(
            mapName,
            sourcePath,
            localPath,
            runtimePath,
            runtimePath is null
                ? $"Imported {mapName}.DAT to assets-local/MapImports. Run build.cmd to stage it."
                : $"Imported and staged {mapName}.DAT. It is ready in the map selector.");
    }
}
