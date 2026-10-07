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
        BuildLoadoutView();
        BuildArsenalView();
        BuildModsView();
        BuildSettingsView();

        content.Children.Add(HomeView);
        content.Children.Add(LoadoutView);
        content.Children.Add(ArsenalView);
        content.Children.Add(ModsView);
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

        var title = new TextBlock
        {
            Text = "FIRETEAM",
            FontFamily =
                new FontFamily(
                    "Bahnschrift SemiCondensed"),
            FontSize = 19,
            FontWeight =
                Microsoft.UI.Text.FontWeights.Bold,
            CharacterSpacing = 35,
            Foreground = PrimaryTextBrush,
            VerticalAlignment =
                VerticalAlignment.Center,
            Margin =
                new Thickness(16, 0, 0, 0)
        };

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

        playButton.Flyout =
            playMenu;
        bar.Children.Add(
            playButton);

        bar.Children.Add(
            NavButton(
                "LOADOUT",
                "loadout"));

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
                MaxWidth = 1220,
                HorizontalAlignment =
                    HorizontalAlignment.Stretch
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

    private void BuildHomeView()
    {
        HomeView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;
        HomeView.VerticalScrollBarVisibility =
            ScrollBarVisibility.Auto;

        var page =
            new StackPanel
            {
                Spacing = 18,
                Margin =
                    new Thickness(
                        30,
                        20,
                        30,
                        30),
                MaxWidth = 1220,
                HorizontalAlignment =
                    HorizontalAlignment.Stretch
            };

        var header =
            new Grid
            {
                ColumnSpacing = 24
            };

        header.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1,
                        GridUnitType.Star)
            });
        header.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    GridLength.Auto
            });

        var identity =
            new StackPanel
            {
                Spacing = 4
            };

        identity.Children.Add(
            Eyebrow(
                "FIRETEAM / LOCAL ALPHA"));
        identity.Children.Add(
            SectionTitle(
                "Ready Room",
                32));
        identity.Children.Add(
            BodyText(
                "Configure a run, review your loadout and deploy. Cabin Fever is the first supported FIRETEAM map, not the launcher identity."));

        header.Children.Add(identity);

        var launch =
            new StackPanel
            {
                Spacing = 6,
                HorizontalAlignment =
                    HorizontalAlignment.Right,
                VerticalAlignment =
                    VerticalAlignment.Center,
                MinWidth = 220
            };

        PlayButton.Content =
            "LAUNCH GAME";
        PlayButton.Background =
            AccentBrush;
        PlayButton.Foreground =
            new SolidColorBrush(
                Color.FromArgb(
                    255, 24, 16, 0));
        PlayButton.BorderBrush =
            AccentBrush;
        PlayButton.Padding =
            new Thickness(
                26,
                11,
                26,
                11);
        PlayButton.MinHeight = 46;
        PlayButton.HorizontalAlignment =
            HorizontalAlignment.Right;
        PlayButton.Click +=
            PlayButton_Click;

        RunStatusText.TextWrapping =
            TextWrapping.Wrap;
        RunStatusText.TextAlignment =
            TextAlignment.Right;
        RunStatusText.Foreground =
            SecondaryTextBrush;
        RunStatusText.MaxWidth = 310;

        launch.Children.Add(
            PlayButton);
        launch.Children.Add(
            RunStatusText);

        Grid.SetColumn(
            launch,
            1);
        header.Children.Add(
            launch);

        page.Children.Add(
            header);

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
        var difficulty =
            LabeledControl(
                "DIFFICULTY",
                DifficultyCombo);

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
        Grid.SetRow(join, 1);
        Grid.SetColumn(join, 1);

        quickFields.Children.Add(player);
        quickFields.Children.Add(mode);
        quickFields.Children.Add(difficulty);
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

        var page =
            NewPagePanel(
                "FIRETEAM / LOADOUT",
                "Deployment Kit",
                "Choose the equipment you take into the round. Player-facing loadout work stays here; authoring tools live under Tools.");

        var columns =
            new Grid
            {
                ColumnSpacing = 18
            };

        columns.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        1.45,
                        GridUnitType.Star)
            });
        columns.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(
                        0.75,
                        GridUnitType.Star)
            });

        var slots =
            CardHeading(
                "ACTIVE SLOTS",
                "Loadout Builder",
                "Available choices come directly from config/weapons.cfg.");

        var combos =
            new[]
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

        slots.Children.Add(
            LabeledControl(
                "PRIMARY",
                PrimaryCombo));
        slots.Children.Add(
            LabeledControl(
                "SIDEARM",
                SidearmCombo));
        slots.Children.Add(
            LabeledControl(
                "MELEE",
                MeleeCombo));
        slots.Children.Add(
            LabeledControl(
                "EXTRA",
                SecondaryCombo));
        slots.Children.Add(
            LabeledControl(
                "SPECIAL",
                SpecialCombo));

        var actions =
            new StackPanel
            {
                Orientation =
                    Orientation.Horizontal,
                Spacing = 10,
                Margin =
                    new Thickness(0, 10, 0, 0)
            };

        var apply =
            PrimaryButton(
                "APPLY LOADOUT");
        apply.Click +=
            ApplyLoadoutButton_Click;

        var editor =
            SecondaryButton(
                "WEAPON EDITOR");
        editor.Click +=
            OpenArsenalButton_Click;

        actions.Children.Add(apply);
        actions.Children.Add(editor);
        slots.Children.Add(actions);

        var info =
            CardHeading(
                "LOADOUT STATUS",
                "Equipment Rules",
                "Five active slots are written back to both source and runtime config when available.",
                CyanBrush);

        LoadoutStatusText.TextWrapping =
            TextWrapping.Wrap;
        LoadoutStatusText.Foreground =
            SecondaryTextBrush;
        LoadoutStatusText.Margin =
            new Thickness(0, 10, 0, 0);
        info.Children.Add(
            LoadoutStatusText);
        info.Children.Add(
            BodyText(
                "Future pass: weapon cards, icons, unlock state, stats and progression presentation."));

        var slotCard =
            Card(
                slots,
                AccentBrush);
        var infoCard =
            Card(
                info,
                CyanBrush);

        Grid.SetColumn(slotCard, 0);
        Grid.SetColumn(infoCard, 1);
        columns.Children.Add(slotCard);
        columns.Children.Add(infoCard);

        page.Children.Add(columns);
        LoadoutView.Content = page;
    }

    private void BuildModsView()
    {
        ModsView.Visibility =
            Visibility.Collapsed;
        ModsView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;

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

        var future =
            CardHeading(
                "NEXT TOOLING",
                "Round / Powerup Tuner",
                "Planned: data-driven difficulty, infected, round and powerup editing without recompiling FIRETEAM.",
                MutedTextBrush);
        future.Children.Add(
            BodyText(
                "This follows after the current gameplay configs stabilize."));

        var futureCard =
            Card(
                future,
                MutedTextBrush);

        Grid.SetRow(weapon, 0);
        Grid.SetColumn(weapon, 0);
        Grid.SetRow(mods, 0);
        Grid.SetColumn(mods, 1);
        Grid.SetRow(content, 1);
        Grid.SetColumn(content, 0);
        Grid.SetRow(futureCard, 1);
        Grid.SetColumn(futureCard, 1);

        tools.Children.Add(weapon);
        tools.Children.Add(mods);
        tools.Children.Add(content);
        tools.Children.Add(futureCard);

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

    private void BuildArsenalView()
    {
        ArsenalView.Visibility =
            Visibility.Collapsed;
        ArsenalView.Margin =
            new Thickness(30, 24, 30, 30);
        ArsenalView.ColumnSpacing = 18;

        ArsenalView.ColumnDefinitions.Add(
            new ColumnDefinition
            {
                Width =
                    new GridLength(330)
            });
        ArsenalView.ColumnDefinitions.Add(
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

        left.Children.Add(
            Eyebrow(
                "TOOLS / WEAPON EDITOR",
                CyanBrush));
        left.Children.Add(
            SectionTitle(
                "Weapon Catalog",
                28));
        left.Children.Add(
            BodyText(
                "Development/content tool. Search an installed definition, edit supported values, then save back to config."));

        WeaponSearchBox.PlaceholderText =
            "Search name or ID...";
        WeaponSearchBox.TextChanged +=
            WeaponSearchBox_TextChanged;
        left.Children.Add(
            WeaponSearchBox);

        WeaponList.DisplayMemberPath =
            "DisplayName";
        WeaponList.MinHeight = 420;
        WeaponList.MaxHeight = 560;
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

        Grid.SetColumn(leftCard, 0);
        ArsenalView.Children.Add(leftCard);

        var editor =
            CardHeading(
                "WEAPON AUTHORING",
                "Definition",
                "Gameplay authority remains server-side. This editor changes external FIRETEAM weapon data.",
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
                    new Thickness(0, 10, 0, 0)
            };

        for(var i = 0; i < 4; ++i)
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

        WeaponEnabledCheckBox.Content =
            "Enabled for Loadout";
        WeaponEnabledCheckBox.Foreground =
            PrimaryTextBrush;
        WeaponEnabledCheckBox.Margin =
            new Thickness(0, 8, 0, 0);

        var enabledHelp =
            BodyText(
                "Disabled catalog weapons stay in the Weapon Editor but are hidden from the player loadout picker. Active slot definitions are always enabled.");
        enabledHelp.Margin =
            new Thickness(26, -4, 0, 0);

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
                    new Thickness(0, 10, 0, 0)
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

        Grid.SetColumn(editorCard, 1);
        ArsenalView.Children.Add(
            editorCard);
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

        var page =
            NewPagePanel(
                "FIRETEAM / SETTINGS",
                "Game Settings",
                "Launcher-side settings mirror the real FIRETEAM runtime file. Unsupported engine switches are not exposed until their LithTech variables are verified.");

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

        var developer =
            CardHeading(
                "NEXT SETTINGS PASS",
                "Engine / Developer",
                "VSync, FPS limit, debug menu and related switches need their actual Jupiter console variables verified before the launcher writes them.",
                AccentRedBrush);

        developer.Children.Add(
            BodyText(
                "They are deliberately not fake toggles in this build. Once verified, they will live here with clear restart/apply behavior."));

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
        var developerCard =
            Card(
                developer,
                AccentRedBrush);

        Grid.SetRow(displayCard, 0);
        Grid.SetColumn(displayCard, 0);
        Grid.SetRow(inputCard, 0);
        Grid.SetColumn(inputCard, 1);
        Grid.SetRow(audioCard, 1);
        Grid.SetColumn(audioCard, 0);
        Grid.SetRow(developerCard, 1);
        Grid.SetColumn(developerCard, 1);

        groups.Children.Add(displayCard);
        groups.Children.Add(inputCard);
        groups.Children.Add(audioCard);
        groups.Children.Add(developerCard);

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
            ReloadArsenal();
        }
        else if(tag == "tools")
        {
            RefreshToolPaths();
        }
        else if(tag == "settings")
        {
            LoadSettings();
        }
    }
}
