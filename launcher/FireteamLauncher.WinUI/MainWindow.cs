using FireteamLauncher.Infrastructure;
using FireteamLauncher.Models;
using FireteamLauncher.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.Graphics;
using Windows.Storage.Pickers;

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
    private IReadOnlyList<LoadoutPreset> _loadoutPresets = [];
    private readonly WeaponDefinition?[] _draftLoadout =
        new WeaponDefinition?[5];
    private WeaponDefinition? _selectedWeapon;
    private WeaponDefinition? _selectedLoadoutWeapon;
    private string _loadoutCategory = "AR";
    private int _selectedLoadoutPresetIndex = 1;
    private int _selectedLoadoutSlot;
    private readonly Dictionary<string, Button> _loadoutCategoryButtons =
        new(StringComparer.OrdinalIgnoreCase);

    private readonly Grid AppTitleBar = new();
    private readonly TextBlock AppSectionTitleText = new();

    private readonly ScrollViewer HomeView = new();
    private readonly TextBox PlayerNameBox = new();
    private readonly ComboBox ModeCombo = new();
    private readonly ComboBox MapCombo = new();
    private readonly Slider DifficultySlider = new();
    private readonly TextBlock DifficultyValueText = new();
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
    private readonly ScrollViewer PlayerGearView = new();
    private readonly ScrollViewer WeaponModsView = new();
    private readonly ListView LoadoutPresetList = new();
    private readonly TextBox LoadoutPresetNameBox = new();
    private readonly Button LoadoutSlot1Button = new();
    private readonly Button LoadoutSlot2Button = new();
    private readonly Button LoadoutSlot3Button = new();
    private readonly Button LoadoutSlot4Button = new();
    private readonly Button LoadoutSlot5Button = new();
    private readonly TextBox LoadoutInventorySearchBox = new();
    private readonly ListView LoadoutInventoryList = new();
    private readonly TextBlock LoadoutCategoryStatusText = new();
    private readonly TextBlock LoadoutWeaponTitleText = new();
    private readonly TextBlock LoadoutWeaponMetaText = new();
    private readonly TextBlock LoadoutWeaponStatsText = new();
    private readonly ComboBox LoadoutEquipSlotCombo = new();
    private readonly Button LoadoutEquipButton = new();
    private readonly TextBlock LoadoutStatusText = new();

    private readonly ScrollViewer CaImportView = new();
    private readonly ProgressBar CaImportProgressBar = new();
    private readonly TextBlock CaImportStatusText = new();
    private readonly TextBox CaImportLogBox = new();
    private readonly Button CaImportRunButton = new();
    private readonly Button CaAttachmentImportButton = new();
    private readonly TextBlock CaWeaponsSourceText = new();
    private readonly TextBlock CaGunsSourceText = new();
    private readonly TextBlock CaAttachmentsSourceText = new();
    private readonly TextBlock CaMapSourceText = new();
    private readonly Button CaMapImportButton = new();
    private string? _caWeaponsPath;
    private string? _caGunsPath;
    private string? _caAttachmentsPath;
    private string? _caMapPath;

    private readonly ScrollViewer MiniToolsView = new();
    private readonly TextBlock MiniToolsStatusText = new();

    private readonly ScrollViewer ArsenalView = new();
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
    private readonly TextBox AnimSelectBox = new();
    private readonly TextBox AnimIdleBox = new();
    private readonly TextBox AnimFireBox = new();
    private readonly TextBox AnimAltFireBox = new();
    private readonly TextBox AnimReloadBox = new();
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

        WeaponTypeCombo.ItemsSource = new string[]
        {
            "hitscan",
            "melee",
            "grenade",
            "rocket"
        };

        ResolutionCombo.ItemsSource = Resolutions;

        RefreshMaps();

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
            AppWindow.Resize(new SizeInt32(1480, 900));
        }
        catch
        {
        }
    }

    private void ShowView(string tag)
    {
        HomeView.Visibility = tag == "home" ? Visibility.Visible : Visibility.Collapsed;
        ServerBrowserView.Visibility = tag == "servers" ? Visibility.Visible : Visibility.Collapsed;
        LoadoutView.Visibility = tag == "loadout" ? Visibility.Visible : Visibility.Collapsed;
        PlayerGearView.Visibility = tag == "player-gear" ? Visibility.Visible : Visibility.Collapsed;
        WeaponModsView.Visibility = tag == "weapon-mods" ? Visibility.Visible : Visibility.Collapsed;
        CaImportView.Visibility = tag == "ca-importer" ? Visibility.Visible : Visibility.Collapsed;
        MiniToolsView.Visibility = tag == "mini-tools" ? Visibility.Visible : Visibility.Collapsed;
        ArsenalView.Visibility = tag == "weapon-editor" ? Visibility.Visible : Visibility.Collapsed;
        ModsView.Visibility = tag == "tools" ? Visibility.Visible : Visibility.Collapsed;
        PlayerProfileView.Visibility = tag == "profile" ? Visibility.Visible : Visibility.Collapsed;
        SettingsView.Visibility = tag == "settings" ? Visibility.Visible : Visibility.Collapsed;

        AppSectionTitleText.Text = tag switch
        {
            "loadout" => "ARMORY",
            "player-gear" => "PLAYER GEAR",
            "weapon-mods" => "WEAPON MODS",
            "weapon-editor" => "WEAPON CATALOG",
            "ca-importer" => "COMBAT ARMS IMPORTER",
            "mini-tools" => "MINI TOOLS",
            "tools" => "MODS & CONTENT TOOLS",
            "settings" => "SETTINGS",
            "profile" => "PLAYER PROFILE",
            "servers" => "SERVER BROWSER",
            _ => "READY ROOM"
        };
    }

    private void LoadProfile()
    {
        var profile = App.Instance.Services.Settings.LoadProfile();

        PlayerNameBox.Text = profile.PlayerName;
        JoinIpBox.Text = profile.JoinIp;
        CommandsBox.Text = profile.CustomCommands;

        SelectString(ModeCombo, profile.Mode, "Single Player");

        var map =
            (profile.Map ?? "CABINFEVER")
                .ToUpperInvariant();

        if(MapCombo.ItemsSource is IEnumerable<string> maps &&
           maps.Contains(
               map,
               StringComparer.OrdinalIgnoreCase))
        {
            MapCombo.SelectedItem =
                maps.First(
                    item =>
                        item.Equals(
                            map,
                            StringComparison.OrdinalIgnoreCase));
        }
        else
        {
            MapCombo.SelectedItem =
                "CABINFEVER";
        }

        DifficultySlider.Value =
            ParseDifficultyLevel(
                profile.Difficulty);
        UpdateDifficultyLabel();

        RefreshModeVisibility();

        var game = LauncherPaths.FindGameDirectory();
        PlayButton.IsEnabled = game is not null;
        RunStatusText.Text = game is null
            ? "FIRETEAM runtime not found. Run build.cmd first."
            : $"FIRETEAM runtime ready. {(MapCombo.SelectedItem?.ToString() ?? "CABINFEVER")} is selected.";
    }

    private void RefreshMaps()
    {
        var maps =
            new List<string>
            {
                "CABINFEVER"
            };

        var game =
            LauncherPaths.FindGameDirectory();

        if(game is not null)
        {
            var worlds =
                Path.Combine(
                    game,
                    "rez",
                    "Worlds");

            if(Directory.Exists(worlds))
            {
                maps.AddRange(
                    Directory
                        .EnumerateFiles(
                            worlds,
                            "*.DAT",
                            SearchOption.TopDirectoryOnly)
                        .Select(
                            path =>
                                Path
                                    .GetFileNameWithoutExtension(
                                        path)
                                    .ToUpperInvariant())
                        .Where(
                            name =>
                                !name.Equals(
                                    "WORLD",
                                    StringComparison.OrdinalIgnoreCase) &&
                                !name.Equals(
                                    "JUNK_FLEA",
                                    StringComparison.OrdinalIgnoreCase) &&
                                !name.Equals(
                                    "JUNKFLEA",
                                    StringComparison.OrdinalIgnoreCase)));
            }
        }

        MapCombo.ItemsSource =
            maps
                .Distinct(
                    StringComparer.OrdinalIgnoreCase)
                .OrderBy(
                    value =>
                        value.Equals(
                            "CABINFEVER",
                            StringComparison.OrdinalIgnoreCase)
                            ? 0
                            : 1)
                .ThenBy(
                    value =>
                        value)
                .ToArray();

        if(MapCombo.SelectedItem is null)
        {
            MapCombo.SelectedItem =
                "CABINFEVER";
        }
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

    private void ReloadWeapons(
        bool force = false)
    {
        try
        {
            var all =
                App.Instance.Services.Weapons.Load(
                    force);

            var active =
                all
                    .Where(
                        weapon =>
                            weapon.IsActiveSlot)
                    .OrderBy(
                        weapon =>
                            weapon.Section)
                    .ToList();

            _arsenal = all;
            _choices =
                App.Instance.Services.Weapons.GetArsenalChoices(
                    all);

            RefreshHomeLoadoutSummary(
                active);

            _loadoutPresets =
                App.Instance.Services.Loadouts.Load();

            LoadoutPresetList.ItemsSource =
                null;
            LoadoutPresetList.ItemsSource =
                _loadoutPresets;

            var selectedIndex =
                App.Instance.Services.Loadouts.GetSelectedIndex();

            var preset =
                _loadoutPresets.FirstOrDefault(
                    item =>
                        item.Index == selectedIndex) ??
                _loadoutPresets.FirstOrDefault();

            if(preset is not null)
            {
                _selectedLoadoutPresetIndex =
                    preset.Index;

                LoadoutPresetList.SelectedItem =
                    preset;

                LoadPresetIntoDraft(
                    preset,
                    active);
            }

            if(LoadoutView.Visibility ==
               Visibility.Visible)
            {
                ApplyLoadoutInventoryFilter();
            }

            if(ArsenalView.Visibility ==
               Visibility.Visible)
            {
                ApplyArsenalFilter();
            }

            var catalogCount =
                all.Count(
                    weapon =>
                        weapon.Section.StartsWith(
                            "catalog.",
                            StringComparison.OrdinalIgnoreCase));

            LoadoutStatusText.Text =
                $"{_choices.Count} enabled weapons available • " +
                $"{catalogCount} catalog definitions installed.";
        }
        catch(Exception ex)
        {
            LoadoutStatusText.Text =
                "Loadout error: " +
                ex.Message;

            ArsenalStatusText.Text =
                "Arsenal error: " +
                ex.Message;
        }
    }

    private void ReloadArsenal(
        bool force = false)
    {
        try
        {
            var all = App.Instance.Services.Weapons.Load(force);
            _arsenal = all;
            _choices = App.Instance.Services.Weapons.GetArsenalChoices(all);
            ApplyArsenalFilter();
        }
        catch(Exception ex)
        {
            ArsenalStatusText.Text = "Arsenal error: " + ex.Message;
        }
    }

    private void RefreshHomeLoadoutSummary(
        IReadOnlyList<WeaponDefinition>? active = null)
    {
        try
        {
            active ??=
                App.Instance.Services.Weapons.Load()
                    .Where(
                        weapon =>
                            weapon.IsActiveSlot)
                    .OrderBy(
                        weapon =>
                            weapon.Section)
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
                "BACKPACK 1  •  ",
                "BACKPACK 2  •  "
            };

            for(var i = 0;
                i < labels.Length;
                ++i)
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
            PrimarySummaryText.Text =
                "LOADOUT ERROR  •  " +
                ex.Message;
        }
    }

    private void LoadoutPresetList_SelectionChanged(
        object sender,
        SelectionChangedEventArgs e)
    {
        if(LoadoutPresetList.SelectedItem is not LoadoutPreset preset)
        {
            return;
        }

        _selectedLoadoutPresetIndex =
            preset.Index;

        var active =
            _arsenal
                .Where(
                    weapon =>
                        weapon.IsActiveSlot)
                .OrderBy(
                    weapon =>
                        weapon.Section)
                .ToList();

        LoadPresetIntoDraft(
            preset,
            active);

        LoadoutStatusText.Text =
            $"Editing {preset.Name}. Changes stay in the draft until you save or activate it.";
    }

    private void LoadPresetIntoDraft(
        LoadoutPreset preset,
        IReadOnlyList<WeaponDefinition> active)
    {
        LoadoutPresetNameBox.Text =
            preset.Name;

        var used =
            new List<WeaponDefinition>();

        for(var slot = 0;
            slot < _draftLoadout.Length;
            ++slot)
        {
            var id =
                slot < preset.WeaponIds.Count
                ? preset.WeaponIds[slot]
                : string.Empty;

            WeaponDefinition? weapon =
                null;

            if(!string.IsNullOrWhiteSpace(
                id))
            {
                weapon =
                    _arsenal.FirstOrDefault(
                        candidate =>
                            !candidate.IsActiveSlot &&
                            candidate.Id.Equals(
                                id,
                                StringComparison.OrdinalIgnoreCase)) ??
                    _arsenal.FirstOrDefault(
                        candidate =>
                            candidate.Id.Equals(
                                id,
                                StringComparison.OrdinalIgnoreCase));
            }

            if(weapon is not null &&
               (!CategoryAllowedForSlot(
                    weapon.LoadoutCategory,
                    slot) ||
                used.Any(
                    existing =>
                        SameWeapon(
                            existing,
                            weapon))))
            {
                weapon = null;
            }

            if(weapon is null)
            {
                // Old Gear Tabs can predate the current role rules. Repair
                // them in-memory instead of allowing impossible combinations
                // such as a Sickle in SIDEARM or the same gun in two slots.
                weapon =
                    _choices.FirstOrDefault(
                        candidate =>
                            CategoryAllowedForSlot(
                                candidate.LoadoutCategory,
                                slot) &&
                            HasUsablePlayerView(
                                candidate) &&
                            !used.Any(
                                existing =>
                                    SameWeapon(
                                        existing,
                                        candidate))) ??
                    active.FirstOrDefault(
                        candidate =>
                            CategoryAllowedForSlot(
                                candidate.LoadoutCategory,
                                slot) &&
                            !used.Any(
                                existing =>
                                    SameWeapon(
                                        existing,
                                        candidate))) ??
                    _choices.FirstOrDefault(
                        candidate =>
                            CategoryAllowedForSlot(
                                candidate.LoadoutCategory,
                                slot) &&
                            !used.Any(
                                existing =>
                                    SameWeapon(
                                        existing,
                                        candidate)));
            }

            _draftLoadout[slot] =
                weapon;

            if(weapon is not null)
            {
                used.Add(
                    weapon);
            }
        }

        RefreshLoadoutSlotButtons();
        SelectLoadoutSlot(
            _selectedLoadoutSlot);
    }

    private void RefreshLoadoutSlotButtons()
    {
        var buttons =
            LoadoutSlotButtons();

        var labels =
            new[]
            {
                "PRIMARY",
                "SIDEARM",
                "MELEE",
                "BACKPACK 1",
                "BACKPACK 2"
            };

        for(var slot = 0;
            slot < buttons.Count;
            ++slot)
        {
            var weapon =
                _draftLoadout[slot];

            buttons[slot].Content =
                labels[slot] +
                "\n" +
                (weapon?.Name ??
                 "EMPTY");

            buttons[slot].BorderBrush =
                slot == _selectedLoadoutSlot
                    ? CyanBrush
                    : DividerBrush;

            buttons[slot].BorderThickness =
                slot == _selectedLoadoutSlot
                    ? new Thickness(2)
                    : new Thickness(1);
        }
    }

    private List<Button> LoadoutSlotButtons() =>
    [
        LoadoutSlot1Button,
        LoadoutSlot2Button,
        LoadoutSlot3Button,
        LoadoutSlot4Button,
        LoadoutSlot5Button
    ];

    private void LoadoutSlotButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        if(sender is Button button &&
           button.Tag is int slot)
        {
            SelectLoadoutSlot(
                slot);
        }
    }

    private void SelectLoadoutSlot(
        int slot)
    {
        _selectedLoadoutSlot =
            Math.Clamp(
                slot,
                0,
                4);

        var allowed =
            AllowedCategoriesForSlot(
                _selectedLoadoutSlot);

        foreach(var pair in _loadoutCategoryButtons)
        {
            pair.Value.IsEnabled =
                allowed.Contains(
                    pair.Key,
                    StringComparer.OrdinalIgnoreCase);
        }

        var equippedCategory =
            _draftLoadout[_selectedLoadoutSlot]?
                .LoadoutCategory;

        if(!string.IsNullOrWhiteSpace(
               equippedCategory) &&
           allowed.Contains(
               equippedCategory,
               StringComparer.OrdinalIgnoreCase))
        {
            _loadoutCategory =
                equippedCategory;
        }
        else if(!allowed.Contains(
                    _loadoutCategory,
                    StringComparer.OrdinalIgnoreCase))
        {
            _loadoutCategory =
                allowed[0];
        }

        RefreshLoadoutSlotButtons();
        ApplyLoadoutInventoryFilter();
    }

    private static IReadOnlyList<string> AllowedCategoriesForSlot(
        int slot) =>
        slot switch
        {
            0 =>
            [
                "AR",
                "SR",
                "Launcher",
                "MG",
                "SG",
                "SMG"
            ],
            1 =>
            [
                "Pistol"
            ],
            2 =>
            [
                "Melee"
            ],
            _ =>
            [
                "AR",
                "SR",
                "Launcher",
                "Melee",
                "MG",
                "Pistol",
                "SG",
                "SMG",
                "Throwing"
            ]
        };

    private static bool CategoryAllowedForSlot(
        string category,
        int slot) =>
        AllowedCategoriesForSlot(
            slot)
            .Contains(
                category,
                StringComparer.OrdinalIgnoreCase);

    private static bool HasUsablePlayerView(
        WeaponDefinition weapon)
    {
        if(weapon.Type.Equals(
               "melee",
               StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }

        return weapon.Values.TryGetValue(
                   "pv_model",
                   out var model) &&
               !string.IsNullOrWhiteSpace(
                   model);
    }

    private void SetLoadoutCategory(
        string category)
    {
        _loadoutCategory =
            category;

        ApplyLoadoutInventoryFilter();
    }

    private void LoadoutInventorySearchBox_TextChanged(
        object sender,
        TextChangedEventArgs e)
    {
        ApplyLoadoutInventoryFilter();
    }

    private void ApplyLoadoutInventoryFilter()
    {
        var query =
            LoadoutInventorySearchBox.Text?
                .Trim() ??
            string.Empty;

        var visible =
            _choices
                .Where(
                    weapon =>
                        weapon.LoadoutCategory.Equals(
                            _loadoutCategory,
                            StringComparison.OrdinalIgnoreCase) &&
                        (query.Length == 0 ||
                         weapon.Name.Contains(
                             query,
                             StringComparison.OrdinalIgnoreCase) ||
                         weapon.Id.Contains(
                             query,
                             StringComparison.OrdinalIgnoreCase)))
                .OrderBy(
                    weapon =>
                        weapon.Name)
                .ToList();

        LoadoutInventoryList.ItemsSource =
            visible;

        LoadoutCategoryStatusText.Text =
            $"{_loadoutCategory.ToUpperInvariant()}  •  {visible.Count} ENABLED";

        if(visible.Count > 0)
        {
            LoadoutInventoryList.SelectedIndex =
                0;
        }
        else
        {
            _selectedLoadoutWeapon =
                null;

            LoadoutWeaponTitleText.Text =
                "No enabled weapons";

            LoadoutWeaponMetaText.Text =
                "Enable supported weapons in Tools → Weapon Editor.";

            LoadoutWeaponStatsText.Text =
                string.Empty;

            LoadoutEquipSlotCombo.ItemsSource =
                null;

            LoadoutEquipButton.IsEnabled =
                false;
        }
    }

    private void LoadoutInventoryList_SelectionChanged(
        object sender,
        SelectionChangedEventArgs e)
    {
        if(LoadoutInventoryList.SelectedItem is not WeaponDefinition weapon)
        {
            return;
        }

        _selectedLoadoutWeapon =
            weapon;

        LoadoutWeaponTitleText.Text =
            weapon.Name;

        var timing =
            weapon.Values.TryGetValue(
                "ca_timing_verified",
                out var verified) &&
            verified == "1"
                ? "TIMING VERIFIED"
                : "DEV TIMING";

        var validation =
            weapon.Supported
                ? "SUPPORTED"
                : "MANUAL TEST";

        LoadoutWeaponMetaText.Text =
            $"{weapon.LoadoutCategory.ToUpperInvariant()}  •  {validation}  •  {timing}  •  {weapon.Source}";

        LoadoutWeaponStatsText.Text =
            "DAMAGE        " +
            WeaponValue(
                weapon,
                "damage") +
            "\nMAGAZINE      " +
            WeaponValue(
                weapon,
                "clip") +
            "\nRESERVE       " +
            WeaponValue(
                weapon,
                "reserve") +
            "\nRANGE         " +
            WeaponValue(
                weapon,
                "range") +
            "\nFIRE INTERVAL " +
            WeaponValue(
                weapon,
                "fire_interval") +
            " s\nRELOAD        " +
            WeaponValue(
                weapon,
                "reload") +
            " s";

        var equippedSlot =
            Array.FindIndex(
                _draftLoadout,
                candidate =>
                    candidate is not null &&
                    SameWeapon(
                        candidate,
                        weapon));

        var canEquipHere =
            CategoryAllowedForSlot(
                weapon.LoadoutCategory,
                _selectedLoadoutSlot);

        var selectedSlotLabel =
            LoadoutSlotLabel(
                _selectedLoadoutSlot);

        if(canEquipHere)
        {
            LoadoutEquipSlotCombo.ItemsSource =
                new[]
                {
                    selectedSlotLabel
                };

            LoadoutEquipSlotCombo.SelectedIndex =
                0;

            if(equippedSlot ==
               _selectedLoadoutSlot)
            {
                LoadoutEquipButton.Content =
                    "EQUIPPED";
                LoadoutEquipButton.IsEnabled =
                    false;
            }
            else
            {
                LoadoutEquipButton.Content =
                    equippedSlot >= 0
                    ? "MOVE WEAPON"
                    : "EQUIP WEAPON";

                LoadoutEquipButton.IsEnabled =
                    true;
            }

            return;
        }

        if(equippedSlot >= 0)
        {
            LoadoutEquipSlotCombo.ItemsSource =
                new[]
                {
                    LoadoutSlotLabel(
                        equippedSlot)
                };
            LoadoutEquipSlotCombo.SelectedIndex =
                0;
            LoadoutEquipButton.Content =
                "EQUIPPED";
        }
        else
        {
            LoadoutEquipSlotCombo.ItemsSource =
                Array.Empty<string>();
            LoadoutEquipSlotCombo.SelectedIndex =
                -1;
            LoadoutEquipButton.Content =
                "NOT VALID FOR SLOT";
        }

        LoadoutEquipButton.IsEnabled =
            false;
    }

    private static bool SameWeapon(
        WeaponDefinition left,
        WeaponDefinition right)
    {
        var leftKey =
            string.IsNullOrWhiteSpace(
                left.Id)
            ? left.Name
            : left.Id;

        var rightKey =
            string.IsNullOrWhiteSpace(
                right.Id)
            ? right.Name
            : right.Id;

        return leftKey.Equals(
            rightKey,
            StringComparison.OrdinalIgnoreCase);
    }

    private static string LoadoutSlotLabel(
        int slot) =>
        slot switch
        {
            0 => "PRIMARY",
            1 => "SIDEARM",
            2 => "MELEE",
            3 => "BACKPACK 1",
            4 => "BACKPACK 2",
            _ => "UNKNOWN"
        };

    private static string WeaponValue(
        WeaponDefinition weapon,
        string key,
        string fallback = "—") =>
        weapon.Values.TryGetValue(
            key,
            out var value) &&
        !string.IsNullOrWhiteSpace(
            value)
            ? value
            : fallback;

    private static IReadOnlyList<string> AllowedLoadoutSlots(
        string category)
    {
        if(category.Equals(
            "Pistol",
            StringComparison.OrdinalIgnoreCase))
        {
            return
            [
                "SIDEARM",
                "BACKPACK 1",
                "BACKPACK 2"
            ];
        }

        if(category.Equals(
            "Melee",
            StringComparison.OrdinalIgnoreCase))
        {
            return
            [
                "MELEE",
                "BACKPACK 1",
                "BACKPACK 2"
            ];
        }

        if(category.Equals(
            "Throwing",
            StringComparison.OrdinalIgnoreCase))
        {
            return
            [
                "BACKPACK 1",
                "BACKPACK 2"
            ];
        }

        return
        [
            "PRIMARY",
            "BACKPACK 1",
            "BACKPACK 2"
        ];
    }

    private void LoadoutEquipButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        if(_selectedLoadoutWeapon is null ||
           LoadoutEquipSlotCombo.SelectedItem is not string slotName)
        {
            return;
        }

        var slot =
            slotName switch
            {
                "PRIMARY" => 0,
                "SIDEARM" => 1,
                "MELEE" => 2,
                "BACKPACK 1" => 3,
                "BACKPACK 2" => 4,
                _ => -1
            };

        if(slot < 0)
        {
            return;
        }

        for(var index = 0;
            index < _draftLoadout.Length;
            ++index)
        {
            if(index != slot &&
               _draftLoadout[index] is not null &&
               SameWeapon(
                   _draftLoadout[index]!,
                   _selectedLoadoutWeapon))
            {
                _draftLoadout[index] =
                    null;
            }
        }

        _draftLoadout[slot] =
            _selectedLoadoutWeapon;

        RefreshLoadoutSlotButtons();

        LoadoutEquipButton.Content =
            "EQUIPPED";
        LoadoutEquipButton.IsEnabled =
            false;

        LoadoutStatusText.Text =
            $"Equipped {_selectedLoadoutWeapon.Name} to {slotName}. Save the preset or activate it for FIRETEAM.";
    }

    private void SaveLoadoutPresetButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        try
        {
            var selections =
                GetDraftLoadout();

            App.Instance.Services.Loadouts.SavePreset(
                _selectedLoadoutPresetIndex,
                LoadoutPresetNameBox.Text,
                selections);

            RefreshLoadoutPresetList(
                _selectedLoadoutPresetIndex);

            LoadoutStatusText.Text =
                $"Saved {LoadoutPresetNameBox.Text.Trim()} without changing the active FIRETEAM loadout.";
        }
        catch(Exception ex)
        {
            LoadoutStatusText.Text =
                "Could not save loadout: " +
                ex.Message;
        }
    }

    private void ApplyLoadoutButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        try
        {
            var selections =
                GetDraftLoadout();

            ValidateDraftLoadout(
                selections);

            App.Instance.Services.Loadouts.SavePreset(
                _selectedLoadoutPresetIndex,
                LoadoutPresetNameBox.Text,
                selections);

            App.Instance.Services.Loadouts.SetSelectedIndex(
                _selectedLoadoutPresetIndex);

            App.Instance.Services.Weapons.SaveLoadout(
                selections);

            RefreshHomeLoadoutSummary(
                selections);

            RefreshLoadoutPresetList(
                _selectedLoadoutPresetIndex);

            LoadoutStatusText.Text =
                $"{LoadoutPresetNameBox.Text.Trim()} is now the active FIRETEAM loadout.";
        }
        catch(Exception ex)
        {
            LoadoutStatusText.Text =
                "Could not activate loadout: " +
                ex.Message;
        }
    }

    private IReadOnlyList<WeaponDefinition> GetDraftLoadout()
    {
        var result =
            new List<WeaponDefinition>(
                5);

        for(var slot = 0;
            slot < _draftLoadout.Length;
            ++slot)
        {
            var weapon =
                _draftLoadout[slot];

            if(weapon is null)
            {
                throw new InvalidOperationException(
                    $"Loadout slot {slot + 1} is empty.");
            }

            result.Add(
                weapon);
        }

        return result;
    }

    private static void ValidateDraftLoadout(
        IReadOnlyList<WeaponDefinition> selections)
    {
        if(!IsMainCategory(
            selections[0].LoadoutCategory))
        {
            throw new InvalidOperationException(
                "PRIMARY must be an AR, SR, Launcher, MG, SG or SMG.");
        }

        if(!selections[1].LoadoutCategory.Equals(
            "Pistol",
            StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                "SIDEARM must be a pistol.");
        }

        if(!selections[2].LoadoutCategory.Equals(
            "Melee",
            StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException(
                "MELEE must be a melee weapon.");
        }

        foreach(var weapon in selections)
        {
            if(!weapon.Enabled &&
               !weapon.IsActiveSlot)
            {
                throw new InvalidOperationException(
                    $"{weapon.Name} is disabled.");
            }
        }
    }

    private static bool IsMainCategory(
        string category) =>
        category.Equals(
            "AR",
            StringComparison.OrdinalIgnoreCase) ||
        category.Equals(
            "SR",
            StringComparison.OrdinalIgnoreCase) ||
        category.Equals(
            "Launcher",
            StringComparison.OrdinalIgnoreCase) ||
        category.Equals(
            "MG",
            StringComparison.OrdinalIgnoreCase) ||
        category.Equals(
            "SG",
            StringComparison.OrdinalIgnoreCase) ||
        category.Equals(
            "SMG",
            StringComparison.OrdinalIgnoreCase);

    private void RefreshLoadoutPresetList(
        int selectedIndex)
    {
        _loadoutPresets =
            App.Instance.Services.Loadouts.Load();

        LoadoutPresetList.ItemsSource =
            null;

        LoadoutPresetList.ItemsSource =
            _loadoutPresets;

        var preset =
            _loadoutPresets.FirstOrDefault(
                item =>
                    item.Index == selectedIndex);

        if(preset is not null)
        {
            LoadoutPresetList.SelectedItem =
                preset;
        }
    }

    private void ModeCombo_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        RefreshModeVisibility();
    }

    private string DifficultyProfileValue()
    {
        var level =
            (int)Math.Round(
                DifficultySlider.Value);

        return Math.Clamp(
            level,
            0,
            10).ToString(
                System.Globalization.CultureInfo.InvariantCulture);
    }

    private static int ParseDifficultyLevel(
        string? value)
    {
        if(int.TryParse(
               value,
               out var level))
        {
            return Math.Clamp(
                level,
                0,
                10);
        }

        return value?.Trim().ToLowerInvariant() switch
        {
            "easy" => 2,
            "normal" => 4,
            "medium" => 5,
            "hard" => 6,
            "expert" => 7,
            "extreme" => 8,
            "nightmare" => 10,
            _ => 4
        };
    }

    private static string DifficultyName(
        int level) =>
        level switch
        {
            0 => "TRAINING",
            1 => "VERY EASY",
            2 => "EASY",
            3 => "STANDARD",
            4 => "NORMAL",
            5 => "MEDIUM",
            6 => "HARD",
            7 => "EXPERT",
            8 => "EXTREME",
            9 => "NIGHTMARE",
            10 => "NIGHTMARE++",
            _ => "NORMAL"
        };

    private void UpdateDifficultyLabel()
    {
        var level =
            Math.Clamp(
                (int)Math.Round(
                    DifficultySlider.Value),
                0,
                10);

        DifficultyValueText.Text =
            $"{level}  •  {DifficultyName(level)}";
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

    private void WeaponSearchBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        ApplyArsenalFilter();
    }

    private void ApplyArsenalFilter()
    {
        const int RenderLimit = 100;

        var query =
            WeaponSearchBox.Text?
                .Trim() ??
            string.Empty;

        var matches =
            _arsenal
                .Where(w =>
                    query.Length == 0 ||
                    w.Name.Contains(
                        query,
                        StringComparison.OrdinalIgnoreCase) ||
                    w.Id.Contains(
                        query,
                        StringComparison.OrdinalIgnoreCase) ||
                    w.Type.Contains(
                        query,
                        StringComparison.OrdinalIgnoreCase) ||
                    w.LoadoutCategory.Contains(
                        query,
                        StringComparison.OrdinalIgnoreCase))
                .OrderByDescending(w =>
                    w.IsActiveSlot)
                .ThenByDescending(w =>
                    w.Enabled)
                .ThenBy(w =>
                    w.Name)
                .ToList();

        var visible =
            matches
                .Take(
                    RenderLimit)
                .ToList();

        WeaponList.ItemsSource =
            visible;

        if(matches.Count > RenderLimit)
        {
            ArsenalStatusText.Text =
                $"Showing first {RenderLimit} of {matches.Count} definitions. Search by name, ID or category to narrow the catalog.";
        }

        if(visible.Count > 0)
        {
            WeaponList.SelectedIndex =
                0;
        }
        else
        {
            _selectedWeapon = null;
            WeaponTitleText.Text =
                "No matching weapons";
            WeaponSectionText.Text =
                string.Empty;
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
            $"{weapon.Section}  •  {weapon.Id}  •  " +
            $"{(weapon.Supported ? "SUPPORTED" : "UNVERIFIED")}  •  " +
            weapon.Source;

        WeaponNameBox.Text = weapon.Name;
        WeaponIdBox.Text = weapon.Id;
        WeaponTypeCombo.SelectedItem = weapon.Type;

        SetWeaponField(DamageBox, weapon, "damage");
        SetWeaponField(ClipBox, weapon, "clip");
        SetWeaponField(ReserveBox, weapon, "reserve");
        SetWeaponField(FireIntervalBox, weapon, "fire_interval");
        SetWeaponField(ReloadBox, weapon, "reload");
        SetWeaponField(AnimSelectBox, weapon, "anim_select");
        SetWeaponField(AnimIdleBox, weapon, "anim_idle");
        SetWeaponField(AnimFireBox, weapon, "anim_fire");
        SetWeaponField(AnimAltFireBox, weapon, "anim_alt_fire");
        SetWeaponField(AnimReloadBox, weapon, "anim_reload");
        WeaponEnabledCheckBox.IsChecked =
            weapon.Enabled || weapon.IsActiveSlot;
        WeaponEnabledCheckBox.IsEnabled =
            !weapon.IsActiveSlot;
    }

    private void SaveWeaponButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        if(_selectedWeapon is null)
        {
            ArsenalStatusText.Text =
                "Select a weapon first.";
            return;
        }

        try
        {
            var values =
                new Dictionary<string, string>(
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
            Put(values, "anim_select", AnimSelectBox.Text);
            Put(values, "anim_idle", AnimIdleBox.Text);
            Put(values, "anim_fire", AnimFireBox.Text);
            Put(values, "anim_alt_fire", AnimAltFireBox.Text);
            Put(values, "anim_reload", AnimReloadBox.Text);

            var enabled =
                _selectedWeapon.IsActiveSlot ||
                WeaponEnabledCheckBox.IsChecked == true;

            var definitionChanged =
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "name",
                    WeaponNameBox.Text) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "id",
                    WeaponIdBox.Text) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "type",
                    WeaponTypeCombo.SelectedItem?.ToString()) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "damage",
                    DamageBox.Text) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "clip",
                    ClipBox.Text) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "reserve",
                    ReserveBox.Text) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "fire_interval",
                    FireIntervalBox.Text) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "reload",
                    ReloadBox.Text) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "anim_select",
                    AnimSelectBox.Text) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "anim_idle",
                    AnimIdleBox.Text) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "anim_fire",
                    AnimFireBox.Text) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "anim_alt_fire",
                    AnimAltFireBox.Text) ||
                WeaponEditorValueChanged(
                    _selectedWeapon,
                    "anim_reload",
                    AnimReloadBox.Text);

            var enabledChanged =
                enabled !=
                _selectedWeapon.Enabled;

            if(!definitionChanged &&
               !enabledChanged)
            {
                ArsenalStatusText.Text =
                    "No weapon changes to save.";
                return;
            }

            var updated =
                new WeaponDefinition
                {
                    Section = _selectedWeapon.Section,
                    Id = WeaponIdBox.Text.Trim(),
                    Name = WeaponNameBox.Text.Trim(),
                    Type =
                        WeaponTypeCombo.SelectedItem?.ToString()
                        ?? _selectedWeapon.Type,
                    Enabled = enabled,
                    Supported = _selectedWeapon.Supported,
                    IsActiveSlot = _selectedWeapon.IsActiveSlot,
                    Source = _selectedWeapon.Source,
                    Values = values
                };

            if(definitionChanged)
            {
                App.Instance.Services.Weapons.SaveDefinition(
                    updated);
            }
            else
            {
                // Enable/disable is launcher library state. Keep it in the
                // tiny sidecar instead of rewriting the 65k-line CA catalog.
                App.Instance.Services.Weapons.SetEnabled(
                    updated.Section,
                    updated.Enabled);
            }

            ArsenalStatusText.Text =
                definitionChanged
                ? $"Saved {updated.Name} definition."
                : $"{updated.Name} loadout availability updated.";

            ReloadArsenal(
                definitionChanged);
        }
        catch(Exception ex)
        {
            ArsenalStatusText.Text =
                "Save failed: " +
                ex.Message;
        }
    }

    private static bool WeaponEditorValueChanged(
        WeaponDefinition weapon,
        string key,
        string? value)
    {
        var current =
            weapon.Values.TryGetValue(
                key,
                out var stored)
            ? stored.Trim()
            : string.Empty;

        return !string.Equals(
            current,
            value?.Trim() ??
            string.Empty,
            StringComparison.Ordinal);
    }

    private void ReloadArsenalButton_Click(object sender, RoutedEventArgs e)
    {
        ReloadArsenal(
            true);
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

    private async void SelectCaWeaponsSourceButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        var path =
            await PickLauncherFileAsync(
                ".txt");

        if(path is null)
        {
            return;
        }

        _caWeaponsPath =
            path;

        CaWeaponsSourceText.Text =
            Path.GetFileName(
                path);

        UpdateCaImportReadyState();
    }

    private async void SelectCaGunsSourceButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        var path =
            await PickLauncherFileAsync(
                ".zip");

        if(path is null)
        {
            return;
        }

        _caGunsPath =
            path;

        CaGunsSourceText.Text =
            Path.GetFileName(
                path);

        UpdateCaImportReadyState();
    }

    private async void SelectCaAttachmentsSourceButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        var path =
            await PickLauncherFileAsync(
                ".zip");

        if(path is null)
        {
            return;
        }

        _caAttachmentsPath =
            path;

        CaAttachmentsSourceText.Text =
            Path.GetFileName(
                path);

        UpdateCaImportReadyState();
    }

    private async void SelectCaMapSourceButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        var path =
            await PickLauncherFileAsync(
                ".dat");

        if(path is null)
        {
            return;
        }

        _caMapPath =
            path;

        CaMapSourceText.Text =
            Path.GetFileName(
                path);

        UpdateCaImportReadyState();
    }

    private void UpdateCaImportReadyState()
    {
        CaImportRunButton.IsEnabled =
            !string.IsNullOrWhiteSpace(
                _caWeaponsPath) &&
            !string.IsNullOrWhiteSpace(
                _caGunsPath);

        CaAttachmentImportButton.IsEnabled =
            !string.IsNullOrWhiteSpace(
                _caAttachmentsPath);

        CaMapImportButton.IsEnabled =
            !string.IsNullOrWhiteSpace(
                _caMapPath);

        if(CaImportRunButton.IsEnabled)
        {
            CaImportStatusText.Text =
                "Weapon sources ready. START WEAPON IMPORT will audit the selected WEAPONS.txt against Guns.zip.";
        }
        else if(CaAttachmentImportButton.IsEnabled)
        {
            CaImportStatusText.Text =
                "Attachment archive ready. IMPORT ATTACHMENTS will extract it and rebuild config/attachments.cfg.";
        }
        else if(CaMapImportButton.IsEnabled)
        {
            CaImportStatusText.Text =
                "Map source ready. IMPORT MAP will copy the DAT into local map storage and stage it when possible.";
        }
        else
        {
            CaImportStatusText.Text =
                "Select the Combat Arms source files you want to work with.";
        }
    }

    private async void ImportCombatArmsButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        if(string.IsNullOrWhiteSpace(
               _caWeaponsPath) ||
           string.IsNullOrWhiteSpace(
               _caGunsPath))
        {
            CaImportStatusText.Text =
                "Select WEAPONS.txt and Guns.zip first.";
            return;
        }

        try
        {
            CaImportRunButton.IsEnabled = false;
            CaAttachmentImportButton.IsEnabled = false;
            CaImportProgressBar.Value = 0;
            CaImportLogBox.Text = string.Empty;

            AppendImportLog(
                $"WEAPONS: {_caWeaponsPath}");
            AppendImportLog(
                $"GUNS: {_caGunsPath}");
            AppendImportLog(
                "Archive-first weapon audit started.");

            var progress =
                new Progress<WeaponImportProgress>(
                    update =>
                    {
                        CaImportProgressBar.Value =
                            update.Percent;
                        CaImportStatusText.Text =
                            $"{update.Stage}  •  {update.Processed}/{update.Total}";
                        AppendImportLog(
                            $"[{update.Stage}] {update.Detail}");
                    });

            var result =
                await Task.Run(
                    () =>
                        App.Instance.Services.WeaponImports.Import(
                            _caWeaponsPath,
                            _caGunsPath,
                            progress));

            App.Instance.Services.Weapons.Invalidate();
            ReloadWeapons(true);
            CaImportProgressBar.Value = 100;
            CaImportStatusText.Text = result.Summary;
            AppendImportLog(result.Summary);
        }
        catch(Exception ex)
        {
            CaImportStatusText.Text =
                "Combat Arms weapon import failed: " +
                ex.Message;

            AppendImportLog(
                "[ERROR] " +
                ex);
        }
        finally
        {
            UpdateCaImportReadyState();
        }
    }

    private async void ImportCombatArmsAttachmentsButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        if(string.IsNullOrWhiteSpace(
               _caAttachmentsPath))
        {
            CaImportStatusText.Text =
                "Select Attachments.zip first.";
            return;
        }

        try
        {
            CaImportRunButton.IsEnabled = false;
            CaAttachmentImportButton.IsEnabled = false;
            CaImportProgressBar.Value = 0;
            CaImportLogBox.Text = string.Empty;

            AppendImportLog(
                $"ATTACHMENTS: {_caAttachmentsPath}");

            var progress =
                new Progress<AttachmentImportProgress>(
                    update =>
                    {
                        CaImportProgressBar.Value =
                            update.Percent;
                        CaImportStatusText.Text =
                            $"{update.Stage}  •  {update.Processed}/{update.Total}";
                        AppendImportLog(
                            $"[{update.Stage}] {update.Detail}");
                    });

            var result =
                await Task.Run(
                    () =>
                        App.Instance.Services.AttachmentImports.Import(
                            _caAttachmentsPath,
                            progress));

            CaImportProgressBar.Value = 100;
            CaImportStatusText.Text =
                result.Summary;

            AppendImportLog(
                result.Summary);
        }
        catch(Exception ex)
        {
            CaImportStatusText.Text =
                "Combat Arms attachment import failed: " +
                ex.Message;

            AppendImportLog(
                "[ERROR] " +
                ex);
        }
        finally
        {
            UpdateCaImportReadyState();
        }
    }

    private async void ImportCombatArmsMapButton_Click(
        object sender,
        RoutedEventArgs e)
    {
        if(string.IsNullOrWhiteSpace(
               _caMapPath))
        {
            CaImportStatusText.Text =
                "Select a .DAT map first.";
            return;
        }

        try
        {
            CaMapImportButton.IsEnabled =
                false;

            CaImportStatusText.Text =
                "Importing map and refreshing local dependencies...";

            var result =
                await Task.Run(
                    () =>
                        App.Instance.Services.MapImports.Import(
                            _caMapPath));

            RefreshMaps();

            var selected =
                (MapCombo.ItemsSource as IEnumerable<string>)?
                    .FirstOrDefault(
                        item =>
                            item.Equals(
                                result.MapName,
                                StringComparison.OrdinalIgnoreCase));

            if(selected is not null)
            {
                MapCombo.SelectedItem =
                    selected;
            }

            CaImportStatusText.Text =
                result.Summary;

            AppendImportLog(
                $"MAP: {result.SourcePath}");
            AppendImportLog(
                result.Summary);
        }
        catch(Exception ex)
        {
            CaImportStatusText.Text =
                "Combat Arms map import failed: " +
                ex.Message;

            AppendImportLog(
                "[ERROR] " +
                ex);
        }
        finally
        {
            UpdateCaImportReadyState();
        }
    }

    private void AppendImportLog(string line)
    {
        var current = CaImportLogBox.Text;
        if(current.Length > 24000)
        {
            current = current[^16000..];
        }

        CaImportLogBox.Text = current.Length == 0
            ? line
            : current + Environment.NewLine + line;
        CaImportLogBox.SelectionStart = CaImportLogBox.Text.Length;
    }
    private async Task<string?> PickLauncherFileAsync(
        params string[] extensions)
    {
        var picker =
            new FileOpenPicker
            {
                SuggestedStartLocation =
                    PickerLocationId.DocumentsLibrary
            };

        foreach(var extension in extensions)
        {
            picker.FileTypeFilter.Add(
                extension);
        }

        var hwnd =
            WinRT.Interop.WindowNative.GetWindowHandle(
                this);

        WinRT.Interop.InitializeWithWindow.Initialize(
            picker,
            hwnd);

        var file =
            await picker.PickSingleFileAsync();

        return file?.Path;
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

    private void LaunchRezExtract()
    {
        if(LauncherPaths.LaunchMiniTool(
               "RezExtract.exe"))
        {
            MiniToolsStatusText.Text =
                "RezExtract opened.";
        }
        else
        {
            MiniToolsStatusText.Text =
                "RezExtract.exe was not found. Put it in modTools, Tools, or assets-local/Tools.";
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

    private void LaunchSpectatorQa()
    {
        try
        {
            var customCommands =
                CommandsBox.Text ??
                string.Empty;

            if(!string.IsNullOrWhiteSpace(
                   customCommands))
            {
                customCommands +=
                    " ";
            }

            customCommands +=
                "+devspectator 1";

            var profile =
                new LauncherProfile(
                    string.IsNullOrWhiteSpace(
                        PlayerNameBox.Text)
                        ? "Player"
                        : PlayerNameBox.Text.Trim(),
                    "Single Player",
                    DifficultyProfileValue(),
                    "127.0.0.1",
                    customCommands,
                    MapCombo.SelectedItem?.ToString()
                        ?? "CABINFEVER");

            var settings =
                App.Instance.Services.Settings.LoadSettings();

            RunStatusText.Text =
                App.Instance.Services.Game.Launch(
                    profile,
                    settings) +
                " Spectator QA freecam enabled.";
        }
        catch(Exception ex)
        {
            RunStatusText.Text =
                "Spectator QA launch failed: " +
                ex.Message;
        }
    }

    private void LaunchWeaponQa()
    {
        try
        {
            var customCommands =
                CommandsBox.Text ??
                string.Empty;

            if(!string.IsNullOrWhiteSpace(
                   customCommands))
            {
                customCommands +=
                    " ";
            }

            customCommands +=
                "+devweaponqa 1";

            var profile =
                new LauncherProfile(
                    string.IsNullOrWhiteSpace(
                        PlayerNameBox.Text)
                        ? "Player"
                        : PlayerNameBox.Text.Trim(),
                    "Single Player",
                    DifficultyProfileValue(),
                    "127.0.0.1",
                    customCommands,
                    MapCombo.SelectedItem?.ToString()
                        ?? "CABINFEVER");

            var settings =
                App.Instance.Services.Settings.LoadSettings();

            RunStatusText.Text =
                App.Instance.Services.Game.Launch(
                    profile,
                    settings) +
                " Weapon QA: wheel cycles the full catalog, Q toggles launcher quarantine, and shots are visual/audio only.";
        }
        catch(Exception ex)
        {
            RunStatusText.Text =
                "Weapon QA launch failed: " +
                ex.Message;
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
                DifficultyProfileValue(),
                string.IsNullOrWhiteSpace(JoinIpBox.Text)
                    ? "127.0.0.1"
                    : JoinIpBox.Text.Trim(),
                CommandsBox.Text ?? string.Empty,
                MapCombo.SelectedItem?.ToString()
                    ?? "CABINFEVER");

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
