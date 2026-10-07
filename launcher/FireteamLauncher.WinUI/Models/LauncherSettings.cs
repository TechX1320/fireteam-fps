namespace FireteamLauncher.Models;

public sealed record LauncherSettings(
    int Width,
    int Height,
    bool Windowed,
    double SensitivityMultiplier,
    int Volume,
    double Gamma);

public sealed record LauncherProfile(
    string PlayerName,
    string Mode,
    string Difficulty,
    string JoinIp,
    string CustomCommands);
