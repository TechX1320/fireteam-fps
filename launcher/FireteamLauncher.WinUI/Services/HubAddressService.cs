using FireteamLauncher.Infrastructure;

namespace FireteamLauncher.Services;

// The central directory URL is public configuration, NOT a user account or
// secret. Seeded once in BUILT/config/hub-url.txt when a hub is deployed.
public static class HubAddressService
{
    private static string FilePath
    {
        get
        {
            var game = LauncherPaths.FindGameDirectory();
            return Path.Combine(game ?? LauncherPaths.SupportRoot,
                                "config", "hub-url.txt");
        }
    }

    public static string? Load()
    {
        try
        {
            if(!File.Exists(FilePath)) return null;
            var value = File.ReadLines(FilePath)
                .Select(s => s.Trim())
                .FirstOrDefault(s => s.Length > 0 && !s.StartsWith('#'));
            return TryNormalize(value, out var url) ? url : null;
        }
        catch(IOException) { return null; }
        catch(UnauthorizedAccessException) { return null; }
    }

    /// <summary>
    /// Probe the development Hub on loopback only. This never enables public
    /// hosting and never modifies launcher settings or router configuration.
    /// </summary>
    public static async Task<string?> FindRunningLocalHubAsync()
    {
        const string localUrl = "http://127.0.0.1:27890";
        try
        {
            using var http = new HttpClient
            {
                Timeout = TimeSpan.FromSeconds(2),
                MaxResponseContentBufferSize = 4096
            };
            var json = await http.GetStringAsync(localUrl + "/healthz");
            using var result = System.Text.Json.JsonDocument.Parse(json);
            if(result.RootElement.GetProperty("status").GetString() == "ready" &&
               result.RootElement.GetProperty("name").GetString() == "FIRETEAM Hub")
                return localUrl;
        }
        catch(Exception ex) when(ex is HttpRequestException or
                                 TaskCanceledException or
                                 System.Text.Json.JsonException or
                                 KeyNotFoundException or
                                 InvalidOperationException)
        {
            // No local Hub running. LAN-only hosting must remain possible.
        }
        return null;
    }

    public static string Save(string rawUrl)
    {
        if(!TryNormalize(rawUrl, out var url))
            throw new ArgumentException(
                "Enter the hub's HTTPS base URL (HTTP is allowed only for localhost testing).");
        Directory.CreateDirectory(Path.GetDirectoryName(FilePath)!);
        File.WriteAllText(FilePath, url + Environment.NewLine);
        return url;
    }

    public static bool TryNormalize(string? raw, out string normalized)
    {
        normalized = "";
        if(string.IsNullOrWhiteSpace(raw) || raw.Length > 240 ||
           !Uri.TryCreate(raw.Trim(), UriKind.Absolute, out var uri) ||
           !string.IsNullOrEmpty(uri.UserInfo) ||
           !string.IsNullOrEmpty(uri.Query) ||
           !string.IsNullOrEmpty(uri.Fragment) ||
           uri.AbsolutePath != "/" || uri.Port is < 1 or > 65535)
            return false;

        var local = uri.Host.Equals("localhost", StringComparison.OrdinalIgnoreCase)
                    || uri.Host == "127.0.0.1";
        if(uri.Scheme != Uri.UriSchemeHttps &&
           !(local && uri.Scheme == Uri.UriSchemeHttp))
            return false;

        normalized = uri.GetLeftPart(UriPartial.Authority);
        return normalized.Length <= 240;
    }
}
