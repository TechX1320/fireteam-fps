using FireteamLauncher.Infrastructure;
using FireteamLauncher.Models;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.Graphics;

namespace FireteamLauncher;

public sealed partial class MainWindow : Window
{
    private static readonly string[] Resolutions =
    [
        "1024 x 768",
        "1280 x 720",
        "1366 x 768",
        "1600 x 900",
        "1920 x 1080",
        "2560 x 1440",
        "3440 x 1440",
        "3840 x 2160"
    ];

    private IReadOnlyList<WeaponDefinition> _arsenal = [];
    private IReadOnlyList<WeaponDefinition> _choices = [];
    private WeaponDefinition? _selectedWeapon;

    private readonly Grid AppTitleBar = new();

    private readonly ScrollViewer HomeView = new();
    private readonly TextBox PlayerNameBox = new();
    private readonly ComboBox ModeCombo = new();
    private readonly ComboBox DifficultyCombo = new();
    private readonly TextBlock JoinIpLabel = new();
    private readonly TextBox JoinIpBox = new();
    private readonly TextBox CommandsBox = new();
    private readonly TextBlock PrimarySummaryText = new();
    private readonly TextBlock SidearmSummaryText = new();
    private readonly TextBlock MeleeSummaryText = new();
    private readonly TextBlock SecondarySummaryText = new();
    private readonly TextBlock SpecialSummaryText = new();
    private readonly TextBlock RunStatusText = new();
    private readonly Button PlayButton = new();

    private readonly ScrollViewer LoadoutView = new();
    private readonly ComboBox PrimaryCombo = new();
    private readonly ComboBox SidearmCombo = new();
    private readonly ComboBox MeleeCombo = new();
    private readonly ComboBox SecondaryCombo = new();
    private readonly ComboBox SpecialCombo = new();
    private readonly TextBlock LoadoutStatusText = new();

    private readonly Grid ArsenalView = new();
    private readonly TextBox WeaponSearchBox = new();
    private readonly ListView WeaponList = new();
    private readonly TextBlock WeaponTitleText = new();
    private readonly TextBlock WeaponSectionText = new();
    private readonly TextBox WeaponNameBox = new();
    private readonly TextBox WeaponIdBox = new();
    private readonly ComboBox WeaponTypeCombo = new();
    private readonly TextBox DamageBox = new();
    private readonly TextBox ClipBox = new();
    private readonly TextBox ReserveBox = new();
    private readonly TextBox FireIntervalBox = new();
    private readonly TextBox ReloadBox = new();
    private readonly CheckBox WeaponEnabledCheckBox = new();
    private readonly TextBlock ArsenalStatusText = new();

    private readonly ScrollViewer ModsView = new();
    private readonly TextBlock ModsPathText = new();
    private readonly TextBlock ToolsPathText = new();

    private readonly ScrollViewer SettingsView = new();
    private readonly ComboBox ResolutionCombo = new();
    private readonly ToggleSwitch WindowedToggle = new();
    private readonly NumberBox SensitivityXBox = new();
    private readonly NumberBox SensitivityYBox = new();
    private readonly NumberBox VolumeBox = new();
    private readonly NumberBox GammaBox = new();
    private readonly TextBlock SettingsStatusText = new();

    public MainWindow()
    {
        BuildUi();

        Title = "FIRETEAM Launcher";
        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);

        StartupDiagnostics.Write(
            "MainWindow title bar configured.");

        ModeCombo.ItemsSource = new string[]
        {
            "Single Player",
            "Host Multiplayer",
            "Join Multiplayer"
        };

        DifficultyCombo.ItemsSource = new string[]
        {
            "Easy",
            "Normal",
            "Hard",
            "Extreme",
            "Nightmare"
        };

        WeaponTypeCombo.ItemsSource = new string[]
        {
            "hitscan",
            "melee",
            "grenade",
            "rocket"
        };

        ResolutionCombo.ItemsSource = Resolutions;

        StartupDiagnostics.Write(
            "MainWindow control data sources configured.");

        LoadProfile();
        StartupDiagnostics.Write(
            "MainWindow profile loaded.");

        LoadSettings();
        StartupDiagnostics.Write(
            "MainWindow settings loaded.");

        ReloadWeapons();
        StartupDiagnostics.Write(
            "MainWindow weapon catalog loaded.");

        RefreshToolPaths();
        StartupDiagnostics.Write(
            "MainWindow tool paths refreshed.");

        ShowView("home");
        StartupDiagnostics.Write(
            "MainWindow constructor completed.");
    }

    public void ApplyInitialSize()
    {
        try
        {
            AppWindow.Resize(new SizeInt32(1360, 860));
        }
        catch
        {
        }
    }

    private void ShowView(string tag)
    {
        HomeView.Visibility = tag == "home" ? Visibility.Visible : Visibility.Collapsed;
        LoadoutView.Visibility = tag == "loadout" ? Visibility.Visible : Visibility.Collapsed;
        ArsenalView.Visibility = tag == "weapon-editor" ? Visibility.Visible : Visibility.Collapsed;
        ModsView.Visibility = tag == "tools" ? Visibility.Visible : Visibility.Collapsed;
        SettingsView.Visibility = tag == "settings" ? Visibility.Visible : Visibility.Collapsed;
    }

    private void LoadProfile()
    {
        var profile = App.Instance.Services.Settings.LoadProfile();

        PlayerNameBox.Text = profile.PlayerName;
        JoinIpBox.Text = profile.JoinIp;
        CommandsBox.Text = profile.CustomCommands;

        SelectString(ModeCombo, profile.Mode, "Single Player");
        SelectString(DifficultyCombo, profile.Difficulty, "Normal");

        RefreshModeVisibility();

        var game = LauncherPaths.FindGameDirectory();
        PlayButton.IsEnabled = game is not null;
        RunStatusText.Text = game is null
            ? "FIRETEAM runtime not found. Run build.cmd first."
            : "FIRETEAM runtime ready. Cabin Fever is available for this run.";
    }

    private void LoadSettings()
    {
        var settings = App.Instance.Services.Settings.LoadSettings();
        var resolution = $"{settings.Width} x {settings.Height}";

        ResolutionCombo.SelectedItem =
            Resolutions.Contains(resolution)
            ? resolution
            : "1920 x 1080";

        WindowedToggle.IsOn = settings.Windowed;
        SensitivityXBox.Value = settings.SensitivityXMultiplier;
        SensitivityYBox.Value = settings.SensitivityYMultiplier;
        VolumeBox.Value = settings.Volume;
        GammaBox.Value = settings.Gamma;
        SettingsStatusText.Text = "Current FIRETEAM settings loaded.";
    }

    private void ReloadWeapons()
    {
        try
        {
            var all = App.Instance.Services.Weapons.Load();
            var active = all
                .Where(w => w.IsActiveSlot)
                .OrderBy(w => w.Section)
                .ToList();

            _choices = App.Instance.Services.Weapons.GetArsenalChoices();

            foreach(var combo in LoadoutCombos())
            {
                combo.ItemsSource = _choices;
            }

            for(var slot = 0; slot < LoadoutCombos().Count; ++slot)
            {
                var current = slot < active.Count ? active[slot] : null;
                var index = current is null
                    ? 0
                    : FindChoiceIndex(current.Id, current.Name);

                LoadoutCombos()[slot].SelectedIndex =
                    _choices.Count == 0
                    ? -1
                    : Math.Clamp(index, 0, _choices.Count - 1);
            }

            _arsenal = all;
            ApplyArsenalFilter();
            RefreshHomeLoadoutSummary(active);

            LoadoutStatusText.Text =
                _choices.Count == 0
                ? "No weapon definitions are available."
                : $"{_choices.Count} weapon definitions are available to the current loadout.";
        }
        catch(Exception ex)
        {
            LoadoutStatusText.Text = "Loadout error: " + ex.Message;
            ArsenalStatusText.Text = "Arsenal error: " + ex.Message;
        }
    }

    private void ReloadArsenal()
    {
        ReloadWeapons();
    }

    private void RefreshHomeLoadoutSummary(IReadOnlyList<WeaponDefinition>? active = null)
    {
        try
        {
            active ??= App.Instance.Services.Weapons.Load()
                .Where(w => w.IsActiveSlot)
                .OrderBy(w => w.Section)
                .ToList();

            var labels = new[]
            {
                PrimarySummaryText,
                SidearmSummaryText,
                MeleeSummaryText,
                SecondarySummaryText,
                SpecialSummaryText
            };

            var prefixes = new[]
            {
                "PRIMARY  •  ",
                "SIDEARM  •  ",
                "MELEE  •  ",
                "EXTRA  •  ",
                "SPECIAL  •  "
            };

            for(var i = 0; i < labels.Length; ++i)
            {
                labels[i].Text =
                    prefixes[i] +
                    (i < active.Count
                        ? active[i].Name
                        : "Not configured");
            }
        }
        catch(Exception ex)
        {
            PrimarySummaryText.Text = "LOADOUT ERROR  •  " + ex.Message;
        }
    }

    private void ModeCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        RefreshModeVisibility();
    }

    private void RefreshModeVisibility()
    {
        var join = string.Equals(
            ModeCombo.SelectedItem?.ToString(),
            "Join Multiplayer",
            StringComparison.OrdinalIgnoreCase);

        JoinIpLabel.Visibility =
            join ? Visibility.Visible : Visibility.Collapsed;

        JoinIpBox.Visibility =
            join ? Visibility.Visible : Visibility.Collapsed;
    }

    private void EditLoadoutButton_Click(object sender, RoutedEventArgs e)
    {
        NavigateTo("loadout");
    }

    private void OpenArsenalButton_Click(object sender, RoutedEventArgs e)
    {
        NavigateTo("weapon-editor");
    }

    private void ApplyLoadoutButton_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            var selections = new List<WeaponDefinition>(5);

            foreach(var combo in LoadoutCombos())
            {
                if(combo.SelectedItem is not WeaponDefinition weapon)
                {
                    throw new InvalidOperationException(
                        "Every loadout slot needs a weapon.");
                }

                selections.Add(weapon);
            }

            App.Instance.Services.Weapons.SaveLoadout(selections);
            LoadoutStatusText.Text =
                "Loadout applied to source + BUILT config. Start FIRETEAM to test it.";

            ReloadWeapons();
        }
        catch(Exception ex)
        {
            LoadoutStatusText.Text =
                "Could not apply loadout: " + ex.Message;
        }
    }

    private List<ComboBox> LoadoutCombos() =>
    [
        PrimaryCombo,
        SidearmCombo,
        MeleeCombo,
        SecondaryCombo,
        SpecialCombo
    ];

    private int FindChoiceIndex(string id, string name)
    {
        for(var i = 0; i < _choices.Count; ++i)
        {
            if((!string.IsNullOrWhiteSpace(id) &&
                _choices[i].Id.Equals(id, StringComparison.OrdinalIgnoreCase)) ||
               _choices[i].Name.Equals(name, StringComparison.OrdinalIgnoreCase))
            {
                return i;
            }
        }

        return 0;
    }

    private void WeaponSearchBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        ApplyArsenalFilter();
    }

    private void ApplyArsenalFilter()
    {
        var query = WeaponSearchBox.Text?.Trim() ?? string.Empty;

        var visible = _arsenal
            .Where(w =>
                query.Length == 0 ||
                w.Name.Contains(query, StringComparison.OrdinalIgnoreCase) ||
                w.Id.Contains(query, StringComparison.OrdinalIgnoreCase) ||
                w.Type.Contains(query, StringComparison.OrdinalIgnoreCase))
            .OrderByDescending(w => w.IsActiveSlot)
            .ThenBy(w => w.Name)
            .ToList();

        WeaponList.ItemsSource = visible;

        if(visible.Count > 0)
        {
            WeaponList.SelectedIndex = 0;
        }
        else
        {
            _selectedWeapon = null;
            WeaponTitleText.Text = "No matching weapons";
            WeaponSectionText.Text = string.Empty;
        }
    }

    private void WeaponList_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if(WeaponList.SelectedItem is not WeaponDefinition weapon)
        {
            return;
        }

        _selectedWeapon = weapon;

        WeaponTitleText.Text = weapon.Name;
        WeaponSectionText.Text =
            $"{weapon.Section}  •  {weapon.Id}  •  {weapon.Source}";

        WeaponNameBox.Text = weapon.Name;
        WeaponIdBox.Text = weapon.Id;
        WeaponTypeCombo.SelectedItem = weapon.Type;

        SetWeaponField(DamageBox, weapon, "damage");
        SetWeaponField(ClipBox, weapon, "clip");
        SetWeaponField(ReserveBox, weapon, "reserve");
        SetWeaponField(FireIntervalBox, weapon, "fire_interval");
        SetWeaponField(ReloadBox, weapon, "reload");
        WeaponEnabledCheckBox.IsChecked =
            weapon.Enabled || weapon.IsActiveSlot;
        WeaponEnabledCheckBox.IsEnabled =
            !weapon.IsActiveSlot;
    }

    private void SaveWeaponButton_Click(object sender, RoutedEventArgs e)
    {
        if(_selectedWeapon is null)
        {
            ArsenalStatusText.Text = "Select a weapon first.";
            return;
        }

        try
        {
            var values = new Dictionary<string, string>(
                _selectedWeapon.Values,
                StringComparer.OrdinalIgnoreCase);

            Put(values, "name", WeaponNameBox.Text);
            Put(values, "id", WeaponIdBox.Text);
            Put(values, "type", WeaponTypeCombo.SelectedItem?.ToString());
            Put(values, "damage", DamageBox.Text);
            Put(values, "clip", ClipBox.Text);
            Put(values, "reserve", ReserveBox.Text);
            Put(values, "fire_interval", FireIntervalBox.Text);
            Put(values, "reload", ReloadBox.Text);

            var updated = new WeaponDefinition
            {
                Section = _selectedWeapon.Section,
                Id = WeaponIdBox.Text.Trim(),
                Name = WeaponNameBox.Text.Trim(),
                Type = WeaponTypeCombo.SelectedItem?.ToString()
                    ?? _selectedWeapon.Type,
                Enabled =
                    _selectedWeapon.IsActiveSlot ||
                    WeaponEnabledCheckBox.IsChecked == true,
                Supported = _selectedWeapon.Supported,
                IsActiveSlot = _selectedWeapon.IsActiveSlot,
                Source = _selectedWeapon.Source,
                Values = values
            };

            App.Instance.Services.Weapons.SaveDefinition(updated);
            ArsenalStatusText.Text =
                $"Saved {updated.Name}. Gameplay authority remains server-side.";

            ReloadWeapons();
        }
        catch(Exception ex)
        {
            ArsenalStatusText.Text =
                "Save failed: " + ex.Message;
        }
    }

    private void ReloadArsenalButton_Click(object sender, RoutedEventArgs e)
    {
        ReloadArsenal();
        ArsenalStatusText.Text = "Reloaded config/weapons.cfg.";
    }

    private static void SetWeaponField(
        TextBox box,
        WeaponDefinition weapon,
        string key)
    {
        box.Text = weapon.Values.TryGetValue(key, out var value)
            ? value
            : string.Empty;
    }

    private static void Put(
        Dictionary<string, string> values,
        string key,
        string? value)
    {
        values[key] = value?.Trim() ?? string.Empty;
    }

    private void OpenModsButton_Click(object sender, RoutedEventArgs e)
    {
        if(!LauncherPaths.OpenFolder(LauncherPaths.ModsDirectory))
        {
            ModsPathText.Text =
                "FIRETEAM runtime not found. Run build.cmd first.";
        }
    }

    private void OpenToolsButton_Click(object sender, RoutedEventArgs e)
    {
        if(!LauncherPaths.OpenFolder(LauncherPaths.ModToolsDirectory))
        {
            ToolsPathText.Text =
                "FIRETEAM project/runtime not found.";
        }
    }

    private void RefreshToolPaths()
    {
        ModsPathText.Text =
            LauncherPaths.ModsDirectory ??
            "FIRETEAM runtime not found.";

        ToolsPathText.Text =
            LauncherPaths.ModToolsDirectory ??
            "FIRETEAM project/runtime not found.";
    }

    private void SaveSettingsButton_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            var (width, height) =
                ParseResolution(
                    ResolutionCombo.SelectedItem?.ToString()
                    ?? "1920 x 1080");

            var settings = new LauncherSettings(
                width,
                height,
                WindowedToggle.IsOn,
                double.IsNaN(SensitivityXBox.Value)
                    ? 0.32
                    : SensitivityXBox.Value,
                double.IsNaN(SensitivityYBox.Value)
                    ? 0.32
                    : SensitivityYBox.Value,
                double.IsNaN(VolumeBox.Value)
                    ? 60
                    : (int)Math.Round(VolumeBox.Value),
                double.IsNaN(GammaBox.Value)
                    ? 1.0
                    : GammaBox.Value);

            App.Instance.Services.Settings.SaveSettings(settings);
            SettingsStatusText.Text =
                "Saved fireteam-settings.cfg.";
        }
        catch(Exception ex)
        {
            SettingsStatusText.Text =
                "Could not save settings: " + ex.Message;
        }
    }

    private void PlayButton_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            var profile = new LauncherProfile(
                string.IsNullOrWhiteSpace(PlayerNameBox.Text)
                    ? "Player"
                    : PlayerNameBox.Text.Trim(),
                ModeCombo.SelectedItem?.ToString()
                    ?? "Single Player",
                DifficultyCombo.SelectedItem?.ToString()
                    ?? "Normal",
                string.IsNullOrWhiteSpace(JoinIpBox.Text)
                    ? "127.0.0.1"
                    : JoinIpBox.Text.Trim(),
                CommandsBox.Text ?? string.Empty);

            var settings =
                App.Instance.Services.Settings.LoadSettings();

            RunStatusText.Text =
                App.Instance.Services.Game.Launch(
                    profile,
                    settings);
        }
        catch(Exception ex)
        {
            RunStatusText.Text =
                "Launch failed: " + ex.Message;
        }
    }

    private static (int Width, int Height) ParseResolution(string text)
    {
        var parts = text.Split(
            'x',
            StringSplitOptions.TrimEntries);

        if(parts.Length != 2 ||
           !int.TryParse(parts[0], out var width) ||
           !int.TryParse(parts[1], out var height))
        {
            return (1920, 1080);
        }

        return (width, height);
    }

    private static void SelectString(
        ComboBox combo,
        string value,
        string fallback)
    {
        var items = combo.ItemsSource as IEnumerable<string>;
        var selected = items?.FirstOrDefault(
            item => item.Equals(
                value,
                StringComparison.OrdinalIgnoreCase));

        combo.SelectedItem = selected ?? fallback;
    }
}
