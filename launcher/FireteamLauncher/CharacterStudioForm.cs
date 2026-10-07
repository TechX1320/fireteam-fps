namespace FireteamLauncher;

public sealed class CharacterStudioForm : Form
{
    private static readonly Color Bg = Color.FromArgb(14, 16, 18);
    private static readonly Color Panel = Color.FromArgb(26, 29, 31);
    private static readonly Color Panel2 = Color.FromArgb(33, 36, 39);
    private static readonly Color Border = Color.FromArgb(57, 61, 65);
    private static readonly Color TextMain = Color.FromArgb(235, 238, 241);
    private static readonly Color TextMuted = Color.FromArgb(155, 161, 166);
    private static readonly Color Green = Color.FromArgb(38, 216, 88);
    private static readonly Color Cyan = Color.FromArgb(44, 184, 224);

    private readonly TextBox _search = new();
    private readonly ListBox _profiles = new();
    private readonly Label _title = new();
    private readonly Label _subtitle = new();
    private readonly Label _status = new();

    private readonly Dictionary<string, TextBox> _characterFields =
        new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, CheckBox> _characterChecks =
        new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, TextBox> _gameplayFields =
        new(StringComparer.OrdinalIgnoreCase);

    private FireteamConfigDocument? _characters;
    private FireteamConfigDocument? _infected;
    private string? _charactersSource;
    private string? _charactersRuntime;
    private string? _infectedSource;
    private string? _infectedRuntime;
    private string? _selectedSection;

    public CharacterStudioForm()
    {
        Text = "FIRETEAM Content Studio — Characters";
        StartPosition = FormStartPosition.CenterParent;
        MinimumSize = new Size(1040, 700);
        Size = new Size(1240, 800);
        BackColor = Bg;
        ForeColor = TextMain;
        Font = new Font("Segoe UI", 9.5F);

        Controls.Add(BuildRoot());
        Shown += (_, _) => LoadConfigs();
    }

    private Control BuildRoot()
    {
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 2,
            BackColor = Bg
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 270));
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 58));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));

        var top = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = Color.FromArgb(9, 11, 12),
            Padding = new Padding(14, 8, 14, 7)
        };
        root.Controls.Add(top, 0, 0);
        root.SetColumnSpan(top, 2);

        top.Controls.Add(new Label
        {
            Text = "FIRETEAM",
            AutoSize = true,
            Font = new Font("Segoe UI Semibold", 12F, FontStyle.Bold),
            ForeColor = Green,
            Location = new Point(12, 10)
        });
        top.Controls.Add(new Label
        {
            Text = "CONTENT STUDIO / CHARACTERS",
            AutoSize = true,
            Font = new Font("Segoe UI Semibold", 9F, FontStyle.Bold),
            ForeColor = TextMain,
            Location = new Point(97, 13)
        });

        var buttons = new FlowLayoutPanel
        {
            Dock = DockStyle.Right,
            AutoSize = true,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false
        };
        buttons.Controls.Add(Button("SAVE", (_, _) => Save(), true));
        buttons.Controls.Add(Button("RELOAD", (_, _) => LoadConfigs()));
        buttons.Controls.Add(Button("OPEN CHARACTER CFG", (_, _) => Open(_charactersSource ?? _charactersRuntime)));
        buttons.Controls.Add(Button("OPEN INFECTED CFG", (_, _) => Open(_infectedSource ?? _infectedRuntime)));
        top.Controls.Add(buttons);

        root.Controls.Add(BuildSidebar(), 0, 1);
        root.Controls.Add(BuildEditor(), 1, 1);
        return root;
    }

    private Control BuildSidebar()
    {
        var panel = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = Color.FromArgb(17, 19, 21),
            Padding = new Padding(12)
        };

        panel.Controls.Add(new Label
        {
            Text = "CHARACTER PROFILES",
            Dock = DockStyle.Top,
            Height = 25,
            Font = new Font("Segoe UI Semibold", 8F, FontStyle.Bold),
            ForeColor = Green
        });

        _search.Dock = DockStyle.Top;
        _search.Height = 32;
        _search.PlaceholderText = "Search profile...";
        _search.BackColor = Panel2;
        _search.ForeColor = TextMain;
        _search.BorderStyle = BorderStyle.FixedSingle;
        _search.TextChanged += (_, _) => RefreshProfiles();
        panel.Controls.Add(_search);
        _search.BringToFront();

        _profiles.Dock = DockStyle.Fill;
        _profiles.BackColor = Panel;
        _profiles.ForeColor = TextMain;
        _profiles.BorderStyle = BorderStyle.None;
        _profiles.SelectedIndexChanged += (_, _) => LoadSelection();
        panel.Controls.Add(_profiles);
        _profiles.BringToFront();

        return panel;
    }

    private Control BuildEditor()
    {
        var host = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = Bg,
            Padding = new Padding(18, 14, 18, 14)
        };

        var header = new Panel
        {
            Dock = DockStyle.Top,
            Height = 86
        };
        header.Controls.Add(new Label
        {
            Text = "INFECTED / CHARACTER LAB",
            AutoSize = true,
            ForeColor = Green,
            Font = new Font("Segoe UI Semibold", 8F, FontStyle.Bold),
            Location = new Point(0, 2)
        });

        _title.Text = "Select a character";
        _title.AutoSize = true;
        _title.Font = new Font("Segoe UI Semibold", 17F, FontStyle.Bold);
        _title.Location = new Point(0, 21);
        header.Controls.Add(_title);

        _subtitle.AutoSize = true;
        _subtitle.ForeColor = Cyan;
        _subtitle.Location = new Point(2, 56);
        header.Controls.Add(_subtitle);

        host.Controls.Add(header);

        var tabs = new TabControl
        {
            Dock = DockStyle.Fill,
            Padding = new Point(14, 6)
        };
        tabs.TabPages.Add(BuildBodyTab());
        tabs.TabPages.Add(BuildFaceTab());
        tabs.TabPages.Add(BuildAnimationTab());
        tabs.TabPages.Add(BuildGameplayTab());
        host.Controls.Add(tabs);
        tabs.BringToFront();

        _status.Dock = DockStyle.Bottom;
        _status.Height = 30;
        _status.ForeColor = TextMuted;
        _status.TextAlign = ContentAlignment.MiddleLeft;
        host.Controls.Add(_status);

        return host;
    }

    private TabPage BuildBodyTab()
    {
        var tab = NewTab("Body / Model");
        var flow = Flow();

        var model = Card("BODY COMPOSITION");
        var grid = Grid();
        AddCharacterText(grid, "Body model", "body_model", 0);
        AddCharacterText(grid, "Animation child", "animation_model", 1);
        AddCharacterText(grid, "Body texture 0", "body_texture0", 2);
        AddCharacterText(grid, "Body texture 1", "body_texture1", 3);
        AddCharacterText(grid, "RenderStyle 0", "body_renderstyle0", 4);
        AddCharacterText(grid, "RenderStyle 1", "body_renderstyle1", 5);
        AddCharacterText(grid, "Optional head model", "head_model", 6);
        AddCharacterText(grid, "Optional head texture", "head_texture", 7);
        model.Controls.Add(grid);
        flow.Controls.Add(model);

        tab.Controls.Add(flow);
        return tab;
    }

    private TabPage BuildFaceTab()
    {
        var tab = NewTab("Face");
        var flow = Flow();

        var face = Card("ATTACH FACE");
        var grid = Grid();
        AddCharacterText(grid, "Mode", "face_mode", 0);
        AddCharacterText(grid, "Face model", "face_model", 1);
        AddCharacterText(grid, "Face texture", "face_texture", 2);
        AddCharacterText(grid, "RenderStyle 0", "face_renderstyle0", 3);
        AddCharacterText(grid, "RenderStyle 1", "face_renderstyle1", 4);
        AddCharacterText(grid, "Body socket", "face_socket", 5);
        AddCharacterText(grid, "Face align node", "face_align_node", 6);
        AddCharacterCheck(grid, "Auto align skeleton", "face_auto_align", 7);
        face.Controls.Add(grid);
        flow.Controls.Add(face);

        var transform = Card("MANUAL ATTACHMENT OFFSET");
        var transformGrid = Grid();
        AddCharacterText(transformGrid, "Position X", "face_pos_x", 0);
        AddCharacterText(transformGrid, "Position Y", "face_pos_y", 1);
        AddCharacterText(transformGrid, "Position Z", "face_pos_z", 2);
        AddCharacterText(transformGrid, "Rotation X", "face_rot_x", 3);
        AddCharacterText(transformGrid, "Rotation Y", "face_rot_y", 4);
        AddCharacterText(transformGrid, "Rotation Z", "face_rot_z", 5);
        transform.Controls.Add(transformGrid);
        flow.Controls.Add(transform);

        tab.Controls.Add(flow);
        return tab;
    }

    private TabPage BuildAnimationTab()
    {
        var tab = NewTab("Animations");
        var flow = Flow();

        var animations = Card("STATE ANIMATION MAP");
        var grid = Grid();
        AddCharacterText(grid, "Idle", "idle_anim", 0);
        AddCharacterText(grid, "Walk", "walk_anim", 1);
        AddCharacterText(grid, "Run", "run_anim", 2);
        AddCharacterText(grid, "Death", "death_anim", 3);
        AddCharacterText(grid, "Death visibility seconds", "death_seconds", 4);

        var note = new Label
        {
            Text = "Normal infected currently uses the ST_M_CHILD virus animation bank: VLST / VLWFR / VLRFR / VDIE.",
            AutoSize = true,
            MaximumSize = new Size(660, 0),
            ForeColor = TextMuted,
            Margin = new Padding(3, 10, 3, 7)
        };
        grid.Controls.Add(note, 1, 5);
        animations.Controls.Add(grid);
        flow.Controls.Add(animations);

        tab.Controls.Add(flow);
        return tab;
    }

    private TabPage BuildGameplayTab()
    {
        var tab = NewTab("Gameplay / Collision");
        var flow = Flow();

        var gameplay = Card("SERVER GAMEPLAY");
        var grid = Grid();
        AddGameplayText(grid, "Health", "health", 0);
        AddGameplayText(grid, "Run speed", "run_speed", 1);
        AddGameplayText(grid, "Attack damage", "attack_damage", 2);
        AddGameplayText(grid, "Attack range", "attack_range", 3);
        AddGameplayText(grid, "Attack cooldown", "attack_cooldown", 4);
        AddGameplayText(grid, "AI update seconds", "update_seconds", 5);
        gameplay.Controls.Add(grid);
        flow.Controls.Add(gameplay);

        var collision = Card("COLLISION");
        var collisionGrid = Grid();
        AddGameplayText(collisionGrid, "Mode", "collision_mode", 0);
        AddGameplayText(collisionGrid, "Half-width X", "collision_x", 1);
        AddGameplayText(collisionGrid, "Half-height Y", "collision_y", 2);
        AddGameplayText(collisionGrid, "Half-depth Z", "collision_z", 3);
        collision.Controls.Add(collisionGrid);
        flow.Controls.Add(collision);

        tab.Controls.Add(flow);
        return tab;
    }

    private void LoadConfigs()
    {
        try
        {
            var sourceRoot = FindRepositoryRoot();
            var gameRoot = FindGameRoot();

            _charactersSource = sourceRoot is null ? null : Path.Combine(sourceRoot, "config", "characters.cfg");
            _infectedSource = sourceRoot is null ? null : Path.Combine(sourceRoot, "config", "infected.cfg");
            _charactersRuntime = gameRoot is null ? null : Path.Combine(gameRoot, "config", "characters.cfg");
            _infectedRuntime = gameRoot is null ? null : Path.Combine(gameRoot, "config", "infected.cfg");

            var chars = FirstExisting(_charactersSource, _charactersRuntime)
                ?? throw new FileNotFoundException("Could not locate config\\characters.cfg.");
            var infected = FirstExisting(_infectedSource, _infectedRuntime)
                ?? throw new FileNotFoundException("Could not locate config\\infected.cfg.");

            _characters = FireteamConfigDocument.Load(chars);
            _infected = FireteamConfigDocument.Load(infected);
            _selectedSection = null;
            RefreshProfiles();
            _status.Text = "Character presentation and infected gameplay are both external/editable.";
        }
        catch(Exception ex)
        {
            Error(ex.Message);
        }
    }

    private void RefreshProfiles()
    {
        if(_characters is null)
            return;

        var selected = _selectedSection;
        var filter = _search.Text.Trim();

        _profiles.BeginUpdate();
        _profiles.Items.Clear();

        foreach(var section in _characters.Sections)
        {
            if(section.Equals("settings", StringComparison.OrdinalIgnoreCase))
                continue;

            if(filter.Length > 0 &&
               !section.Contains(filter, StringComparison.OrdinalIgnoreCase))
                continue;

            _profiles.Items.Add(new ProfileItem(section));
        }

        _profiles.EndUpdate();

        if(selected is not null)
        {
            foreach(ProfileItem item in _profiles.Items)
            {
                if(item.Section.Equals(selected, StringComparison.OrdinalIgnoreCase))
                {
                    _profiles.SelectedItem = item;
                    break;
                }
            }
        }

        if(_profiles.SelectedIndex < 0 && _profiles.Items.Count > 0)
            _profiles.SelectedIndex = 0;
    }

    private void LoadSelection()
    {
        if(_characters is null || _infected is null ||
           _profiles.SelectedItem is not ProfileItem item)
            return;

        SaveFieldsToDocs();
        _selectedSection = item.Section;

        _title.Text = _characters.GetValue(item.Section, "name", item.Section.Replace('_', ' '));
        _subtitle.Text = item.Section;

        foreach(var (key, box) in _characterFields)
            box.Text = _characters.GetValue(item.Section, key);

        foreach(var (key, check) in _characterChecks)
            check.Checked = _characters.GetBool(item.Section, key);

        foreach(var (key, box) in _gameplayFields)
            box.Text = _infected.GetValue(item.Section, key);

        _status.Text = "Edits save to config/characters.cfg and config/infected.cfg; no C++ rebuild is required for pure tuning.";
    }

    private void Save()
    {
        try
        {
            SaveFieldsToDocs();

            if(_characters is not null)
            {
                if(_charactersSource is not null)
                    _characters.Save(_charactersSource);
                if(_charactersRuntime is not null)
                    _characters.Save(_charactersRuntime);
            }

            if(_infected is not null)
            {
                if(_infectedSource is not null)
                    _infected.Save(_infectedSource);
                if(_infectedRuntime is not null)
                    _infected.Save(_infectedRuntime);
            }

            _status.Text = "Saved character + infected configs.";
        }
        catch(Exception ex)
        {
            Error(ex.Message);
        }
    }

    private void SaveFieldsToDocs()
    {
        if(string.IsNullOrWhiteSpace(_selectedSection) ||
           _characters is null ||
           _infected is null)
            return;

        foreach(var (key, box) in _characterFields)
            _characters.SetValue(_selectedSection, key, box.Text.Trim());

        foreach(var (key, check) in _characterChecks)
            _characters.SetValue(_selectedSection, key, check.Checked ? "1" : "0");

        _infected.EnsureSection(_selectedSection);
        foreach(var (key, box) in _gameplayFields)
            _infected.SetValue(_selectedSection, key, box.Text.Trim());
    }

    private void AddCharacterText(TableLayoutPanel grid, string label, string key, int row)
    {
        var box = EditBox();
        _characterFields[key] = box;
        grid.Controls.Add(Label(label), 0, row);
        grid.Controls.Add(box, 1, row);
    }

    private void AddGameplayText(TableLayoutPanel grid, string label, string key, int row)
    {
        var box = EditBox();
        _gameplayFields[key] = box;
        grid.Controls.Add(Label(label), 0, row);
        grid.Controls.Add(box, 1, row);
    }

    private void AddCharacterCheck(TableLayoutPanel grid, string label, string key, int row)
    {
        var check = new CheckBox
        {
            AutoSize = true,
            ForeColor = TextMain,
            Margin = new Padding(3, 7, 3, 7)
        };
        _characterChecks[key] = check;
        grid.Controls.Add(Label(label), 0, row);
        grid.Controls.Add(check, 1, row);
    }

    private static TextBox EditBox() =>
        new()
        {
            Dock = DockStyle.Fill,
            BackColor = Panel2,
            ForeColor = TextMain,
            BorderStyle = BorderStyle.FixedSingle,
            Margin = new Padding(3, 4, 3, 4)
        };

    private static Panel Card(string heading)
    {
        var card = new Panel
        {
            Width = 820,
            AutoSize = true,
            BackColor = Panel,
            Padding = new Padding(12),
            Margin = new Padding(0, 0, 0, 12)
        };
        card.Controls.Add(new Label
        {
            Text = heading,
            Dock = DockStyle.Top,
            Height = 28,
            ForeColor = Green,
            Font = new Font("Segoe UI Semibold", 8F, FontStyle.Bold)
        });
        return card;
    }

    private static TableLayoutPanel Grid()
    {
        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            ColumnCount = 2,
            Padding = new Padding(0, 30, 0, 0)
        };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 200));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        return grid;
    }

    private static FlowLayoutPanel Flow() =>
        new()
        {
            Dock = DockStyle.Fill,
            AutoScroll = true,
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            Padding = new Padding(3)
        };

    private static TabPage NewTab(string title) =>
        new(title)
        {
            BackColor = Bg,
            ForeColor = TextMain,
            Padding = new Padding(8)
        };

    private static Label Label(string text) =>
        new()
        {
            Text = text,
            AutoSize = true,
            ForeColor = TextMuted,
            Margin = new Padding(3, 8, 8, 5)
        };

    private static Button Button(string text, EventHandler handler, bool accent = false)
    {
        var button = new Button
        {
            Text = text,
            AutoSize = true,
            Height = 34,
            FlatStyle = FlatStyle.Flat,
            BackColor = accent ? Green : Panel2,
            ForeColor = accent ? Color.Black : TextMain,
            Font = new Font("Segoe UI Semibold", 8F, FontStyle.Bold),
            Margin = new Padding(0, 2, 8, 2)
        };
        button.FlatAppearance.BorderColor = accent ? Green : Border;
        button.Click += handler;
        return button;
    }

    private static void Open(string? path)
    {
        if(path is null || !File.Exists(path))
            return;
        System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(path)
        {
            UseShellExecute = true
        });
    }

    private static string? FirstExisting(params string?[] paths) =>
        paths.FirstOrDefault(path => path is not null && File.Exists(path));

    private static string? FindRepositoryRoot()
    {
        var current = new DirectoryInfo(AppContext.BaseDirectory);
        for(var i = 0; current is not null && i < 8; ++i, current = current.Parent)
        {
            if(File.Exists(Path.Combine(current.FullName, "build.cmd")) &&
               Directory.Exists(Path.Combine(current.FullName, "config")))
                return current.FullName;
        }
        return null;
    }

    private static string? FindGameRoot()
    {
        var current = new DirectoryInfo(AppContext.BaseDirectory);
        for(var i = 0; current is not null && i < 8; ++i, current = current.Parent)
        {
            if(File.Exists(Path.Combine(current.FullName, "Lithtech.exe")))
                return current.FullName;

            var built = Path.Combine(current.FullName, "BUILT");
            if(File.Exists(Path.Combine(built, "Lithtech.exe")))
                return built;
        }
        return null;
    }

    private void Error(string message)
    {
        _status.Text = message;
        MessageBox.Show(
            message,
            "FIRETEAM Character Studio",
            MessageBoxButtons.OK,
            MessageBoxIcon.Error);
    }

    private sealed record ProfileItem(string Section)
    {
        public override string ToString() => Section.Replace('_', ' ');
    }
}
