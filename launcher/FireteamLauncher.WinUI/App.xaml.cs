using Microsoft.UI.Xaml;
using FireteamLauncher.Infrastructure;
using FireteamLauncher.Services;

namespace FireteamLauncher;

public partial class App : Application
{
    private MainWindow? _window;

    public App()
    {
        StartupDiagnostics.Write(
            "App constructor entered.");

        UnhandledException +=
            App_UnhandledException;

        AppDomain.CurrentDomain.UnhandledException +=
            CurrentDomain_UnhandledException;

        try
        {
            StartupDiagnostics.Write(
                "App.InitializeComponent starting.");

            InitializeComponent();

            StartupDiagnostics.Write(
                "App.InitializeComponent completed.");

            StartupDiagnostics.Write(
                "LauncherServices construction starting.");

            Services =
                new LauncherServices();

            StartupDiagnostics.Write(
                "LauncherServices construction completed.");
        }
        catch(Exception exception)
        {
            StartupDiagnostics.WriteException(
                "Fatal App-constructor startup failure",
                exception);

            NativeMessageBox.ShowFatal(
                "FIRETEAM Launcher failed before its main window could open.\n\n" +
                "Startup log:\n" +
                StartupDiagnostics.LogFile +
                "\n\n" +
                exception);

            throw;
        }
    }

    public LauncherServices Services { get; }

    public MainWindow? MainWindow =>
        _window;

    public static App Instance =>
        (App)Current;

    protected override void OnLaunched(
        LaunchActivatedEventArgs args)
    {
        StartupDiagnostics.Write(
            "OnLaunched entered.");

        try
        {
            LauncherPaths.EnsureLayout();

            _window =
                new MainWindow();

            _window.Activate();
            _window.ApplyInitialSize();

            StartupDiagnostics.Write(
                "Main window activated successfully.");
        }
        catch(Exception exception)
        {
            StartupDiagnostics.WriteException(
                "Fatal managed startup failure",
                exception);

            NativeMessageBox.ShowFatal(
                "FIRETEAM Launcher could not start.\n\n" +
                "A startup log was written to:\n" +
                StartupDiagnostics.LogFile +
                "\n\n" +
                exception.Message);
        }
    }

    private void App_UnhandledException(
        object sender,
        Microsoft.UI.Xaml.UnhandledExceptionEventArgs e)
    {
        StartupDiagnostics.WriteException(
            "Unhandled WinUI exception",
            e.Exception);

        e.Handled = true;

        NativeMessageBox.ShowFatal(
            "FIRETEAM Launcher hit an unexpected WinUI error.\n\n" +
            e.Exception.Message +
            "\n\nFull details:\n" +
            StartupDiagnostics.LogFile);
    }

    private static void CurrentDomain_UnhandledException(
        object? sender,
        System.UnhandledExceptionEventArgs e)
    {
        StartupDiagnostics.Write(
            $"Unhandled AppDomain exception. IsTerminating={e.IsTerminating}. Object={e.ExceptionObject}");
    }
}
