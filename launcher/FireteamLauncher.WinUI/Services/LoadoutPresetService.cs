using FireteamLauncher.Infrastructure;
using FireteamLauncher.Models;

namespace FireteamLauncher.Services;

public sealed class LoadoutPresetService
{
    private const int PresetCount = 3;
    private const int SlotCount = 5;

    private readonly WeaponCatalogService _weapons;

    public LoadoutPresetService(
        WeaponCatalogService weapons)
    {
        _weapons = weapons;
    }

    public IReadOnlyList<LoadoutPreset> Load()
    {
        var paths =
            ResolvePaths();

        var path =
            paths.Source ??
            paths.Runtime ??
            throw new FileNotFoundException(
                "Could not resolve FIRETEAM loadouts.cfg.");

        if(!File.Exists(path))
        {
            CreateDefaults(
                paths);
        }

        var doc =
            FireteamConfigDocument.Load(
                path);

        var result =
            new List<LoadoutPreset>(
                PresetCount);

        for(var preset = 1;
            preset <= PresetCount;
            ++preset)
        {
            var section =
                $"preset{preset}";

            var ids =
                new List<string>(
                    SlotCount);

            for(var slot = 1;
                slot <= SlotCount;
                ++slot)
            {
                ids.Add(
                    doc.GetValue(
                        section,
                        $"slot{slot}"));
            }

            result.Add(
                new LoadoutPreset
                {
                    Index = preset,
                    Name = doc.GetValue(
                        section,
                        "name",
                        $"Loadout {preset}"),
                    WeaponIds = ids
                });
        }

        return result;
    }

    public int GetSelectedIndex()
    {
        var paths =
            ResolvePaths();

        var path =
            paths.Source ??
            paths.Runtime;

        if(path is null ||
           !File.Exists(path))
        {
            return 1;
        }

        var doc =
            FireteamConfigDocument.Load(
                path);

        return int.TryParse(
            doc.GetValue(
                "loadouts",
                "selected",
                "1"),
            out var index)
            ? Math.Clamp(
                index,
                1,
                PresetCount)
            : 1;
    }

    public void SavePreset(
        int index,
        string name,
        IReadOnlyList<WeaponDefinition> selections)
    {
        if(index < 1 ||
           index > PresetCount)
        {
            throw new ArgumentOutOfRangeException(
                nameof(index));
        }

        if(selections.Count != SlotCount)
        {
            throw new ArgumentException(
                "A FIRETEAM saved loadout requires five slots.",
                nameof(selections));
        }

        var paths =
            ResolvePaths();

        EnsureExists(
            paths);

        var sourcePath =
            paths.Source ??
            paths.Runtime!;

        var doc =
            FireteamConfigDocument.Load(
                sourcePath);

        var section =
            $"preset{index}";

        doc.SetValue(
            section,
            "name",
            string.IsNullOrWhiteSpace(
                name)
                ? $"Loadout {index}"
                : name.Trim());

        for(var slot = 0;
            slot < SlotCount;
            ++slot)
        {
            var weapon =
                selections[slot];

            doc.SetValue(
                section,
                $"slot{slot + 1}",
                string.IsNullOrWhiteSpace(
                    weapon.Id)
                    ? weapon.Name
                    : weapon.Id);
        }

        SaveBoth(
            doc,
            paths);
    }

    public void SetSelectedIndex(
        int index)
    {
        index =
            Math.Clamp(
                index,
                1,
                PresetCount);

        var paths =
            ResolvePaths();

        EnsureExists(
            paths);

        var sourcePath =
            paths.Source ??
            paths.Runtime!;

        var doc =
            FireteamConfigDocument.Load(
                sourcePath);

        doc.SetValue(
            "loadouts",
            "selected",
            index.ToString());

        SaveBoth(
            doc,
            paths);
    }

    private void CreateDefaults(
        (string? Source, string? Runtime) paths)
    {
        var destination =
            paths.Source ??
            paths.Runtime ??
            throw new FileNotFoundException(
                "Could not create FIRETEAM loadouts.cfg.");

        var doc =
            FireteamConfigDocument.Load(
                destination);

        doc.SetValue(
            "loadouts",
            "selected",
            "1");

        var active =
            _weapons.Load()
                .Where(
                    weapon =>
                        weapon.IsActiveSlot)
                .OrderBy(
                    weapon =>
                        weapon.Section)
                .Take(
                    SlotCount)
                .ToList();

        for(var preset = 1;
            preset <= PresetCount;
            ++preset)
        {
            var section =
                $"preset{preset}";

            doc.SetValue(
                section,
                "name",
                $"Loadout {preset}");

            for(var slot = 0;
                slot < SlotCount;
                ++slot)
            {
                var id =
                    slot < active.Count
                    ? active[slot].Id
                    : string.Empty;

                doc.SetValue(
                    section,
                    $"slot{slot + 1}",
                    id);
            }
        }

        SaveBoth(
            doc,
            paths);
    }

    private void EnsureExists(
        (string? Source, string? Runtime) paths)
    {
        var path =
            paths.Source ??
            paths.Runtime ??
            throw new FileNotFoundException(
                "Could not resolve FIRETEAM loadouts.cfg.");

        if(!File.Exists(path))
        {
            CreateDefaults(
                paths);
        }
    }

    private static (
        string? Source,
        string? Runtime) ResolvePaths()
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
                    "loadouts.cfg"),
            game is null
                ? null
                : Path.Combine(
                    game,
                    "config",
                    "loadouts.cfg"));
    }

    private static void SaveBoth(
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
}
