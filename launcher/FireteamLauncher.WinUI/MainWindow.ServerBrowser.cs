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
    private readonly TextBox DedicatedServerName = new();
    private readonly TextBox DedicatedServerPort = new();
    private readonly TextBlock ServerBrowserStatus = new();
    private readonly ServerDirectoryService _serverDirectory = new();
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
            "Join by IP:port, save favorites, or browse the curated public directory. Public entries are not live ping results."));

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

        ServerBrowserList.MinHeight = 240;
        ServerBrowserList.MaxHeight = 370;
        ServerBrowserList.SelectionMode = ListViewSelectionMode.Single;
        ServerBrowserList.SelectionChanged += (sender, args) =>
        {
            if(ServerBrowserList.SelectedItem is FireteamServerListing entry)
            {
                ServerBrowserName.Text = entry.Name;
                ServerBrowserAddress.Text = entry.Address;
            }
        };
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

        page.Children.Add(CardHeading(
            "HOSTING",
            "Dedicated Server",
            "Runs separately from the game with its own settings. Uses the selected Quick Play map and difficulty."));

        var hostOptions = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 10
        };
        DedicatedServerName.PlaceholderText = "Public server name";
        DedicatedServerName.Text = "FIRETEAM Dedicated";
        DedicatedServerName.Width = 260;
        DedicatedServerPort.PlaceholderText = "Port";
        DedicatedServerPort.Text = "27889";
        DedicatedServerPort.Width = 110;
        hostOptions.Children.Add(DedicatedServerName);
        hostOptions.Children.Add(DedicatedServerPort);
        page.Children.Add(hostOptions);

        var dedicated = SecondaryButton("START DEDICATED SERVER");
        dedicated.Click += (sender, args) => LaunchDedicatedServer();
        page.Children.Add(dedicated);

        ServerBrowserStatus.Foreground = SecondaryTextBrush;
        ServerBrowserStatus.TextWrapping = TextWrapping.Wrap;
        ServerBrowserStatus.Text = "Public directory is community-curated. To submit a server, open an issue or pull request on the FIRETEAM GitHub repository.";
        page.Children.Add(ServerBrowserStatus);

        ServerBrowserView.Content = page;
        RefreshServerBrowserList();
    }

    private void RefreshServerBrowserList()
    {
        var entries = _serverDirectory.LoadFavorites()
            .Concat(_communityServers)
            .GroupBy(entry => entry.Address, StringComparer.OrdinalIgnoreCase)
            .Select(group => group.First())
            .OrderBy(entry => entry.Name)
            .ToList();

        ServerBrowserList.ItemsSource = entries;
    }

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

        var selection = ServerBrowserList.SelectedItem as FireteamServerListing;
        var map = selection is not null &&
                  selection.Address.Equals(address, StringComparison.OrdinalIgnoreCase)
            ? selection.Map
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

    private void LaunchDedicatedServer()
    {
        var gameDirectory = LauncherPaths.FindGameDirectory();
        var serverDirectory = gameDirectory is null
            ? null
            : Path.Combine(gameDirectory, "Dedicated");
        var serverPath = serverDirectory is null
            ? null
            : Path.Combine(serverDirectory, "FireteamDedicatedServer.exe");

        if(serverPath is null || !File.Exists(serverPath))
        {
            ServerBrowserStatus.Text =
                "Dedicated server is not installed. Run build.cmd, then build-dedicated.cmd.";
            return;
        }

        if(!ushort.TryParse(DedicatedServerPort.Text, out var port) || port == 0)
        {
            ServerBrowserStatus.Text = "Dedicated port must be between 1 and 65535.";
            return;
        }

        var mapName = MapCombo.SelectedItem?.ToString() ?? "CABINFEVER";
        mapName = Path.GetFileNameWithoutExtension(mapName).ToUpperInvariant();
        if(mapName.Length == 0 || mapName.Any(ch =>
            !(char.IsLetterOrDigit(ch) || ch == '_' || ch == '-')))
        {
            ServerBrowserStatus.Text = "Invalid map selection.";
            return;
        }

        if(!File.Exists(Path.Combine(serverDirectory!, "rez", "Worlds", mapName + ".DAT")))
        {
            ServerBrowserStatus.Text =
                $"The dedicated runtime cannot find {mapName}.DAT. Stage map assets and rebuild.";
            return;
        }

        var serverName = (DedicatedServerName.Text ?? "").Trim();
        if(string.IsNullOrWhiteSpace(serverName))
            serverName = "FIRETEAM Dedicated";
        if(serverName.Length > 64)
            serverName = serverName[..64];

        try
        {
            var configDir = Path.Combine(serverDirectory!, "config");
            Directory.CreateDirectory(configDir);

            // Keep a server-local config: launching a playable client can
            // rewrite BUILT/config/session.cfg at any time.
            var difficulty = DifficultyProfileValue();
            var cabinGuard = mapName == "CABINFEVER" ? 1 : 0;
            File.WriteAllText(
                Path.Combine(configDir, "session.cfg"),
                $"difficulty={difficulty}\nfirst_round_prep=45\ncabin_spawn_guard={cabinGuard}\n");

            var start = new System.Diagnostics.ProcessStartInfo(serverPath)
            {
                WorkingDirectory = serverDirectory!,
                UseShellExecute = false
            };
            start.ArgumentList.Add("--map");
            start.ArgumentList.Add(mapName);
            start.ArgumentList.Add("--port");
            start.ArgumentList.Add(port.ToString());
            start.ArgumentList.Add("--max-players");
            start.ArgumentList.Add("24");
            start.ArgumentList.Add("--name");
            start.ArgumentList.Add(serverName);

            var process = System.Diagnostics.Process.Start(start);
            ServerBrowserStatus.Text = process is null
                ? "Unable to start dedicated server."
                : $"Dedicated server started (PID {process.Id}). Join 127.0.0.1:{port} on this PC. Verify the server console reports 'running' before joining.";
        }
        catch(Exception ex)
        {
            ServerBrowserStatus.Text = "Dedicated server start failed: " + ex.Message;
        }
    }
}
