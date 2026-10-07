using System.Globalization;
using System.IO.Compression;
using System.Text.RegularExpressions;
using FireteamLauncher.Infrastructure;

namespace FireteamLauncher.Services;

public sealed record WeaponImportProgress(
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

public sealed record WeaponImportResult(
    int Imported,
    int ExistingSkipped,
    int Unsupported,
    int Warnings,
    int GunsFiles,
    int GunsHhFiles,
    bool FoundGunsHH,
    string ConfigPath,
    string ReportPath)
{
    public string Summary =>
        $"Imported {Imported} Combat Arms weapon definitions. " +
        $"{ExistingSkipped} existing FIRETEAM weapons were kept. " +
        $"{Unsupported} entries are catalog-only/unsupported. " +
        $"{Warnings} asset warnings. " +
        $"Extracted {GunsFiles} files from Guns.zip" +
        (FoundGunsHH
            ? $" and {GunsHhFiles} files from GunsHH.zip. "
            : ". GunsHH.zip was not found beside Guns.zip. ") +
        "New catalog weapons are disabled until you enable them in Definition. " +
        $"Report: {ReportPath}";
}

public sealed class WeaponImportService
{
    private sealed class CaSection
    {
        public required string Section { get; init; }
        public Dictionary<string, string> Values { get; } =
            new(StringComparer.OrdinalIgnoreCase);

        public string Get(
            string key,
            string fallback = "") =>
            Values.TryGetValue(
                key,
                out var value)
            ? value
            : fallback;
    }

    private sealed class ArchiveIndex
    {
        public ArchiveIndex(
            ZipArchive archive)
        {
            Entries =
                archive.Entries
                    .Where(entry =>
                        !string.IsNullOrWhiteSpace(
                            entry.Name))
                    .ToList();

            Exact =
                Entries
                    .GroupBy(
                        entry =>
                            NormalizeArchivePath(
                                entry.FullName),
                        StringComparer.OrdinalIgnoreCase)
                    .ToDictionary(
                        group =>
                            group.Key,
                        group =>
                            group.First(),
                        StringComparer.OrdinalIgnoreCase);

            ByFileName =
                Entries
                    .GroupBy(
                        entry =>
                            entry.Name,
                        StringComparer.OrdinalIgnoreCase)
                    .ToDictionary(
                        group =>
                            group.Key,
                        group =>
                            group.ToList(),
                        StringComparer.OrdinalIgnoreCase);
        }

        public IReadOnlyList<ZipArchiveEntry> Entries { get; }
        public Dictionary<string, ZipArchiveEntry> Exact { get; }
        public Dictionary<string, List<ZipArchiveEntry>> ByFileName { get; }
    }

    private sealed record AssetResult(
        string RuntimePath,
        bool Found);

    public WeaponImportResult Import(
        string weaponsTxtPath,
        string gunsZipPath,
        IProgress<WeaponImportProgress>? progress = null,
        CancellationToken cancellationToken = default)
    {
        if(!File.Exists(weaponsTxtPath))
        {
            throw new FileNotFoundException(
                "WEAPONS.txt was not found.",
                weaponsTxtPath);
        }

        if(!File.Exists(gunsZipPath))
        {
            throw new FileNotFoundException(
                "Guns.zip was not found.",
                gunsZipPath);
        }

        var repo =
            LauncherPaths.FindRepositoryDirectory()
            ?? throw new InvalidOperationException(
                "Combat Arms import is a development/content-tool operation and requires the FIRETEAM repository folder.");

        var configPaths =
            LauncherPaths.FindConfigPaths(
                "weapons.cfg");

        var configPath =
            configPaths.Source ??
            configPaths.Runtime ??
            throw new FileNotFoundException(
                "Could not locate config\\weapons.cfg.");

        var sections =
            ParseSections(
                weaponsTxtPath);

        var weapons =
            sections
                .Where(section =>
                    Regex.IsMatch(
                        section.Section,
                        @"^Weapon\d+$",
                        RegexOptions.IgnoreCase) &&
                    !string.IsNullOrWhiteSpace(
                        section.Get("Name")))
                .ToList();

        progress?.Report(
            new WeaponImportProgress(
                0,
                weapons.Count,
                "ATTRIBUTES",
                $"Parsed {weapons.Count} weapon definitions from WEAPONS.txt."));

        var ammoByName =
            sections
                .Where(section =>
                    Regex.IsMatch(
                        section.Section,
                        @"^Ammo\d+$",
                        RegexOptions.IgnoreCase) &&
                    !string.IsNullOrWhiteSpace(
                        section.Get("Name")))
                .GroupBy(
                    section =>
                        NormalizeIdentity(
                            section.Get("Name")),
                    StringComparer.OrdinalIgnoreCase)
                .ToDictionary(
                    group =>
                        group.Key,
                    group =>
                        group.First(),
                    StringComparer.OrdinalIgnoreCase);

        var doc =
            FireteamConfigDocument.Load(
                configPath);

        var activeNames =
            new HashSet<string>(
                StringComparer.OrdinalIgnoreCase);

        var existingCatalog =
            new Dictionary<string, string>(
                StringComparer.OrdinalIgnoreCase);

        foreach(var section in doc.Sections)
        {
            if(Regex.IsMatch(
                section,
                @"^weapon[1-5]$",
                RegexOptions.IgnoreCase))
            {
                AddIdentity(
                    activeNames,
                    doc.GetValue(
                        section,
                        "name"));

                AddIdentity(
                    activeNames,
                    doc.GetValue(
                        section,
                        "ca_name"));
            }
            else if(section.StartsWith(
                "catalog.",
                StringComparison.OrdinalIgnoreCase))
            {
                var identity =
                    NormalizeIdentity(
                        doc.GetValue(
                            section,
                            "ca_name",
                            doc.GetValue(
                                section,
                                "name")));

                if(identity.Length > 0 &&
                   !existingCatalog.ContainsKey(
                       identity))
                {
                    existingCatalog[identity] =
                        section;
                }
            }
        }

        var localImportRoot =
            Path.Combine(
                repo,
                "assets-local",
                "WeaponImports");

        Directory.CreateDirectory(
            localImportRoot);

        // Import is intentionally local-only. Copying thousands of archive
        // files into the live runtime during the UI import made the operation
        // unnecessarily slow and doubled disk I/O. stage-imported-weapons.cmd
        // performs the explicit runtime staging step after import.
        string? runtimeRezRoot = null;

        var gunsHHPath =
            FindGunsHH(
                gunsZipPath);

        using var gunsFile =
            File.OpenRead(
                gunsZipPath);

        using var gunsZip =
            new ZipArchive(
                gunsFile,
                ZipArchiveMode.Read);

        var guns =
            new ArchiveIndex(
                gunsZip);

        FileStream? hhFile = null;
        ZipArchive? hhZip = null;

        try
        {
            ArchiveIndex? hh = null;

            if(gunsHHPath is not null)
            {
                hhFile =
                    File.OpenRead(
                        gunsHHPath);

                hhZip =
                    new ZipArchive(
                        hhFile,
                        ZipArchiveMode.Read);

                hh =
                    new ArchiveIndex(
                        hhZip);
            }

            // WEAPONS.txt can be newer than the user's Guns archives.
            // Index the archives first and copy only assets belonging to
            // definitions that can be proven to exist in this exact archive.
            var gunsFiles =
                guns.Entries.Count;

            var gunsHhFiles =
                hh?.Entries.Count ??
                0;

            progress?.Report(
                new WeaponImportProgress(
                    0,
                    weapons.Count,
                    "ARCHIVE",
                    $"Indexed {gunsFiles} Guns.zip files" +
                    (hh is null
                        ? "; GunsHH.zip not found."
                        : $" and {gunsHhFiles} GunsHH.zip files.")));

            var imported = 0;
            var processed = 0;
            var existingSkipped = 0;
            var unsupported = 0;
            var warnings = 0;
            var report =
                new List<string>
                {
                    "FIRETEAM Combat Arms weapon import",
                    $"WEAPONS: {weaponsTxtPath}",
                    $"GUNS: {gunsZipPath}",
                    $"GUNS HH: {gunsHHPath ?? "(not found)"}",
                    string.Empty
                };

            foreach(var weapon in weapons)
            {
                cancellationToken.ThrowIfCancellationRequested();
                ++processed;

                var caName =
                    weapon.Get("Name");

                var identity =
                    NormalizeIdentity(
                        caName);

                if(identity.Length == 0)
                {
                    continue;
                }

                if(activeNames.Contains(
                       identity) &&
                   !existingCatalog.ContainsKey(
                       identity))
                {
                    ++existingSkipped;
                    continue;
                }

                var ammoName =
                    weapon.Get(
                        "AmmoName0");

                ammoByName.TryGetValue(
                    NormalizeIdentity(
                        ammoName),
                    out var ammo);

                var slug =
                    Slugify(
                        caName);

                var section =
                    existingCatalog.TryGetValue(
                        identity,
                        out var existingSection)
                    ? existingSection
                    : $"catalog.{slug}";

                if(doc.HasSection(section) &&
                   !string.Equals(
                       NormalizeIdentity(
                           doc.GetValue(
                               section,
                               "ca_name")),
                       identity,
                       StringComparison.OrdinalIgnoreCase))
                {
                    section =
                        $"catalog.{slug}_{weapon.Section.ToLowerInvariant()}";
                }

                var preservedEnabled =
                    doc.HasSection(section) &&
                    doc.GetBool(
                        section,
                        "enabled");

                var tokens =
                    BuildTokens(
                        caName);

                var warningList =
                    new List<string>();

                var pvModelDeclared =
                    weapon.Get(
                        "PVModelNormal");

                var pvModelEntry =
                    ResolveAsset(
                        guns,
                        pvModelDeclared,
                        tokens,
                        "GUNS_M_PV",
                        ".ltb",
                        entry =>
                            !IsAnimationEntry(entry) &&
                            !entry.Name.Contains(
                                "I_INFO",
                                StringComparison.OrdinalIgnoreCase));

                var pvTextureSource =
                    FindSkinPath(
                        weapon,
                        "PVSkin",
                        "GUNS_T_PV");

                var pvTextureEntry =
                    ResolveAsset(
                        guns,
                        pvTextureSource,
                        tokens,
                        "GUNS_T_PV",
                        ".dtx",
                        entry =>
                            !entry.Name.Contains(
                                "I_INFO",
                                StringComparison.OrdinalIgnoreCase));

                if(pvModelEntry is null ||
                   pvTextureEntry is null)
                {
                    ++unsupported;

                    if(existingCatalog.TryGetValue(
                           identity,
                           out var staleSection))
                    {
                        doc.RemoveSection(
                            staleSection);
                        existingCatalog.Remove(
                            identity);
                    }

                    report.Add(
                        $"[SKIP/NO-ARCHIVE] {FriendlyName(caName)} [{weapon.Section}]");
                    report.Add(
                        $"  PV model declared: {DisplayDeclaredPath(pvModelDeclared)}");
                    report.Add(
                        $"  PV model archive:  {DisplayArchiveMatch(pvModelEntry)}");
                    report.Add(
                        $"  PV skin declared:  {DisplayDeclaredPath(pvTextureSource)}");
                    report.Add(
                        $"  PV skin archive:   {DisplayArchiveMatch(pvTextureEntry)}");
                    report.Add(
                        "  Result: not imported; WEAPONS.txt entry is not backed by this Guns.zip.");
                    report.Add(
                        string.Empty);

                    ReportImportProgress(
                        progress,
                        processed,
                        weapons.Count,
                        "MATCHING",
                        $"Skipped {FriendlyName(caName)}: required PV asset missing from supplied archive.");

                    continue;
                }

                var pvModel =
                    StageAsset(
                        pvModelEntry,
                        localImportRoot,
                        runtimeRezRoot,
                        null);

                var pvTexture =
                    StageAsset(
                        pvTextureEntry,
                        localImportRoot,
                        runtimeRezRoot,
                        null);

                StageAdditionalSkins(
                    weapon,
                    "PVSkin",
                    "GUNS_T_PV",
                    guns,
                    tokens,
                    localImportRoot,
                    runtimeRezRoot);
                var hhSource =
                    hh ?? guns;

                var hhModel =
                    StageAsset(
                        ResolveAsset(
                            hhSource,
                            weapon.Get(
                                "HHModel"),
                            tokens,
                            "GUNS_M_HH",
                            ".ltb",
                            entry =>
                                !entry.Name.Contains(
                                    "I_INFO",
                                    StringComparison.OrdinalIgnoreCase)),
                        localImportRoot,
                        runtimeRezRoot,
                        null);

                if(!hhModel.Found)
                {
                    warningList.Add(
                        "HH model missing");
                }

                var hhTextureSource =
                    FindSkinPath(
                        weapon,
                        "HHSkin",
                        "GUNS_T_HH");

                var hhTexture =
                    StageAsset(
                        ResolveAsset(
                            hhSource,
                            hhTextureSource,
                            tokens,
                            "GUNS_T_HH",
                            ".dtx",
                            entry =>
                                !entry.Name.Contains(
                                    "I_INFO",
                                    StringComparison.OrdinalIgnoreCase)),
                        localImportRoot,
                        runtimeRezRoot,
                        null);

                if(!hhTexture.Found)
                {
                    warningList.Add(
                        "HH texture missing");
                }

                StageAdditionalSkins(
                    weapon,
                    "HHSkin",
                    "GUNS_T_HH",
                    hhSource,
                    tokens,
                    localImportRoot,
                    runtimeRezRoot);

                var animation =
                    StageAsset(
                        FindAnimation(
                            guns,
                            pvModelEntry,
                            weapon.Get(
                                "PVModelNormal")),
                        localImportRoot,
                        runtimeRezRoot,
                        null);

                var soundDirectory =
                    $"Weapons/ca/imported_snd/{slug}";

                var fireSound =
                    StageAsset(
                        ResolveSound(
                            guns,
                            weapon.Get(
                                "FireSnd"),
                            tokens,
                            "FIRE"),
                        localImportRoot,
                        runtimeRezRoot,
                        $"{soundDirectory}/FIRE.WAV");

                if(!fireSound.Found)
                {
                    warningList.Add(
                        "fire sound missing");
                }

                StageAsset(
                    ResolveSound(
                        guns,
                        weapon.Get(
                            "SelectSnd"),
                        tokens,
                        "SELECT"),
                    localImportRoot,
                    runtimeRezRoot,
                    $"{soundDirectory}/SELECT.WAV");

                StageAsset(
                    ResolveSound(
                        guns,
                        weapon.Get(
                            "ReloadSnd1"),
                        tokens,
                        "RELOAD"),
                    localImportRoot,
                    runtimeRezRoot,
                    $"{soundDirectory}/RELOAD.WAV");

                var guntype =
                    ParseInt(
                        weapon.Get(
                            "Guntype"),
                        -1);

                var fireteamType =
                    MapWeaponType(
                        guntype);

                var typeSupported =
                    fireteamType is not null;

                if(!typeSupported)
                {
                    ++unsupported;
                    fireteamType =
                        "hitscan";
                    warningList.Add(
                        $"unsupported CA Guntype {guntype}");
                }

                var clip =
                    guntype == 0
                    ? 0
                    : Math.Max(
                        0,
                        ParseInt(
                            weapon.Get(
                                "ShotsPerClip"),
                            0));

                var totalAmmo =
                    Math.Max(
                        clip,
                        ParseInt(
                            ammo?.Get(
                                "SelectionAmount") ??
                            string.Empty,
                            clip));

                var reserve =
                    guntype == 0
                    ? 0
                    : Math.Max(
                        0,
                        totalAmmo - clip);

                var damage =
                    ParseInt(
                        ammo?.Get(
                            "InstDamage") ??
                        string.Empty,
                        0);

                if((fireteamType == "grenade" ||
                    fireteamType == "rocket") &&
                   damage <= 0)
                {
                    damage =
                        ParseInt(
                            ammo?.Get(
                                "AreaDamage") ??
                            string.Empty,
                            0);
                }

                var supported =
                    typeSupported &&
                    pvModel.Found &&
                    pvTexture.Found &&
                    damage > 0;

                if(!supported &&
                   typeSupported)
                {
                    ++unsupported;
                }

                var friendlyName =
                    FriendlyName(
                        caName);

                doc.EnsureSection(
                    section);

                Set(
                    doc,
                    section,
                    "id",
                    slug);
                Set(
                    doc,
                    section,
                    "name",
                    friendlyName);
                Set(
                    doc,
                    section,
                    "type",
                    fireteamType);
                Set(
                    doc,
                    section,
                    "category",
                    MapLoadoutCategory(
                        guntype));
                Set(
                    doc,
                    section,
                    "clip",
                    clip);
                Set(
                    doc,
                    section,
                    "reserve",
                    reserve);
                Set(
                    doc,
                    section,
                    "damage",
                    damage);
                Set(
                    doc,
                    section,
                    "fire_interval",
                    DeriveFireInterval(
                        guntype,
                        weapon,
                        ammo));
                Set(
                    doc,
                    section,
                    "range",
                    weapon.Get(
                        "Range",
                        "2500"));
                Set(
                    doc,
                    section,
                    "effect_range0",
                    weapon.Get(
                        "Effectrange0",
                        weapon.Get(
                            "Range",
                            "2500")));
                Set(
                    doc,
                    section,
                    "effect_range1",
                    weapon.Get(
                        "Effectrange1",
                        weapon.Get(
                            "Range",
                            "2500")));
                Set(
                    doc,
                    section,
                    "effect_range2",
                    weapon.Get(
                        "Effectrange2",
                        weapon.Get(
                            "Range",
                            "2500")));
                Set(
                    doc,
                    section,
                    "damage_mult0",
                    ammo?.Get(
                        "Rangedamage0",
                        "1.0") ??
                    "1.0");
                Set(
                    doc,
                    section,
                    "damage_mult1",
                    ammo?.Get(
                        "Rangedamage1",
                        "0.7") ??
                    "0.7");
                Set(
                    doc,
                    section,
                    "damage_mult2",
                    ammo?.Get(
                        "Rangedamage2",
                        "0.35") ??
                    "0.35");
                Set(
                    doc,
                    section,
                    "reload",
                    DeriveReloadTime(
                        guntype));
                Set(
                    doc,
                    section,
                    "automatic",
                    IsAutomatic(
                        weapon)
                    ? "1"
                    : "0");
                Set(
                    doc,
                    section,
                    "auto_reload",
                    guntype == 0
                    ? "0"
                    : "1");
                Set(
                    doc,
                    section,
                    "show_crosshair",
                    guntype == 0
                    ? "0"
                    : "1");

                SetDefaultCrosshair(
                    doc,
                    section,
                    guntype);

                SetViewPosition(
                    doc,
                    section,
                    weapon.Get("Pos"));

                Set(
                    doc,
                    section,
                    "pv_model",
                    pvModel.RuntimePath);
                Set(
                    doc,
                    section,
                    "pv_anim",
                    animation.RuntimePath);
                Set(
                    doc,
                    section,
                    "pv_texture",
                    pvTexture.RuntimePath);
                Set(
                    doc,
                    section,
                    "hh_model",
                    hhModel.RuntimePath);
                Set(
                    doc,
                    section,
                    "hh_texture",
                    hhTexture.RuntimePath);
                Set(
                    doc,
                    section,
                    "sound_dir",
                    fireSound.Found
                    ? soundDirectory
                    : string.Empty);

                if(fireteamType == "grenade" ||
                   fireteamType == "rocket")
                {
                    Set(
                        doc,
                        section,
                        "splash_radius",
                        ammo?.Get(
                            "AreaDamageRadius",
                            "0") ??
                        "0");

                    Set(
                        doc,
                        section,
                        "projectile_speed",
                        fireteamType == "rocket"
                        ? "900"
                        : "650");

                    Set(
                        doc,
                        section,
                        "fuse",
                        fireteamType == "grenade"
                        ? "3.0"
                        : "6.0");
                }

                Set(
                    doc,
                    section,
                    "source",
                    "COMBAT ARMS AUTO IMPORT");
                Set(
                    doc,
                    section,
                    "supported",
                    supported
                    ? "1"
                    : "0");
                Set(
                    doc,
                    section,
                    "enabled",
                    preservedEnabled
                    ? "1"
                    : "0");

                Set(
                    doc,
                    section,
                    "ca_name",
                    caName);
                Set(
                    doc,
                    section,
                    "ca_weapon_section",
                    weapon.Section);
                Set(
                    doc,
                    section,
                    "ca_ammo_name",
                    ammoName);
                Set(
                    doc,
                    section,
                    "ca_guntype",
                    guntype);
                Set(
                    doc,
                    section,
                    "ca_vectors_per_round",
                    weapon.Get(
                        "VectorsPerRound",
                        "1"));
                Set(
                    doc,
                    section,
                    "ca_pv_model",
                    weapon.Get(
                        "PVModelNormal"));
                Set(
                    doc,
                    section,
                    "ca_pv_skin",
                    pvTextureSource);
                Set(
                    doc,
                    section,
                    "ca_hh_model",
                    weapon.Get(
                        "HHModel"));
                Set(
                    doc,
                    section,
                    "ca_hh_skin",
                    hhTextureSource);
                Set(
                    doc,
                    section,
                    "ca_timing_verified",
                    "0");
                Set(
                    doc,
                    section,
                    "ca_import_status",
                    warningList.Count == 0
                    ? "ready_for_test"
                    : string.Join(
                        "; ",
                        warningList));

                warnings +=
                    warningList.Count;

                report.Add(
                    $"[{(supported ? "READY" : "PARTIAL")}] {friendlyName} [{weapon.Section}] -> [{section}]");
                report.Add(
                    $"  PV model: {DisplayDeclaredPath(pvModelDeclared)} => {DisplayArchiveMatch(pvModelEntry)}");
                report.Add(
                    $"  PV skin:  {DisplayDeclaredPath(pvTextureSource)} => {DisplayArchiveMatch(pvTextureEntry)}");
                report.Add(
                    $"  Result: type={fireteamType}, supported={(supported ? 1 : 0)} | " +
                    (warningList.Count == 0
                        ? "OK"
                        : string.Join(
                            ", ",
                            warningList)));
                report.Add(
                    string.Empty);

                ++imported;

                ReportImportProgress(
                    progress,
                    processed,
                    weapons.Count,
                    "MATCHING",
                    $"Matched {friendlyName} to exact archive filenames.");
            }

            progress?.Report(
                new WeaponImportProgress(
                    weapons.Count,
                    weapons.Count,
                    "SAVING",
                    $"Saving {imported} archive-backed definitions..."));

            SaveConfigCopies(
                doc,
                configPaths);

            var reportPath =
                Path.Combine(
                    localImportRoot,
                    "last-import.txt");

            File.WriteAllLines(
                reportPath,
                report);

            progress?.Report(
                new WeaponImportProgress(
                    weapons.Count,
                    weapons.Count,
                    "COMPLETE",
                    $"Imported {imported}; full audit written to {reportPath}."));

            return new WeaponImportResult(
                imported,
                existingSkipped,
                unsupported,
                warnings,
                gunsFiles,
                gunsHhFiles,
                gunsHHPath is not null,
                configPath,
                reportPath);
        }
        finally
        {
            hhZip?.Dispose();
            hhFile?.Dispose();
        }
    }

    private static List<CaSection> ParseSections(
        string path)
    {
        var result =
            new List<CaSection>();

        CaSection? current = null;

        foreach(var rawLine in File.ReadLines(path))
        {
            var line =
                rawLine
                    .TrimStart('\uFEFF')
                    .Trim();

            if(line.Length == 0)
            {
                continue;
            }

            if(line.StartsWith('[') &&
               line.EndsWith(']'))
            {
                current =
                    new CaSection
                    {
                        Section =
                            line[1..^1]
                                .Trim()
                    };

                result.Add(
                    current);

                continue;
            }

            if(current is null ||
               line.StartsWith("//"))
            {
                continue;
            }

            var equals =
                line.IndexOf('=');

            if(equals <= 0)
            {
                continue;
            }

            var key =
                line[..equals]
                    .Trim();

            var value =
                StripInlineComment(
                    line[(equals + 1)..])
                    .Trim();

            if(value.Length >= 2 &&
               value[0] == '"' &&
               value[^1] == '"')
            {
                value =
                    value[1..^1];
            }

            current.Values[key] =
                value;
        }

        return result;
    }

    private static string StripInlineComment(
        string value)
    {
        var inQuote = false;

        for(var i = 0;
            i < value.Length - 1;
            ++i)
        {
            if(value[i] == '"')
            {
                inQuote = !inQuote;
                continue;
            }

            if(!inQuote &&
               value[i] == '/' &&
               value[i + 1] == '/')
            {
                return value[..i];
            }
        }

        return value;
    }

    private static void SaveConfigCopies(
        FireteamConfigDocument doc,
        (string? Source, string? Runtime) paths)
    {
        if(paths.Source is not null)
        {
            doc.Save(
                paths.Source);
        }

        if(paths.Runtime is not null &&
           !string.Equals(
               paths.Runtime,
               paths.Source,
               StringComparison.OrdinalIgnoreCase))
        {
            doc.Save(
                paths.Runtime);
        }
    }

    private static void ReportImportProgress(
        IProgress<WeaponImportProgress>? progress,
        int processed,
        int total,
        string stage,
        string detail)
    {
        if(progress is null)
        {
            return;
        }

        if(processed == 1 ||
           processed == total ||
           processed % 10 == 0)
        {
            progress.Report(
                new WeaponImportProgress(
                    processed,
                    total,
                    stage,
                    detail));
        }
    }

    private static string DisplayDeclaredPath(
        string value) =>
        string.IsNullOrWhiteSpace(
            value)
            ? "(not declared)"
            : NormalizeArchivePath(
                value);

    private static string DisplayArchiveMatch(
        ZipArchiveEntry? entry) =>
        entry is null
            ? "MISSING"
            : NormalizeArchivePath(
                entry.FullName);
    private static string? FindGunsHH(
        string gunsZipPath)
    {
        var directory =
            Path.GetDirectoryName(
                gunsZipPath);

        if(string.IsNullOrWhiteSpace(
            directory))
        {
            return null;
        }

        var exact =
            Path.Combine(
                directory,
                "GunsHH.zip");

        if(File.Exists(exact))
        {
            return exact;
        }

        return Directory
            .EnumerateFiles(
                directory,
                "*.zip")
            .FirstOrDefault(path =>
                Path.GetFileName(path)
                    .Contains(
                        "gunshh",
                        StringComparison.OrdinalIgnoreCase) ||
                Path.GetFileName(path)
                    .Contains(
                        "guns_hh",
                        StringComparison.OrdinalIgnoreCase));
    }

    private static ZipArchiveEntry? ResolveAsset(
        ArchiveIndex archive,
        string desiredPath,
        IReadOnlyList<string> tokens,
        string pathToken,
        string extension,
        Func<ZipArchiveEntry, bool> extraFilter)
    {
        if(!string.IsNullOrWhiteSpace(
            desiredPath))
        {
            // Decrypted CA attributes already tell us the intended asset.
            // Never substitute an unrelated gun just because a generic token
            // such as "Gold" or "Event" happens to match another filename.
            return ResolveDeclaredAsset(
                archive,
                desiredPath,
                pathToken,
                extension,
                extraFilter);
        }

        return null;
    }

    private static ZipArchiveEntry? ResolveDeclaredAsset(
        ArchiveIndex archive,
        string desiredPath,
        string pathToken,
        string extension,
        Func<ZipArchiveEntry, bool> extraFilter)
    {
        var normalized =
            NormalizeArchivePath(
                desiredPath);

        if(archive.Exact.TryGetValue(
               normalized,
               out var exact) &&
           AssetMatches(
               exact,
               pathToken,
               extension,
               extraFilter))
        {
            return exact;
        }

        var fileName =
            normalized
                .Split('\\')
                .LastOrDefault() ??
            string.Empty;

        if(fileName.Length == 0 ||
           !archive.ByFileName.TryGetValue(
               fileName,
               out var candidates))
        {
            return null;
        }

        var valid =
            candidates
                .Where(entry =>
                    AssetMatches(
                        entry,
                        pathToken,
                        extension,
                        extraFilter))
                .ToList();

        var suffix =
            valid.FirstOrDefault(entry =>
                NormalizeArchivePath(
                    entry.FullName)
                    .EndsWith(
                        normalized,
                        StringComparison.OrdinalIgnoreCase));

        if(suffix is not null)
        {
            return suffix;
        }

        // Filename + expected archive family is still deterministic when
        // exactly one candidate exists. More than one candidate is ambiguous
        // and should remain unsupported rather than silently choosing wrong.
        return valid.Count == 1
            ? valid[0]
            : null;
    }

    private static ZipArchiveEntry? ResolveByTokens(
        ArchiveIndex archive,
        IReadOnlyList<string> tokens,
        string pathToken,
        string extension,
        Func<ZipArchiveEntry, bool> extraFilter)
    {
        ZipArchiveEntry? best = null;
        var bestScore = 0;

        foreach(var entry in archive.Entries)
        {
            if(!AssetMatches(
                   entry,
                   pathToken,
                   extension,
                   extraFilter))
            {
                continue;
            }

            var identity =
                NormalizeIdentity(
                    entry.Name);

            var score = 0;

            foreach(var token in tokens)
            {
                if(token.Length >= 4 &&
                   identity.Contains(
                       token,
                       StringComparison.OrdinalIgnoreCase))
                {
                    score =
                        Math.Max(
                            score,
                            token.Length);
                }
            }

            if(score > bestScore)
            {
                bestScore = score;
                best = entry;
            }
        }

        return bestScore >= 4
            ? best
            : null;
    }

    private static bool AssetMatches(
        ZipArchiveEntry entry,
        string pathToken,
        string extension,
        Func<ZipArchiveEntry, bool> extraFilter) =>
        entry.FullName.Contains(
            pathToken,
            StringComparison.OrdinalIgnoreCase) &&
        entry.Name.EndsWith(
            extension,
            StringComparison.OrdinalIgnoreCase) &&
        extraFilter(entry);

    private static ZipArchiveEntry? ResolveSound(
        ArchiveIndex archive,
        string desiredPath,
        IReadOnlyList<string> tokens,
        string stem)
    {
        if(!string.IsNullOrWhiteSpace(
            desiredPath))
        {
            return ResolveDeclaredAsset(
                archive,
                desiredPath,
                "GUNS_SND",
                ".wav",
                entry =>
                    entry.Name.StartsWith(
                        stem,
                        StringComparison.OrdinalIgnoreCase));
        }

        return null;
    }

    private static ZipArchiveEntry? FindAnimation(
        ArchiveIndex archive,
        ZipArchiveEntry? pvModel,
        string declaredModelPath)
    {
        var modelName =
            pvModel?.Name;

        if(string.IsNullOrWhiteSpace(
            modelName))
        {
            var normalized =
                NormalizeArchivePath(
                    declaredModelPath);

            modelName =
                normalized
                    .Split('\\')
                    .LastOrDefault();
        }

        if(string.IsNullOrWhiteSpace(
            modelName))
        {
            return null;
        }

        var modelTokens =
            BuildModelTokens(
                modelName);

        if(modelTokens.Count == 0)
        {
            return null;
        }

        var pvFamily =
            pvModel is null
            ? string.Empty
            : NormalizeArchivePath(
                pvModel.FullName);

        var familySlash =
            pvFamily.LastIndexOf('\\');

        var family =
            familySlash > 0
            ? pvFamily[..familySlash]
            : string.Empty;

        ZipArchiveEntry? best = null;
        var bestScore = 0;

        foreach(var entry in archive.Entries)
        {
            if(!entry.FullName.Contains(
                   "GUNS_M_PV",
                   StringComparison.OrdinalIgnoreCase) ||
               !entry.Name.EndsWith(
                   ".ltb",
                   StringComparison.OrdinalIgnoreCase) ||
               !IsAnimationEntry(entry))
            {
                continue;
            }

            var entryPath =
                NormalizeArchivePath(
                    entry.FullName);

            if(family.Length > 0 &&
               !entryPath.StartsWith(
                   family + "\\",
                   StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            var identity =
                NormalizeIdentity(
                    entry.Name);

            var score =
                modelTokens
                    .Where(token =>
                        identity.Contains(
                            token,
                            StringComparison.OrdinalIgnoreCase))
                    .Select(token =>
                        token.Length)
                    .DefaultIfEmpty(0)
                    .Max();

            if(score > bestScore)
            {
                bestScore = score;
                best = entry;
            }
        }

        return bestScore >= 4
            ? best
            : null;
    }

    private static IReadOnlyList<string> BuildModelTokens(
        string modelName)
    {
        var ignored =
            new HashSet<string>(
                new[]
                {
                    "PV", "AR", "SR", "MG", "SMG", "SG", "PISTOL",
                    "ML", "HND", "CM", "NM", "DF", "SH", "CH", "BC",
                    "LTB", "ANI", "ANIBASE"
                },
                StringComparer.OrdinalIgnoreCase);

        return Regex.Split(
                Path.GetFileNameWithoutExtension(
                    modelName),
                @"[^A-Za-z0-9]+")
            .Select(
                NormalizeIdentity)
            .Where(token =>
                token.Length >= 4 &&
                !ignored.Contains(
                    token))
            .Distinct(
                StringComparer.OrdinalIgnoreCase)
            .OrderByDescending(token =>
                token.Length)
            .ToList();
    }

    private static bool IsAnimationEntry(
        ZipArchiveEntry entry) =>
        entry.Name.Contains(
            "ANIBASE",
            StringComparison.OrdinalIgnoreCase) ||
        entry.Name.StartsWith(
            "ANI_",
            StringComparison.OrdinalIgnoreCase);

    private static int ExtractArchiveAssets(
        ArchiveIndex archive,
        string localRoot,
        string? runtimeRezRoot)
    {
        var extracted = 0;

        foreach(var entry in archive.Entries)
        {
            var relative =
                "Weapons/ca/" +
                entry.FullName
                    .Replace('\\', '/')
                    .TrimStart('/');

            StageAsset(
                entry,
                localRoot,
                runtimeRezRoot,
                relative);

            ++extracted;
        }

        return extracted;
    }

    private static AssetResult StageAsset(
        ZipArchiveEntry? entry,
        string localRoot,
        string? runtimeRezRoot,
        string? forcedRelativePath)
    {
        if(entry is null)
        {
            return new AssetResult(
                string.Empty,
                false);
        }

        var relative =
            forcedRelativePath ??
            ("Weapons/ca/" +
             entry.FullName
                 .Replace(
                     '\\',
                     '/')
                 .TrimStart('/'));

        CopyEntry(
            entry,
            localRoot,
            relative);

        if(runtimeRezRoot is not null)
        {
            CopyLocalAssetToRuntime(
                localRoot,
                runtimeRezRoot,
                relative);
        }

        return new AssetResult(
            relative.Replace(
                '\\',
                '/'),
            true);
    }

    private static void CopyEntry(
        ZipArchiveEntry entry,
        string root,
        string relative)
    {
        var destination =
            Path.Combine(
                root,
                relative.Replace(
                    '/',
                    Path.DirectorySeparatorChar));

        var parent =
            Path.GetDirectoryName(
                destination);

        if(!string.IsNullOrWhiteSpace(
            parent))
        {
            Directory.CreateDirectory(
                parent);
        }

        if(File.Exists(
               destination) &&
           new FileInfo(
               destination).Length ==
           entry.Length)
        {
            return;
        }

        using var input =
            entry.Open();

        using var output =
            File.Create(
                destination);

        input.CopyTo(
            output);
    }

    private static void CopyLocalAssetToRuntime(
        string localRoot,
        string runtimeRezRoot,
        string relative)
    {
        var source =
            Path.Combine(
                localRoot,
                relative.Replace(
                    '/',
                    Path.DirectorySeparatorChar));

        var destination =
            Path.Combine(
                runtimeRezRoot,
                relative.Replace(
                    '/',
                    Path.DirectorySeparatorChar));

        var parent =
            Path.GetDirectoryName(
                destination);

        if(!string.IsNullOrWhiteSpace(
            parent))
        {
            Directory.CreateDirectory(
                parent);
        }

        File.Copy(
            source,
            destination,
            true);
    }

    private static string FindSkinPath(
        CaSection weapon,
        string prefix,
        string pathToken)
    {
        for(var i = 0;
            i < 16;
            ++i)
        {
            var value =
                weapon.Get(
                    prefix + i);

            if(value.EndsWith(
                   ".dtx",
                   StringComparison.OrdinalIgnoreCase) &&
               value.Contains(
                   pathToken,
                   StringComparison.OrdinalIgnoreCase))
            {
                return value;
            }
        }

        return string.Empty;
    }

    private static void StageAdditionalSkins(
        CaSection weapon,
        string prefix,
        string pathToken,
        ArchiveIndex archive,
        IReadOnlyList<string> tokens,
        string localRoot,
        string? runtimeRezRoot)
    {
        for(var i = 0;
            i < 16;
            ++i)
        {
            var value =
                weapon.Get(
                    prefix + i);

            if(!value.EndsWith(
                    ".dtx",
                    StringComparison.OrdinalIgnoreCase) ||
               !value.Contains(
                    pathToken,
                    StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            StageAsset(
                ResolveAsset(
                    archive,
                    value,
                    tokens,
                    pathToken,
                    ".dtx",
                    entry => true),
                localRoot,
                runtimeRezRoot,
                null);
        }
    }

    private static IReadOnlyList<string> BuildTokens(
        string name)
    {
        var result =
            new HashSet<string>(
                StringComparer.OrdinalIgnoreCase);

        var collapsed =
            NormalizeIdentity(
                name);

        if(collapsed.Length >= 3)
        {
            result.Add(
                collapsed);
        }

        foreach(var part in Regex.Split(
            name,
            @"[^A-Za-z0-9]+"))
        {
            var token =
                NormalizeIdentity(
                    part);

            if(token.Length >= 3)
            {
                result.Add(
                    token);
            }
        }

        return result
            .OrderByDescending(
                token =>
                    token.Length)
            .ToList();
    }

    private static string NormalizeArchivePath(
        string path) =>
        path
            .Replace(
                '/',
                '\\')
            .TrimStart('\\');

    private static string NormalizeIdentity(
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
            : "ca_weapon";
    }

    private static string FriendlyName(
        string value) =>
        Regex.Replace(
            value.Replace(
                '_',
                ' '),
            @"\s+",
            " ")
        .Trim();

    private static void AddIdentity(
        HashSet<string> set,
        string value)
    {
        var identity =
            NormalizeIdentity(
                value);

        if(identity.Length > 0)
        {
            set.Add(
                identity);
        }
    }

    private static string? MapWeaponType(
        int guntype) =>
        guntype switch
        {
            0 => "melee",
            >= 1 and <= 6 => "hitscan",
            7 => "grenade",
            9 => "rocket",
            13 => "hitscan",
            15 => "grenade",
            _ => null
        };

    private static string MapLoadoutCategory(
        int guntype) =>
        guntype switch
        {
            0 => "Melee",
            1 => "Pistol",
            2 or 13 => "SG",
            3 => "SMG",
            4 => "AR",
            5 => "MG",
            6 => "SR",
            7 => "Throwing",
            9 or 15 => "Launcher",
            _ => "Other"
        };

    private static bool IsAutomatic(
        CaSection weapon) =>
        weapon.Get(
            "HudFireMode") == "2" &&
        weapon.Get(
            "RapidFireCount") != "1";

    private static string DeriveFireInterval(
        int guntype,
        CaSection weapon,
        CaSection? ammo)
    {
        if(guntype == 0)
        {
            return "0.55";
        }

        if(guntype == 1)
        {
            return "0.18";
        }

        if(guntype == 2 ||
           guntype == 13)
        {
            return "0.80";
        }

        if(guntype == 6)
        {
            return "1.25";
        }

        if(guntype == 7 ||
           guntype == 15)
        {
            return "0.75";
        }

        if(guntype == 9)
        {
            return "1.00";
        }

        var delay =
            ParseDouble(
                ammo?.Get(
                    "DecDelay") ??
                string.Empty,
                100.0);

        return Math.Clamp(
                delay / 1000.0,
                0.06,
                0.25)
            .ToString(
                "0.000",
                CultureInfo.InvariantCulture);
    }

    private static string DeriveReloadTime(
        int guntype) =>
        guntype switch
        {
            0 => "0",
            1 => "1.80",
            2 or 13 => "2.60",
            6 => "3.20",
            7 or 15 => "0",
            9 => "3.00",
            _ => "2.40"
        };

    private static void SetDefaultCrosshair(
        FireteamConfigDocument doc,
        string section,
        int guntype)
    {
        if(guntype == 0)
        {
            Set(
                doc,
                section,
                "crosshair_base_gap",
                "0");
            Set(
                doc,
                section,
                "crosshair_shot_kick",
                "0");
            Set(
                doc,
                section,
                "crosshair_move_kick",
                "0");
            Set(
                doc,
                section,
                "crosshair_max_gap",
                "0");
            Set(
                doc,
                section,
                "crosshair_recover",
                "0");
            return;
        }

        Set(
            doc,
            section,
            "crosshair_base_gap",
            guntype == 6
            ? "4"
            : "6");
        Set(
            doc,
            section,
            "crosshair_shot_kick",
            guntype == 6
            ? "8"
            : "3");
        Set(
            doc,
            section,
            "crosshair_move_kick",
            guntype == 6
            ? "8"
            : "4");
        Set(
            doc,
            section,
            "crosshair_max_gap",
            guntype == 6
            ? "24"
            : "18");
        Set(
            doc,
            section,
            "crosshair_recover",
            "12");

        if(guntype == 6)
        {
            Set(
                doc,
                section,
                "zoom_fov",
                "24");
            Set(
                doc,
                section,
                "zoom_hide_weapon",
                "1");
        }
        else
        {
            Set(
                doc,
                section,
                "zoom_fov",
                "0");
            Set(
                doc,
                section,
                "zoom_hide_weapon",
                "0");
        }
    }

    private static void SetViewPosition(
        FireteamConfigDocument doc,
        string section,
        string value)
    {
        var cleaned =
            value
                .Trim()
                .Trim('<', '>');

        var parts =
            cleaned.Split(
                ',',
                StringSplitOptions.TrimEntries);

        if(parts.Length != 3)
        {
            Set(
                doc,
                section,
                "view_x",
                "0");
            Set(
                doc,
                section,
                "view_y",
                "0");
            Set(
                doc,
                section,
                "view_z",
                "0");
            return;
        }

        Set(
            doc,
            section,
            "view_x",
            parts[0]);
        Set(
            doc,
            section,
            "view_y",
            parts[1]);
        Set(
            doc,
            section,
            "view_z",
            parts[2]);
    }

    private static int ParseInt(
        string value,
        int fallback) =>
        int.TryParse(
            value,
            NumberStyles.Integer,
            CultureInfo.InvariantCulture,
            out var result)
        ? result
        : fallback;

    private static double ParseDouble(
        string value,
        double fallback) =>
        double.TryParse(
            value,
            NumberStyles.Float,
            CultureInfo.InvariantCulture,
            out var result)
        ? result
        : fallback;

    private static void Set(
        FireteamConfigDocument doc,
        string section,
        string key,
        object? value) =>
        doc.SetValue(
            section,
            key,
            Convert.ToString(
                value,
                CultureInfo.InvariantCulture) ??
            string.Empty);
}
