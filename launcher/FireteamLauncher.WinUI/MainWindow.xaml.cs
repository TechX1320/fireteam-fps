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

    public MainWindow()
    {
        InitializeComponent();

        Title = "FIRETEAM Launcher";
        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);

        ModeCombo.ItemsSource =
        [
            "Single Player",
            "Host Multiplayer",
            "Join Multiplayer"
        ];

        DifficultyCombo.ItemsSource =
        [
            "Easy",
            "Normal",
            "Hard",
            "Extreme",
            "Nightmare"
        ];

        WeaponTypeCombo.ItemsSource =
        [
            "hitscan",
            "melee",
            "grenade",
            "rocket"
        ];

        ResolutionCombo.ItemsSource = Resolutions;

        LoadProfile();
        LoadSettings();
        ReloadWeapons();
        RefreshToolPaths();

        RootNavigation.SelectedItem = HomeItem;
        ShowView("home");
    }

    public void ApplyInitialSize()
    {
        try
        {
            AppWindow.Resize(new SizeInt32(1280, 820));
        }
        catch
        {
        }
    }

    private void RootNavigation_SelectionChanged(
        NavigationView sender,
        NavigationViewSelectionChangedEventArgs args)
    {
        if(args.IsSettingsSelected)
        {
            ShowView("settings");
            LoadSettings();
            return;
        }

        if(args.SelectedItemContainer?.Tag is string tag)
        {
            ShowView(tag);

            if(tag == "loadout")
            {
                ReloadWeapons();
            }
            else if(tag == "arsenal")
            {
                ReloadArsenal();
            }
            else if(tag == "mods")
            {
                RefreshToolPaths();
            }
        }
    }

    private void ShowView(string tag)
    {
        HomeView.Visibility = tag == "home" ? Visibility.Visible : Visibility.Collapsed;
        LoadoutView.Visibility = tag == "loadout" ? Visibility.Visible : Visibility.Collapsed;
        ArsenalView.Visibility = tag == "arsenal" ? Visibility.Visible : Visibility.Collapsed;
        ModsView.Visibility = tag == "mods" ? Visibility.Visible : Visibility.Collapsed;
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
            : "Cabin Fever runtime found. Configure the run and launch when ready.";
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
        SensitivityBox.Value = settings.SensitivityMultiplier;
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
        RootNavigation.SelectedItem = LoadoutItem;
        ShowView("loadout");
        ReloadWeapons();
    }

    private void OpenArsenalButton_Click(object sender, RoutedEventArgs e)
    {
        RootNavigation.SelectedItem = ArsenalItem;
        ShowView("arsenal");
        ReloadArsenal();
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
                Enabled = _selectedWeapon.Enabled,
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
                double.IsNaN(SensitivityBox.Value)
                    ? 0.32
                    : SensitivityBox.Value,
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
