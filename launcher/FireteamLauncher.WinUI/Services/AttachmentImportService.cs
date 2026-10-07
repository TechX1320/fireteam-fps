using System.Globalization;
using System.IO.Compression;
using System.Text;
using System.Text.RegularExpressions;
using FireteamLauncher.Infrastructure;

namespace FireteamLauncher.Services;

public sealed record AttachmentImportProgress(
    int Processed,
    int Total,
    string Stage,
    string Detail)
{
    public double Percent =>
        Total <= 0
            ? 0.0
            : Math.Clamp(
                Processed * 100.0 / Total,
                0.0,
                100.0);
}

public sealed record AttachmentImportResult(
    int Files,
    int Models,
    int Textures,
    int PairedTextures,
    string ConfigPath,
    string OutputPath)
{
    public string Summary =>
        $"Imported {Files} Combat Arms attachment files: " +
        $"{Models} models, {Textures} textures/sprites, " +
        $"{PairedTextures} model/texture matches. " +
        $"Generated {ConfigPath}.";
}

public sealed class AttachmentImportService
{
    private sealed record ImportedEntry(
        string SourcePath,
        string RuntimePath,
        string Name,
        string Extension);

    public AttachmentImportResult Import(
        string archivePath,
        IProgress<AttachmentImportProgress>? progress = null,
        CancellationToken cancellationToken = default)
    {
        if(!File.Exists(
               archivePath))
        {
            throw new FileNotFoundException(
                "Attachments.zip was not found.",
                archivePath);
        }

        var repo =
            LauncherPaths.FindRepositoryDirectory()
            ?? throw new InvalidOperationException(
                "Attachment import requires the FIRETEAM repository folder.");

        var outputRoot =
            Path.Combine(
                repo,
                "assets-local",
                "AttachmentImports");

        var assetRoot =
            Path.Combine(
                outputRoot,
                "Attachments",
                "ca");

        if(Directory.Exists(
               assetRoot))
        {
            Directory.Delete(
                assetRoot,
                true);
        }

        Directory.CreateDirectory(
            assetRoot);

        using var file =
            File.OpenRead(
                archivePath);

        using var archive =
            new ZipArchive(
                file,
                ZipArchiveMode.Read);

        var entries =
            archive.Entries
                .Where(entry =>
                    !string.IsNullOrWhiteSpace(
                        entry.Name))
                .ToList();

        var imported =
            new List<ImportedEntry>(
                entries.Count);

        for(var index = 0;
            index < entries.Count;
            ++index)
        {
            cancellationToken.ThrowIfCancellationRequested();

            var entry =
                entries[index];

            var safeRelative =
                SafeRelativePath(
                    entry.FullName);

            if(string.IsNullOrWhiteSpace(
                   safeRelative))
            {
                continue;
            }

            var destination =
                Path.GetFullPath(
                    Path.Combine(
                        assetRoot,
                        safeRelative));

            var rootedAssetPath =
                Path.GetFullPath(
                    assetRoot +
                    Path.DirectorySeparatorChar);

            if(!destination.StartsWith(
                   rootedAssetPath,
                   StringComparison.OrdinalIgnoreCase))
            {
                throw new InvalidDataException(
                    $"Unsafe attachment archive path: {entry.FullName}");
            }

            Directory.CreateDirectory(
                Path.GetDirectoryName(
                    destination)!);

            using(var input =
                entry.Open())
            using(var output =
                File.Create(
                    destination))
            {
                input.CopyTo(
                    output);
            }

            var runtimePath =
                "Attachments/ca/" +
                safeRelative
                    .Replace(
                        '\\',
                        '/');

            imported.Add(
                new ImportedEntry(
                    entry.FullName,
                    runtimePath,
                    entry.Name,
                    Path.GetExtension(
                        entry.Name)
                        .ToLowerInvariant()));

            progress?.Report(
                new AttachmentImportProgress(
                    index + 1,
                    entries.Count,
                    "EXTRACT",
                    entry.FullName));
        }

        var models =
            imported
                .Where(item =>
                    item.Extension == ".ltb")
                .ToList();

        var textures =
            imported
                .Where(item =>
                    item.Extension == ".dtx" ||
                    item.Extension == ".spr")
                .ToList();

        progress?.Report(
            new AttachmentImportProgress(
                entries.Count,
                entries.Count,
                "CATALOG",
                $"Classifying {models.Count} attachment models..."));

        var configPath =
            Path.Combine(
                repo,
                "config",
                "attachments.cfg");

        Directory.CreateDirectory(
            Path.GetDirectoryName(
                configPath)!);

        var text =
            new StringBuilder();

        text.AppendLine(
            "# Generated by the FIRETEAM Combat Arms attachment importer.");
        text.AppendLine(
            "# Commercial model/texture bytes remain local under assets-local/AttachmentImports.");
        text.AppendLine();

        var usedIds =
            new HashSet<string>(
                StringComparer.OrdinalIgnoreCase);

        var paired =
            0;

        foreach(var model in models)
        {
            var id =
                UniqueId(
                    Slugify(
                        Path.GetFileNameWithoutExtension(
                            model.Name)),
                    usedIds);

            var texture =
                FindBestTexture(
                    model,
                    textures);

            if(texture is not null)
            {
                ++paired;
            }

            text.AppendLine(
                $"[attachment.{id}]");
            text.AppendLine(
                $"id={id}");
            text.AppendLine(
                $"name={FriendlyName(model.Name)}");
            text.AppendLine(
                $"slot={ClassifySlot(model.Name)}");
            text.AppendLine(
                $"model={model.RuntimePath}");
            text.AppendLine(
                $"texture={texture?.RuntimePath ?? string.Empty}");
            text.AppendLine(
                "enabled=0");
            text.AppendLine(
                "source=COMBAT ARMS ATTACHMENT IMPORT");
            text.AppendLine();
        }

        File.WriteAllText(
            configPath,
            text.ToString(),
            new UTF8Encoding(
                false));

        var reportPath =
            Path.Combine(
                outputRoot,
                "last-import.txt");

        File.WriteAllText(
            reportPath,
            $"Archive: {archivePath}{Environment.NewLine}" +
            $"Files: {imported.Count}{Environment.NewLine}" +
            $"Models: {models.Count}{Environment.NewLine}" +
            $"Textures/sprites: {textures.Count}{Environment.NewLine}" +
            $"Model/texture matches: {paired}{Environment.NewLine}" +
            $"Config: {configPath}{Environment.NewLine}");

        return new AttachmentImportResult(
            imported.Count,
            models.Count,
            textures.Count,
            paired,
            configPath,
            outputRoot);
    }

    private static string SafeRelativePath(
        string archivePath)
    {
        var normalized =
            archivePath
                .Replace(
                    '/',
                    Path.DirectorySeparatorChar)
                .Replace(
                    '\\',
                    Path.DirectorySeparatorChar)
                .TrimStart(
                    Path.DirectorySeparatorChar);

        var parts =
            normalized.Split(
                Path.DirectorySeparatorChar,
                StringSplitOptions.RemoveEmptyEntries);

        if(parts.Any(part =>
               part == ".." ||
               part == "."))
        {
            return string.Empty;
        }

        return Path.Combine(
            parts);
    }

    private static ImportedEntry? FindBestTexture(
        ImportedEntry model,
        IReadOnlyList<ImportedEntry> textures)
    {
        var modelKey =
            Identity(
                Path.GetFileNameWithoutExtension(
                    model.Name));

        ImportedEntry? best =
            null;
        var bestScore =
            0;

        foreach(var texture in textures)
        {
            var textureKey =
                Identity(
                    Path.GetFileNameWithoutExtension(
                        texture.Name));

            var score =
                SharedScore(
                    modelKey,
                    textureKey);

            if(score > bestScore)
            {
                bestScore =
                    score;
                best =
                    texture;
            }
        }

        return bestScore >= 5
            ? best
            : null;
    }

    private static int SharedScore(
        string left,
        string right)
    {
        if(left.Length == 0 ||
           right.Length == 0)
        {
            return 0;
        }

        if(left.Equals(
               right,
               StringComparison.OrdinalIgnoreCase))
        {
            return 1000;
        }

        if(left.Contains(
               right,
               StringComparison.OrdinalIgnoreCase) ||
           right.Contains(
               left,
               StringComparison.OrdinalIgnoreCase))
        {
            return Math.Min(
                left.Length,
                right.Length);
        }

        var leftTokens =
            Tokenize(left);

        var rightTokens =
            Tokenize(right);

        return leftTokens
            .Intersect(
                rightTokens,
                StringComparer.OrdinalIgnoreCase)
            .Sum(token =>
                token.Length);
    }

    private static IReadOnlyList<string> Tokenize(
        string value) =>
        Regex.Split(
                value,
                @"[^A-Za-z0-9]+")
            .Select(
                Identity)
            .Where(token =>
                token.Length >= 2 &&
                token is not "attach" and
                not "model" and
                not "texture" and
                not "cm" and
                not "mt")
            .Distinct(
                StringComparer.OrdinalIgnoreCase)
            .ToList();

    private static string ClassifySlot(
        string filename)
    {
        var value =
            filename.ToUpperInvariant();

        if(value.Contains("BACK") ||
           value.Contains("BAG") ||
           value.Contains("PACK"))
        {
            return "Backpack";
        }

        if(value.Contains("VEST") ||
           value.Contains("ARMOR") ||
           value.Contains("ARMOUR") ||
           value.Contains("BODY"))
        {
            return "Armor";
        }

        if(value.Contains("MASK") ||
           value.Contains("GLASS") ||
           value.Contains("GOGGLE") ||
           value.Contains("FACE"))
        {
            return "Face";
        }

        if(value.Contains("HELM") ||
           value.Contains("HEAD") ||
           value.Contains("HAT") ||
           value.Contains("CAP"))
        {
            return "Head";
        }

        return "Other";
    }

    private static string FriendlyName(
        string filename)
    {
        var value =
            Path.GetFileNameWithoutExtension(
                filename);

        value =
            Regex.Replace(
                value,
                @"^(ATTACH[_-]?M[_-]?|ATTACH[_-]?T[_-]?|CM[_-]?|MT[_-]?)",
                string.Empty,
                RegexOptions.IgnoreCase);

        return Regex.Replace(
                value.Replace(
                    '_',
                    ' ')
                     .Replace(
                         '-',
                         ' '),
                @"\s+",
                " ")
            .Trim();
    }

    private static string Identity(
        string value) =>
        Regex.Replace(
                value ?? string.Empty,
                @"[^A-Za-z0-9]+",
                string.Empty)
            .ToLowerInvariant();

    private static string Slugify(
        string value)
    {
        var slug =
            Regex.Replace(
                    value.ToLowerInvariant(),
                    @"[^a-z0-9]+",
                    "_")
                .Trim('_');

        return slug.Length > 0
            ? slug
            : "attachment";
    }

    private static string UniqueId(
        string requested,
        HashSet<string> used)
    {
        var id =
            requested;
        var suffix =
            2;

        while(!used.Add(
            id))
        {
            id =
                requested +
                "_" +
                suffix.ToString(
                    CultureInfo.InvariantCulture);
            ++suffix;
        }

        return id;
    }
}
