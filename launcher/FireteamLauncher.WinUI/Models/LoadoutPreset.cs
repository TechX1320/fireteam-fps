namespace FireteamLauncher.Models;

public sealed class LoadoutPreset
{
    public required int Index { get; init; }
    public required string Name { get; init; }
    public required IReadOnlyList<string> WeaponIds { get; init; }

    public string DisplayName => Name;
}
