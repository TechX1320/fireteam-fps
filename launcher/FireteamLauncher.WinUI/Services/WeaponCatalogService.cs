using System.Text.RegularExpressions;
using FireteamLauncher.Infrastructure;
using FireteamLauncher.Models;

namespace FireteamLauncher.Services;

public sealed class WeaponCatalogService
{
    public static readonly string[] RuntimeKeys =
    [
        "id", "name", "type",
        "clip", "reserve", "damage",
        "fire_interval", "range",
        "effect_range0", "effect_range1", "effect_range2",
        "damage_mult0", "damage_mult1", "damage_mult2",
        "reload",
        "penetration_max_thickness",
        "penetration_damage_mult",
        "penetration_range_mult",
        "automatic", "auto_reload", "show_crosshair",
        "zoom_fov", "zoom_hide_weapon",
        "crosshair_base_gap", "crosshair_shot_kick",
        "crosshair_move_kick", "crosshair_max_gap",
        "crosshair_recover",
        "view_x", "view_y", "view_z",
        "pv_model", "pv_anim", "pv_texture",
        "hh_model", "hh_texture", "sound_dir",
        "anim_select", "anim_idle", "anim_fire",
        "anim_alt_fire", "anim_reload"
    ];

    private static readonly string[] EditorKeys =
        RuntimeKeys.Concat([
            "source", "supported", "enabled", "category",
            "ca_name", "ca_pv_model", "ca_pv_skin", "ca_hh_model", "ca_hh_skin",
            "ca_weapon_section", "ca_ammo_name", "ca_guntype",
            "ca_vectors_per_round", "ca_import_status",
            "ca_timing_verified"
        ]).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();

    private readonly object _cacheLock = new();
    private List<WeaponDefinition>? _cache;
    private string? _cachePath;
    private DateTime _cacheWriteUtc;
    private long _cacheLength;
    private long _cacheQuarantineWriteTicks;
    private long _cacheQuarantineLength;

    public IReadOnlyList<WeaponDefinition> Load(
        bool force = false)
    {
        var path =
            LauncherPaths.FindEditableConfig(
                "weapons.cfg")
            ?? throw new FileNotFoundException(
                "Could not locate config\\weapons.cfg.");

        var info =
            new FileInfo(
                path);

        var quarantineSignature =
            GetQaQuarantineSignature();

        lock(_cacheLock)
        {
            if(!force &&
               _cache is not null &&
               string.Equals(
                   _cachePath,
                   path,
                   StringComparison.OrdinalIgnoreCase) &&
               _cacheWriteUtc ==
                   info.LastWriteTimeUtc &&
               _cacheLength ==
                   info.Length &&
               _cacheQuarantineWriteTicks ==
                   quarantineSignature.WriteTicks &&
               _cacheQuarantineLength ==
                   quarantineSignature.Length)
            {
                return _cache;
            }
        }

        var parsed =
            ParseCatalog(
                path);

        lock(_cacheLock)
        {
            _cache = parsed;
            _cachePath = path;
            _cacheWriteUtc =
                info.LastWriteTimeUtc;
            _cacheLength =
                info.Length;
            _cacheQuarantineWriteTicks =
                quarantineSignature.WriteTicks;
            _cacheQuarantineLength =
                quarantineSignature.Length;

            return _cache;
        }
    }

    public IReadOnlyList<WeaponDefinition> GetArsenalChoices(
        IReadOnlyList<WeaponDefinition>? all = null)
    {
        all ??=
            Load();

        var result =
            new List<WeaponDefinition>();

        var seen =
            new HashSet<string>(
                StringComparer.OrdinalIgnoreCase);

        foreach(var weapon in all
            .Where(w =>
                w.IsActiveSlot ||
                w.Enabled)
            .OrderByDescending(w =>
                w.IsActiveSlot)
            .ThenBy(w =>
                w.Name))
        {
            var key =
                string.IsNullOrWhiteSpace(
                    weapon.Id)
                ? weapon.Name
                : weapon.Id;

            if(seen.Add(key))
            {
                result.Add(
                    weapon);
            }
        }

        return result;
    }

    public void SetEnabled(
        string section,
        bool enabled)
    {
        var paths =
            ResolveWritableConfigPaths(
                "weapon-library.cfg");

        var destination =
            paths.Source ??
            paths.Runtime
            ?? throw new FileNotFoundException(
                "Could not resolve config\\weapon-library.cfg.");

        var doc =
            FireteamConfigDocument.Load(
                destination);

        doc.SetValue(
            section,
            "enabled",
            enabled
                ? "1"
                : "0");

        SaveBoth(
            doc,
            paths);

        lock(_cacheLock)
        {
            if(_cache is null)
            {
                return;
            }

            var index =
                _cache.FindIndex(
                    weapon =>
                        weapon.Section.Equals(
                            section,
                            StringComparison.OrdinalIgnoreCase));

            if(index < 0)
            {
                return;
            }

            var old =
                _cache[index];

            _cache[index] =
                CloneWithEnabled(
                    old,
                    old.IsActiveSlot ||
                    enabled);
        }
    }

    public void SaveDefinition(
        WeaponDefinition definition)
    {
        var paths =
            LauncherPaths.FindConfigPaths(
                "weapons.cfg");

        var sourcePath =
            paths.Source ??
            paths.Runtime
            ?? throw new FileNotFoundException(
                "Could not locate config\\weapons.cfg.");

        var doc =
            FireteamConfigDocument.Load(
                sourcePath);

        foreach(var (key, value) in definition.Values)
        {
            doc.SetValue(
                definition.Section,
                key,
                value);
        }

        SaveBoth(
            doc,
            paths);

        SetEnabled(
            definition.Section,
            definition.Enabled);

        Invalidate();
    }

    public void SaveLoadout(
        IReadOnlyList<WeaponDefinition> selections)
    {
        if(selections.Count != 5)
        {
            throw new ArgumentException(
                "FIRETEAM currently requires exactly five active loadout slots.");
        }

        var paths =
            LauncherPaths.FindConfigPaths(
                "weapons.cfg");

        var sourcePath =
            paths.Source ??
            paths.Runtime
            ?? throw new FileNotFoundException(
                "Could not locate config\\weapons.cfg.");

        var doc =
            FireteamConfigDocument.Load(
                sourcePath);

        PreserveActiveDefinitions(
            doc);

        for(var slot = 0;
            slot < 5;
            ++slot)
        {
            var weapon =
                selections[slot];

            var destination =
                $"weapon{slot + 1}";

            foreach(var key in RuntimeKeys)
            {
                weapon.Values.TryGetValue(
                    key,
                    out var value);

                doc.SetValue(
                    destination,
                    key,
                    value ??
                    string.Empty);
            }

            doc.SetValue(
                destination,
                "category",
                weapon.LoadoutCategory);
        }

        SaveBoth(
            doc,
            paths);

        Invalidate();
    }

    private static void PreserveActiveDefinitions(
        FireteamConfigDocument doc)
    {
        var existingIds =
            new HashSet<string>(
                StringComparer.OrdinalIgnoreCase);

        foreach(var section in doc.Sections)
        {
            if(!section.StartsWith(
                    "catalog.",
                    StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            var id =
                doc.GetValue(
                    section,
                    "id");

            if(!string.IsNullOrWhiteSpace(
                id))
            {
                existingIds.Add(
                    id);
            }
        }

        for(var slot = 1;
            slot <= 5;
            ++slot)
        {
            var source =
                $"weapon{slot}";

            var id =
                doc.GetValue(
                    source,
                    "id");

            if(string.IsNullOrWhiteSpace(
                   id) ||
               existingIds.Contains(
                   id))
            {
                continue;
            }

            var safeId =
                Regex.Replace(
                    id,
                    @"[^A-Za-z0-9_\-]+",
                    "_")
                .Trim('_');

            if(string.IsNullOrWhiteSpace(
                safeId))
            {
                safeId =
                    $"slot_{slot}";
            }

            var destination =
                $"catalog.stock_{safeId}";

            foreach(var key in RuntimeKeys)
            {
                doc.SetValue(
                    destination,
                    key,
                    doc.GetValue(
                        source,
                        key));
            }

            doc.SetValue(
                destination,
                "category",
                doc.GetValue(
                    source,
                    "category"));

            doc.SetValue(
                destination,
                "source",
                "FIRETEAM PREVIOUS LOADOUT");

            doc.SetValue(
                destination,
                "supported",
                "1");

            doc.SetValue(
                destination,
                "enabled",
                "1");

            existingIds.Add(
                id);
        }
    }

    public void Invalidate()
    {
        lock(_cacheLock)
        {
            _cache = null;
            _cachePath = null;
            _cacheWriteUtc =
                default;
            _cacheLength = 0;
            _cacheQuarantineWriteTicks = 0;
            _cacheQuarantineLength = 0;
        }
    }

    private static List<WeaponDefinition> ParseCatalog(
        string path)
    {
        var doc =
            FireteamConfigDocument.Load(
                path);

        var enabledOverrides =
            LoadEnabledOverrides();

        var quarantined =
            LoadQaQuarantinedSections();

        var result =
            new List<WeaponDefinition>();

        foreach(var section in doc.Sections)
        {
            var active =
                Regex.IsMatch(
                    section,
                    @"^weapon[1-5]$",
                    RegexOptions.IgnoreCase);

            var catalog =
                section.StartsWith(
                    "catalog.",
                    StringComparison.OrdinalIgnoreCase);

            if(!active &&
               !catalog)
            {
                continue;
            }

            if(catalog &&
               quarantined.Contains(
                   section))
            {
                continue;
            }

            var values =
                new Dictionary<string, string>(
                    StringComparer.OrdinalIgnoreCase);

            foreach(var key in EditorKeys)
            {
                values[key] =
                    doc.GetValue(
                        section,
                        key);
            }

            var id =
                doc.GetValue(
                    section,
                    "id",
                    section);

            var name =
                doc.GetValue(
                    section,
                    "name",
                    id);

            var type =
                doc.GetValue(
                    section,
                    "type",
                    "hitscan");

            var enabled =
                active ||
                (enabledOverrides.TryGetValue(
                     section,
                     out var overrideEnabled)
                    ? overrideEnabled
                    : doc.GetBool(
                        section,
                        "enabled"));

            result.Add(
                new WeaponDefinition
                {
                    Section = section,
                    Id = id,
                    Name = name,
                    Type = type,
                    Enabled = enabled,
                    Supported =
                        active ||
                        doc.GetBool(
                            section,
                            "supported",
                            true),
                    IsActiveSlot = active,
                    Source =
                        doc.GetValue(
                            section,
                            "source",
                            active
                                ? "ACTIVE LOADOUT"
                                : "LOCAL / CUSTOM"),
                    Values = values
                });
        }

        return result;
    }

    private static HashSet<string> LoadQaQuarantinedSections()
    {
        var result =
            new HashSet<string>(
                StringComparer.OrdinalIgnoreCase);

        var paths =
            LauncherPaths.FindConfigPaths(
                "weapon-quarantine.txt");

        var candidates =
            new[]
            {
                paths.Source,
                paths.Runtime
            }
            .Where(path =>
                !string.IsNullOrWhiteSpace(
                    path))
            .Distinct(
                StringComparer.OrdinalIgnoreCase);

        foreach(var path in candidates)
        {
            if(path is null ||
               !File.Exists(path))
            {
                continue;
            }

            foreach(var raw in File.ReadLines(path))
            {
                var line =
                    raw.Trim();

                if(line.Length == 0 ||
                   line.StartsWith("#") ||
                   line.StartsWith(";"))
                {
                    continue;
                }

                result.Add(
                    line);
            }
        }

        return result;
    }

    private static (
        long WriteTicks,
        long Length) GetQaQuarantineSignature()
    {
        var paths =
            LauncherPaths.FindConfigPaths(
                "weapon-quarantine.txt");

        long ticks = 0;
        long length = 0;

        foreach(var path in new[]
        {
            paths.Source,
            paths.Runtime
        }
        .Where(path =>
            !string.IsNullOrWhiteSpace(
                path))
        .Distinct(
            StringComparer.OrdinalIgnoreCase))
        {
            if(path is null ||
               !File.Exists(path))
            {
                continue;
            }

            var info =
                new FileInfo(
                    path);

            ticks ^=
                info.LastWriteTimeUtc.Ticks;
            length +=
                info.Length;
        }

        return (
            ticks,
            length);
    }

    private static Dictionary<string, bool> LoadEnabledOverrides()
    {
        var path =
            LauncherPaths.FindEditableConfig(
                "weapon-library.cfg");

        var result =
            new Dictionary<string, bool>(
                StringComparer.OrdinalIgnoreCase);

        if(path is null ||
           !File.Exists(path))
        {
            return result;
        }

        var doc =
            FireteamConfigDocument.Load(
                path);

        foreach(var section in doc.Sections)
        {
            result[section] =
                doc.GetBool(
                    section,
                    "enabled");
        }

        return result;
    }

    private static WeaponDefinition CloneWithEnabled(
        WeaponDefinition source,
        bool enabled) =>
        new()
        {
            Section = source.Section,
            Id = source.Id,
            Name = source.Name,
            Type = source.Type,
            Enabled = enabled,
            Supported = source.Supported,
            IsActiveSlot = source.IsActiveSlot,
            Source = source.Source,
            Values = source.Values
        };

    private static (
        string? Source,
        string? Runtime) ResolveWritableConfigPaths(
        string fileName)
    {
        var repo =
            LauncherPaths.FindRepositoryDirectory();

        var game =
            LauncherPaths.FindGameDirectory();

        return (
            repo is null
                ? null
                : Path.Combine(
                    repo,
                    "config",
                    fileName),
            game is null
                ? null
                : Path.Combine(
                    game,
                    "config",
                    fileName));
    }

    private static void SaveBoth(
        FireteamConfigDocument doc,
        (string? Source, string? Runtime) paths)
    {
        if(paths.Source is not null)
        {
            doc.Save(
                paths.Source);

            if(paths.Runtime is not null &&
               !string.Equals(
                   paths.Runtime,
                   paths.Source,
                   StringComparison.OrdinalIgnoreCase))
            {
                var parent =
                    Path.GetDirectoryName(
                        paths.Runtime);

                if(!string.IsNullOrWhiteSpace(
                    parent))
                {
                    Directory.CreateDirectory(
                        parent);
                }

                File.Copy(
                    paths.Source,
                    paths.Runtime,
                    true);
            }

            return;
        }

        if(paths.Runtime is not null)
        {
            doc.Save(
                paths.Runtime);
        }
    }
}
