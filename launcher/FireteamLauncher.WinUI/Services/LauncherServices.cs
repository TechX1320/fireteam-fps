namespace FireteamLauncher.Services;

public sealed class LauncherServices
{
    public LauncherServices()
    {
        Settings = new SettingsService();
        Weapons = new WeaponCatalogService();
        WeaponImports = new WeaponImportService();
        Game = new GameLaunchService(Settings);
    }

    public SettingsService Settings { get; }
    public WeaponCatalogService Weapons { get; }
    public WeaponImportService WeaponImports { get; }
    public GameLaunchService Game { get; }
}
