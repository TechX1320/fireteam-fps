using System.Runtime.InteropServices;

namespace FireteamLauncher.Infrastructure;

public static class StartupDiagnostics
{
    private static readonly object SyncRoot = new();

    public static string LogFile =>
        Path.Combine(
            LauncherPaths.LogsRoot,
            "startup.log");

    public static void Write(string message)
    {
        try
        {
            Directory.CreateDirectory(
                LauncherPaths.LogsRoot);

            var line =
                $"{DateTimeOffset.Now:O} {message}{Environment.NewLine}";

            lock(SyncRoot)
            {
                File.AppendAllText(
                    LogFile,
                    line);
            }
        }
        catch
        {
            // Diagnostics must never become the startup failure.
        }
    }

    public static void WriteException(
        string context,
        Exception exception)
    {
        Write(
            $"{context}: HResult=0x{exception.HResult:X8} {exception}");

        var inner = exception.InnerException;
        var depth = 0;

        while(inner is not null &&
              depth < 4)
        {
            Write(
                $"{context} inner[{depth}]: HResult=0x{inner.HResult:X8} {inner}");
            inner = inner.InnerException;
            ++depth;
        }
    }
}

public static class NativeMessageBox
{
    private const uint MbOk = 0x00000000;
    private const uint MbIconError = 0x00000010;

    [DllImport(
        "user32.dll",
        CharSet = CharSet.Unicode,
        SetLastError = true)]
    private static extern int MessageBox(
        nint hWnd,
        string text,
        string caption,
        uint type);

    public static void ShowFatal(string message)
    {
        try
        {
            MessageBox(
                0,
                message,
                "FIRETEAM Launcher",
                MbOk | MbIconError);
        }
        catch
        {
        }
    }
}
