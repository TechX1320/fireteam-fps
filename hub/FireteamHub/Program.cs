using System.Net;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

var builder = WebApplication.CreateBuilder(args);

// The hub is LOCALHOST-ONLY until the operator explicitly configures an
// HTTPS Kestrel listener and certificate. No exposed unauthenticated HTTP.
var listeners = (Environment.GetEnvironmentVariable("FIRETEAM_HUB_LISTEN")
    ?? "http://127.0.0.1:27890")
    .Split(';', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
builder.WebHost.UseUrls(listeners);
builder.WebHost.ConfigureKestrel(options =>
    options.Limits.MaxRequestBodySize = 2048);
builder.Services.AddSingleton<PresenceRegistry>();

var app = builder.Build();
app.Use(async (context, next) =>
{
    context.Response.Headers["Cache-Control"] = "no-store";
    context.Response.Headers["X-Content-Type-Options"] = "nosniff";
    await next();
});

app.MapGet("/healthz", () => Results.Json(new
{
    name = "FIRETEAM Hub",
    version = 1,
    status = "ready"
}));

app.MapGet("/v1/servers", (PresenceRegistry registry) =>
    Results.Json(new
    {
        protocol = "FT-DIRECTORY-1",
        generatedAtUtc = DateTimeOffset.UtcNow,
        servers = registry.Snapshot()
    }));

app.MapPost("/v1/hosts/heartbeat", (HttpContext context,
    HeartbeatRequest request, PresenceRegistry registry) =>
{
    var source = context.Connection.RemoteIpAddress;
    if(!HubNetwork.AllowedSource(context, source))
        return Results.Problem("HTTPS required for Internet registration.",
            statusCode: StatusCodes.Status403Forbidden);
    if(!HubNetwork.ValidPayload(request))
        return Results.BadRequest(new { error = "Invalid host heartbeat." });
    var result = registry.Heartbeat(source!, request);
    return result switch
    {
        RegistryResult.Accepted => Results.Ok(new { accepted = true,
            ttlSeconds = 90, reachability = "unverified" }),
        RegistryResult.Throttled => Results.StatusCode(429),
        RegistryResult.AtCapacity => Results.StatusCode(503),
        _ => Results.StatusCode(403)
    };
});

app.MapPost("/v1/hosts/offline", (HttpContext context,
    OfflineRequest request, PresenceRegistry registry) =>
{
    var source = context.Connection.RemoteIpAddress;
    if(!HubNetwork.AllowedSource(context, source))
        return Results.StatusCode(403);
    if(!HubNetwork.Hex(request.Id, 32) || !HubNetwork.Hex(request.Key, 64))
        return Results.BadRequest();
    return registry.Offline(source!, request)
        ? Results.NoContent() : Results.StatusCode(403);
});

app.Run();

public sealed record HeartbeatRequest(
    string Id, string Key, string Name, string Map, int Port,
    int Players, int MaxPlayers, int Difficulty, string Protocol);
public sealed record OfflineRequest(string Id, string Key);

// Keep external JSON independent of server private keys. A listing is
// *advertised*, not externally verified as joinable or compatible.
public sealed record PublicServer(
    string Name, string Address, string Map, int Players,
    int MaxPlayers, int Difficulty, bool Verified, DateTimeOffset LastSeenUtc);

internal enum RegistryResult { Accepted, Denied, Throttled, AtCapacity }

internal static class HubNetwork
{
    private static readonly Regex AllowedName =
        new("^[A-Za-z0-9 _-]{1,48}$",
            RegexOptions.Compiled | RegexOptions.CultureInvariant);
    private static readonly Regex AllowedMap =
        new("^[A-Za-z0-9_-]{1,64}$",
            RegexOptions.Compiled | RegexOptions.CultureInvariant);

    public static bool Hex(string? s, int chars) =>
        s is not null && s.Length == chars && s.All(Uri.IsHexDigit);

    public static bool ValidPayload(HeartbeatRequest? item) =>
        item is not null && Hex(item.Id, 32) && Hex(item.Key, 64) &&
        item.Name is not null && AllowedName.IsMatch(item.Name) &&
        item.Map is not null && AllowedMap.IsMatch(item.Map) &&
        item.Protocol == "FT1" &&
        item.Port is >= 1 and <= 65535 &&
        item.MaxPlayers is >= 1 and <= 24 &&
        item.Players >= 0 && item.Players <= item.MaxPlayers &&
        item.Difficulty is >= 1 and <= 10;

    public static bool AllowedSource(HttpContext context, IPAddress? ip)
    {
        if(ip is null) return false;
        if(ip.IsIPv4MappedToIPv6) ip = ip.MapToIPv4();
        // HTTP loopback is allowed ONLY when the hub explicitly binds to
        // loopback, for the developer's local smoke test.
        if(!context.Request.IsHttps)
        {
            return IPAddress.IsLoopback(ip) &&
                   IPAddress.IsLoopback(context.Connection.LocalIpAddress
                      is { IsIPv4MappedToIPv6: true } local ? local.MapToIPv4()
                      : context.Connection.LocalIpAddress ?? IPAddress.None);
        }
        if(IPAddress.IsLoopback(ip)) return true;
        if(ip.AddressFamily != System.Net.Sockets.AddressFamily.InterNetwork)
            return false;
        var bytes = ip.GetAddressBytes();
        // Public IPv4 only. Never publish a private/VPN/link-local IP.
        return bytes[0] is not (0 or 10 or 127) &&
               !(bytes[0] == 169 && bytes[1] == 254) &&
               !(bytes[0] == 172 && bytes[1] is >= 16 and <= 31) &&
               !(bytes[0] == 192 && bytes[1] == 168) &&
               !(bytes[0] == 100 && bytes[1] is >= 64 and <= 127) &&
               bytes[0] < 224 &&
               !(bytes[0] == 192 && bytes[1] == 0 && bytes[2] == 0);
    }

    public static string CleanIp(IPAddress ip) =>
        (ip.IsIPv4MappedToIPv6 ? ip.MapToIPv4() : ip).ToString();

    // Operator-only override for a game on the same Lenovo as the hub.
    // A regular host cannot specify its own advertised address in JSON.
    public static bool ValidPublicGameHost(string value)
    {
        if(value.Length is < 4 or > 253 ||
           value.Any(c => !(char.IsAsciiLetterOrDigit(c) ||
                            c == '-' || c == '.')) ||
           value.Contains("..") || value.StartsWith('-') ||
           value.EndsWith('-') || value.EndsWith('.') ||
           value.EndsWith(".local", StringComparison.OrdinalIgnoreCase))
            return false;
        if(IPAddress.TryParse(value, out var ip))
        {
            if(ip.AddressFamily != System.Net.Sockets.AddressFamily.InterNetwork)
                return false;
            var octets = ip.GetAddressBytes();
            return octets[0] is >= 1 and < 224 and not (10 or 127) &&
                   !(octets[0] == 169 && octets[1] == 254) &&
                   !(octets[0] == 172 && octets[1] is >= 16 and <= 31) &&
                   !(octets[0] == 192 && octets[1] == 168) &&
                   !(octets[0] == 100 && octets[1] is >= 64 and <= 127);
        }
        return value.Contains('.') &&
               !value.Equals("localhost", StringComparison.OrdinalIgnoreCase) &&
               value.Split('.').All(label =>
                   label.Length is >= 1 and <= 63 &&
                   char.IsAsciiLetterOrDigit(label[0]) &&
                   char.IsAsciiLetterOrDigit(label[^1]));
    }

    public static bool SameSecret(string a, string b)
    {
        // Secret is presented only over TLS (or loopback dev HTTP).
        // Compare fixed-length hash bytes in constant time.
        if(!Hex(b, 64)) return false;
        var first = SHA256.HashData(Encoding.ASCII.GetBytes(a));
        var stored = Convert.FromHexString(b);
        return CryptographicOperations.FixedTimeEquals(first, stored);
    }
}

internal sealed class PresenceRegistry
{
    private sealed record Entry(
        string Id, string KeyHash, string Name, string Map,
        string SourceIp, string AdvertisedHost,
        int Port, int Players, int MaxPlayers,
        int Difficulty, DateTimeOffset Seen);
    private sealed class RateState
    {
        public DateTimeOffset WindowStart;
        public int Calls;
    }

    private readonly object _gate = new();
    private readonly string? _trustedLocalPublicGameHost;

    public PresenceRegistry()
    {
        var host = Environment.GetEnvironmentVariable(
            "FIRETEAM_HUB_PUBLIC_GAME_HOST")?.Trim();
        if(!string.IsNullOrWhiteSpace(host))
        {
            if(!HubNetwork.ValidPublicGameHost(host))
                throw new InvalidOperationException(
                    "Invalid FIRETEAM_HUB_PUBLIC_GAME_HOST: use a public IPv4 or DNS hostname, without a port.");
            _trustedLocalPublicGameHost = host.ToLowerInvariant();
        }
    }

    private readonly Dictionary<string, Entry> _entries =
        new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, RateState> _rate =
        new(StringComparer.Ordinal);
    private static readonly TimeSpan Expiry = TimeSpan.FromSeconds(90);
    private const int GlobalCap = 256;
    private const int PerAddressCap = 4;

    private void Expire(DateTimeOffset now)
    {
        foreach(var id in _entries.Where(e => now - e.Value.Seen > Expiry)
                     .Select(e => e.Key).ToArray())
            _entries.Remove(id);
        foreach(var ip in _rate.Where(e => now - e.Value.WindowStart >
                    TimeSpan.FromMinutes(3)).Select(e => e.Key).ToArray())
            _rate.Remove(ip);
    }

    public PublicServer[] Snapshot()
    {
        lock(_gate)
        {
            Expire(DateTimeOffset.UtcNow);
            return _entries.Values
                .OrderBy(e => e.Name, StringComparer.OrdinalIgnoreCase)
                .Select(e => new PublicServer(e.Name,
                    e.AdvertisedHost + ":" + e.Port, e.Map,
                    e.Players, e.MaxPlayers, e.Difficulty,
                    false, e.Seen))
                .ToArray();
        }
    }

    public RegistryResult Heartbeat(IPAddress address, HeartbeatRequest body)
    {
        var ip = HubNetwork.CleanIp(address);
        // Trust only the configured operator override when the host
        // registers on loopback. External clients cannot spoof this address.
        var normalizedAddress = address.IsIPv4MappedToIPv6
            ? address.MapToIPv4() : address;
        var advertisedHost = IPAddress.IsLoopback(normalizedAddress)
            ? _trustedLocalPublicGameHost ?? ip
            : ip;
        var now = DateTimeOffset.UtcNow;
        lock(_gate)
        {
            Expire(now);
            if(!_rate.TryGetValue(ip, out var rate))
                _rate[ip] = rate = new RateState { WindowStart = now };
            if(now - rate.WindowStart > TimeSpan.FromMinutes(1))
            {
                rate.Calls = 0;
                rate.WindowStart = now;
            }
            if(++rate.Calls > 24) return RegistryResult.Throttled;

            var keyHash = Convert.ToHexString(SHA256.HashData(
                Encoding.ASCII.GetBytes(body.Key)));
            if(_entries.TryGetValue(body.Id, out var original))
            {
                // Bind BOTH the original token and remote address so a new
                // machine cannot hijack the public listing using its ID.
                if(original.SourceIp != ip ||
                   !HubNetwork.SameSecret(body.Key, original.KeyHash))
                    return RegistryResult.Denied;
                _entries[body.Id] = original with
                {
                    Name = body.Name,
                    Map = body.Map,
                    AdvertisedHost = advertisedHost,
                    Port = body.Port,
                    Players = body.Players,
                    MaxPlayers = body.MaxPlayers,
                    Difficulty = body.Difficulty,
                    Seen = now
                };
                return RegistryResult.Accepted;
            }

            if(_entries.Count >= GlobalCap ||
               _entries.Values.Count(e => e.SourceIp == ip) >= PerAddressCap)
                return RegistryResult.AtCapacity;
            _entries[body.Id] = new Entry(
                body.Id, keyHash, body.Name, body.Map, ip, advertisedHost,
                body.Port, body.Players, body.MaxPlayers, body.Difficulty, now);
            return RegistryResult.Accepted;
        }
    }

    public bool Offline(IPAddress address, OfflineRequest request)
    {
        lock(_gate)
        {
            if(!_entries.TryGetValue(request.Id, out var entry) ||
               entry.SourceIp != HubNetwork.CleanIp(address) ||
               !HubNetwork.SameSecret(request.Key, entry.KeyHash))
                return false;
            _entries.Remove(request.Id);
            return true;
        }
    }
}
