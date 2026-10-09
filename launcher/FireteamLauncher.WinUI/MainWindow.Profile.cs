using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using FireteamLauncher.Infrastructure;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;

namespace FireteamLauncher;

public sealed partial class MainWindow
{
    private readonly ScrollViewer PlayerProfileView = new();
    private readonly TextBlock PlayerProfileTotals = new();
    private readonly TextBlock PlayerProfileBreakdown = new();
    private readonly TextBlock PlayerProfileStatus = new();

    private void BuildPlayerProfileView()
    {
        PlayerProfileView.VerticalScrollBarVisibility =
            ScrollBarVisibility.Auto;
        PlayerProfileView.HorizontalScrollBarVisibility =
            ScrollBarVisibility.Disabled;

        var page = NewPagePanel(
            "PLAYER HISTORY",
            "Player Profile",
            "Local-first stats for the selected username. No login or central database required.");

        page.Children.Add(CardHeading(
            "IDENTITY",
            "Your match history",
            "The launcher reads completed FIRETEAM matches saved by your local game or dedicated server. Usernames are not yet verified cross-server identities."));

        var controls = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 14
        };
        var refresh = PrimaryButton("REFRESH STATS");
        refresh.Click += (sender, args) => RefreshPlayerProfile();
        controls.Children.Add(refresh);
        PlayerProfileStatus.Foreground = SecondaryTextBrush;
        PlayerProfileStatus.TextWrapping = TextWrapping.Wrap;
        PlayerProfileStatus.VerticalAlignment = VerticalAlignment.Center;
        controls.Children.Add(PlayerProfileStatus);
        page.Children.Add(controls);

        PlayerProfileTotals.FontFamily = new FontFamily("Bahnschrift SemiCondensed");
        PlayerProfileTotals.FontSize = 23;
        PlayerProfileTotals.Foreground = PrimaryTextBrush;
        PlayerProfileTotals.TextWrapping = TextWrapping.Wrap;
        page.Children.Add(Card(PlayerProfileTotals));

        PlayerProfileBreakdown.FontFamily =
            new FontFamily("Bahnschrift SemiCondensed");
        PlayerProfileBreakdown.FontSize = 17;
        PlayerProfileBreakdown.Foreground = PrimaryTextBrush;
        PlayerProfileBreakdown.TextWrapping = TextWrapping.Wrap;
        page.Children.Add(Card(PlayerProfileBreakdown));

        PlayerProfileView.Content = page;
        PlayerProfileTotals.Text = "Play a match to start building your stats.";
        PlayerProfileBreakdown.Text = "Kills by zombie type, favorite weapons and power-up history will appear here.";
    }

    private static long ReadStat(JsonElement element, string name)
    {
        return element.TryGetProperty(name, out var value) &&
               value.ValueKind == JsonValueKind.Number &&
               value.TryGetInt64(out var number) ? number : 0L;
    }

    private static void AddCategories(
        JsonElement player, string field, Dictionary<string, long> totals)
    {
        if(!player.TryGetProperty(field, out var record) ||
           record.ValueKind != JsonValueKind.Object)
            return;
        foreach(var property in record.EnumerateObject())
        {
            if(property.Value.ValueKind != JsonValueKind.Number ||
               !property.Value.TryGetInt64(out var count) ||
               count < 0) continue;
            totals[property.Name] = totals.GetValueOrDefault(property.Name) + count;
        }
    }

    private void RefreshPlayerProfile()
    {
        var playerName = (PlayerNameBox.Text ?? string.Empty).Trim();
        // The current multiplayer player-name packet is limited to 31 chars.
        if(playerName.Length > 31)
            playerName = playerName[..31];
        if(playerName.Length == 0)
        {
            PlayerProfileStatus.Text = "Enter your player name on HOME first.";
            return;
        }

        var game = LauncherPaths.FindGameDirectory();
        if(game is null)
        {
            PlayerProfileStatus.Text = "The built game directory was not found.";
            return;
        }

        var roots = new[]
        {
            Path.Combine(game, "data", "matches"),
            Path.Combine(game, "Dedicated", "data", "matches")
        };

        var weaponUsage = new Dictionary<string, long>(
            StringComparer.OrdinalIgnoreCase);
        var zombieKills = new Dictionary<string, long>(
            StringComparer.OrdinalIgnoreCase);
        var powerupTypes = new Dictionary<string, long>(
            StringComparer.OrdinalIgnoreCase);
        var seenMatchIds = new HashSet<string>(
            StringComparer.OrdinalIgnoreCase);
        long matches = 0, kills = 0, shots = 0, hits = 0;
        long deaths = 0, powerups = 0, headshotKills = 0;
        long damageTaken = 0, bestRound = 0, bestMatchKills = 0;
        var invalidFiles = 0;

        foreach(var root in roots)
        {
            if(!Directory.Exists(root)) continue;
            IEnumerable<string> files;
            try
            {
                files = Directory.EnumerateFiles(root, "match-*.json")
                    .OrderByDescending(File.GetLastWriteTimeUtc)
                    .Take(2500)
                    .ToArray();
            }
            catch(IOException)
            {
                continue;
            }
            catch(UnauthorizedAccessException)
            {
                continue;
            }

            foreach(var file in files)
            {
                try
                {
                    using var doc = JsonDocument.Parse(File.ReadAllText(file));
                    var match = doc.RootElement;
                    if(ReadStat(match, "schemaVersion") != 1 ||
                       !match.TryGetProperty("players", out var players) ||
                       players.ValueKind != JsonValueKind.Array)
                        continue;
                    if(match.TryGetProperty("status", out var status) &&
                       (status.ValueKind != JsonValueKind.String ||
                        status.GetString() != "completed"))
                        continue;

                    var identity = match.TryGetProperty("matchId", out var id) &&
                                   id.ValueKind == JsonValueKind.String
                        ? id.GetString() ?? file : file;
                    if(!seenMatchIds.Add(identity))
                        continue;

                    foreach(var player in players.EnumerateArray())
                    {
                        if(!player.TryGetProperty("name", out var name) ||
                           name.ValueKind != JsonValueKind.String ||
                           !string.Equals(name.GetString(), playerName,
                               StringComparison.OrdinalIgnoreCase))
                            continue;

                        ++matches;
                        var matchKills = ReadStat(player, "killsTotal");
                        kills += matchKills;
                        shots += ReadStat(player, "shotsFired");
                        hits += ReadStat(player, "hitscanHits");
                        deaths += ReadStat(player, "deaths");
                        powerups += ReadStat(player, "powerups");
                        headshotKills += ReadStat(player, "headshotKills");
                        damageTaken += ReadStat(player, "damageTaken");
                        bestRound = Math.Max(bestRound,
                            ReadStat(match, "roundReached"));
                        bestMatchKills = Math.Max(bestMatchKills, matchKills);

                        AddCategories(player, "killsByType", zombieKills);
                        AddCategories(player, "powerupsById", powerupTypes);
                        if(player.TryGetProperty("shotsByWeapon", out var guns) &&
                           guns.ValueKind == JsonValueKind.Array)
                        {
                            foreach(var gun in guns.EnumerateArray())
                            {
                                if(!gun.TryGetProperty("id", out var gunId) ||
                                   gunId.ValueKind != JsonValueKind.String)
                                    continue;
                                var key = gunId.GetString() ?? "unknown";
                                weaponUsage[key] = weaponUsage.GetValueOrDefault(key) +
                                    ReadStat(gun, "shots");
                            }
                        }
                    }
                }
                catch(JsonException)
                {
                    ++invalidFiles;
                }
                catch(IOException)
                {
                    ++invalidFiles;
                }
                catch(UnauthorizedAccessException)
                {
                    ++invalidFiles;
                }
            }
        }

        if(matches == 0)
        {
            PlayerProfileTotals.Text =
                playerName + " — no completed local matches recorded yet.";
            PlayerProfileBreakdown.Text =
                "Finish a game, then refresh. Stats are saved under BUILT/data/matches. Dedicated matches are under BUILT/Dedicated/data/matches.";
            PlayerProfileStatus.Text =
                invalidFiles > 0 ? "Some invalid result files were skipped." :
                "Awaiting first completed match.";
            return;
        }

        PlayerProfileTotals.Text =
            $"{playerName.ToUpperInvariant()}\n" +
            $"MATCHES  {matches}       KILLS  {kills}       BEST ROUND  {bestRound}\n" +
            $"SHOTS  {shots}       CONFIRMED HITSCAN HITS  {hits}       DEATHS  {deaths}\n" +
            $"HEADSHOT KILLS  {headshotKills}       POWERUPS  {powerups}\n" +
            $"BEST MATCH KILLS  {bestMatchKills}       DAMAGE TAKEN  {damageTaken}";

        var detail = new StringBuilder();
        detail.AppendLine("MOST USED WEAPONS (accepted trigger pulls)");
        foreach(var item in weaponUsage.OrderByDescending(x => x.Value).Take(8))
            detail.AppendLine($"  {item.Key} — {item.Value}");
        detail.AppendLine();
        detail.AppendLine("ZOMBIES KILLED BY TYPE");
        foreach(var item in zombieKills.OrderByDescending(x => x.Value).Take(12))
            detail.AppendLine($"  {item.Key} — {item.Value}");
        detail.AppendLine();
        detail.AppendLine("POWERUPS COLLECTED");
        foreach(var item in powerupTypes.OrderByDescending(x => x.Value).Take(12))
            detail.AppendLine($"  {item.Key} — {item.Value}");
        if(zombieKills.Count == 0)
            detail.AppendLine("  No subtype-tagged kills in these matches.");
        PlayerProfileBreakdown.Text = detail.ToString();
        PlayerProfileStatus.Text =
            $"{matches} completed local match(es) loaded." +
            (invalidFiles > 0 ? $" {invalidFiles} invalid file(s) skipped." : "");
    }
}
