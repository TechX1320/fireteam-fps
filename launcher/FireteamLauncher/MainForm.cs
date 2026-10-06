using System.Diagnostics;
using System.Globalization;

namespace FireteamLauncher;

public sealed class MainForm : Form
{
    private readonly ComboBox _mode = new();
    private readonly ComboBox _map = new();
    private readonly ComboBox _loadout = new();
    private readonly ComboBox _difficulty = new();
    private readonly TextBox _playerName = new();
    private readonly TextBox _joinIp = new();
    private readonly TextBox _commands = new();

    private readonly ComboBox _resolution = new();
    private readonly CheckBox _windowed = new();
    private readonly NumericUpDown _sensitivity = new();
    private readonly NumericUpDown _volume = new();
    private readonly NumericUpDown _gamma = new();

    private readonly Label _status = new();
    private readonly Button _launch = new();

    private static readonly Color BackColorMain = Color.FromArgb(20, 22, 25);
    private static readonly Color BackColorPanel = Color.FromArgb(31, 34, 38);
    private static readonly Color ForeColorMain = Color.FromArgb(235, 238, 241);
    private static readonly Color AccentColor = Color.FromArgb(227, 151, 27);

    public MainForm()
    {
        Text = "FIRETEAM Launcher";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(780, 650);
        Size = new Size(850, 700);
        BackColor = BackColorMain;
        ForeColor = ForeColorMain;
        Font = new Font("Segoe UI", 10F);

        var header = new Label
        {
            Text = "FIRETEAM",
            Dock = DockStyle.Top,
            Height = 74,
            Font = new Font("Segoe UI Semibold", 27F, FontStyle.Bold),
            ForeColor = AccentColor,
            Padding = new Padding(22, 15, 0, 0)
        };
        Controls.Add(header);

        var tabs = new TabControl
        {
            Dock = DockStyle.Fill,
            Padding = new Point(18, 7)
        };

        tabs.TabPages.Add(BuildPlayPage());
        tabs.TabPages.Add(BuildSettingsPage());
        tabs.TabPages.Add(BuildToolsPage());
        Controls.Add(tabs);
        tabs.BringToFront();

        var footer = new Panel
        {
            Dock = DockStyle.Bottom,
            Height = 75,
            BackColor = BackColorPanel,
            Padding = new Padding(18, 14, 18, 12)
        };

        _status.Text = "Ready";
        _status.AutoSize = false;
        _status.Dock = DockStyle.Fill;
        _status.TextAlign = ContentAlignment.MiddleLeft;

        _launch.Text = "PLAY FIRETEAM";
        _launch.Width = 190;
        _launch.Dock = DockStyle.Right;
        _launch.BackColor = AccentColor;
        _launch.ForeColor = Color.Black;
        _launch.FlatStyle = FlatStyle.Flat;
        _launch.Font = new Font("Segoe UI Semibold", 11F, FontStyle.Bold);
        _launch.Click += (_, _) => LaunchGame();

        footer.Controls.Add(_status);
        footer.Controls.Add(_launch);
        Controls.Add(footer);
        footer.BringToFront();

        ApplyTheme(this);
    }

    private TabPage BuildPlayPage()
    {
        var page = NewPage("Play");
        var layout = NewGrid();

        _playerName.Text = Environment.UserName;
        _playerName.MaxLength = 15;
        AddRow(layout, 0, "Player Name", _playerName);

        _mode.DropDownStyle = ComboBoxStyle.DropDownList;
        _mode.Items.AddRange([
            "Single Player",
            "Host Multiplayer",
            "Join Multiplayer"
        ]);
        _mode.SelectedIndex = 0;
        _mode.SelectedIndexChanged += (_, _) =>
        {
            _joinIp.Enabled = _mode.SelectedIndex == 2;
        };
        AddRow(layout, 1, "Game Mode", _mode);

        _joinIp.Text = "127.0.0.1";
        _joinIp.Enabled = false;
        AddRow(layout, 2, "Server IP", _joinIp);

        _map.DropDownStyle = ComboBoxStyle.DropDownList;
        _map.Items.Add("Cabin Fever");
        _map.SelectedIndex = 0;
        AddRow(layout, 3, "Map", _map);

        _difficulty.DropDownStyle = ComboBoxStyle.DropDownList;
        _difficulty.Items.AddRange([
            "Easy",
            "Normal",
            "Hard",
            "Extreme",
            "Nightmare"
        ]);
        _difficulty.SelectedItem = "Normal";
        AddRow(layout, 4, "Difficulty", _difficulty);

        _loadout.DropDownStyle = ComboBoxStyle.DropDownList;
        _loadout.Items.Add(
            "Rifleman - AK-47 / M92FS / Bowie / Colt MEU / L96A1");
        _loadout.SelectedIndex = 0;
        AddRow(layout, 5, "Loadout", _loadout);

        _commands.PlaceholderText =
            "+consoleenable 1 +SomeLithTechCommand value";
        AddRow(layout, 6, "Commands", _commands);

        var note = new Label
        {
            Text =
                "Commands are appended to the LithTech command line after the " +
                "launcher defaults, so advanced users can pass normal Jupiter " +
                "console/launch arguments directly.",
            AutoSize = true,
            MaximumSize = new Size(590, 0),
            ForeColor = Color.FromArgb(170, 175, 181),
            Margin = new Padding(4, 22, 4, 4)
        };
        layout.Controls.Add(note, 1, 7);

        page.Controls.Add(layout);
        return page;
    }

    private TabPage BuildSettingsPage()
    {
        var page = NewPage("Video & Input");
        var layout = NewGrid();

        _resolution.DropDownStyle = ComboBoxStyle.DropDownList;
        _resolution.Items.AddRange([
            "1024 x 768",
            "1280 x 720",
            "1366 x 768",
            "1600 x 900",
            "1920 x 1080",
            "2560 x 1440",
            "3440 x 1440",
            "3840 x 2160"
        ]);
        _resolution.SelectedItem = "1920 x 1080";
        AddRow(layout, 0, "Resolution", _resolution);

        _windowed.Text = "Windowed";
        _windowed.Checked = true;
        AddRow(layout, 1, "Display Mode", _windowed);

        _sensitivity.DecimalPlaces = 2;
        _sensitivity.Increment = 0.01M;
        _sensitivity.Minimum = 0.01M;
        _sensitivity.Maximum = 4.00M;
        _sensitivity.Value = 0.32M;
        AddRow(layout, 2, "Mouse Sensitivity", _sensitivity, "x");

        _volume.Minimum = 0;
        _volume.Maximum = 100;
        _volume.Increment = 5;
        _volume.Value = 60;
        AddRow(layout, 3, "Game Volume", _volume, "%");

        _gamma.DecimalPlaces = 2;
        _gamma.Minimum = 0.50M;
        _gamma.Maximum = 6.00M;
        _gamma.Increment = 0.10M;
        _gamma.Value = 1.00M;
        AddRow(layout, 4, "Brightness / Gamma", _gamma, "x");

        var note = new Label
        {
            Text =
                "Mouse sensitivity uses the same fine-grained scale as the in-game menu. " +
                "Gamma uses LithTech/NOLF2's native GammaR/G/B controls.",
            AutoSize = true,
            MaximumSize = new Size(590, 0),
            ForeColor = Color.FromArgb(170, 175, 181),
            Margin = new Padding(4, 22, 4, 4)
        };
        layout.Controls.Add(note, 1, 5);

        page.Controls.Add(layout);
        return page;
    }

    private TabPage BuildToolsPage()
    {
        var page = NewPage("Mods & Tools");
        var panel = new FlowLayoutPanel
        {
            Dock = DockStyle.Fill,
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            Padding = new Padding(24)
        };

        panel.Controls.Add(new Label
        {
            Text = "FIRETEAM Content",
            Font = new Font("Segoe UI Semibold", 17F, FontStyle.Bold),
            ForeColor = AccentColor,
            AutoSize = true,
            Margin = new Padding(0, 0, 0, 12)
        });

        panel.Controls.Add(new Label
        {
            Text =
                "The launcher will become the content/modding hub for DEdit, FXed, " +
                "ModelEdit, RenderStyleEditor and REZ packaging. Tool redistribution " +
                "rights still need to be verified before binaries are bundled.",
            AutoSize = true,
            MaximumSize = new Size(650, 0),
            Margin = new Padding(0, 0, 0, 20)
        });

        var modsButton = new Button
        {
            Text = "Open Mods Folder",
            Width = 190,
            Height = 38,
            FlatStyle = FlatStyle.Flat
        };
        modsButton.Click += (_, _) =>
        {
            var gameDir = FindGameDirectory();
            if(gameDir is null)
            {
                MessageBox.Show(
                    "Could not locate the FIRETEAM BUILT folder.",
                    "FIRETEAM",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Warning);
                return;
            }

            var mods = Path.Combine(gameDir, "Mods");
            Directory.CreateDirectory(mods);
            Process.Start(new ProcessStartInfo("explorer.exe", mods)
            {
                UseShellExecute = true
            });
        };
        panel.Controls.Add(modsButton);

        page.Controls.Add(panel);
        return page;
    }

    private static TabPage NewPage(string title)
    {
        return new TabPage(title)
        {
            BackColor = BackColorMain,
            ForeColor = ForeColorMain,
            Padding = new Padding(18)
        };
    }

    private static TableLayoutPanel NewGrid()
    {
        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            ColumnCount = 2,
            RowCount = 10,
            Padding = new Padding(18)
        };

        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 185));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        return grid;
    }

    private static void AddRow(
        TableLayoutPanel grid,
        int row,
        string label,
        Control control,
        string? suffix = null)
    {
        var title = new Label
        {
            Text = label,
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            Margin = new Padding(4, 12, 12, 12)
        };

        control.Anchor = AnchorStyles.Left | AnchorStyles.Right;
        control.Margin = new Padding(4, 8, 4, 8);

        if(suffix is null)
        {
            grid.Controls.Add(title, 0, row);
            grid.Controls.Add(control, 1, row);
            return;
        }

        var line = new FlowLayoutPanel
        {
            AutoSize = true,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false,
            Anchor = AnchorStyles.Left
        };
        line.Controls.Add(control);
        line.Controls.Add(new Label
        {
            Text = suffix,
            AutoSize = true,
            Margin = new Padding(5, 11, 0, 0)
        });

        grid.Controls.Add(title, 0, row);
        grid.Controls.Add(line, 1, row);
    }

    private void ApplyTheme(Control root)
    {
        foreach(Control child in root.Controls)
        {
            switch(child)
            {
                case TabControl:
                    break;
                case TabPage:
                    child.BackColor = BackColorMain;
                    child.ForeColor = ForeColorMain;
                    break;
                case ComboBox combo:
                    combo.BackColor = BackColorPanel;
                    combo.ForeColor = ForeColorMain;
                    combo.FlatStyle = FlatStyle.Flat;
                    break;
                case TextBox text:
                    text.BackColor = BackColorPanel;
                    text.ForeColor = ForeColorMain;
                    text.BorderStyle = BorderStyle.FixedSingle;
                    break;
                case NumericUpDown numeric:
                    numeric.BackColor = BackColorPanel;
                    numeric.ForeColor = ForeColorMain;
                    break;
                case Button button:
                    if(!ReferenceEquals(button, _launch))
                    {
                        button.BackColor = BackColorPanel;
                        button.ForeColor = ForeColorMain;
                    }
                    break;
            }

            if(child.HasChildren)
            {
                ApplyTheme(child);
            }
        }
    }

    private void LaunchGame()
    {
        try
        {
            var gameDir = FindGameDirectory();
            if(gameDir is null)
            {
                throw new InvalidOperationException(
                    "Could not locate Lithtech.exe. Build/publish the launcher into BUILT\\Launcher or run it from the FIRETEAM build folder.");
            }

            var mapFile = Path.Combine(
                gameDir,
                "rez",
                "Worlds",
                "CABINFEVER.DAT");

            if(!File.Exists(mapFile))
            {
                throw new FileNotFoundException(
                    "Cabin Fever is not staged in the BUILT folder.",
                    mapFile);
            }

            WriteSettings(gameDir);
            WriteSession(gameDir);

            var exe = Path.Combine(gameDir, "Lithtech.exe");
            var start = new ProcessStartInfo(exe)
            {
                WorkingDirectory = gameDir,
                UseShellExecute = false
            };

            AddArg(start, "-rez", "Engine.REZ");
            AddArg(start, "-rez", "rez");
            AddArg(start, "-config", "autoexec.cfg");
            AddArg(start, "+runworld", "Worlds/CABINFEVER");
            AddArg(start, "+autostart", "1");

            var mode = _mode.SelectedIndex switch
            {
                1 => "host",
                2 => "join",
                _ => "single"
            };
            AddArg(start, "+fireteammode", mode);

            var playerName = string.IsNullOrWhiteSpace(_playerName.Text)
                ? "Player"
                : _playerName.Text.Trim();
            AddArg(start, "+playername", playerName);

            if(mode == "join")
            {
                var ip = string.IsNullOrWhiteSpace(_joinIp.Text)
                    ? "127.0.0.1"
                    : _joinIp.Text.Trim();
                AddArg(start, "+joinip", ip);
            }

            var (width, height) = SelectedResolution();
            AddArg(start, "+screenwidth", width.ToString(CultureInfo.InvariantCulture));
            AddArg(start, "+screenheight", height.ToString(CultureInfo.InvariantCulture));
            AddArg(start, "+windowed", _windowed.Checked ? "1" : "0");

            AddArg(start, "+errorlog", "1");
            AddArg(start, "+alwaysflushlog", "1");
            AddArg(start, "+errorlogfile", "cabinfever-error.log");
            AddArg(start, "+consoleenable", "1");
            AddArg(start, "+numconsolelines", "0");

            AppendCustomCommands(
                start,
                _commands.Text);

            _status.Text = mode switch
            {
                "host" => "Starting 24-player FIRETEAM host...",
                "join" => "Connecting to FIRETEAM server...",
                _ => "Starting single-player FIRETEAM..."
            };

            Process.Start(start);
        }
        catch(Exception ex)
        {
            _status.Text = "Launch failed";
            MessageBox.Show(
                ex.Message,
                "FIRETEAM Launcher",
                MessageBoxButtons.OK,
                MessageBoxIcon.Error);
        }
    }

    private void WriteSession(string gameDir)
    {
        var configDir =
            Path.Combine(gameDir, "config");
        Directory.CreateDirectory(configDir);

        var difficulty =
            (_difficulty.SelectedItem?.ToString()
                ?? "Normal")
            .Trim()
            .ToLowerInvariant();

        File.WriteAllText(
            Path.Combine(configDir, "session.cfg"),
            $"difficulty={difficulty}\n");
    }

    private static void AppendCustomCommands(
        ProcessStartInfo start,
        string? commandText)
    {
        if(string.IsNullOrWhiteSpace(commandText))
        {
            return;
        }

        foreach(var token in TokenizeCommandLine(commandText))
        {
            start.ArgumentList.Add(token);
        }
    }

    private static IEnumerable<string> TokenizeCommandLine(
        string text)
    {
        var current = new System.Text.StringBuilder();
        var quoted = false;

        for(var i = 0; i < text.Length; ++i)
        {
            var ch = text[i];

            if(ch == '"')
            {
                quoted = !quoted;
                continue;
            }

            if(char.IsWhiteSpace(ch) && !quoted)
            {
                if(current.Length > 0)
                {
                    yield return current.ToString();
                    current.Clear();
                }
                continue;
            }

            current.Append(ch);
        }

        if(current.Length > 0)
        {
            yield return current.ToString();
        }
    }

    private void WriteSettings(string gameDir)
    {
        var (width, height) = SelectedResolution();

        // In-game menu displays sensitivity relative to the original 0.004625
        // SealHunter scale, while storing the actual raw mouse scale.
        var rawSensitivity =
            0.004625M * _sensitivity.Value;

        var line = string.Format(
            CultureInfo.InvariantCulture,
            "{0:F6} {0:F6} {1} {2:F3} {3} {4} {5}\n",
            rawSensitivity,
            (int)_volume.Value,
            _gamma.Value,
            width,
            height,
            _windowed.Checked ? 1 : 0);

        File.WriteAllText(
            Path.Combine(gameDir, "fireteam-settings.cfg"),
            line);
    }

    private (int Width, int Height) SelectedResolution()
    {
        var text = _resolution.SelectedItem?.ToString()
            ?? "1920 x 1080";

        var parts = text.Split(
            'x',
            StringSplitOptions.TrimEntries);

        return (
            int.Parse(parts[0], CultureInfo.InvariantCulture),
            int.Parse(parts[1], CultureInfo.InvariantCulture));
    }

    private static void AddArg(
        ProcessStartInfo start,
        string name,
        string value)
    {
        start.ArgumentList.Add(name);
        start.ArgumentList.Add(value);
    }

    private static string? FindGameDirectory()
    {
        var current =
            new DirectoryInfo(AppContext.BaseDirectory);

        for(var i = 0;
            current is not null && i < 5;
            ++i, current = current.Parent)
        {
            if(File.Exists(
                Path.Combine(
                    current.FullName,
                    "Lithtech.exe")))
            {
                return current.FullName;
            }
        }

        return null;
    }
}
