namespace FireteamLauncher.Models;

public sealed record LauncherSettings(
    int Width,
    int Height,
    bool Windowed,
    double SensitivityXMultiplier,
    double SensitivityYMultiplier,
    int Volume,
    double Gamma);

public sealed record LauncherProfile(
    string PlayerName,
    string Mode,
    string Difficulty,
    string JoinIp,
    string CustomCommands);
