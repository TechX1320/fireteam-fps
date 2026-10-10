using FireteamLauncher.Infrastructure;
using FireteamLauncher.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace FireteamLauncher;

public sealed partial class MainWindow
{
    private readonly ScrollViewer DedicatedServerView = new();
    private readonly DedicatedServerService _dedicatedServer = new();
    private readonly LocalHubService _localHub = new();
    private readonly InfoBar DedicatedBanner = new();
    private Button? DedicatedStartButton;
    private bool _dedicatedLaunchBusy;
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
    private readonly ToggleSwitch DedicatedRouterToggle = new();
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

        // Hosting state must be visible even when the normal status text
        // is below the controls. A blocked launch cannot be silent.
        DedicatedBanner.IsOpen = true;
        DedicatedBanner.IsClosable = false;
        DedicatedBanner.Title = "Dedicated server";
        DedicatedBanner.Severity = InfoBarSeverity.Informational;
        DedicatedBanner.Message = "Not started by this launcher.";
        page.Children.Add(DedicatedBanner);

        _dedicatedServer.ProcessExited += code =>
            DispatcherQueue.TryEnqueue(() =>
            {
                UpdateDedicatedStartButton();
                SetDedicatedStatus(
                    $"Dedicated server process exited (code {code}). " +
                    "See the server console for details.",
                    code == 0 ? InfoBarSeverity.Informational : InfoBarSeverity.Error,
                    "Dedicated process exited");
            });

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
            "Advertise server on FIRETEAM Hub (may publish my Internet IP)";
        DedicatedOnlineToggle.Toggled += async (sender, args) =>
        {
            if(!DedicatedOnlineToggle.IsOn ||
               !string.IsNullOrWhiteSpace(DedicatedHubUrlBox.Text))
                return;
            var local = await HubAddressService.FindRunningLocalHubAsync();
            if(local is not null &&
               string.IsNullOrWhiteSpace(DedicatedHubUrlBox.Text))
                DedicatedHubUrlBox.Text = local;
        };
        access.Children.Add(DedicatedOnlineToggle);
        DedicatedRouterToggle.Header =
            "Automatically request UPnP for detected game ports (opt-in)";
        access.Children.Add(DedicatedRouterToggle);
        DedicatedHubUrlBox.PlaceholderText =
            "Not configured — use the LOCAL HUB button or enter HTTPS URL";
        DedicatedHubUrlBox.MaxLength = 240;
        DedicatedHubUrlBox.MinWidth = 400;
        access.Children.Add(LabeledControl("Hub address", DedicatedHubUrlBox));
        var localHubActions = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10
        };
        var selectLocalHub = SecondaryButton("USE RUNNING LOCAL HUB");
        selectLocalHub.Click += async (sender, args) => await SelectLocalHubAsync();
        localHubActions.Children.Add(selectLocalHub);
        var launchLocalHub = SecondaryButton("START LOCAL HUB");
        launchLocalHub.Click += async (sender, args) => await LaunchLocalHubAsync();
        localHubActions.Children.Add(launchLocalHub);
        access.Children.Add(localHubActions);
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
        DedicatedStartButton = PrimaryButton("START DEDICATED");
        DedicatedStartButton.Click += (s, e) => LaunchDedicatedServer();
        actions.Children.Add(DedicatedStartButton);
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
        DedicatedRouterToggle.IsOn = profile.AutoConfigureRouter;
        DedicatedHubUrlBox.Text = string.IsNullOrWhiteSpace(profile.HubUrl)
            ? HubAddressService.Load() ?? ""
            : profile.HubUrl;
        RefreshDedicatedMapOptions(profile.Map);
        RefreshDedicatedModList(profile.Mods);
        SetDedicatedStatus(
            "Preset loaded. Start Dedicated launches a separate game server; " +
            "the Hub only provides the directory. Local Hub advertising is optional.",
            InfoBarSeverity.Informational, "Dedicated server ready");
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
            DedicatedHubUrlBox.Text?.Trim() ?? "",
            DedicatedRouterToggle.IsOn);
        return profile;
    }

    private void UpdateDedicatedStartButton()
    {
        if(DedicatedStartButton is null) return;
        var running = _dedicatedServer.IsRunning;
        DedicatedStartButton.IsEnabled = !running && !_dedicatedLaunchBusy;
        DedicatedStartButton.Content = running
            ? "DEDICATED RUNNING"
            : _dedicatedLaunchBusy ? "STARTING..." : "START DEDICATED";
    }

    private void SetDedicatedStatus(
        string message, InfoBarSeverity severity = InfoBarSeverity.Informational,
        string title = "Dedicated server")
    {
        DedicatedStatus.Text = message;
        DedicatedBanner.Title = title;
        DedicatedBanner.Message = message;
        DedicatedBanner.Severity = severity;
        DedicatedBanner.IsOpen = true;
    }

    private void SetLocalHubAddress(string url)
    {
        DedicatedHubUrlBox.Text = url;
        ServerBrowserHubUrl.Text = url;
        HubAddressService.Save(url);
    }

    private async Task SelectLocalHubAsync()
    {
        try
        {
            var url = await HubAddressService.FindRunningLocalHubAsync();
            if(url is null)
            {
                SetDedicatedStatus(
                    "Local Hub is not running. Click START LOCAL HUB after building " +
                    "the standalone EXE with build-hub.cmd, or run dotnet run once " +
                    "for a development test.",
                    InfoBarSeverity.Warning, "Local Hub not found");
                return;
            }

            SetLocalHubAddress(url);
            SetDedicatedStatus(
                "Connected to the running local Hub at " + url +
                ". The Hub is not automatically published to the Internet. " +
                "Check Advertise on FIRETEAM Hub to list your dedicated server.",
                InfoBarSeverity.Success, "Local Hub selected");
        }
        catch(Exception ex)
        {
            SetDedicatedStatus(
                "Cannot save local Hub endpoint: " + ex.Message,
                InfoBarSeverity.Error, "Hub configuration error");
        }
    }

    private async Task LaunchLocalHubAsync()
    {
        SetDedicatedStatus("Starting localhost FIRETEAM Hub...",
            InfoBarSeverity.Informational, "Starting local Hub");
        try
        {
            var result = await _localHub.StartAsync();
            var url = await HubAddressService.FindRunningLocalHubAsync();
            if(url is not null)
                SetLocalHubAddress(url);
            SetDedicatedStatus(result,
                url is not null ? InfoBarSeverity.Success : InfoBarSeverity.Warning,
                url is not null ? "Local Hub ready" : "Local Hub starting");
        }
        catch(Exception ex)
        {
            SetDedicatedStatus(ex.Message, InfoBarSeverity.Error,
                "Cannot start local Hub");
        }
    }

    private async Task ShowDedicatedStartErrorAsync(string details)
    {
        SetDedicatedStatus(details, InfoBarSeverity.Error,
            "Dedicated server did not start");
        StartupDiagnostics.Write("Dedicated launch blocked: " + details);
        // The configuration screen is long; text below START can be off
        // screen. Show the actual validation error rather than silently
        // leaving the user with an unchanged LAN list.
        try
        {
            var dialog = new ContentDialog
            {
                Title = "Dedicated server did not start",
                Content = details,
                CloseButtonText = "OK",
                XamlRoot = Content.XamlRoot
            };
            await dialog.ShowAsync();
        }
        catch(Exception dialogError)
        {
            // Keep the visible InfoBar/status as fallback if XAML is still
            // arranging after an early click.
            StartupDiagnostics.Write(
                "Dedicated error dialog unavailable: " + dialogError.Message);
        }
    }

    private void SaveDedicatedPreset()
    {
        try
        {
            _dedicatedServer.Save(GetDedicatedProfile());
            SetDedicatedStatus(
                "Server preset saved locally. Private PIN values are never saved.",
                InfoBarSeverity.Success, "Preset saved");
        }
        catch(Exception ex)
        {
            SetDedicatedStatus("Preset not saved: " + ex.Message,
                InfoBarSeverity.Error, "Preset save failed");
        }
    }

    private async void LaunchDedicatedServer()
    {
        if(_dedicatedLaunchBusy) return;
        _dedicatedLaunchBusy = true;
        UpdateDedicatedStartButton();
        try
        {
            var profile = GetDedicatedProfile();
            // A blank Hub address is not an invitation to send player data
            // to a placeholder domain. Use only an ACTUALLY running local
            // first-party Hub when online listing was explicitly selected.
            if(profile.PublishOnline && string.IsNullOrWhiteSpace(profile.HubUrl))
            {
                var local = await HubAddressService.FindRunningLocalHubAsync();
                if(local is not null)
                {
                    SetLocalHubAddress(local);
                    profile = profile with { HubUrl = local };
                }
            }

            // Save the chosen configuration even if the subsequent start is
            // blocked by an invalid URL or a missing dedicated executable.
            _dedicatedServer.Save(profile);
            if(profile.PublishOnline &&
               !HubAddressService.TryNormalize(profile.HubUrl, out _))
                throw new InvalidOperationException(
                    "Advertise is ON, but no valid Hub address is configured. " +
                    "Click USE RUNNING LOCAL HUB, then START DEDICATED. " +
                    "Or turn Advertise OFF to host on LAN only.");

            SetDedicatedStatus("Launching dedicated server...",
                InfoBarSeverity.Informational, "Starting dedicated");
            var started = _dedicatedServer.Start(profile);

            // Process.Start only establishes that Windows created a child,
            // not that Jupiter has loaded the world or opened the game port.
            await Task.Delay(900);
            if(!_dedicatedServer.IsRunning)
                throw new InvalidOperationException(
                    $"Dedicated executable exited immediately (code " +
                    $"{_dedicatedServer.LastExitCode?.ToString() ?? "unknown"}). " +
                    "Check its console. The port may already be in use by a manually started server.");

            SetDedicatedStatus(
                started + " Process still running; wait for Jupiter's " +
                "world initialization and verify the LAN listing before joining.",
                InfoBarSeverity.Success, "Dedicated process running");

            if(profile.AutoConfigureRouter)
            {
                SetDedicatedStatus(
                    started + " Checking the game's port ownership and UPnP...",
                    InfoBarSeverity.Informational, "Checking router mapping");
                try
                {
                    var result = await _dedicatedServer.ConfigureRouterAsync(profile.Port);
                    if(!_dedicatedServer.IsRunning)
                    {
                        SetDedicatedStatus(
                            "Dedicated exited during router setup (code " +
                            (_dedicatedServer.LastExitCode?.ToString() ?? "unknown") +
                            "). Check its console.",
                            InfoBarSeverity.Error, "Dedicated process exited");
                        return;
                    }
                    var mapped = result.Contains("mapped", StringComparison.OrdinalIgnoreCase);
                    SetDedicatedStatus(
                        started + "\n" + result,
                        mapped ? InfoBarSeverity.Success : InfoBarSeverity.Warning,
                        mapped ? "Dedicated running (UPnP attempted)" :
                                 "Dedicated running (UPnP unavailable)");
                }
                catch(Exception routerError)
                {
                    // UPnP is OPTIONAL. Its failure must never be reported as
                    // failure to launch the actual game server.
                    SetDedicatedStatus(
                        started + "\nRouter mapping failed: " + routerError.Message +
                        ". LAN/direct-IP hosting can still work.",
                        InfoBarSeverity.Warning, "Dedicated running; router unavailable");
                }
            }
        }
        catch(Exception ex)
        {
            await ShowDedicatedStartErrorAsync(ex.Message);
        }
        finally
        {
            _dedicatedLaunchBusy = false;
            UpdateDedicatedStartButton();
        }
    }

    private async void StopDedicatedServer()
    {
        try
        {
            var status = _dedicatedServer.RequestStop();
            SetDedicatedStatus(status, InfoBarSeverity.Informational,
                "Shutdown requested");
            for(var n = 0; n < 16 && _dedicatedServer.IsRunning; ++n)
                await Task.Delay(500);

            if(_dedicatedServer.IsRunning)
            {
                SetDedicatedStatus(
                    "Dedicated is still stopping; router mapping retained " +
                    "until the process exits.",
                    InfoBarSeverity.Warning, "Awaiting shutdown");
                return;
            }

            var cleanup = await _dedicatedServer.CleanupRouterAsync();
            UpdateDedicatedStartButton();
            SetDedicatedStatus(status + "\n" + cleanup,
                InfoBarSeverity.Informational, "Dedicated stopped");
        }
        catch(Exception ex)
        {
            SetDedicatedStatus("Cannot request stop: " + ex.Message,
                InfoBarSeverity.Error, "Stop failed");
        }
    }
}
