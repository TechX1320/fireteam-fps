namespace FireteamLauncher.Models;

public sealed record LauncherSettings(
    int Width,
    int Height,
    bool Windowed,
    double SensitivityXMultiplier,
    double SensitivityYMultiplier,
    int Volume,
    double Gamma,
    bool RadarEnabled = true,
    double RadarScale = 1.0,
    bool DamageFeedback = true,
    bool ReducedEffects = false);

public sealed record LauncherProfile(
    string PlayerName,
    string Mode,
    string Difficulty,
    string JoinIp,
    string CustomCommands,
    string Map = "CABINFEVER");
