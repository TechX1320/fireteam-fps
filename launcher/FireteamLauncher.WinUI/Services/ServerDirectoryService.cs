using System.Text.Json;
using FireteamLauncher.Infrastructure;

namespace FireteamLauncher.Services;

// LAN discovery and a first-party live HTTPS directory are separate. The
// presence registry gives fresh metadata, not proof that a game port works.
public sealed record FireteamServerListing(
    string Name,
    string Address,
    string Map = "CABINFEVER",
    int? Players = null,
    int? MaxPlayers = null,
    int? Difficulty = null,
    bool? Verified = null,
    string Source = "Favorite")
{
    public override string ToString() =>
        $"{Name}  |  {Address}  |  {Map}";
}

public sealed class ServerDirectoryService
{
    private const string CuratedFallbackUrl =
        "https://raw.githubusercontent.com/TechX1320/fireteam-fps/main/config/public-servers.json";

    public string LastDirectorySource { get; private set; } = "Not queried";
    public string? LastDirectoryWarning { get; private set; }

    private sealed class HubReply
    {
        public string? Protocol { get; set; }
        public List<HubListing>? Servers { get; set; }
    }

    private sealed class HubListing
    {
        public string Name { get; set; } = "";
        public string Address { get; set; } = "";
        public string Map { get; set; } = "";
        public int Players { get; set; }
        public int MaxPlayers { get; set; }
        public int Difficulty { get; set; }
        public bool Verified { get; set; }
    }

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
        LastDirectoryWarning = null;
        using var http = new HttpClient
        {
            Timeout = TimeSpan.FromSeconds(7),
            MaxResponseContentBufferSize = 128 * 1024
        };

        var hubUrl = HubAddressService.Load();
        if(hubUrl is not null)
        {
            try
            {
                // The hostname is configured locally, not provided by any
                // untrusted server listing. HTTPS certificate validation stays ON.
                var url = hubUrl + "/v1/servers";
                var json = await http.GetStringAsync(url);
                var response = JsonSerializer.Deserialize<HubReply>(json, JsonOptions);
                if(response?.Protocol != "FT-DIRECTORY-1" ||
                   response.Servers is null || response.Servers.Count > 300)
                    throw new InvalidDataException("Hub directory protocol mismatch.");

                LastDirectorySource = "Live hub";
                return Validate(response.Servers.Select(e =>
                    new FireteamServerListing(
                        e.Name, e.Address, e.Map,
                        e.Players, e.MaxPlayers, e.Difficulty,
                        e.Verified, "Hub")));
            }
            catch(Exception ex) when(ex is HttpRequestException or
                                     TaskCanceledException or
                                     System.Text.Json.JsonException or
                                     InvalidDataException)
            {
                LastDirectoryWarning = "Hub unreachable; showing curated fallback.";
            }
        }
        else
        {
            LastDirectoryWarning = "Hub URL not configured; showing curated fallback.";
        }

        var text = await http.GetStringAsync(CuratedFallbackUrl);
        var items = JsonSerializer.Deserialize<List<FireteamServerListing>>(
            text, JsonOptions);
        LastDirectorySource = "Curated";
        return Validate(items, "Curated");
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

    private static IReadOnlyList<FireteamServerListing> Validate(
        IEnumerable<FireteamServerListing>? servers, string source = "Favorite")
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
            int? players = null, maxPlayers = null, difficulty = null;
            if(server.MaxPlayers is >= 1 and <= 24 &&
               server.Players is >= 0 and <= 24 &&
               server.Players <= server.MaxPlayers)
            {
                players = server.Players;
                maxPlayers = server.MaxPlayers;
            }
            if(server.Difficulty is >= 1 and <= 10)
                difficulty = server.Difficulty;
            valid.Add(new FireteamServerListing(
                name, address, NormalizeMap(server.Map),
                players, maxPlayers, difficulty,
                server.Verified, server.Source == "Hub" ? "Hub" : source));
        }

        return valid;
    }
}
