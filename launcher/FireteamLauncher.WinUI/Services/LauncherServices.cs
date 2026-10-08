namespace FireteamLauncher.Services;

public sealed class LauncherServices
{
    public LauncherServices()
    {
        Settings = new SettingsService();
        Weapons = new WeaponCatalogService();
        Loadouts = new LoadoutPresetService(Weapons);
        WeaponImports = new WeaponImportService();
        AttachmentImports = new AttachmentImportService();
        MapImports = new MapImportService();
        Game = new GameLaunchService(Settings);
    }

    public SettingsService Settings { get; }
    public WeaponCatalogService Weapons { get; }
    public LoadoutPresetService Loadouts { get; }
    public WeaponImportService WeaponImports { get; }
    public AttachmentImportService AttachmentImports { get; }
    public MapImportService MapImports { get; }
    public GameLaunchService Game { get; }
}
