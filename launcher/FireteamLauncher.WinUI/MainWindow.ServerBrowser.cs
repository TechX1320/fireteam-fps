using FireteamLauncher.Infrastructure;
using FireteamLauncher.Models;
using FireteamLauncher.Services;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace FireteamLauncher;

public sealed partial class MainWindow
{
    private readonly ScrollViewer ServerBrowserView = new();
    private readonly ListView ServerBrowserList = new();
    private readonly TextBox ServerBrowserName = new();
    private readonly TextBox ServerBrowserAddress = new();
    private readonly TextBlock ServerBrowserStatus = new();
    private readonly ServerDirectoryService _serverDirectory = new();
    private readonly LanServerDiscoveryService _lanDiscovery = new();
    private Microsoft.UI.Dispatching.DispatcherQueueTimer? _lanRefreshTimer;
    private sealed record ServerBrowserRow(
        FireteamServerListing Listing, bool IsLan, int Players, int MaxPlayers,
        int Difficulty);
    private bool _lanDiscoveryInitialized;
    private bool _curatedDirectoryRequested;
    private IReadOnlyList<FireteamServerListing> _communityServers = [];

    private void BuildServerBrowserView()
    {
        ServerBrowserView.HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled;
        ServerBrowserView.VerticalScrollBarVisibility = ScrollBarVisibility.Auto;
        ServerBrowserView.HorizontalContentAlignment = HorizontalAlignment.Center;

        var page = new StackPanel
        {
            Spacing = 14,
            Margin = new Thickness(30, 18, 30, 26),
            MaxWidth = 1040,
            HorizontalAlignment = HorizontalAlignment.Center
        };

        page.Children.Add(CardHeading(
            "SERVER BROWSER",
            "Community Servers",
            "Nearby dedicated servers appear automatically on LAN. Internet listings still require an opt-in public directory; unknown ping or population is never fabricated."));

        var controls = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10
        };
        ServerBrowserName.PlaceholderText = "Server nickname";
        ServerBrowserName.Width = 220;
        ServerBrowserAddress.PlaceholderText = "IP or hostname:27889";
        ServerBrowserAddress.Width = 265;

        controls.Children.Add(ServerBrowserName);
        controls.Children.Add(ServerBrowserAddress);

        var save = SecondaryButton("SAVE FAVORITE");
        save.Click += (sender, args) => SaveServerFavorite();
        controls.Children.Add(save);
        page.Children.Add(controls);

        ServerBrowserList.MinHeight = 280;
        ServerBrowserList.MaxHeight = 500;
        ServerBrowserList.SelectionMode = ListViewSelectionMode.Single;
        ServerBrowserList.SelectionChanged += (sender, args) =>
        {
            if(ServerBrowserList.SelectedItem is ListViewItem { Tag: ServerBrowserRow selection })
            {
                ServerBrowserName.Text = selection.Listing.Name;
                ServerBrowserAddress.Text = selection.Listing.Address;
            }
        };
        page.Children.Add(ServerGridHeader());
        page.Children.Add(ServerBrowserList);

        var actions = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10
        };

        var refresh = SecondaryButton("REFRESH DIRECTORY");
        refresh.Click += async (sender, args) => await RefreshCommunityServersAsync();
        actions.Children.Add(refresh);

        var join = SecondaryButton("JOIN SERVER");
        join.Click += (sender, args) => JoinServerBrowserAddress();
        actions.Children.Add(join);

        var remove = SecondaryButton("REMOVE FAVORITE");
        remove.Click += (sender, args) => RemoveServerFavorite();
        actions.Children.Add(remove);

        var submit = SecondaryButton("SUBMIT SERVER");
        submit.Click += (sender, args) =>
        {
            try
            {
                System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(
                    "https://github.com/TechX1320/fireteam-fps/issues/new?template=server_listing.md")
                {
                    UseShellExecute = true
                });
                ServerBrowserStatus.Text = "Submission form opened in your browser. Public listings are opt-in and reviewed before publication.";
            }
            catch(Exception ex)
            {
                ServerBrowserStatus.Text = "Could not open submission form: " + ex.Message;
            }
        };
        actions.Children.Add(submit);

        page.Children.Add(actions);

        ServerBrowserStatus.Foreground = SecondaryTextBrush;
        ServerBrowserStatus.TextWrapping = TextWrapping.Wrap;
        ServerBrowserStatus.Text = "Public directory is community-curated. To submit a server, open an issue or pull request on the FIRETEAM GitHub repository.";
        page.Children.Add(ServerBrowserStatus);

        ServerBrowserView.Content = page;
        RefreshServerBrowserList();
    }

    private void StartLanServerDiscovery()
    {
        if(_lanDiscoveryInitialized) return;
        _lanDiscoveryInitialized = true;
        _lanDiscovery.Changed += () =>
            DispatcherQueue.TryEnqueue(() => RefreshServerBrowserList());
        _lanDiscovery.Start();
        _lanRefreshTimer = DispatcherQueue.CreateTimer();
        _lanRefreshTimer.Interval = TimeSpan.FromSeconds(3);
        _lanRefreshTimer.Tick += (sender, args) => RefreshServerBrowserList();
        _lanRefreshTimer.Start();
        Closed += (sender, args) =>
        {
            _lanRefreshTimer.Stop();
            _lanDiscovery.Dispose();
        };
        ServerBrowserStatus.Text = _lanDiscovery.IsListening
            ? "Listening for automatic LAN hosts (UDP 27888). " +
              "Internet servers still require curated listings or direct IP."
            : "LAN discovery unavailable: " + _lanDiscovery.StartError +
              ". Direct IP and saved favorites still work.";
    }

    private void RefreshServerBrowserList()
    {
        var nearby = _lanDiscovery.Snapshot()
            .Select(s => new ServerBrowserRow(
                new FireteamServerListing(s.Name, s.Address, s.Map),
                true, s.Players, s.MaxPlayers, s.Difficulty));
        var saved = _serverDirectory.LoadFavorites()
            .Concat(_communityServers)
            .Select(s => new ServerBrowserRow(s, false, 0, 0, 0));
        var entries = nearby.Concat(saved)
            .GroupBy(row => row.Listing.Address, StringComparer.OrdinalIgnoreCase)
            .Select(group => group.OrderByDescending(item => item.IsLan).First())
            .OrderByDescending(item => item.IsLan)
            .ThenBy(item => item.Listing.Name)
            .ToList();

        ServerBrowserList.Items.Clear();
        foreach(var entry in entries)
        {
            var item = new ListViewItem
            {
                Tag = entry,
                Content = ServerGridRow(entry),
                Padding = new Thickness(5, 7, 5, 7),
                HorizontalContentAlignment = HorizontalAlignment.Stretch
            };
            ServerBrowserList.Items.Add(item);
        }
    }

    // Halo CE-inspired fixed, aligned columns, not variable-length text rows.
    // A dash means NOT QUERIED: never fabricate player counts or ping.
    private static Grid ServerColumns(string name, string map, string mode,
        string players, string ping, string mods, bool header)
    {
        var grid = new Grid { ColumnSpacing = 10 };
        foreach(var width in new[] { 310d, 150d, 130d, 115d, 80d, 125d })
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(width) });
        var values = new[] { name, map, mode, players, ping, mods };
        for(var i = 0; i < values.Length; ++i)
        {
            var cell = new TextBlock
            {
                Text = values[i],
                FontSize = header ? 12 : 13,
                FontWeight = header ? Microsoft.UI.Text.FontWeights.Bold : Microsoft.UI.Text.FontWeights.Normal,
                Foreground = header ? CyanBrush : PrimaryTextBrush,
                TextTrimming = TextTrimming.CharacterEllipsis,
                TextWrapping = TextWrapping.NoWrap,
                VerticalAlignment = VerticalAlignment.Center
            };
            Grid.SetColumn(cell, i);
            grid.Children.Add(cell);
        }
        return grid;
    }

    private static Grid ServerGridHeader() =>
        ServerColumns("SERVER NAME / ADDRESS", "MAP", "MODE", "PLAYERS",
                      "PING", "MODS", true);

    private static Grid ServerGridRow(ServerBrowserRow row) =>
        ServerColumns(row.Listing.Name, row.Listing.Map,
            row.IsLan ? $"LAN • D{row.Difficulty}" : "Directory",
            row.IsLan ? $"{row.Players}/{row.MaxPlayers}" : "—",
            "—", "Not checked", false);

    private async Task RefreshCommunityServersAsync()
    {
        ServerBrowserStatus.Text = "Checking the community server directory...";
        try
        {
            _communityServers = await _serverDirectory.FetchPublicAsync();
            RefreshServerBrowserList();
            ServerBrowserStatus.Text = _communityServers.Count == 0
                ? "No public servers listed yet. Add a favorite or join by IP:port. Public directory registration is curated through GitHub."
                : $"Directory refreshed: {_communityServers.Count} public listing(s). Server availability is not yet verified.";
        }
        catch(Exception ex)
        {
            ServerBrowserStatus.Text = "Directory unavailable; local favorites still work. " + ex.Message;
        }
    }

    private void SaveServerFavorite()
    {
        try
        {
            var listing = new FireteamServerListing(
                ServerBrowserName.Text ?? string.Empty,
                ServerBrowserAddress.Text ?? string.Empty,
                MapCombo.SelectedItem?.ToString() ?? "CABINFEVER");
            _serverDirectory.SaveFavorite(listing);
            RefreshServerBrowserList();
            ServerBrowserStatus.Text = "Favorite saved locally. No online registration was performed.";
        }
        catch(Exception ex)
        {
            ServerBrowserStatus.Text = "Cannot save favorite: " + ex.Message;
        }
    }

    private void RemoveServerFavorite()
    {
        if(!ServerDirectoryService.TryNormalizeAddress(ServerBrowserAddress.Text, out var address))
        {
            ServerBrowserStatus.Text = "Select a favorite or enter its address first.";
            return;
        }

        if(!_serverDirectory.LoadFavorites().Any(entry =>
            entry.Address.Equals(address, StringComparison.OrdinalIgnoreCase)))
        {
            ServerBrowserStatus.Text = "That address is not in local favorites.";
            return;
        }

        _serverDirectory.RemoveFavorite(address);
        RefreshServerBrowserList();
        ServerBrowserStatus.Text = "Local favorite removed.";
    }

    private void JoinServerBrowserAddress()
    {
        if(!ServerDirectoryService.TryNormalizeAddress(ServerBrowserAddress.Text, out var address))
        {
            ServerBrowserStatus.Text = "Enter a valid host or IP with optional :port.";
            return;
        }

        var selection = (ServerBrowserList.SelectedItem as ListViewItem)?.Tag as ServerBrowserRow;
        var map = selection is not null &&
                  selection.Listing.Address.Equals(address, StringComparison.OrdinalIgnoreCase)
            ? selection.Listing.Map
            : MapCombo.SelectedItem?.ToString() ?? "CABINFEVER";

        try
        {
            var profile = new LauncherProfile(
                string.IsNullOrWhiteSpace(PlayerNameBox.Text) ? "Player" : PlayerNameBox.Text.Trim(),
                "Join Multiplayer",
                DifficultyProfileValue(),
                address,
                CommandsBox.Text ?? string.Empty,
                map);

            ServerBrowserStatus.Text = App.Instance.Services.Game.Launch(
                profile, App.Instance.Services.Settings.LoadSettings());
        }
        catch(Exception ex)
        {
            ServerBrowserStatus.Text = "Join failed: " + ex.Message;
        }
    }

}
