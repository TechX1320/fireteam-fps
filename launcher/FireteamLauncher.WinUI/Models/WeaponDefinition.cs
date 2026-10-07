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

    public string LoadoutCategory
    {
        get
        {
            if(Values.TryGetValue(
                   "category",
                   out var configured) &&
               !string.IsNullOrWhiteSpace(
                   configured))
            {
                return NormalizeCategory(
                    configured);
            }

            if(Values.TryGetValue(
                   "ca_guntype",
                   out var rawType) &&
               int.TryParse(
                   rawType,
                   out var caType))
            {
                return caType switch
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
                    _ => FallbackCategory()
                };
            }

            if(IsActiveSlot)
            {
                return Section.ToLowerInvariant() switch
                {
                    "weapon1" => "AR",
                    "weapon2" => "Pistol",
                    "weapon3" => "Melee",
                    "weapon4" => "Pistol",
                    "weapon5" => "SR",
                    _ => FallbackCategory()
                };
            }

            return FallbackCategory();
        }
    }

    private string FallbackCategory() =>
        Type.ToLowerInvariant() switch
        {
            "melee" => "Melee",
            "grenade" => "Throwing",
            "rocket" => "Launcher",
            _ => "AR"
        };

    private static string NormalizeCategory(
        string value) =>
        value.Trim().ToUpperInvariant() switch
        {
            "AR" => "AR",
            "SR" => "SR",
            "LAUNCHER" => "Launcher",
            "MELEE" => "Melee",
            "MG" => "MG",
            "PISTOL" => "Pistol",
            "SG" => "SG",
            "SMG" => "SMG",
            "THROWING" => "Throwing",
            _ => value.Trim()
        };
}
