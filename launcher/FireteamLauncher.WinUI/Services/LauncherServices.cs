namespace FireteamLauncher.Services;

public sealed class LauncherServices
{
    public LauncherServices()
    {
        Settings = new SettingsService();
        Weapons = new WeaponCatalogService();
        Game = new GameLaunchService(Settings);
    }

    public SettingsService Settings { get; }
    public WeaponCatalogService Weapons { get; }
    public GameLaunchService Game { get; }
}
