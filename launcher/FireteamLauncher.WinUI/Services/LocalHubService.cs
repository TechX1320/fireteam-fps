using System.Diagnostics;
using FireteamLauncher.Infrastructure;

namespace FireteamLauncher.Services;

/// <summary>
/// Convenience launcher for the self-contained development Hub, which is
/// bound to loopback only. This never exposes a public network listener.
/// </summary>
public sealed class LocalHubService
{
    private Process? _process;

    public async Task<string> StartAsync()
    {
        var running = await HubAddressService.FindRunningLocalHubAsync();
        if(running is not null)
            return "Local FIRETEAM Hub is already running and healthy.";

        if(_process is not null && !_process.HasExited)
            return "Local Hub was launched but is not yet responding. Check its console.";

        var repo = LauncherPaths.FindRepositoryDirectory();
        var exe = repo is null
            ? null
            : Path.Combine(repo, "out", "hub", "win-x64", "FireteamHub.exe");
        if(exe is null || !File.Exists(exe))
            throw new FileNotFoundException(
                "Standalone Hub executable not found. Run build-hub.cmd once, " +
                "then use START LOCAL HUB, or continue using dotnet run for development.");

        var info = new ProcessStartInfo(exe)
        {
            WorkingDirectory = Path.GetDirectoryName(exe)!,
            UseShellExecute = false,
            CreateNoWindow = false,
            WindowStyle = ProcessWindowStyle.Normal
        };
        // Override a public binding inherited from a development terminal.
        // Exposing HTTPS externally is a separate operator deployment step.
        info.Environment["FIRETEAM_HUB_LISTEN"] = "http://127.0.0.1:27890";
        _process = Process.Start(info)
            ?? throw new InvalidOperationException("Windows did not start FIRETEAM Hub.");

        for(var retry = 0; retry < 12; ++retry)
        {
            await Task.Delay(300);
            if(_process.HasExited)
                throw new InvalidOperationException(
                    "FIRETEAM Hub exited during startup (exit code " +
                    _process.ExitCode + "). Check its console.");
            if(await HubAddressService.FindRunningLocalHubAsync() is not null)
                return "Local FIRETEAM Hub is running at http://127.0.0.1:27890. " +
                       "The endpoint is loopback-only, not public Internet hosting.";
        }
        return "FIRETEAM Hub process started but health was not confirmed. " +
               "Check its console for startup errors.";
    }
}
