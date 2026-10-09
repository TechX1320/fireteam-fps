using FireteamLauncher.Infrastructure;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Windows.UI;

namespace FireteamLauncher;

public sealed partial class MainWindow
{
    private static readonly SolidColorBrush BackgroundBrush =
        Brush(16, 17, 18);
    private static readonly SolidColorBrush TitleBarBrush =
        Brush(11, 12, 13);
    private static readonly SolidColorBrush PanelBrush =
        Brush(26, 28, 30);
    private static readonly SolidColorBrush PanelRaisedBrush =
        Brush(34, 37, 41);
    private static readonly SolidColorBrush BorderBrush =
        Brush(52, 56, 61);
    private static readonly SolidColorBrush DividerBrush =
        Brush(74, 78, 83);
    private static readonly SolidColorBrush PrimaryTextBrush =
        Brush(247, 247, 247);
    private static readonly SolidColorBrush SecondaryTextBrush =
        Brush(167, 171, 176);
    private static readonly SolidColorBrush MutedTextBrush =
        Brush(116, 121, 127);
    private static readonly SolidColorBrush AccentBrush =
        Brush(255, 176, 0);
    private static readonly SolidColorBrush AccentRedBrush =
        Brush(240, 90, 90);
    private static readonly SolidColorBrush CyanBrush =
        Brush(48, 189, 231);

    private static SolidColorBrush Brush(
        byte red,
        byte green,
        byte blue) =>
        new(
            Color.FromArgb(
                255,
                red,
                green,
                blue));

    private void BuildUi()
    {
        StartupDiagnostics.Write(
            "MainWindow.BuildUi starting.");

        var root = new Grid
        {
            Background = BackgroundBrush
        };

        root.RowDefinitions.Add(
            new RowDefinition
            {
                Height = new GridLength(48)
            });
        root.RowDefinitions.Add(
            new RowDefinition
            {
                Height = new GridLength(48)
            });
        root.RowDefinitions.Add(
            new RowDefinition
            {
                Height = new GridLength(
                    1,
                    GridUnitType.Star)
            });

        BuildTitleBar(root);
        BuildTopNavigation(root);

        var content = new Grid
        {
            Background = BackgroundBrush
        };

        BuildHomeView();
        BuildServerBrowserView();
        BuildDedicatedServerView();
        BuildLoadoutView();
        BuildPlayerGearView();
        BuildWeaponModsView();
        BuildCaImportView();
        BuildMiniToolsView();
        BuildArsenalView();
        BuildModsView();
        BuildPlayerProfileView();
        BuildSettingsView();

        content.Children.Add(HomeView);
        content.Children.Add(ServerBrowserView);
        content.Children.Add(DedicatedServerView);
        content.Children.Add(LoadoutView);
        content.Children.Add(PlayerGearView);
        content.Children.Add(WeaponModsView);
        content.Children.Add(CaImportView);
        content.Children.Add(MiniToolsView);
        content.Children.Add(ArsenalView);
        content.Children.Add(ModsView);
        content.Children.Add(PlayerProfileView);
        content.Children.Add(SettingsView);

        Grid.SetRow(content, 2);
        root.Children.Add(content);

        Content = root;

        StartupDiagnostics.Write(
            "MainWindow.BuildUi completed.");
    }

    private void BuildTitleBar(Grid root)
    {
        AppTitleBar.Background =
            TitleBarBrush;

        AppTitleBar.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width = new GridLength(6)
            });
        AppTitleBar.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width = new GridLength(
                    1,
                    GridUnitType.Star)
            });
        AppTitleBar.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width = GridLength.Auto
            });

        var accent = new Border
        {
            Background = AccentBrush
        };

        var title = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(16, 0, 0, 0)
        };

        title.Children.Add(
            new TextBlock
            {
                Text = "FIRETEAM",
                FontFamily = new FontFamily("Bahnschrift SemiCondensed"),
                FontSize = 19,
                FontWeight = Microsoft.UI.Text.FontWeights.Bold,
                CharacterSpacing = 35,
                Foreground = PrimaryTextBrush,
                VerticalAlignment = VerticalAlignment.Center
            });

        title.Children.Add(
            new TextBlock
            {
                Text = "/",
                FontSize = 16,
                Foreground = MutedTextBrush,
                VerticalAlignment = VerticalAlignment.Center
            });

        AppSectionTitleText.Text = "READY ROOM";
        AppSectionTitleText.FontFamily = new FontFamily("Bahnschrift SemiCondensed");
        AppSectionTitleText.FontSize = 13;
        AppSectionTitleText.FontWeight = Microsoft.UI.Text.FontWeights.SemiBold;
        AppSectionTitleText.CharacterSpacing = 55;
        AppSectionTitleText.Foreground = SecondaryTextBrush;
        AppSectionTitleText.VerticalAlignment = VerticalAlignment.Center;
        title.Children.Add(AppSectionTitleText);

        var build = new TextBlock
        {
            Text = "LOCAL ALPHA",
            FontFamily =
                new FontFamily(
                    "Bahnschrift SemiCondensed"),
            FontSize = 11,
            FontWeight =
                Microsoft.UI.Text.FontWeights.SemiBold,
            CharacterSpacing = 55,
            Foreground = AccentBrush,
            VerticalAlignment =
                VerticalAlignment.Center,
            Margin =
                new Thickness(0, 0, 150, 0)
        };

        Grid.SetColumn(accent, 0);
        Grid.SetColumn(title, 1);
        Grid.SetColumn(build, 2);

        AppTitleBar.Children.Add(accent);
        AppTitleBar.Children.Add(title);
        AppTitleBar.Children.Add(build);

        Grid.SetRow(AppTitleBar, 0);
        root.Children.Add(AppTitleBar);
    }

    private void BuildTopNavigation(Grid root)
    {
        var shell = new Border
        {
            Background = PanelBrush,
            BorderBrush = BorderBrush,
            BorderThickness =
                new Thickness(0, 0, 0, 1)
        };

        var bar = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 6,
            Margin =
                new Thickness(22, 7, 22, 7)
        };

        bar.Children.Add(
            NavButton(
                "HOME",
                "home"));

        var playButton =
            SecondaryButton("PLAY  ▾");
        var playMenu =
            new MenuFlyout();

        playMenu.Items.Add(
            MenuItem(
                "Single Player",
                () => SelectPlayMode(
                    "Single Player")));
        playMenu.Items.Add(
            MenuItem(
                "Host Multiplayer",
                () => SelectPlayMode(
                    "Host Multiplayer")));
        playMenu.Items.Add(
            MenuItem(
                "Join Multiplayer",
                () => SelectPlayMode(
                    "Join Multiplayer")));
        playMenu.Items.Add(
            new MenuFlyoutSeparator());
        playMenu.Items.Add(
            MenuItem(
                "Weapon QA (Local)",
                LaunchWeaponQa));
        playMenu.Items.Add(
            MenuItem(
                "Spectator QA (Local)",
                LaunchSpectatorQa));

        playButton.Flyout =
            playMenu;
        bar.Children.Add(
            playButton);

        var loadoutButton =
            SecondaryButton("LOADOUT  ▾");
        var loadoutMenu =
            new MenuFlyout();

        loadoutMenu.Items.Add(
            MenuItem(
                "Weapon Loadout",
                () => NavigateTo(
                    "loadout")));
        loadoutMenu.Items.Add(
            MenuItem(
                "Player Gear",
                () => NavigateTo(
                    "player-gear")));
        loadoutMenu.Items.Add(
            MenuItem(
                "Weapon Mods",
                () => NavigateTo(
                    "weapon-mods")));

        loadoutButton.Flyout =
            loadoutMenu;
        bar.Children.Add(
            loadoutButton);

        var toolsButton =
            SecondaryButton("TOOLS  ▾");
        var toolsMenu =
            new MenuFlyout();

        toolsMenu.Items.Add(
            MenuItem(
                "Tools Home",
                () => NavigateTo("tools")));
        toolsMenu.Items.Add(
            MenuItem(
                "Weapon Editor",
                () => NavigateTo(
                    "weapon-editor")));
        toolsMenu.Items.Add(
            MenuItem(
                "Combat Arms Importer",
                () => NavigateTo(
                    "ca-importer")));
        toolsMenu.Items.Add(
            MenuItem(
                "Mini Tools",
                () => NavigateTo(
                    "mini-tools")));
        toolsMenu.Items.Add(
            MenuItem(
                "Open Mod Library",
                () => LauncherPaths.OpenFolder(
                    LauncherPaths.ModsDirectory)));
        toolsMenu.Items.Add(
            MenuItem(
                "Open Content Tools",
                () => LauncherPaths.OpenFolder(
                    LauncherPaths.ModToolsDirectory)));

        toolsButton.Flyout =
            toolsMenu;
        bar.Children.Add(
            toolsButton);

        bar.Children.Add(
            NavButton(
                "PLAYER PROFILE",
                "profile"));

        bar.Children.Add(
            NavButton(
                "SERVERS",
                "servers"));

        bar.Children.Add(
            NavButton(
                "DEDICATED",
                "dedicated"));

        bar.Children.Add(
            NavButton(
                "SETTINGS",
                "settings"));

        shell.Child = bar;

        Grid.SetRow(shell, 1);
        root.Children.Add(shell);
    }

    private static MenuFlyoutItem MenuItem(
        string text,
        Action action)
    {
        var item =
            new MenuFlyoutItem
            {
                Text = text
            };

        item.Click +=
            (sender, args) =>
                action();

        return item;
    }

    private Button NavButton(
        string text,
        string tag)
    {
        var button =
            SecondaryButton(text);

        button.MinWidth = 112;
        button.Click +=
            (sender, args) =>
                NavigateTo(tag);

        return button;
    }

    private static Button PrimaryButton(string text) =>
        new()
        {
            Content = text,
            Background = AccentBrush,
            Foreground =
                new SolidColorBrush(
                    Color.FromArgb(
                        255, 24, 16, 0)),
            BorderBrush = AccentBrush,
            BorderThickness =
                new Thickness(1),
            CornerRadius =
                new CornerRadius(3),
            Padding =
                new Thickness(18, 9, 18, 9),
            FontFamily =
                new FontFamily(
                    "Bahnschrift SemiCondensed"),
            FontWeight =
                Microsoft.UI.Text.FontWeights.Bold
        };

    private static Button SecondaryButton(string text) =>
        new()
        {
            Content = text,
            Background = PanelRaisedBrush,
            Foreground = PrimaryTextBrush,
            BorderBrush = DividerBrush,
            BorderThickness =
                new Thickness(1),
            CornerRadius =
                new CornerRadius(3),
            Padding =
                new Thickness(15, 8, 15, 8),
            FontFamily =
                new FontFamily(
                    "Bahnschrift SemiCondensed"),
            FontWeight =
                Microsoft.UI.Text.FontWeights.SemiBold
        };

    private static TextBlock Eyebrow(
        string text,
        SolidColorBrush? color = null) =>
        new()
        {
            Text = text,
            FontFamily =
                new FontFamily(
                    "Bahnschrift SemiCondensed"),
            FontSize = 12,
            FontWeight =
                Microsoft.UI.Text.FontWeights.SemiBold,
            CharacterSpacing = 70,
            Foreground =
                color ?? AccentBrush
        };

    private static TextBlock SectionTitle(
        string text,
        double size = 24) =>
        new()
        {
            Text = text,
            FontFamily =
                new FontFamily(
                    "Bahnschrift SemiCondensed"),
            FontSize = size,
            FontWeight =
                Microsoft.UI.Text.FontWeights.SemiBold,
            Foreground =
                PrimaryTextBrush
        };

    private static TextBlock BodyText(string text) =>
        new()
        {
            Text = text,
            TextWrapping =
                TextWrapping.Wrap,
            Foreground =
                SecondaryTextBrush
        };

    private static Border Card(
        UIElement content,
        SolidColorBrush? accent = null)
    {
        var body = new Grid();
        body.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width = new GridLength(5)
            });
        body.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width = new GridLength(
                    1,
                    GridUnitType.Star)
            });

        body.Children.Add(
            new Border
            {
                Background =
                    accent ?? AccentBrush
            });

        var contentBorder =
            new Border
            {
                Padding =
                    new Thickness(18),
                Child = content
            };

        Grid.SetColumn(
            contentBorder,
            1);
        body.Children.Add(
            contentBorder);

        return new Border
        {
            Background = PanelBrush,
            BorderBrush = BorderBrush,
            BorderThickness =
                new Thickness(1),
            CornerRadius =
                new CornerRadius(4),
            Child = body
        };
    }

    private static Grid LabeledControl(
        string label,
        FrameworkElement control,
        double labelWidth = 145)
    {
        var row = new Grid
        {
            ColumnSpacing = 12
        };

        row.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        labelWidth)
            });
        row.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });

        var text =
            new TextBlock
            {
                Text = label,
                FontFamily =
                    new FontFamily(
                        "Bahnschrift SemiCondensed"),
                FontSize = 12,
                FontWeight =
                    Microsoft.UI.Text.FontWeights.SemiBold,
                Foreground =
                    SecondaryTextBrush,
                VerticalAlignment =
                    VerticalAlignment.Center
            };

        Grid.SetColumn(control, 1);
        row.Children.Add(text);
        row.Children.Add(control);

        return row;
    }

    private static StackPanel CardHeading(
        string eyebrow,
        string title,
        string description,
        SolidColorBrush? color = null)
    {
        var heading =
            new StackPanel
            {
                Spacing = 4
            };

        heading.Children.Add(
            Eyebrow(
                eyebrow,
                color));
        heading.Children.Add(
            SectionTitle(title));

        if(!string.IsNullOrWhiteSpace(
            description))
        {
            heading.Children.Add(
                BodyText(description));
        }

        return heading;
    }

    private static StackPanel NewPagePanel(
        string eyebrow,
        string title,
        string description)
    {
        var panel =
            new StackPanel
            {
                Spacing = 18,
                Margin =
                    new Thickness(
                        30, 24, 30, 32),
                MaxWidth = 1120,
                HorizontalAlignment =
                    HorizontalAlignment.Center
            };

        var header =
            new StackPanel
            {
                Spacing = 4
            };

        header.Children.Add(
            Eyebrow(eyebrow));
        header.Children.Add(
            SectionTitle(
                title,
                32));
        header.Children.Add(
            BodyText(description));

        panel.Children.Add(header);
        return panel;
    }

    private void BuildPlayerGearView()
    {
        PlayerGearView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;
        PlayerGearView.VerticalScrollBarVisibility =
            ScrollBarVisibility.Auto;
        PlayerGearView.HorizontalContentAlignment =
            HorizontalAlignment.Center;

        // The window title already says FIRETEAM / PLAYER GEAR. Keep the
        // page itself compact instead of repeating a second large header.
        var page =
            new StackPanel
            {
                Spacing = 16,
                Margin =
                    new Thickness(
                        30, 20, 30, 30),
                MaxWidth = 1120,
                HorizontalAlignment =
                    HorizontalAlignment.Center
            };

        var grid =
            new Grid
            {
                ColumnSpacing = 16,
                RowSpacing = 16
            };

        grid.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        grid.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        grid.RowDefinitions.Add(
            new RowDefinition
            {
                Height = GridLength.Auto
            });
        grid.RowDefinitions.Add(
            new RowDefinition
            {
                Height = GridLength.Auto
            });

        Border GearCard(
            string eyebrow,
            string title,
            string body,
            string status)
        {
            var panel =
                CardHeading(
                    eyebrow,
                    title,
                    body,
                    CyanBrush);

            panel.Children.Add(
                new TextBlock
                {
                    Text = status,
                    FontFamily =
                        new FontFamily(
                            "Bahnschrift SemiCondensed"),
                    FontSize = 12,
                    FontWeight =
                        Microsoft.UI.Text.FontWeights.Bold,
                    Foreground = AccentBrush,
                    Margin =
                        new Thickness(
                            0, 8, 0, 0)
                });

            return Card(
                panel,
                CyanBrush);
        }

        var backpack =
            GearCard(
                "BACKPACK SLOT",
                "Backpack",
                "Visual inventory/utility slot sourced from the Combat Arms attachment catalog.",
                "NOT EQUIPPED");

        var armor =
            GearCard(
                "ARMOR SLOT",
                "Body Armor",
                "Reserved for the planned Armor layer that absorbs damage before HP.",
                "NO ARMOR");

        var head =
            GearCard(
                "HEAD SLOT",
                "Headgear",
                "Helmets and head attachments stay separate from gameplay armor.",
                "NOT EQUIPPED");

        var face =
            GearCard(
                "FACE SLOT",
                "Mask / Glasses",
                "Masks, glasses and goggles can remain cosmetic without consuming a weapon slot.",
                "NOT EQUIPPED");

        Grid.SetRow(backpack, 0);
        Grid.SetColumn(backpack, 0);
        Grid.SetRow(armor, 0);
        Grid.SetColumn(armor, 1);
        Grid.SetRow(head, 1);
        Grid.SetColumn(head, 0);
        Grid.SetRow(face, 1);
        Grid.SetColumn(face, 1);

        grid.Children.Add(backpack);
        grid.Children.Add(armor);
        grid.Children.Add(head);
        grid.Children.Add(face);

        page.Children.Add(grid);

        var source =
            CardHeading(
                "ATTACHMENT CATALOG",
                "Combat Arms Gear",
                "Use Tools → Combat Arms Importer to extract ATTACH_M / ATTACH_T and rebuild config/attachments.cfg.",
                AccentBrush);

        page.Children.Add(
            Card(
                source,
                AccentBrush));

        PlayerGearView.Content =
            page;
    }

    private void BuildWeaponModsView()
    {
        WeaponModsView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;
        WeaponModsView.VerticalScrollBarVisibility =
            ScrollBarVisibility.Auto;
        WeaponModsView.HorizontalContentAlignment =
            HorizontalAlignment.Center;

        // FIRETEAM / WEAPON MODS is already in the chrome.
        var page =
            new StackPanel
            {
                Spacing = 16,
                Margin =
                    new Thickness(
                        30, 20, 30, 30),
                MaxWidth = 1120,
                HorizontalAlignment =
                    HorizontalAlignment.Center
            };

        var grid =
            new Grid
            {
                ColumnSpacing = 16,
                RowSpacing = 16
            };

        grid.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        grid.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        grid.RowDefinitions.Add(
            new RowDefinition
            {
                Height = GridLength.Auto
            });
        grid.RowDefinitions.Add(
            new RowDefinition
            {
                Height = GridLength.Auto
            });

        Border ModCard(
            string eyebrow,
            string title,
            string body)
        {
            var panel =
                CardHeading(
                    eyebrow,
                    title,
                    body,
                    AccentBrush);

            panel.Children.Add(
                new TextBlock
                {
                    Text = "NO MOD INSTALLED",
                    FontFamily =
                        new FontFamily(
                            "Bahnschrift SemiCondensed"),
                    FontSize = 12,
                    FontWeight =
                        Microsoft.UI.Text.FontWeights.Bold,
                    Foreground = CyanBrush,
                    Margin =
                        new Thickness(
                            0, 8, 0, 0)
                });

            return Card(
                panel,
                AccentBrush);
        }

        var optic =
            ModCard(
                "OPTIC SLOT",
                "Scope / Sight",
                "Optics can change FOV, scope presentation and zoom behavior without duplicating the base gun.");

        var muzzle =
            ModCard(
                "MUZZLE SLOT",
                "Suppressor / Muzzle",
                "Suppressors will reduce the zombie hearing radius when sound-based aggro is added.");

        var magazine =
            ModCard(
                "MAGAZINE SLOT",
                "Magazine",
                "Extended magazines alter capacity while the base weapon keeps its damage and fire rate.");

        var ammo =
            ModCard(
                "AMMO SLOT",
                "Reserve / Ammo Stash",
                "Extra carried ammunition increases reserve capacity independently from magazine size.");

        Grid.SetRow(optic, 0);
        Grid.SetColumn(optic, 0);
        Grid.SetRow(muzzle, 0);
        Grid.SetColumn(muzzle, 1);
        Grid.SetRow(magazine, 1);
        Grid.SetColumn(magazine, 0);
        Grid.SetRow(ammo, 1);
        Grid.SetColumn(ammo, 1);

        grid.Children.Add(optic);
        grid.Children.Add(muzzle);
        grid.Children.Add(magazine);
        grid.Children.Add(ammo);

        page.Children.Add(grid);

        var note =
            CardHeading(
                "DATA MODEL",
                "Base weapon + installed mods",
                "Weapon Catalog remains the authoring tool for the base gun; installed mods live here as loadout data.",
                CyanBrush);

        page.Children.Add(
            Card(
                note,
                CyanBrush));

        WeaponModsView.Content =
            page;
    }

    private void BuildHomeView()
    {
        HomeView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;
        HomeView.VerticalScrollBarVisibility =
            ScrollBarVisibility.Auto;
        HomeView.HorizontalContentAlignment =
            HorizontalAlignment.Center;

        var page =
            new StackPanel
            {
                Spacing = 14,
                Margin =
                    new Thickness(
                        30,
                        14,
                        30,
                        26),
                MaxWidth = 1120,
                HorizontalAlignment =
                    HorizontalAlignment.Center
            };

        var dashboard =
            new Grid
            {
                ColumnSpacing = 18
            };

        dashboard.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1.7,
                        GridUnitType.Star)
            });
        dashboard.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        0.9,
                        GridUnitType.Star)
            });

        var left =
            new StackPanel
            {
                Spacing = 16
            };

        var quick =
            CardHeading(
                "QUICK PLAY",
                "Run Setup",
                "Player identity and session settings for the next FIRETEAM launch.");

        var quickFields =
            new Grid
            {
                ColumnSpacing = 18,
                RowSpacing = 10,
                Margin =
                    new Thickness(0, 10, 0, 0)
            };

        quickFields.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        quickFields.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        quickFields.RowDefinitions.Add(
            new RowDefinition
            {
                Height = GridLength.Auto
            });
        quickFields.RowDefinitions.Add(
            new RowDefinition
            {
                Height = GridLength.Auto
            });
        quickFields.RowDefinitions.Add(
            new RowDefinition
            {
                Height = GridLength.Auto
            });

        PlayerNameBox.MaxLength = 15;
        PlayerNameBox.MaxWidth = 320;
        PlayerNameBox.HorizontalAlignment =
            HorizontalAlignment.Stretch;

        ModeCombo.SelectionChanged +=
            ModeCombo_SelectionChanged;

        JoinIpLabel.Text =
            "SERVER IP";
        JoinIpLabel.Visibility =
            Visibility.Collapsed;
        JoinIpBox.Visibility =
            Visibility.Collapsed;
        JoinIpLabel.Foreground =
            SecondaryTextBrush;
        JoinIpLabel.FontFamily =
            new FontFamily(
                "Bahnschrift SemiCondensed");
        JoinIpLabel.FontSize = 12;
        JoinIpLabel.FontWeight =
            Microsoft.UI.Text.FontWeights.SemiBold;
        JoinIpLabel.VerticalAlignment =
            VerticalAlignment.Center;

        var player =
            LabeledControl(
                "PLAYER NAME",
                PlayerNameBox);
        var mode =
            LabeledControl(
                "MODE",
                ModeCombo);

        MapCombo.HorizontalAlignment =
            HorizontalAlignment.Stretch;

        var map =
            LabeledControl(
                "MAP",
                MapCombo);

        DifficultySlider.Minimum = 0;
        DifficultySlider.Maximum = 10;
        DifficultySlider.StepFrequency = 1;
        DifficultySlider.Value = 4;
        DifficultySlider.HorizontalAlignment =
            HorizontalAlignment.Stretch;
        DifficultySlider.ValueChanged +=
            (sender, args) =>
                UpdateDifficultyLabel();

        DifficultyValueText.Foreground =
            AccentBrush;
        DifficultyValueText.FontFamily =
            new FontFamily(
                "Bahnschrift SemiCondensed");
        DifficultyValueText.FontSize = 12;
        DifficultyValueText.FontWeight =
            Microsoft.UI.Text.FontWeights.Bold;
        DifficultyValueText.CharacterSpacing = 25;
        UpdateDifficultyLabel();

        var difficultyControl =
            new StackPanel
            {
                Spacing = 0,
                Width = 174
            };
        difficultyControl.Children.Add(
            DifficultySlider);
        difficultyControl.Children.Add(
            DifficultyValueText);

        var difficulty =
            LabeledControl(
                "DIFFICULTY 0-10",
                difficultyControl);

        var join =
            new Grid
            {
                ColumnSpacing = 12
            };
        join.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(145)
            });
        join.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        Grid.SetColumn(
            JoinIpBox,
            1);
        join.Children.Add(
            JoinIpLabel);
        join.Children.Add(
            JoinIpBox);

        Grid.SetRow(player, 0);
        Grid.SetColumn(player, 0);
        Grid.SetRow(mode, 0);
        Grid.SetColumn(mode, 1);
        Grid.SetRow(difficulty, 1);
        Grid.SetColumn(difficulty, 0);
        Grid.SetRow(map, 1);
        Grid.SetColumn(map, 1);
        Grid.SetRow(join, 2);
        Grid.SetColumn(join, 0);
        Grid.SetColumnSpan(join, 2);

        quickFields.Children.Add(player);
        quickFields.Children.Add(mode);
        quickFields.Children.Add(difficulty);
        quickFields.Children.Add(map);
        quickFields.Children.Add(join);
        quick.Children.Add(quickFields);

        CommandsBox.PlaceholderText =
            "+consoleenable 1 +SomeLithTechCommand value";
        CommandsBox.Margin =
            new Thickness(0, 4, 0, 0);
        quick.Children.Add(
            Eyebrow(
                "ADVANCED COMMANDS",
                MutedTextBrush));
        quick.Children.Add(
            CommandsBox);

        var runActions =
            new Grid
            {
                ColumnSpacing = 14,
                Margin =
                    new Thickness(
                        0,
                        8,
                        0,
                        0)
            };

        runActions.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        runActions.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    GridLength.Auto
            });

        RunStatusText.TextWrapping =
            TextWrapping.Wrap;
        RunStatusText.TextAlignment =
            TextAlignment.Left;
        RunStatusText.Foreground =
            SecondaryTextBrush;
        RunStatusText.VerticalAlignment =
            VerticalAlignment.Center;

        PlayButton.Content =
            "LAUNCH GAME";
        PlayButton.Background =
            AccentBrush;
        PlayButton.Foreground =
            new SolidColorBrush(
                Color.FromArgb(
                    255,
                    24,
                    16,
                    0));
        PlayButton.BorderBrush =
            AccentBrush;
        PlayButton.Padding =
            new Thickness(
                24,
                10,
                24,
                10);
        PlayButton.MinHeight = 44;
        PlayButton.HorizontalAlignment =
            HorizontalAlignment.Right;
        PlayButton.Click +=
            PlayButton_Click;

        Grid.SetColumn(
            RunStatusText,
            0);
        Grid.SetColumn(
            PlayButton,
            1);
        runActions.Children.Add(
            RunStatusText);
        runActions.Children.Add(
            PlayButton);

        quick.Children.Add(
            runActions);

        left.Children.Add(
            Card(
                quick,
                AccentBrush));

        var loadout =
            CardHeading(
                "DEPLOYMENT KIT",
                "Current Loadout",
                "Your active five-slot equipment set. Use Loadout to change it.");

        var loadoutGrid =
            new Grid
            {
                ColumnSpacing = 16,
                RowSpacing = 8,
                Margin =
                    new Thickness(0, 10, 0, 0)
            };

        loadoutGrid.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        loadoutGrid.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });

        var summary =
            new[]
            {
                PrimarySummaryText,
                SidearmSummaryText,
                MeleeSummaryText,
                SecondarySummaryText,
                SpecialSummaryText
            };

        for(var i = 0;
            i < summary.Length;
            ++i)
        {
            summary[i].Foreground =
                i == 0
                ? PrimaryTextBrush
                : SecondaryTextBrush;
            summary[i].TextWrapping =
                TextWrapping.Wrap;

            while(
                loadoutGrid.RowDefinitions.Count <=
                i / 2)
            {
                loadoutGrid.RowDefinitions.Add(
                    new RowDefinition
                    {
                        Height =
                            GridLength.Auto
                    });
            }

            Grid.SetRow(
                summary[i],
                i / 2);
            Grid.SetColumn(
                summary[i],
                i % 2);
            loadoutGrid.Children.Add(
                summary[i]);
        }

        loadout.Children.Add(
            loadoutGrid);

        var edit =
            SecondaryButton(
                "EDIT LOADOUT");
        edit.Margin =
            new Thickness(0, 10, 0, 0);
        edit.HorizontalAlignment =
            HorizontalAlignment.Left;
        edit.Click +=
            EditLoadoutButton_Click;
        loadout.Children.Add(edit);

        left.Children.Add(
            Card(
                loadout,
                CyanBrush));

        var right =
            new StackPanel
            {
                Spacing = 16
            };

        var news =
            CardHeading(
                "NEWS / UPDATES",
                "Development Feed",
                "Local alpha notes for the build you are testing.",
                CyanBrush);

        news.Children.Add(
            NewsItem(
                "POWERUPS",
                "Mutation Box drops and timed gameplay buffs are now in the round loop."));
        news.Children.Add(
            NewsItem(
                "HUD",
                "Timed One Hit Kill and Bottomless Mag effects now have countdown support."));
        news.Children.Add(
            NewsItem(
                "INFECTED AI",
                "AIVolume routing, target memory and recovery behavior are under active tuning."));
        news.Children.Add(
            NewsItem(
                "LAUNCHER",
                "The launcher shell now uses compact top navigation and dedicated tool hubs."));

        right.Children.Add(
            Card(
                news,
                CyanBrush));

        var build =
            CardHeading(
                "BUILD STATUS",
                "FIRETEAM Alpha",
                "A focused community-driven zombie FPS and modding platform.");

        build.Children.Add(
            StatusLine(
                "SUPPORTED MAP",
                "Cabin Fever"));
        build.Children.Add(
            StatusLine(
                "GAMEPLAY",
                "Round survival"));
        build.Children.Add(
            StatusLine(
                "CONTENT",
                "External configs + REZ assets"));
        build.Children.Add(
            StatusLine(
                "TOOLS",
                "Mod workspace enabled"));

        right.Children.Add(
            Card(
                build,
                AccentBrush));

        Grid.SetColumn(left, 0);
        Grid.SetColumn(right, 1);
        dashboard.Children.Add(left);
        dashboard.Children.Add(right);

        page.Children.Add(dashboard);
        HomeView.Content = page;
    }

    private static Border NewsItem(
        string tag,
        string text)
    {
        var panel =
            new StackPanel
            {
                Spacing = 3
            };

        panel.Children.Add(
            Eyebrow(
                tag,
                AccentBrush));
        panel.Children.Add(
            BodyText(text));

        return new Border
        {
            Background =
                PanelRaisedBrush,
            BorderBrush =
                BorderBrush,
            BorderThickness =
                new Thickness(1),
            CornerRadius =
                new CornerRadius(3),
            Padding =
                new Thickness(12),
            Margin =
                new Thickness(0, 8, 0, 0),
            Child = panel
        };
    }

    private static Grid StatusLine(
        string label,
        string value)
    {
        var row = new Grid
        {
            Margin =
                new Thickness(0, 8, 0, 0)
        };

        row.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        row.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    GridLength.Auto
            });

        var left =
            Eyebrow(
                label,
                MutedTextBrush);
        var right =
            new TextBlock
            {
                Text = value,
                Foreground =
                    PrimaryTextBrush,
                FontWeight =
                    Microsoft.UI.Text.FontWeights.SemiBold
            };

        Grid.SetColumn(right, 1);
        row.Children.Add(left);
        row.Children.Add(right);

        return row;
    }

    private void BuildLoadoutView()
    {
        LoadoutView.Visibility =
            Visibility.Collapsed;

        LoadoutView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;

        LoadoutView.VerticalScrollBarVisibility =
            ScrollBarVisibility.Auto;
        LoadoutView.HorizontalContentAlignment =
            HorizontalAlignment.Center;

        var page =
            new StackPanel
            {
                Spacing = 14,
                Margin =
                    new Thickness(
                        26,
                        18,
                        26,
                        24),
                MaxWidth = 1120,
                HorizontalAlignment =
                    HorizontalAlignment.Center
            };

        var workspace =
            new Grid
            {
                ColumnSpacing = 16
            };

        workspace.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        205)
            });

        workspace.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });

        var presets =
            CardHeading(
                "SAVED LOADOUTS",
                "Gear Tabs",
                "Switch between three saved kits. Saving does not change the active in-game kit until you activate it.");

        LoadoutPresetList.DisplayMemberPath =
            "DisplayName";

        LoadoutPresetList.MinHeight =
            150;

        LoadoutPresetList.MaxHeight =
            190;

        LoadoutPresetList.SelectionChanged +=
            LoadoutPresetList_SelectionChanged;

        presets.Children.Add(
            LoadoutPresetList);

        presets.Children.Add(
            Eyebrow(
                "LOADOUT NAME",
                MutedTextBrush));

        LoadoutPresetNameBox.MaxLength =
            24;

        presets.Children.Add(
            LoadoutPresetNameBox);

        var savePreset =
            SecondaryButton(
                "SAVE LOADOUT");

        savePreset.HorizontalAlignment =
            HorizontalAlignment.Stretch;

        savePreset.Click +=
            SaveLoadoutPresetButton_Click;

        presets.Children.Add(
            savePreset);

        var presetCard =
            Card(
                presets,
                AccentBrush);

        Grid.SetColumn(
            presetCard,
            0);

        workspace.Children.Add(
            presetCard);

        var armory =
            new StackPanel
            {
                Spacing = 12
            };

        var fightingLoad =
            CardHeading(
                "FIGHTING LOAD",
                "Current Kit",
                "Primary, sidearm and melee have fixed roles. Backpack slots carry additional enabled equipment.",
                CyanBrush);

        var slots =
            new Grid
            {
                ColumnSpacing = 8,
                Margin =
                    new Thickness(
                        0,
                        8,
                        0,
                        0)
            };

        for(var i = 0;
            i < 5;
            ++i)
        {
            slots.ColumnDefinitions.Add(
                new ColumnDefinition
                {
                    Width =
                        new GridLength(
                            1,
                            GridUnitType.Star)
                });
        }

        var slotButtons =
            LoadoutSlotButtons();

        for(var i = 0;
            i < slotButtons.Count;
            ++i)
        {
            var button =
                slotButtons[i];

            button.Background =
                PanelRaisedBrush;

            button.Foreground =
                PrimaryTextBrush;

            button.BorderBrush =
                DividerBrush;

            button.BorderThickness =
                new Thickness(1);

            button.CornerRadius =
                new CornerRadius(3);

            button.MinHeight =
                64;

            button.HorizontalAlignment =
                HorizontalAlignment.Stretch;

            button.IsHitTestVisible =
                true;

            button.Tag =
                i;

            button.Click +=
                LoadoutSlotButton_Click;

            Grid.SetColumn(
                button,
                i);

            slots.Children.Add(
                button);
        }

        fightingLoad.Children.Add(
            slots);

        var kitActions =
            new StackPanel
            {
                Orientation =
                    Orientation.Horizontal,
                Spacing = 10,
                Margin =
                    new Thickness(
                        0,
                        8,
                        0,
                        0)
            };

        var activate =
            PrimaryButton(
                "ACTIVATE LOADOUT");

        activate.Click +=
            ApplyLoadoutButton_Click;

        var editor =
            SecondaryButton(
                "OPEN WEAPON EDITOR");

        editor.Click +=
            OpenArsenalButton_Click;

        kitActions.Children.Add(
            activate);

        kitActions.Children.Add(
            editor);

        fightingLoad.Children.Add(
            kitActions);

        armory.Children.Add(
            Card(
                fightingLoad,
                CyanBrush));

        var inventory =
            CardHeading(
                "INVENTORY",
                "Weapon Locker",
                "Enabled weapons appear here. UNVERIFIED imports can still be enabled for deliberate manual testing.");

        var categories =
            new StackPanel
            {
                Orientation =
                    Orientation.Horizontal,
                Spacing = 5,
                Margin =
                    new Thickness(
                        0,
                        8,
                        0,
                        0)
            };

        foreach(var category in new[]
        {
            "AR",
            "SR",
            "Launcher",
            "Melee",
            "MG",
            "Pistol",
            "SG",
            "SMG",
            "Throwing"
        })
        {
            categories.Children.Add(
                LoadoutCategoryButton(
                    category));
        }

        inventory.Children.Add(
            categories);

        var filterRow =
            new Grid
            {
                ColumnSpacing = 10
            };

        filterRow.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });

        filterRow.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    GridLength.Auto
            });

        LoadoutInventorySearchBox.PlaceholderText =
            "Search enabled weapons...";

        LoadoutInventorySearchBox.TextChanged +=
            LoadoutInventorySearchBox_TextChanged;

        LoadoutCategoryStatusText.Foreground =
            AccentBrush;

        LoadoutCategoryStatusText.FontFamily =
            new FontFamily(
                "Bahnschrift SemiCondensed");

        LoadoutCategoryStatusText.FontWeight =
            Microsoft.UI.Text.FontWeights.SemiBold;

        LoadoutCategoryStatusText.VerticalAlignment =
            VerticalAlignment.Center;

        Grid.SetColumn(
            LoadoutCategoryStatusText,
            1);

        filterRow.Children.Add(
            LoadoutInventorySearchBox);

        filterRow.Children.Add(
            LoadoutCategoryStatusText);

        inventory.Children.Add(
            filterRow);

        var browser =
            new Grid
            {
                ColumnSpacing = 14,
                MinHeight = 210
            };

        browser.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1.1,
                        GridUnitType.Star)
            });

        browser.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        0.9,
                        GridUnitType.Star)
            });

        LoadoutInventoryList.DisplayMemberPath =
            "DisplayName";

        LoadoutInventoryList.MinHeight =
            205;

        LoadoutInventoryList.MaxHeight =
            245;

        LoadoutInventoryList.SelectionChanged +=
            LoadoutInventoryList_SelectionChanged;

        Grid.SetColumn(
            LoadoutInventoryList,
            0);

        browser.Children.Add(
            LoadoutInventoryList);

        var detail =
            new StackPanel
            {
                Spacing = 8,
                Margin =
                    new Thickness(
                        6,
                        0,
                        0,
                        0)
            };

        LoadoutWeaponTitleText.Text =
            "Select a weapon";

        LoadoutWeaponTitleText.FontFamily =
            new FontFamily(
                "Bahnschrift SemiCondensed");

        LoadoutWeaponTitleText.FontSize =
            24;

        LoadoutWeaponTitleText.FontWeight =
            Microsoft.UI.Text.FontWeights.SemiBold;

        LoadoutWeaponTitleText.Foreground =
            PrimaryTextBrush;

        LoadoutWeaponMetaText.Foreground =
            AccentBrush;

        LoadoutWeaponMetaText.TextWrapping =
            TextWrapping.Wrap;

        LoadoutWeaponStatsText.FontFamily =
            new FontFamily(
                "Consolas");

        LoadoutWeaponStatsText.Foreground =
            SecondaryTextBrush;

        LoadoutWeaponStatsText.TextWrapping =
            TextWrapping.Wrap;

        detail.Children.Add(
            LoadoutWeaponTitleText);

        detail.Children.Add(
            LoadoutWeaponMetaText);

        detail.Children.Add(
            LoadoutWeaponStatsText);

        detail.Children.Add(
            Eyebrow(
                "EQUIP TO",
                MutedTextBrush));

        LoadoutEquipSlotCombo.HorizontalAlignment =
            HorizontalAlignment.Stretch;

        detail.Children.Add(
            LoadoutEquipSlotCombo);

        LoadoutEquipButton.Content =
            "EQUIP WEAPON";

        LoadoutEquipButton.Background =
            AccentBrush;

        LoadoutEquipButton.Foreground =
            new SolidColorBrush(
                Color.FromArgb(
                    255,
                    24,
                    16,
                    0));

        LoadoutEquipButton.BorderBrush =
            AccentBrush;

        LoadoutEquipButton.Padding =
            new Thickness(
                18,
                9,
                18,
                9);

        LoadoutEquipButton.HorizontalAlignment =
            HorizontalAlignment.Left;

        LoadoutEquipButton.IsEnabled =
            false;

        LoadoutEquipButton.Click +=
            LoadoutEquipButton_Click;

        detail.Children.Add(
            LoadoutEquipButton);

        Grid.SetColumn(
            detail,
            1);

        browser.Children.Add(
            detail);

        inventory.Children.Add(
            browser);

        LoadoutStatusText.Foreground =
            SecondaryTextBrush;

        LoadoutStatusText.TextWrapping =
            TextWrapping.Wrap;

        inventory.Children.Add(
            LoadoutStatusText);

        armory.Children.Add(
            Card(
                inventory,
                AccentBrush));

        Grid.SetColumn(
            armory,
            1);

        workspace.Children.Add(
            armory);

        page.Children.Add(
            workspace);

        LoadoutView.Content =
            page;
    }

    private Button LoadoutCategoryButton(
        string category)
    {
        var button =
            SecondaryButton(
                category.ToUpperInvariant());

        button.MinWidth =
            category.Length > 5
            ? 92
            : 66;

        button.Padding =
            new Thickness(
                11,
                7,
                11,
                7);

        button.Click +=
            (sender, args) =>
                SetLoadoutCategory(
                    category);

        _loadoutCategoryButtons[category] =
            button;

        return button;
    }

    private void BuildModsView()
    {
        ModsView.Visibility =
            Visibility.Collapsed;
        ModsView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;
        ModsView.HorizontalContentAlignment =
            HorizontalAlignment.Center;

        var page =
            NewPagePanel(
                "FIRETEAM / TOOLS",
                "Mods & Content Tools",
                "Player configuration stays in the launcher. Content authoring lives here as dedicated tools and workspaces.");

        var tools =
            new Grid
            {
                ColumnSpacing = 16,
                RowSpacing = 16
            };

        tools.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        tools.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        tools.RowDefinitions.Add(
            new RowDefinition
            {
                Height =
                    GridLength.Auto
            });
        tools.RowDefinitions.Add(
            new RowDefinition
            {
                Height =
                    GridLength.Auto
            });
        tools.RowDefinitions.Add(
            new RowDefinition
            {
                Height =
                    GridLength.Auto
            });

        var weapon =
            ToolCard(
                "WEAPON TOOL",
                "Weapon Editor",
                "Edit supported weapon definitions, damage, magazines, fire cadence and related external config values.",
                CyanBrush,
                "OPEN WEAPON EDITOR",
                () => NavigateTo(
                    "weapon-editor"));

        var mods =
            ToolCard(
                "MOD WORKSPACE",
                "Mod Library",
                "Open the FIRETEAM Mods folder. Enable/disable/version management comes in a later launcher pass.",
                AccentBrush,
                "OPEN MOD LIBRARY",
                () => LauncherPaths.OpenFolder(
                    LauncherPaths.ModsDirectory));

        var content =
            ToolCard(
                "LITHTECH SDK",
                "Content Tools",
                "Open the modTools workspace for DEdit, ModelEdit, FXed, RenderStyleEditor and future FIRETEAM utilities.",
                AccentRedBrush,
                "OPEN CONTENT TOOLS",
                () => LauncherPaths.OpenFolder(
                    LauncherPaths.ModToolsDirectory));

        var importer =
            ToolCard(
                "MIGRATION TOOL",
                "Combat Arms Importer",
                "Import Combat Arms weapons, attachments and map DAT files into ignored local content storage.",
                AccentBrush,
                "OPEN IMPORTER",
                () => NavigateTo(
                    "ca-importer"));

        var miniTools =
            ToolCard(
                "UTILITY TOOLS",
                "Mini Tools",
                "Small single-purpose utilities such as RezExtract that open, do one job, and return you to the launcher.",
                CyanBrush,
                "OPEN MINI TOOLS",
                () => NavigateTo(
                    "mini-tools"));

        Grid.SetRow(weapon, 0);
        Grid.SetColumn(weapon, 0);
        Grid.SetRow(mods, 0);
        Grid.SetColumn(mods, 1);
        Grid.SetRow(content, 1);
        Grid.SetColumn(content, 0);
        Grid.SetRow(importer, 1);
        Grid.SetColumn(importer, 1);
        Grid.SetRow(miniTools, 2);
        Grid.SetColumn(miniTools, 0);
        Grid.SetColumnSpan(miniTools, 2);

        tools.Children.Add(weapon);
        tools.Children.Add(mods);
        tools.Children.Add(content);
        tools.Children.Add(importer);
        tools.Children.Add(miniTools);

        page.Children.Add(tools);

        var paths =
            CardHeading(
                "WORKSPACE PATHS",
                "Local Development",
                "These paths stay outside the clean WinUI runtime dependency folder.");

        ModsPathText.TextWrapping =
            TextWrapping.Wrap;
        ModsPathText.Foreground =
            SecondaryTextBrush;
        ToolsPathText.TextWrapping =
            TextWrapping.Wrap;
        ToolsPathText.Foreground =
            SecondaryTextBrush;

        paths.Children.Add(
            ModsPathText);
        paths.Children.Add(
            ToolsPathText);

        page.Children.Add(
            Card(
                paths,
                CyanBrush));

        ModsView.Content = page;
    }

    private static Border ToolCard(
        string eyebrow,
        string title,
        string description,
        SolidColorBrush accent,
        string actionText,
        Action action)
    {
        var panel =
            CardHeading(
                eyebrow,
                title,
                description,
                accent);

        var button =
            SecondaryButton(
                actionText);
        button.HorizontalAlignment =
            HorizontalAlignment.Left;
        button.Margin =
            new Thickness(0, 10, 0, 0);
        button.Click +=
            (sender, args) =>
                action();

        panel.Children.Add(button);

        return Card(
            panel,
            accent);
    }

    private void BuildMiniToolsView()
    {
        MiniToolsView.Visibility =
            Visibility.Collapsed;
        MiniToolsView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;
        MiniToolsView.VerticalScrollBarVisibility =
            ScrollBarVisibility.Auto;
        MiniToolsView.HorizontalContentAlignment =
            HorizontalAlignment.Center;

        var page =
            NewPagePanel(
                "FIRETEAM / TOOLS",
                "Mini Tools",
                "Small focused utilities live here instead of cluttering the full LithTech content-tools workspace.");

        var tools =
            new Grid
            {
                ColumnSpacing = 16,
                RowSpacing = 16
            };

        tools.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        tools.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });

        var rezExtract =
            ToolCard(
                "REZ UTILITY",
                "RezExtract",
                "Open RezExtract for unpacking LithTech REZ archives into ordinary folders.",
                CyanBrush,
                "OPEN REZEXTRACT",
                LaunchRezExtract);

        var folder =
            ToolCard(
                "UTILITY FOLDER",
                "Mini Tool Files",
                "Open the local tool workspace where standalone utilities can be placed without mixing them into the launcher runtime.",
                AccentBrush,
                "OPEN TOOL FOLDER",
                () => LauncherPaths.OpenFolder(
                    LauncherPaths.ModToolsDirectory));

        Grid.SetColumn(
            rezExtract,
            0);
        Grid.SetColumn(
            folder,
            1);

        tools.Children.Add(
            rezExtract);
        tools.Children.Add(
            folder);

        page.Children.Add(
            tools);

        MiniToolsStatusText.Text =
            LauncherPaths.FindMiniTool(
                "RezExtract.exe") is null
                ? "RezExtract.exe not found yet."
                : "RezExtract.exe detected and ready.";
        MiniToolsStatusText.Foreground =
            SecondaryTextBrush;
        MiniToolsStatusText.TextWrapping =
            TextWrapping.Wrap;

        var status =
            CardHeading(
                "STATUS",
                "Utility Runtime",
                "Mini tools launch as separate utility windows and return control to FIRETEAM when they close.",
                AccentBrush);
        status.Children.Add(
            MiniToolsStatusText);

        var back =
            SecondaryButton(
                "BACK TO TOOLS");
        back.HorizontalAlignment =
            HorizontalAlignment.Left;
        back.Click +=
            (sender, args) =>
                NavigateTo(
                    "tools");
        status.Children.Add(
            back);

        page.Children.Add(
            Card(
                status,
                AccentBrush));

        MiniToolsView.Content =
            page;
    }


    private void BuildCaImportView()
    {
        CaImportView.Visibility =
            Visibility.Collapsed;
        CaImportView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;
        CaImportView.VerticalScrollBarVisibility =
            ScrollBarVisibility.Auto;
        CaImportView.HorizontalContentAlignment =
            HorizontalAlignment.Center;

        var page =
            new StackPanel
            {
                Spacing = 12,
                Margin =
                    new Thickness(
                        20, 14, 20, 20),
                MaxWidth = 1120,
                HorizontalAlignment =
                    HorizontalAlignment.Center
            };

        var sourcePanel =
            CardHeading(
                "CONTENT SOURCES",
                "Combat Arms Import",
                "Pick each archive once, then run the import you want. Weapon metadata and attachment assets remain separate.",
                AccentBrush);

        var sources =
            new Grid
            {
                ColumnSpacing = 12,
                Margin =
                    new Thickness(
                        0, 8, 0, 0)
            };

        for(var i = 0;
            i < 3;
            ++i)
        {
            sources.ColumnDefinitions.Add(
                new ColumnDefinition
                {
                    Width =
                        new GridLength(
                            1,
                            GridUnitType.Star)
                });
        }

        StackPanel SourcePicker(
            string label,
            string buttonText,
            Button button,
            TextBlock status,
            RoutedEventHandler handler)
        {
            var panel =
                new StackPanel
                {
                    Spacing = 6
                };

            panel.Children.Add(
                Eyebrow(
                    label,
                    CyanBrush));

            button.Content =
                buttonText;
            button.Background =
                PanelRaisedBrush;
            button.Foreground =
                PrimaryTextBrush;
            button.BorderBrush =
                DividerBrush;
            button.BorderThickness =
                new Thickness(1);
            button.CornerRadius =
                new CornerRadius(3);
            button.Padding =
                new Thickness(
                    12, 8, 12, 8);
            button.HorizontalAlignment =
                HorizontalAlignment.Stretch;
            button.Click +=
                handler;

            status.Text =
                "Not selected";
            status.Foreground =
                SecondaryTextBrush;
            status.TextWrapping =
                TextWrapping.Wrap;
            status.FontSize =
                12;

            panel.Children.Add(
                button);
            panel.Children.Add(
                status);

            return panel;
        }

        var weaponsPicker =
            SourcePicker(
                "ATTRIBUTES",
                "SELECT WEAPONS.TXT",
                new Button(),
                CaWeaponsSourceText,
                SelectCaWeaponsSourceButton_Click);

        var gunsPicker =
            SourcePicker(
                "WEAPONS",
                "SELECT GUNS.ZIP",
                new Button(),
                CaGunsSourceText,
                SelectCaGunsSourceButton_Click);

        var attachmentsPicker =
            SourcePicker(
                "PLAYER GEAR",
                "SELECT ATTACHMENTS.ZIP",
                new Button(),
                CaAttachmentsSourceText,
                SelectCaAttachmentsSourceButton_Click);

        Grid.SetColumn(
            weaponsPicker,
            0);
        Grid.SetColumn(
            gunsPicker,
            1);
        Grid.SetColumn(
            attachmentsPicker,
            2);

        sources.Children.Add(
            weaponsPicker);
        sources.Children.Add(
            gunsPicker);
        sources.Children.Add(
            attachmentsPicker);

        sourcePanel.Children.Add(
            sources);

        var importActions =
            new StackPanel
            {
                Orientation =
                    Orientation.Horizontal,
                Spacing = 10,
                Margin =
                    new Thickness(
                        0, 8, 0, 0)
            };

        CaImportRunButton.Content =
            "START WEAPON IMPORT";
        CaImportRunButton.Background =
            AccentBrush;
        CaImportRunButton.Foreground =
            new SolidColorBrush(
                Color.FromArgb(
                    255, 24, 16, 0));
        CaImportRunButton.BorderBrush =
            AccentBrush;
        CaImportRunButton.Padding =
            new Thickness(
                18, 9, 18, 9);
        CaImportRunButton.IsEnabled =
            false;
        CaImportRunButton.Click +=
            ImportCombatArmsButton_Click;

        CaAttachmentImportButton.Content =
            "IMPORT ATTACHMENTS";
        CaAttachmentImportButton.Background =
            CyanBrush;
        CaAttachmentImportButton.Foreground =
            new SolidColorBrush(
                Color.FromArgb(
                    255, 0, 20, 28));
        CaAttachmentImportButton.BorderBrush =
            CyanBrush;
        CaAttachmentImportButton.Padding =
            new Thickness(
                18, 9, 18, 9);
        CaAttachmentImportButton.IsEnabled =
            false;
        CaAttachmentImportButton.Click +=
            ImportCombatArmsAttachmentsButton_Click;

        importActions.Children.Add(
            CaImportRunButton);
        importActions.Children.Add(
            CaAttachmentImportButton);

        sourcePanel.Children.Add(
            importActions);

        CaImportProgressBar.Minimum = 0;
        CaImportProgressBar.Maximum = 100;
        CaImportProgressBar.Value = 0;
        CaImportProgressBar.Height = 8;
        CaImportProgressBar.HorizontalAlignment =
            HorizontalAlignment.Stretch;
        sourcePanel.Children.Add(
            CaImportProgressBar);

        CaImportStatusText.Text =
            "Select the Combat Arms source files you want to work with.";
        CaImportStatusText.Foreground =
            SecondaryTextBrush;
        CaImportStatusText.TextWrapping =
            TextWrapping.Wrap;
        sourcePanel.Children.Add(
            CaImportStatusText);

        page.Children.Add(
            Card(
                sourcePanel,
                AccentBrush));

        var mapImport =
            CardHeading(
                "MAP IMPORTER",
                "Combat Arms World DAT",
                "Import a Combat Arms .DAT world into ignored local storage and stage it into rez/Worlds when the runtime is available.",
                AccentRedBrush);

        var mapRow =
            new Grid
            {
                ColumnSpacing = 12,
                Margin =
                    new Thickness(
                        0, 8, 0, 0)
            };

        mapRow.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        mapRow.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    GridLength.Auto
            });

        var mapSourceButton =
            SecondaryButton(
                "SELECT MAP .DAT");
        mapSourceButton.HorizontalAlignment =
            HorizontalAlignment.Stretch;
        mapSourceButton.Click +=
            SelectCaMapSourceButton_Click;

        CaMapSourceText.Text =
            "Not selected";
        CaMapSourceText.Foreground =
            SecondaryTextBrush;
        CaMapSourceText.TextWrapping =
            TextWrapping.Wrap;
        CaMapSourceText.Margin =
            new Thickness(
                0, 6, 0, 0);

        var mapSource =
            new StackPanel
            {
                Spacing = 4
            };
        mapSource.Children.Add(
            mapSourceButton);
        mapSource.Children.Add(
            CaMapSourceText);

        CaMapImportButton.Content =
            "IMPORT MAP";
        CaMapImportButton.Background =
            AccentRedBrush;
        CaMapImportButton.Foreground =
            PrimaryTextBrush;
        CaMapImportButton.BorderBrush =
            AccentRedBrush;
        CaMapImportButton.Padding =
            new Thickness(
                18, 9, 18, 9);
        CaMapImportButton.IsEnabled =
            false;
        CaMapImportButton.VerticalAlignment =
            VerticalAlignment.Top;
        CaMapImportButton.Click +=
            ImportCombatArmsMapButton_Click;

        Grid.SetColumn(
            mapSource,
            0);
        Grid.SetColumn(
            CaMapImportButton,
            1);

        mapRow.Children.Add(
            mapSource);
        mapRow.Children.Add(
            CaMapImportButton);

        mapImport.Children.Add(
            mapRow);

        page.Children.Add(
            Card(
                mapImport,
                AccentRedBrush));

        var audit =
            CardHeading(
                "LIVE AUDIT",
                "Importer Log",
                "Weapon reports stay under assets-local/WeaponImports. Attachment imports stay under assets-local/AttachmentImports.",
                CyanBrush);

        CaImportLogBox.AcceptsReturn =
            true;
        CaImportLogBox.IsReadOnly =
            true;
        CaImportLogBox.TextWrapping =
            TextWrapping.Wrap;
        CaImportLogBox.MinHeight =
            350;
        CaImportLogBox.FontFamily =
            new FontFamily(
                "Consolas");
        CaImportLogBox.FontSize =
            12;
        audit.Children.Add(
            CaImportLogBox);

        var actions =
            new StackPanel
            {
                Orientation =
                    Orientation.Horizontal,
                Spacing = 10
            };

        var openWeapons =
            SecondaryButton(
                "OPEN WEAPON IMPORTS");
        openWeapons.Click +=
            (sender, args) =>
            {
                var repo =
                    LauncherPaths.FindRepositoryDirectory();

                if(repo is not null)
                {
                    LauncherPaths.OpenFolder(
                        Path.Combine(
                            repo,
                            "assets-local",
                            "WeaponImports"));
                }
            };

        var openAttachments =
            SecondaryButton(
                "OPEN ATTACHMENT IMPORTS");
        openAttachments.Click +=
            (sender, args) =>
            {
                var repo =
                    LauncherPaths.FindRepositoryDirectory();

                if(repo is not null)
                {
                    LauncherPaths.OpenFolder(
                        Path.Combine(
                            repo,
                            "assets-local",
                            "AttachmentImports"));
                }
            };

        var openMaps =
            SecondaryButton(
                "OPEN MAP IMPORTS");
        openMaps.Click +=
            (sender, args) =>
            {
                var repo =
                    LauncherPaths.FindRepositoryDirectory();

                if(repo is not null)
                {
                    LauncherPaths.OpenFolder(
                        Path.Combine(
                            repo,
                            "assets-local",
                            "MapImports"));
                }
            };

        var back =
            SecondaryButton(
                "BACK TO TOOLS");
        back.Click +=
            (sender, args) =>
                NavigateTo(
                    "tools");

        actions.Children.Add(
            openWeapons);
        actions.Children.Add(
            openAttachments);
        actions.Children.Add(
            openMaps);
        actions.Children.Add(
            back);
        audit.Children.Add(
            actions);

        page.Children.Add(
            Card(
                audit,
                CyanBrush));

        CaImportView.Content =
            page;
    }

    private void BuildArsenalView()
    {
        ArsenalView.Visibility =
            Visibility.Collapsed;
        ArsenalView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;
        ArsenalView.VerticalScrollBarVisibility =
            ScrollBarVisibility.Auto;
        ArsenalView.HorizontalContentAlignment =
            HorizontalAlignment.Center;

        var page =
            new Grid
            {
                Margin =
                    new Thickness(
                        22,
                        18,
                        22,
                        26),
                ColumnSpacing = 16,
                MaxWidth = 1120,
                HorizontalAlignment =
                    HorizontalAlignment.Center
            };

        page.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(280)
            });
        page.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });

        var left =
            new StackPanel
            {
                Spacing = 12
            };

        WeaponSearchBox.PlaceholderText =
            "Search name, ID or category...";
        WeaponSearchBox.TextChanged +=
            WeaponSearchBox_TextChanged;
        left.Children.Add(
            WeaponSearchBox);

        WeaponList.DisplayMemberPath =
            "DisplayName";
        WeaponList.MinHeight = 430;
        WeaponList.MaxHeight = 600;
        WeaponList.SelectionChanged +=
            WeaponList_SelectionChanged;
        left.Children.Add(
            WeaponList);

        var back =
            SecondaryButton(
                "BACK TO TOOLS");
        back.Click +=
            (sender, args) =>
                NavigateTo("tools");
        left.Children.Add(back);

        var leftCard =
            Card(
                left,
                CyanBrush);

        Grid.SetColumn(
            leftCard,
            0);
        page.Children.Add(
            leftCard);

        var editor =
            CardHeading(
                "WEAPON AUTHORING",
                "Definition",
                "Gameplay authority remains server-side. Presentation, animation aliases and loadout availability are data-driven here.",
                AccentBrush);

        WeaponTitleText.Text =
            "Select a weapon";
        WeaponTitleText.FontSize = 22;
        WeaponTitleText.Foreground =
            PrimaryTextBrush;
        editor.Children.Add(
            WeaponTitleText);

        WeaponSectionText.Foreground =
            SecondaryTextBrush;
        WeaponSectionText.TextWrapping =
            TextWrapping.Wrap;
        editor.Children.Add(
            WeaponSectionText);

        var fields =
            new Grid
            {
                ColumnSpacing = 14,
                RowSpacing = 10,
                Margin =
                    new Thickness(
                        0,
                        10,
                        0,
                        0)
            };

        for(var i = 0;
            i < 4;
            ++i)
        {
            fields.RowDefinitions.Add(
                new RowDefinition
                {
                    Height =
                        GridLength.Auto
                });
        }

        fields.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(90)
            });
        fields.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        fields.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(90)
            });
        fields.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });

        AddEditorField(
            fields, 0, 0,
            "NAME",
            WeaponNameBox);
        AddEditorField(
            fields, 0, 2,
            "ID",
            WeaponIdBox);
        AddEditorField(
            fields, 1, 0,
            "TYPE",
            WeaponTypeCombo);
        AddEditorField(
            fields, 1, 2,
            "DAMAGE",
            DamageBox);
        AddEditorField(
            fields, 2, 0,
            "MAGAZINE",
            ClipBox);
        AddEditorField(
            fields, 2, 2,
            "RESERVE",
            ReserveBox);
        AddEditorField(
            fields, 3, 0,
            "FIRE RATE",
            FireIntervalBox);
        AddEditorField(
            fields, 3, 2,
            "RELOAD",
            ReloadBox);

        editor.Children.Add(fields);

        editor.Children.Add(
            Eyebrow(
                "ANIMATION ALIASES",
                CyanBrush));

        editor.Children.Add(
            BodyText(
                "Use the animation names already embedded in an LTB from another LithTech title instead of renaming them in ModelEdit. Leave blank to keep FIRETEAM's current select/idle/fire/reload fallback behavior. FIRETEAM firing and reload timing are code-driven, so model string keyframes are not currently required."));

        var animationFields =
            new Grid
            {
                ColumnSpacing = 14,
                RowSpacing = 10
            };

        for(var i = 0;
            i < 3;
            ++i)
        {
            animationFields.RowDefinitions.Add(
                new RowDefinition
                {
                    Height =
                        GridLength.Auto
                });
        }

        animationFields.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(90)
            });
        animationFields.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        animationFields.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(90)
            });
        animationFields.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });

        AddEditorField(
            animationFields, 0, 0,
            "SELECT",
            AnimSelectBox);
        AddEditorField(
            animationFields, 0, 2,
            "IDLE",
            AnimIdleBox);
        AddEditorField(
            animationFields, 1, 0,
            "FIRE",
            AnimFireBox);
        AddEditorField(
            animationFields, 1, 2,
            "ALT FIRE",
            AnimAltFireBox);
        AddEditorField(
            animationFields, 2, 0,
            "RELOAD",
            AnimReloadBox);

        editor.Children.Add(
            animationFields);

        WeaponEnabledCheckBox.Content =
            "Enabled for Loadout";
        WeaponEnabledCheckBox.Foreground =
            PrimaryTextBrush;
        WeaponEnabledCheckBox.Margin =
            new Thickness(
                0,
                8,
                0,
                0);

        var enabledHelp =
            BodyText(
                "Enable/disable is stored in a tiny launcher sidecar now, so this checkbox no longer rewrites the entire imported CA catalog.");
        enabledHelp.Margin =
            new Thickness(
                26,
                -4,
                0,
                0);

        editor.Children.Add(
            WeaponEnabledCheckBox);
        editor.Children.Add(
            enabledHelp);

        var actions =
            new StackPanel
            {
                Orientation =
                    Orientation.Horizontal,
                Spacing = 10,
                Margin =
                    new Thickness(
                        0,
                        10,
                        0,
                        0)
            };

        var reload =
            SecondaryButton(
                "RELOAD CONFIG");
        reload.Click +=
            ReloadArsenalButton_Click;

        var save =
            PrimaryButton(
                "SAVE WEAPON");
        save.Click +=
            SaveWeaponButton_Click;

        actions.Children.Add(reload);
        actions.Children.Add(save);
        editor.Children.Add(actions);

        ArsenalStatusText.Foreground =
            SecondaryTextBrush;
        ArsenalStatusText.TextWrapping =
            TextWrapping.Wrap;
        editor.Children.Add(
            ArsenalStatusText);

        var editorCard =
            Card(
                editor,
                AccentBrush);

        Grid.SetColumn(
            editorCard,
            1);
        page.Children.Add(
            editorCard);

        ArsenalView.Content =
            page;
    }

    private static void AddEditorField(
        Grid grid,
        int row,
        int labelColumn,
        string label,
        FrameworkElement control)
    {
        var text =
            Eyebrow(
                label,
                MutedTextBrush);
        text.VerticalAlignment =
            VerticalAlignment.Center;

        Grid.SetRow(text, row);
        Grid.SetColumn(text, labelColumn);
        Grid.SetRow(control, row);
        Grid.SetColumn(
            control,
            labelColumn + 1);

        grid.Children.Add(text);
        grid.Children.Add(control);
    }

    private void BuildSettingsView()
    {
        SettingsView.Visibility =
            Visibility.Collapsed;
        SettingsView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;
        SettingsView.HorizontalContentAlignment =
            HorizontalAlignment.Center;

        // The top chrome already says SETTINGS; avoid repeating page titles.
        var page = new StackPanel
        {
            Spacing = 16,
            Margin = new Thickness(24, 18, 24, 32),
            MaxWidth = 1120,
            HorizontalAlignment = HorizontalAlignment.Center
        };

        var groups =
            new Grid
            {
                ColumnSpacing = 16,
                RowSpacing = 16
            };

        groups.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        groups.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        groups.RowDefinitions.Add(
            new RowDefinition
            {
                Height =
                    GridLength.Auto
            });
        groups.RowDefinitions.Add(
            new RowDefinition
            {
                Height =
                    GridLength.Auto
            });

        var display =
            CardHeading(
                "DISPLAY",
                "Video",
                "Resolution, display mode and the existing LithTech gamma setting.");

        WindowedToggle.OnContent =
            "Windowed";
        WindowedToggle.OffContent =
            "Fullscreen";

        display.Children.Add(
            LabeledControl(
                "RESOLUTION",
                ResolutionCombo));
        display.Children.Add(
            LabeledControl(
                "DISPLAY MODE",
                WindowedToggle));

        SetupNumberBox(
            GammaBox,
            0.5,
            6.0,
            0.1);
        display.Children.Add(
            LabeledControl(
                "GAMMA",
                GammaBox));

        var input =
            CardHeading(
                "INPUT",
                "Mouse",
                "FIRETEAM already stores horizontal and vertical mouse scales separately.",
                CyanBrush);

        SetupNumberBox(
            SensitivityXBox,
            0.01,
            4.0,
            0.01);
        SetupNumberBox(
            SensitivityYBox,
            0.01,
            4.0,
            0.01);

        input.Children.Add(
            LabeledControl(
                "HORIZONTAL",
                SensitivityXBox));
        input.Children.Add(
            LabeledControl(
                "VERTICAL",
                SensitivityYBox));

        var audio =
            CardHeading(
                "AUDIO",
                "Game Audio",
                "Current FIRETEAM master game volume.");

        SetupNumberBox(
            VolumeBox,
            0,
            100,
            5);
        audio.Children.Add(
            LabeledControl(
                "GAME VOLUME",
                VolumeBox));

        var displayCard =
            Card(
                display,
                AccentBrush);
        var inputCard =
            Card(
                input,
                CyanBrush);
        var audioCard =
            Card(
                audio,
                AccentBrush);
        Grid.SetRow(displayCard, 0);
        Grid.SetColumn(displayCard, 0);
        Grid.SetRow(inputCard, 0);
        Grid.SetColumn(inputCard, 1);
        Grid.SetRow(audioCard, 1);
        Grid.SetColumn(audioCard, 0);
        Grid.SetColumnSpan(audioCard, 2);

        groups.Children.Add(displayCard);
        groups.Children.Add(inputCard);
        groups.Children.Add(audioCard);

        page.Children.Add(groups);

        var saveRow =
            new StackPanel
            {
                Orientation =
                    Orientation.Horizontal,
                Spacing = 14
            };

        var save =
            PrimaryButton(
                "SAVE SETTINGS");
        save.Click +=
            SaveSettingsButton_Click;

        SettingsStatusText.Foreground =
            SecondaryTextBrush;
        SettingsStatusText.VerticalAlignment =
            VerticalAlignment.Center;

        saveRow.Children.Add(save);
        saveRow.Children.Add(
            SettingsStatusText);

        page.Children.Add(saveRow);
        SettingsView.Content = page;
    }

    private static void SetupNumberBox(
        NumberBox box,
        double minimum,
        double maximum,
        double step)
    {
        box.Minimum = minimum;
        box.Maximum = maximum;
        box.SmallChange = step;
        box.SpinButtonPlacementMode =
            NumberBoxSpinButtonPlacementMode.Inline;
    }

    private void SelectPlayMode(
        string mode)
    {
        SelectString(
            ModeCombo,
            mode,
            "Single Player");
        RefreshModeVisibility();
        NavigateTo("home");
    }

    private void NavigateTo(
        string tag)
    {
        ShowView(tag);

        if(tag == "loadout")
        {
            ReloadWeapons();
        }
        else if(tag ==
                "weapon-editor")
        {
            // QA mode can change the runtime quarantine file while the
            // launcher remains open. Reload here so quarantined weapons
            // disappear as soon as the user returns to the editor.
            ReloadArsenal();
        }
        else if(tag == "tools")
        {
            RefreshToolPaths();
        }
        else if(tag == "mini-tools")
        {
            MiniToolsStatusText.Text =
                LauncherPaths.FindMiniTool(
                    "RezExtract.exe") is null
                    ? "RezExtract.exe not found yet."
                    : "RezExtract.exe detected and ready.";
        }
        else if(tag == "profile")
        {
            RefreshPlayerProfile();
        }
        else if(tag == "settings")
        {
            LoadSettings();
        }
    }
}
