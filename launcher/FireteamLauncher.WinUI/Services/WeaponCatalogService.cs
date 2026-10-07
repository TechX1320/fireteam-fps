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
        "hh_model", "hh_texture", "sound_dir"
    ];

    private static readonly string[] EditorKeys =
        RuntimeKeys.Concat([
            "source", "supported", "enabled",
            "ca_name", "ca_pv_model", "ca_pv_skin", "ca_hh_model", "ca_hh_skin"
        ]).Distinct(StringComparer.OrdinalIgnoreCase).ToArray();

    public IReadOnlyList<WeaponDefinition> Load()
    {
        var path = LauncherPaths.FindEditableConfig("weapons.cfg")
            ?? throw new FileNotFoundException("Could not locate config\\weapons.cfg.");

        var doc = FireteamConfigDocument.Load(path);
        var result = new List<WeaponDefinition>();

        foreach(var section in doc.Sections)
        {
            var active = Regex.IsMatch(section, @"^weapon[1-5]$", RegexOptions.IgnoreCase);
            var catalog = section.StartsWith("catalog.", StringComparison.OrdinalIgnoreCase);
            if(!active && !catalog)
            {
                continue;
            }

            var values = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach(var key in EditorKeys)
            {
                values[key] = doc.GetValue(section, key);
            }

            var id = doc.GetValue(section, "id", section);
            var name = doc.GetValue(section, "name", id);
            var type = doc.GetValue(section, "type", "hitscan");

            result.Add(new WeaponDefinition
            {
                Section = section,
                Id = id,
                Name = name,
                Type = type,
                Enabled = active || doc.GetBool(section, "enabled"),
                Supported = active || doc.GetBool(section, "supported", true),
                IsActiveSlot = active,
                Source = doc.GetValue(section, "source", active ? "ACTIVE LOADOUT" : "LOCAL / CUSTOM"),
                Values = values
            });
        }

        return result;
    }

    public IReadOnlyList<WeaponDefinition> GetArsenalChoices()
    {
        var all = Load();
        var result = new List<WeaponDefinition>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach(var weapon in all
            .Where(w => w.IsActiveSlot || w.Supported)
            .OrderByDescending(w => w.IsActiveSlot)
            .ThenBy(w => w.Name))
        {
            var key = string.IsNullOrWhiteSpace(weapon.Id) ? weapon.Name : weapon.Id;
            if(seen.Add(key))
            {
                result.Add(weapon);
            }
        }

        return result;
    }

    public void SaveDefinition(WeaponDefinition definition)
    {
        var paths = LauncherPaths.FindConfigPaths("weapons.cfg");
        var sourcePath = paths.Source ?? paths.Runtime
            ?? throw new FileNotFoundException("Could not locate config\\weapons.cfg.");

        var doc = FireteamConfigDocument.Load(sourcePath);
        foreach(var (key, value) in definition.Values)
        {
            doc.SetValue(definition.Section, key, value);
        }

        doc.SetValue(definition.Section, "enabled", definition.Enabled ? "1" : "0");
        SaveBoth(doc, paths);
    }

    public void SaveLoadout(IReadOnlyList<WeaponDefinition> selections)
    {
        if(selections.Count != 5)
        {
            throw new ArgumentException("FIRETEAM currently requires exactly five active loadout slots.");
        }

        var paths = LauncherPaths.FindConfigPaths("weapons.cfg");
        var sourcePath = paths.Source ?? paths.Runtime
            ?? throw new FileNotFoundException("Could not locate config\\weapons.cfg.");

        var doc = FireteamConfigDocument.Load(sourcePath);

        for(var slot = 0; slot < 5; ++slot)
        {
            var weapon = selections[slot];
            var destination = $"weapon{slot + 1}";

            foreach(var key in RuntimeKeys)
            {
                weapon.Values.TryGetValue(key, out var value);
                doc.SetValue(destination, key, value ?? string.Empty);
            }

            if(weapon.Section.StartsWith("catalog.", StringComparison.OrdinalIgnoreCase))
            {
                doc.SetValue(weapon.Section, "enabled", "1");
            }
        }

        SaveBoth(doc, paths);
    }

    private static void SaveBoth(
        FireteamConfigDocument doc,
        (string? Source, string? Runtime) paths)
    {
        if(paths.Source is not null)
        {
            doc.Save(paths.Source);
        }

        if(paths.Runtime is not null &&
           !string.Equals(paths.Runtime, paths.Source, StringComparison.OrdinalIgnoreCase))
        {
            doc.Save(paths.Runtime);
        }
    }
}
