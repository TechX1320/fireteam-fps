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
    private readonly TextBlock ArsenalStatusText = new();

    private readonly ScrollViewer ModsView = new();
    private readonly TextBlock ModsPathText = new();
    private readonly TextBlock ToolsPathText = new();

    private readonly ScrollViewer SettingsView = new();
    private readonly ComboBox ResolutionCombo = new();
    private readonly ToggleSwitch WindowedToggle = new();
    private readonly NumberBox SensitivityBox = new();
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

    private void BuildUi()
    {
        StartupDiagnostics.Write("MainWindow.BuildUi starting.");

        var root = new Grid();
        root.RowDefinitions.Add(
            new RowDefinition { Height = new GridLength(50) });
        root.RowDefinitions.Add(
            new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });

        var title = new TextBlock
        {
            Text = "FIRETEAM  /  CABIN FEVER",
            FontSize = 18,
            FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(18, 0, 0, 0)
        };

        AppTitleBar.Children.Add(title);
        Grid.SetRow(AppTitleBar, 0);
        root.Children.Add(AppTitleBar);

        var body = new Grid();
        body.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width = new GridLength(220)
            });
        body.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width = new GridLength(
                    1,
                    GridUnitType.Star)
            });

        var navigation = new StackPanel
        {
            Spacing = 6,
            Margin = new Thickness(12, 18, 12, 18)
        };

        navigation.Children.Add(
            NavigationButton(
                "HOME",
                "home"));
        navigation.Children.Add(
            NavigationButton(
                "LOADOUT",
                "loadout"));
        navigation.Children.Add(
            NavigationButton(
                "ARSENAL",
                "arsenal"));
        navigation.Children.Add(
            NavigationButton(
                "MODS & CONTENT TOOLS",
                "mods"));
        navigation.Children.Add(
            NavigationButton(
                "SETTINGS",
                "settings"));

        var content = new Grid();
        BuildHomeView();
        BuildLoadoutView();
        BuildArsenalView();
        BuildModsView();
        BuildSettingsView();

        content.Children.Add(HomeView);
        content.Children.Add(LoadoutView);
        content.Children.Add(ArsenalView);
        content.Children.Add(ModsView);
        content.Children.Add(SettingsView);

        Grid.SetColumn(navigation, 0);
        Grid.SetColumn(content, 1);
        body.Children.Add(navigation);
        body.Children.Add(content);

        Grid.SetRow(body, 1);
        root.Children.Add(body);

        Content = root;

        StartupDiagnostics.Write("MainWindow.BuildUi completed.");
    }

    private static StackPanel NewPagePanel(string title)
    {
        var panel = new StackPanel
        {
            Spacing = 12,
            Margin = new Thickness(28)
        };

        panel.Children.Add(
            new TextBlock
            {
                Text = title,
                FontSize = 30,
                FontWeight =
                    Microsoft.UI.Text.FontWeights.SemiBold
            });

        return panel;
    }

    private static Grid LabeledControl(
        string label,
        FrameworkElement control)
    {
        var row = new Grid
        {
            ColumnSpacing = 12
        };

        row.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width = new GridLength(180)
            });
        row.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width = new GridLength(
                    1,
                    GridUnitType.Star)
            });

        var text = new TextBlock
        {
            Text = label,
            VerticalAlignment =
                VerticalAlignment.Center
        };

        Grid.SetColumn(text, 0);
        Grid.SetColumn(control, 1);
        row.Children.Add(text);
        row.Children.Add(control);

        return row;
    }

    private static Button ActionButton(
        string text,
        RoutedEventHandler handler)
    {
        var button = new Button
        {
            Content = text,
            Padding = new Thickness(16, 8, 16, 8)
        };

        button.Click += handler;
        return button;
    }

    private Button NavigationButton(
        string text,
        string tag)
    {
        var button = new Button
        {
            Content = text,
            HorizontalAlignment =
                HorizontalAlignment.Stretch,
            Padding =
                new Thickness(14, 10, 14, 10)
        };

        button.Click +=
            (sender, args) =>
                NavigateTo(tag);

        return button;
    }

    private void NavigateTo(string tag)
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
        else if(tag == "settings")
        {
            LoadSettings();
        }
    }

    private void BuildHomeView()
    {
        var panel = NewPagePanel("FIRETEAM");

        panel.Children.Add(
            new TextBlock
            {
                Text = "Configure the run, then deploy directly into Cabin Fever.",
                TextWrapping = TextWrapping.Wrap
            });

        PlayerNameBox.MaxLength = 15;
        panel.Children.Add(
            LabeledControl(
                "PLAYER NAME",
                PlayerNameBox));

        ModeCombo.SelectionChanged +=
            ModeCombo_SelectionChanged;
        panel.Children.Add(
            LabeledControl(
                "MODE",
                ModeCombo));

        panel.Children.Add(
            LabeledControl(
                "DIFFICULTY",
                DifficultyCombo));

        JoinIpLabel.Text = "SERVER IP";
        JoinIpLabel.VerticalAlignment =
            VerticalAlignment.Center;
        JoinIpLabel.Visibility =
            Visibility.Collapsed;
        JoinIpBox.Visibility =
            Visibility.Collapsed;

        var joinRow = new Grid
        {
            ColumnSpacing = 12
        };
        joinRow.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width = new GridLength(180)
            });
        joinRow.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width = new GridLength(
                    1,
                    GridUnitType.Star)
            });
        Grid.SetColumn(JoinIpLabel, 0);
        Grid.SetColumn(JoinIpBox, 1);
        joinRow.Children.Add(JoinIpLabel);
        joinRow.Children.Add(JoinIpBox);
        panel.Children.Add(joinRow);

        CommandsBox.PlaceholderText =
            "+consoleenable 1 +SomeLithTechCommand value";
        panel.Children.Add(
            LabeledControl(
                "ADVANCED COMMANDS",
                CommandsBox));

        panel.Children.Add(
            new TextBlock
            {
                Text = "CURRENT LOADOUT",
                FontWeight =
                    Microsoft.UI.Text.FontWeights.SemiBold,
                Margin = new Thickness(0, 12, 0, 0)
            });

        panel.Children.Add(PrimarySummaryText);
        panel.Children.Add(SidearmSummaryText);
        panel.Children.Add(MeleeSummaryText);
        panel.Children.Add(SecondarySummaryText);
        panel.Children.Add(SpecialSummaryText);

        panel.Children.Add(
            ActionButton(
                "EDIT LOADOUT",
                EditLoadoutButton_Click));

        RunStatusText.TextWrapping =
            TextWrapping.Wrap;
        panel.Children.Add(RunStatusText);

        PlayButton.Content = "PLAY FIRETEAM";
        PlayButton.MinHeight = 48;
        PlayButton.Click +=
            PlayButton_Click;
        panel.Children.Add(PlayButton);

        HomeView.Content = panel;
    }

    private void BuildLoadoutView()
    {
        LoadoutView.Visibility =
            Visibility.Collapsed;

        var panel =
            NewPagePanel("Build your deployment kit");

        var combos = new[]
        {
            PrimaryCombo,
            SidearmCombo,
            MeleeCombo,
            SecondaryCombo,
            SpecialCombo
        };

        foreach(var combo in combos)
        {
            combo.DisplayMemberPath =
                "DisplayName";
            combo.HorizontalAlignment =
                HorizontalAlignment.Stretch;
        }

        panel.Children.Add(
            LabeledControl(
                "PRIMARY",
                PrimaryCombo));
        panel.Children.Add(
            LabeledControl(
                "SIDEARM",
                SidearmCombo));
        panel.Children.Add(
            LabeledControl(
                "MELEE",
                MeleeCombo));
        panel.Children.Add(
            LabeledControl(
                "SECONDARY / EXTRA",
                SecondaryCombo));
        panel.Children.Add(
            LabeledControl(
                "SPECIAL",
                SpecialCombo));

        var buttons = new StackPanel
        {
            Orientation =
                Orientation.Horizontal,
            Spacing = 10
        };
        buttons.Children.Add(
            ActionButton(
                "OPEN ARSENAL",
                OpenArsenalButton_Click));
        buttons.Children.Add(
            ActionButton(
                "APPLY LOADOUT",
                ApplyLoadoutButton_Click));
        panel.Children.Add(buttons);

        LoadoutStatusText.TextWrapping =
            TextWrapping.Wrap;
        panel.Children.Add(
            LoadoutStatusText);

        LoadoutView.Content = panel;
    }

    private void BuildArsenalView()
    {
        ArsenalView.Visibility =
            Visibility.Collapsed;

        var scroll = new ScrollViewer();
        var panel =
            NewPagePanel("Weapon Catalog");

        WeaponSearchBox.PlaceholderText =
            "Search name or ID...";
        WeaponSearchBox.TextChanged +=
            WeaponSearchBox_TextChanged;
        panel.Children.Add(WeaponSearchBox);

        WeaponList.DisplayMemberPath =
            "DisplayName";
        WeaponList.MinHeight = 180;
        WeaponList.MaxHeight = 260;
        WeaponList.SelectionChanged +=
            WeaponList_SelectionChanged;
        panel.Children.Add(WeaponList);

        WeaponTitleText.Text =
            "Select a weapon";
        WeaponTitleText.FontSize = 22;
        panel.Children.Add(
            WeaponTitleText);

        WeaponSectionText.TextWrapping =
            TextWrapping.Wrap;
        panel.Children.Add(
            WeaponSectionText);

        panel.Children.Add(
            LabeledControl(
                "NAME",
                WeaponNameBox));
        panel.Children.Add(
            LabeledControl(
                "ID",
                WeaponIdBox));

        panel.Children.Add(
            LabeledControl(
                "TYPE",
                WeaponTypeCombo));
        panel.Children.Add(
            LabeledControl(
                "DAMAGE",
                DamageBox));
        panel.Children.Add(
            LabeledControl(
                "MAGAZINE",
                ClipBox));
        panel.Children.Add(
            LabeledControl(
                "RESERVE",
                ReserveBox));
        panel.Children.Add(
            LabeledControl(
                "FIRE INTERVAL",
                FireIntervalBox));
        panel.Children.Add(
            LabeledControl(
                "RELOAD SEC",
                ReloadBox));

        var buttons = new StackPanel
        {
            Orientation =
                Orientation.Horizontal,
            Spacing = 10
        };
        buttons.Children.Add(
            ActionButton(
                "RELOAD CONFIG",
                ReloadArsenalButton_Click));
        buttons.Children.Add(
            ActionButton(
                "SAVE WEAPON",
                SaveWeaponButton_Click));
        panel.Children.Add(buttons);

        ArsenalStatusText.TextWrapping =
            TextWrapping.Wrap;
        panel.Children.Add(
            ArsenalStatusText);

        scroll.Content = panel;
        ArsenalView.Children.Add(scroll);
    }

    private void BuildModsView()
    {
        ModsView.Visibility =
            Visibility.Collapsed;

        var panel =
            NewPagePanel("Mods & Content Tools");

        panel.Children.Add(
            ActionButton(
                "OPEN MOD LIBRARY",
                OpenModsButton_Click));

        ModsPathText.TextWrapping =
            TextWrapping.Wrap;
        panel.Children.Add(
            ModsPathText);

        panel.Children.Add(
            ActionButton(
                "OPEN CONTENT TOOLS",
                OpenToolsButton_Click));

        ToolsPathText.TextWrapping =
            TextWrapping.Wrap;
        panel.Children.Add(
            ToolsPathText);

        ModsView.Content = panel;
    }

    private void BuildSettingsView()
    {
        SettingsView.Visibility =
            Visibility.Collapsed;

        var panel =
            NewPagePanel("Game settings");

        panel.Children.Add(
            LabeledControl(
                "RESOLUTION",
                ResolutionCombo));

        WindowedToggle.OnContent =
            "Windowed";
        WindowedToggle.OffContent =
            "Fullscreen";
        panel.Children.Add(
            LabeledControl(
                "DISPLAY MODE",
                WindowedToggle));

        SensitivityBox.Minimum = 0.01;
        SensitivityBox.Maximum = 4.0;
        SensitivityBox.SmallChange = 0.01;
        SensitivityBox.SpinButtonPlacementMode =
            NumberBoxSpinButtonPlacementMode.Inline;
        panel.Children.Add(
            LabeledControl(
                "MOUSE SENSITIVITY",
                SensitivityBox));

        VolumeBox.Minimum = 0;
        VolumeBox.Maximum = 100;
        VolumeBox.SmallChange = 5;
        VolumeBox.SpinButtonPlacementMode =
            NumberBoxSpinButtonPlacementMode.Inline;
        panel.Children.Add(
            LabeledControl(
                "GAME VOLUME",
                VolumeBox));

        GammaBox.Minimum = 0.5;
        GammaBox.Maximum = 6.0;
        GammaBox.SmallChange = 0.1;
        GammaBox.SpinButtonPlacementMode =
            NumberBoxSpinButtonPlacementMode.Inline;
        panel.Children.Add(
            LabeledControl(
                "BRIGHTNESS / GAMMA",
                GammaBox));

        panel.Children.Add(
            ActionButton(
                "SAVE SETTINGS",
                SaveSettingsButton_Click));

        panel.Children.Add(
            SettingsStatusText);

        SettingsView.Content = panel;
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
        NavigateTo("loadout");
    }

    private void OpenArsenalButton_Click(object sender, RoutedEventArgs e)
    {
        NavigateTo("arsenal");
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
