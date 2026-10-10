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
    private readonly TextBlock PlayerProfileIdentity = new();
    private readonly TextBlock PlayerProfileStatus = new();
    private readonly Grid PlayerProfileRows = new();
    private readonly StackPanel PlayerProfileLeft = new() { Spacing = 14 };
    private readonly StackPanel PlayerProfileRight = new() { Spacing = 14 };
    private bool _profileNarrowLayout;

    // Inspired by Minecraft's statistics screen: a compact label/value grid
    // with consistent right-aligned values, instead of giant monospaced blocks.
    private void BuildPlayerProfileView()
    {
        PlayerProfileView.Visibility = Visibility.Collapsed;
        PlayerProfileView.VerticalScrollBarVisibility = ScrollBarVisibility.Auto;
        PlayerProfileView.HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled;
        PlayerProfileView.HorizontalContentAlignment = HorizontalAlignment.Stretch;

        // ScrollViewer + a vertical StackPanel with MaxWidth measured the
        // profile content at its desired width, NOT the visible viewport.
        // The old centered single-column origin survived: first column
        // began near x=470 and the second column was cut off on the right.
        //
        // A fixed-width Grid, sized from the ScrollViewer viewport, keeps
        // BOTH columns together and centers the full page as one unit.
        var page = new Grid
        {
            RowSpacing = 12,
            Margin = new Thickness(0, 16, 0, 32),
            Width = 1120,
            HorizontalAlignment = HorizontalAlignment.Center
        };
        page.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        page.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
        page.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

        var toolbar = new Grid { ColumnSpacing = 14 };
        toolbar.ColumnDefinitions.Add(new ColumnDefinition
        {
            Width = new GridLength(1, GridUnitType.Star)
        });
        toolbar.ColumnDefinitions.Add(new ColumnDefinition
        {
            Width = GridLength.Auto
        });

        PlayerProfileIdentity.Text = "LOCAL MATCH STATISTICS";
        PlayerProfileIdentity.Foreground = PrimaryTextBrush;
        PlayerProfileIdentity.FontSize = 18;
        PlayerProfileIdentity.FontFamily = new FontFamily("Bahnschrift SemiCondensed");
        PlayerProfileIdentity.VerticalAlignment = VerticalAlignment.Center;
        toolbar.Children.Add(PlayerProfileIdentity);

        var refresh = SecondaryButton("REFRESH");
        refresh.Click += (sender, args) => RefreshPlayerProfile();
        Grid.SetColumn(refresh, 1);
        toolbar.Children.Add(refresh);
        Grid.SetRow(toolbar, 0);
        page.Children.Add(toolbar);

        PlayerProfileStatus.Foreground = SecondaryTextBrush;
        PlayerProfileStatus.FontSize = 12;
        PlayerProfileStatus.TextWrapping = TextWrapping.Wrap;
        Grid.SetRow(PlayerProfileStatus, 1);
        page.Children.Add(PlayerProfileStatus);

        PlayerProfileRows.HorizontalAlignment = HorizontalAlignment.Stretch;
        PlayerProfileRows.ColumnSpacing = 16;
        PlayerProfileRows.RowSpacing = 0;
        PlayerProfileRows.RowDefinitions.Add(
            new RowDefinition { Height = GridLength.Auto });
        PlayerProfileRows.RowDefinitions.Add(
            new RowDefinition { Height = GridLength.Auto });
        PlayerProfileRows.ColumnDefinitions.Add(new ColumnDefinition
        {
            Width = new GridLength(1, GridUnitType.Star)
        });
        PlayerProfileRows.ColumnDefinitions.Add(new ColumnDefinition
        {
            Width = new GridLength(1, GridUnitType.Star)
        });
        Grid.SetColumn(PlayerProfileRight, 1);
        PlayerProfileRows.Children.Add(PlayerProfileLeft);
        PlayerProfileRows.Children.Add(PlayerProfileRight);
        Grid.SetRow(PlayerProfileRows, 2);
        page.Children.Add(PlayerProfileRows);
        PlayerProfileView.Content = page;

        // No horizontal scrolling or clipped values. Two equal-width
        // columns on desktop; stack them on narrow windows.
        PlayerProfileView.SizeChanged += (_, args) =>
        {
            var viewport = args.NewSize.Width;
            if(viewport < 1) return;

            // Reserve 24 px at both edges inside the available viewport.
            // Width is explicit because ScrollViewer may otherwise measure
            // a vertical StackPanel's child Grid with infinite width.
            var width = Math.Max(1, Math.Min(1320, viewport - 48));
            if(Math.Abs(page.Width - width) > 0.5)
                page.Width = width;

            var narrow = width < 860;
            if(narrow == _profileNarrowLayout) return;

            _profileNarrowLayout = narrow;
            PlayerProfileRows.ColumnDefinitions[1].Width =
                narrow ? new GridLength(0) :
                new GridLength(1, GridUnitType.Star);
            Grid.SetColumn(PlayerProfileRight, narrow ? 0 : 1);
            Grid.SetRow(PlayerProfileRight, narrow ? 1 : 0);
            PlayerProfileRows.RowSpacing = narrow ? 16 : 0;
        };
        ShowProfileEmpty(
            "No local matches found yet.",
            "Play for at least 15 seconds to create your first checkpoint.");
    }

    private void ShowProfileEmpty(string summary, string instructions)
    {
        ClearProfileSections();
        AddProfileSection("LOCAL HISTORY", new[]
        {
            (Label: summary, Value: "—"),
            (Label: instructions, Value: "")
        });
    }

    private void ClearProfileSections()
    {
        PlayerProfileLeft.Children.Clear();
        PlayerProfileRight.Children.Clear();
    }

    private static string ProfileDisplayName(string id)
    {
        var cleaned = (id ?? "").Replace('_', ' ').Replace('-', ' ').Trim();
        if(cleaned.Length == 0) return "Unknown";
        return System.Globalization.CultureInfo.InvariantCulture.TextInfo
            .ToTitleCase(cleaned.ToLowerInvariant());
    }

    private void AddProfileSection(
        string section, IEnumerable<(string Label, string Value)> values)
    {
        var table = new StackPanel { Spacing = 0 };

        var header = new Grid { Padding = new Thickness(15, 10, 15, 10) };
        header.ColumnDefinitions.Add(new ColumnDefinition
        {
            Width = new GridLength(1, GridUnitType.Star)
        });
        header.ColumnDefinitions.Add(new ColumnDefinition
        {
            Width = GridLength.Auto
        });

        var sectionLabel = new TextBlock
        {
            Text = section,
            FontFamily = new FontFamily("Bahnschrift SemiCondensed"),
            FontWeight = Microsoft.UI.Text.FontWeights.Bold,
            FontSize = 13,
            Foreground = CyanBrush
        };
        var totalLabel = new TextBlock
        {
            Text = "TOTAL",
            FontSize = 11,
            FontWeight = Microsoft.UI.Text.FontWeights.Bold,
            Foreground = SecondaryTextBrush,
            HorizontalAlignment = HorizontalAlignment.Right
        };
        header.Children.Add(sectionLabel);
        Grid.SetColumn(totalLabel, 1);
        header.Children.Add(totalLabel);

        table.Children.Add(new Border
        {
            Background = PanelRaisedBrush,
            BorderBrush = DividerBrush,
            BorderThickness = new Thickness(0, 0, 0, 1),
            Child = header
        });

        var index = 0;
        foreach(var (label, value) in values)
        {
            var row = new Grid
            {
                ColumnSpacing = 16,
                Padding = new Thickness(16, 7, 16, 7),
                MinHeight = 34
            };
            row.ColumnDefinitions.Add(new ColumnDefinition
            {
                Width = new GridLength(1, GridUnitType.Star)
            });
            row.ColumnDefinitions.Add(new ColumnDefinition
            {
                Width = GridLength.Auto
            });

            var name = new TextBlock
            {
                Text = label,
                FontSize = 14,
                Foreground = PrimaryTextBrush,
                VerticalAlignment = VerticalAlignment.Center,
                TextTrimming = TextTrimming.CharacterEllipsis,
                TextWrapping = TextWrapping.NoWrap
            };
            var amount = new TextBlock
            {
                Text = value,
                FontSize = 14,
                Foreground = AccentBrush,
                FontWeight = Microsoft.UI.Text.FontWeights.SemiBold,
                HorizontalAlignment = HorizontalAlignment.Right,
                VerticalAlignment = VerticalAlignment.Center
            };
            row.Children.Add(name);
            Grid.SetColumn(amount, 1);
            row.Children.Add(amount);

            table.Children.Add(new Border
            {
                Background = index++ % 2 == 0 ? PanelBrush : PanelRaisedBrush,
                BorderBrush = BorderBrush,
                BorderThickness = new Thickness(0, 0, 0, 1),
                Child = row
            });
        }

        if(index == 0)
        {
            var blank = new TextBlock
            {
                Text = "No entries recorded",
                Foreground = MutedTextBrush,
                FontSize = 13,
                Margin = new Thickness(16, 10, 16, 10)
            };
            table.Children.Add(blank);
        }

        // Pair the compact tables side-by-side on desktop. Heavy weapon
        // breakdowns stay in the right-hand column; small stats on the left.
        var column = section is "WEAPONS / SHOTS FIRED" or "POWER-UPS COLLECTED"
            ? PlayerProfileRight
            : PlayerProfileLeft;
        column.Children.Add(new Border
        {
            BorderBrush = BorderBrush,
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(3),
            Child = table
        });
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
        long matches = 0, completedMatches = 0, interruptedMatches = 0;
        long kills = 0, shots = 0, hits = 0;
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
                    // A stable matchId points to the newest checkpoint; do
                    // not drop earned progress from crashed/unfinished runs.
                    var matchStatus = match.TryGetProperty("status", out var status) &&
                                      status.ValueKind == JsonValueKind.String
                        ? status.GetString() : "completed";
                    if(matchStatus != "completed" &&
                       matchStatus != "in_progress")
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
                        if(matchStatus == "completed")
                            ++completedMatches;
                        else
                            ++interruptedMatches;
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
            PlayerProfileIdentity.Text = playerName.ToUpperInvariant();
            ShowProfileEmpty(
                "No local match checkpoints for this player.",
                "Start round 1, play 15+ seconds, then refresh. Check BUILT/data/matches.");
            PlayerProfileStatus.Text = invalidFiles > 0
                ? $"{invalidFiles} invalid result file(s) were skipped."
                : "Waiting for a match checkpoint.";
            return;
        }

        PlayerProfileIdentity.Text =
            $"{playerName.ToUpperInvariant()}  /  {matches:N0} MATCHES";

        ClearProfileSections();
        AddProfileSection("GENERAL", new (string Label, string Value)[]
        {
            ("Matches played", matches.ToString("N0")),
            ("Finished matches", completedMatches.ToString("N0")),
            ("Interrupted matches", interruptedMatches.ToString("N0")),
            ("Best round reached", bestRound.ToString("N0")),
            ("Zombies eliminated", kills.ToString("N0")),
            ("Best single-match kills", bestMatchKills.ToString("N0")),
            ("Deaths", deaths.ToString("N0")),
            ("Damage taken", damageTaken.ToString("N0")),
            ("Power-ups collected", powerups.ToString("N0"))
        });

        AddProfileSection("COMBAT", new (string Label, string Value)[]
        {
            ("Accepted weapon shots", shots.ToString("N0")),
            ("Confirmed hitscan impacts", hits.ToString("N0")),
            ("Headshot kills", headshotKills.ToString("N0"))
        });

        AddProfileSection("INFECTED ELIMINATED", zombieKills
            .OrderByDescending(x => x.Value)
            .ThenBy(x => x.Key)
            .Select(x => (ProfileDisplayName(x.Key), x.Value.ToString("N0"))));

        AddProfileSection("WEAPONS / SHOTS FIRED", weaponUsage
            .OrderByDescending(x => x.Value)
            .ThenBy(x => x.Key)
            .Select(x => (ProfileDisplayName(x.Key), x.Value.ToString("N0"))));

        AddProfileSection("POWER-UPS COLLECTED", powerupTypes
            .OrderByDescending(x => x.Value)
            .ThenBy(x => x.Key)
            .Select(x => (ProfileDisplayName(x.Key), x.Value.ToString("N0"))));

        PlayerProfileStatus.Text =
            "Local match records • unfinished checkpoints included • unauthenticated player name" +
            (invalidFiles > 0 ? $" • {invalidFiles} invalid file(s) skipped" : "");
    }
}
