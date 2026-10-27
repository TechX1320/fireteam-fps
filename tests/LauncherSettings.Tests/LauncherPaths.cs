namespace FireteamLauncher.Infrastructure;
// Test-only path boundary; the actual settings service and models are linked.
public static class LauncherPaths
{
    public static string? TestDirectory { get; set; }
    public static string? FindGameDirectory() => TestDirectory;
}
