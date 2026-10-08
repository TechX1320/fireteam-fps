using System.Text.Json;
using FireteamLauncher.Infrastructure;

namespace FireteamLauncher.Services;

// Public listings are curated metadata, not live pings or verified sessions.
// No account, registration server, or third-party master-server dependency.
public sealed record FireteamServerListing(
    string Name,
    string Address,
    string Map = "CABINFEVER")
{
    public override string ToString() =>
        $"{Name}  |  {Address}  |  {Map}";
}

public sealed class ServerDirectoryService
{
    private const string PublicDirectoryUrl =
        "https://raw.githubusercontent.com/TechX1320/fireteam-fps/main/config/public-servers.json";

    private static readonly JsonSerializerOptions JsonOptions =
        new() { PropertyNameCaseInsensitive = true, WriteIndented = true };

    private static string FavoritesPath =>
        Path.Combine(LauncherPaths.SupportRoot, "server-favorites.json");

    public IReadOnlyList<FireteamServerListing> LoadFavorites()
    {
        try
        {
            if(!File.Exists(FavoritesPath))
                return [];

            var items = JsonSerializer.Deserialize<List<FireteamServerListing>>(
                File.ReadAllText(FavoritesPath), JsonOptions);
            return Validate(items);
        }
        catch
        {
            return [];
        }
    }

    public void SaveFavorite(FireteamServerListing listing)
    {
        if(!TryNormalizeAddress(listing.Address, out var address))
            throw new ArgumentException("Enter a valid host or IP, optionally followed by :port.");

        var list = LoadFavorites().ToList();
        list.RemoveAll(entry => string.Equals(entry.Address, address, StringComparison.OrdinalIgnoreCase));
        list.Add(new FireteamServerListing(
            string.IsNullOrWhiteSpace(listing.Name) ? address : listing.Name.Trim()[..Math.Min(listing.Name.Trim().Length, 48)],
            address,
            NormalizeMap(listing.Map)));
        Directory.CreateDirectory(LauncherPaths.SupportRoot);
        File.WriteAllText(FavoritesPath, JsonSerializer.Serialize(list, JsonOptions));
    }

    public void RemoveFavorite(string address)
    {
        var list = LoadFavorites().ToList();
        list.RemoveAll(entry => string.Equals(entry.Address, address, StringComparison.OrdinalIgnoreCase));
        File.WriteAllText(FavoritesPath, JsonSerializer.Serialize(list, JsonOptions));
    }

    public async Task<IReadOnlyList<FireteamServerListing>> FetchPublicAsync()
    {
        using var http = new HttpClient { Timeout = TimeSpan.FromSeconds(8) };
        var text = await http.GetStringAsync(PublicDirectoryUrl);
        var items = JsonSerializer.Deserialize<List<FireteamServerListing>>(text, JsonOptions);
        return Validate(items);
    }

    public static bool TryNormalizeAddress(string? text, out string address)
    {
        address = string.Empty;
        var value = text?.Trim() ?? string.Empty;
        if(value.Length == 0 || value.Length > 250 ||
           value.Contains('/') || value.Contains('\\') ||
           value.Contains(' ') || value.Contains('@') ||
           value.Contains('?') || value.Contains('#'))
            return false;

        var parts = value.Split(':');
        if(parts.Length > 2 || parts.Length == 0)
            return false;
        var host = parts[0];
        if(Uri.CheckHostName(host) == UriHostNameType.Unknown)
            return false;

        if(parts.Length == 2)
        {
            if(!ushort.TryParse(parts[1], out var port) || port == 0)
                return false;
            address = host + ":" + port;
        }
        else
        {
            address = host;
        }

        return true;
    }

    private static string NormalizeMap(string? map)
    {
        var value = Path.GetFileNameWithoutExtension(
            string.IsNullOrWhiteSpace(map) ? "CABINFEVER" : map.Trim());
        var clean = new string(value.Where(ch => char.IsLetterOrDigit(ch) || ch == '_' || ch == '-').ToArray());
        return string.IsNullOrWhiteSpace(clean) ? "CABINFEVER" : clean.ToUpperInvariant();
    }

    private static IReadOnlyList<FireteamServerListing> Validate(IEnumerable<FireteamServerListing>? servers)
    {
        if(servers is null)
            return [];

        var valid = new List<FireteamServerListing>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach(var server in servers.Take(300))
        {
            if(server is null || !TryNormalizeAddress(server.Address, out var address) || !seen.Add(address))
                continue;

            var name = string.IsNullOrWhiteSpace(server.Name) ? address : server.Name.Trim();
            name = name[..Math.Min(name.Length, 48)];
            valid.Add(new FireteamServerListing(name, address, NormalizeMap(server.Map)));
        }

        return valid;
    }
}
