using Microsoft.UI.Xaml;
using FireteamLauncher.Infrastructure;
using FireteamLauncher.Services;

namespace FireteamLauncher;

public partial class App : Application
{
    private MainWindow? _window;

    public App()
    {
        InitializeComponent();
        Services = new LauncherServices();
    }

    public LauncherServices Services { get; }

    public MainWindow? MainWindow => _window;

    public static App Instance => (App)Current;

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        LauncherPaths.EnsureLayout();

        _window = new MainWindow();
        _window.Activate();
        _window.ApplyInitialSize();
    }
}
