namespace FireteamLauncher.Models;

public sealed class WeaponDefinition
{
    public required string Section { get; init; }
    public required string Id { get; init; }
    public required string Name { get; init; }
    public required string Type { get; init; }
    public required bool Enabled { get; init; }
    public required bool Supported { get; init; }
    public required bool IsActiveSlot { get; init; }
    public required string Source { get; init; }
    public required Dictionary<string, string> Values { get; init; }

    public string DisplayName => Name;
}
