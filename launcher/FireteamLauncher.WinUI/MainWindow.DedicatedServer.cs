using FireteamLauncher.Infrastructure;
using FireteamLauncher.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace FireteamLauncher;

public sealed partial class MainWindow
{
    private readonly ScrollViewer DedicatedServerView = new();
    private readonly DedicatedServerService _dedicatedServer = new();
    private readonly TextBox DedicatedNameBox = new();
    private readonly NumberBox DedicatedPortBox = new();
    private readonly NumberBox DedicatedPlayersBox = new();
    private readonly ComboBox DedicatedMapCombo = new();
    private readonly Slider DedicatedDifficultySlider = new();
    private readonly TextBlock DedicatedDifficultyValue = new();
    private readonly NumberBox DedicatedPrepBox = new();
    private readonly ComboBox DedicatedVisibilityCombo = new();
    private readonly PasswordBox DedicatedPinBox = new();
    private readonly ToggleSwitch DedicatedOnlineToggle = new();
    private readonly TextBox DedicatedHubUrlBox = new();
    private readonly ToggleSwitch DedicatedStatsToggle = new();
    private readonly ListView DedicatedModsList = new();
    private readonly TextBlock DedicatedModsStatus = new();
    private readonly TextBlock DedicatedStatus = new();

    private void BuildDedicatedServerView()
    {
        DedicatedServerView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;
        DedicatedServerView.VerticalScrollBarVisibility =
            ScrollBarVisibility.Auto;

        var page = NewPagePanel(
            "FIRETEAM / HOSTING",
            "Dedicated Server",
            "Independent headless hosting. Server settings do not overwrite your Quick Play profile.");

        var basics = new StackPanel { Spacing = 12 };
        basics.Children.Add(CardHeading("IDENTITY & NETWORK", "Server setup",
            "The port must be reachable through your firewall/router for Internet joins. Public listing is opt-in."));
        DedicatedNameBox.PlaceholderText = "Server name";
        DedicatedNameBox.MaxLength = 64;
        basics.Children.Add(LabeledControl("Server name", DedicatedNameBox));
        DedicatedPortBox.Minimum = 1;
        DedicatedPortBox.Maximum = 65535;
        DedicatedPortBox.SmallChange = 1;
        DedicatedPortBox.Width = 155;
        basics.Children.Add(LabeledControl("Network port", DedicatedPortBox));
        DedicatedPlayersBox.Minimum = 1;
        DedicatedPlayersBox.Maximum = 24;
        DedicatedPlayersBox.SmallChange = 1;
        DedicatedPlayersBox.Width = 155;
        basics.Children.Add(LabeledControl("Player slots (1-24)", DedicatedPlayersBox));
        page.Children.Add(Card(basics));

        var gameplay = new StackPanel { Spacing = 12 };
        gameplay.Children.Add(CardHeading("GAMEPLAY", "Map & difficulty",
            "Only locally staged DAT maps can be hosted. Difficulty is independent of Quick Play."));
        DedicatedMapCombo.MinWidth = 220;
        gameplay.Children.Add(LabeledControl("Starting map", DedicatedMapCombo));
        DedicatedDifficultySlider.Minimum = 1;
        DedicatedDifficultySlider.Maximum = 10;
        DedicatedDifficultySlider.StepFrequency = 1;
        DedicatedDifficultySlider.Width = 280;
        DedicatedDifficultySlider.ValueChanged += (s, e) =>
            DedicatedDifficultyValue.Text =
                $"Difficulty {(int)Math.Round(DedicatedDifficultySlider.Value)} / 10";
        var difficultyRow = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 14 };
        difficultyRow.Children.Add(DedicatedDifficultySlider);
        DedicatedDifficultyValue.VerticalAlignment = VerticalAlignment.Center;
        DedicatedDifficultyValue.Foreground = SecondaryTextBrush;
        difficultyRow.Children.Add(DedicatedDifficultyValue);
        gameplay.Children.Add(LabeledControl("Difficulty", difficultyRow));
        DedicatedPrepBox.Minimum = 0;
        DedicatedPrepBox.Maximum = 120;
        DedicatedPrepBox.SmallChange = 5;
        DedicatedPrepBox.Width = 155;
        gameplay.Children.Add(LabeledControl("Round 1 prep (sec)", DedicatedPrepBox));
        page.Children.Add(Card(gameplay, CyanBrush));

        var access = new StackPanel { Spacing = 12 };
        access.Children.Add(CardHeading("ACCESS", "Visibility & privacy",
            "LAN/unlisted by default. Online heartbeat is opt-in; reachability and router mapping remain separate."));
        DedicatedVisibilityCombo.ItemsSource = new[]
        {
            "Public / unlisted", "Private / 4-digit PIN (unavailable)"
        };
        DedicatedVisibilityCombo.SelectionChanged += (s, e) =>
        {
            DedicatedPinBox.IsEnabled = DedicatedVisibilityCombo.SelectedIndex == 1;
        };
        access.Children.Add(LabeledControl("Visibility", DedicatedVisibilityCombo));
        DedicatedPinBox.MaxLength = 4;
        DedicatedPinBox.PasswordChar = "●";
        DedicatedPinBox.Width = 150;
        DedicatedPinBox.PlaceholderText = "4-digit PIN";
        access.Children.Add(LabeledControl("Private PIN", DedicatedPinBox));

        DedicatedOnlineToggle.Header =
            "Advertise server on FIRETEAM Hub (publishes my Internet IP)";
        access.Children.Add(DedicatedOnlineToggle);
        DedicatedHubUrlBox.PlaceholderText = "https://your-fireteam-hub.example";
        DedicatedHubUrlBox.MaxLength = 240;
        DedicatedHubUrlBox.MinWidth = 400;
        access.Children.Add(LabeledControl("Hub address", DedicatedHubUrlBox));
        access.Children.Add(BodyText(
            "Public listing does not open your router port. " +
            "Other players may see an UNVERIFIED listing until Internet connectivity is solved."));
        page.Children.Add(Card(access));

        var mods = new StackPanel { Spacing = 10 };
        mods.Children.Add(CardHeading("CONTENT", "Server mod selection",
            "Local packages are listed for planning only. A verified manifest, engine mount order, and join handshake are still needed."));
        DedicatedModsList.SelectionMode = ListViewSelectionMode.Multiple;
        DedicatedModsList.MaxHeight = 125;
        DedicatedModsList.MinHeight = 54;
        mods.Children.Add(DedicatedModsList);
        DedicatedModsStatus.Foreground = SecondaryTextBrush;
        DedicatedModsStatus.TextWrapping = TextWrapping.Wrap;
        mods.Children.Add(DedicatedModsStatus);
        var rescan = SecondaryButton("RESCAN MOD FOLDER");
        rescan.Click += (s, e) => RefreshDedicatedModList();
        mods.Children.Add(rescan);
        page.Children.Add(Card(mods));

        var stats = new StackPanel { Spacing = 10 };
        stats.Children.Add(CardHeading("MATCH HISTORY", "Statistics checkpoints",
            "Server-owned match JSON, saved about every 15 seconds and on round completion."));
        DedicatedStatsToggle.Header = "Save match history for this server";
        stats.Children.Add(DedicatedStatsToggle);
        page.Children.Add(Card(stats, CyanBrush));

        var actions = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 10 };
        var save = SecondaryButton("SAVE SERVER PRESET");
        save.Click += (s, e) => SaveDedicatedPreset();
        actions.Children.Add(save);
        var start = PrimaryButton("START DEDICATED");
        start.Click += (s, e) => LaunchDedicatedServer();
        actions.Children.Add(start);
        var stop = SecondaryButton("REQUEST STOP");
        stop.Click += (s, e) => StopDedicatedServer();
        actions.Children.Add(stop);
        page.Children.Add(actions);

        DedicatedStatus.Foreground = SecondaryTextBrush;
        DedicatedStatus.TextWrapping = TextWrapping.Wrap;
        page.Children.Add(DedicatedStatus);
        DedicatedServerView.Content = page;
    }

    private void LoadDedicatedProfile()
    {
        var profile = _dedicatedServer.Load();
        DedicatedNameBox.Text = profile.Name;
        DedicatedPortBox.Value = profile.Port;
        DedicatedPlayersBox.Value = profile.MaxPlayers;
        DedicatedDifficultySlider.Value = profile.Difficulty;
        DedicatedPrepBox.Value = profile.FirstRoundPrepSeconds;
        DedicatedStatsToggle.IsOn = profile.TrackStats;
        DedicatedVisibilityCombo.SelectedIndex = profile.Private ? 1 : 0;
        DedicatedPinBox.Password = "";
        DedicatedOnlineToggle.IsOn = profile.PublishOnline;
        DedicatedHubUrlBox.Text = string.IsNullOrWhiteSpace(profile.HubUrl)
            ? HubAddressService.Load() ?? ""
            : profile.HubUrl;
        RefreshDedicatedMapOptions(profile.Map);
        RefreshDedicatedModList(profile.Mods);
        DedicatedStatus.Text =
            "Hosting is experimental until build-dedicated.cmd passes Windows and LAN join testing. " +
            "Use public/unlisted without mods for the first smoke test.";
    }

    private void RefreshDedicatedMapOptions(string? preferred = null)
    {
        var maps = (MapCombo.ItemsSource as IEnumerable<string>)?.ToArray()
                   ?? ["CABINFEVER"];
        DedicatedMapCombo.ItemsSource = maps;
        DedicatedMapCombo.SelectedItem = maps.FirstOrDefault(
            m => m.Equals(preferred ?? "", StringComparison.OrdinalIgnoreCase))
            ?? maps.FirstOrDefault(m => m == "CABINFEVER")
            ?? maps.FirstOrDefault();
    }

    private void RefreshDedicatedModList(string[]? selected = null)
    {
        var options = _dedicatedServer.DiscoverLocalMods();
        DedicatedModsList.ItemsSource = options;
        if(selected is not null)
            foreach(var entry in options.Where(x => selected.Contains(x,
                         StringComparer.OrdinalIgnoreCase)))
                DedicatedModsList.SelectedItems.Add(entry);
        DedicatedModsStatus.Text = options.Count == 0
            ? "No .ftmod or .zip packages in BUILT/Mods. An empty selection runs the normal game."
            : $"{options.Count} local package(s) detected. Selected mods are blocked at launch until runtime loading is supported.";
    }

    private static int ReadDedicatedNumber(NumberBox control, int min, int max, string label)
    {
        var value = control.Value;
        if(double.IsNaN(value) || value < min || value > max ||
           Math.Floor(value) != value)
            throw new ArgumentException($"{label} must be a whole number from {min} to {max}.");
        return (int)value;
    }

    private DedicatedHostProfile GetDedicatedProfile()
    {
        var profile = new DedicatedHostProfile(
            (DedicatedNameBox.Text ?? "").Trim(),
            DedicatedMapCombo.SelectedItem?.ToString() ?? "",
            (int)Math.Round(DedicatedDifficultySlider.Value),
            ReadDedicatedNumber(DedicatedPortBox, 1, 65535, "Port"),
            ReadDedicatedNumber(DedicatedPlayersBox, 1, 24, "Player slots"),
            DedicatedVisibilityCombo.SelectedIndex == 1,
            DedicatedPinBox.Password,
            DedicatedStatsToggle.IsOn,
            ReadDedicatedNumber(DedicatedPrepBox, 0, 120, "Preparation"),
            DedicatedModsList.SelectedItems.OfType<string>().ToArray(),
            DedicatedOnlineToggle.IsOn,
            DedicatedHubUrlBox.Text?.Trim() ?? "");
        return profile;
    }

    private void SaveDedicatedPreset()
    {
        try
        {
            _dedicatedServer.Save(GetDedicatedProfile());
            DedicatedStatus.Text =
                "Server preset saved locally. Private PIN values are never saved.";
        }
        catch(Exception ex)
        {
            DedicatedStatus.Text = "Preset not saved: " + ex.Message;
        }
    }

    private void LaunchDedicatedServer()
    {
        try
        {
            DedicatedStatus.Text = _dedicatedServer.Start(GetDedicatedProfile());
        }
        catch(Exception ex)
        {
            DedicatedStatus.Text = "Dedicated launch blocked: " + ex.Message;
        }
    }

    private void StopDedicatedServer()
    {
        try
        {
            DedicatedStatus.Text = _dedicatedServer.RequestStop();
        }
        catch(Exception ex)
        {
            DedicatedStatus.Text = "Cannot request stop: " + ex.Message;
        }
    }
}
