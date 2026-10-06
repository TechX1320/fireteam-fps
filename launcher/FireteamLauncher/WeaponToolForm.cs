using System.Diagnostics;
using System.Globalization;
using System.Text.RegularExpressions;

namespace FireteamLauncher;

public sealed class WeaponToolForm : Form
{
    private readonly ListBox _slots = new();
    private readonly Dictionary<string, TextBox> _text = new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, CheckBox> _checks = new(StringComparer.OrdinalIgnoreCase);
    private readonly Label _status = new();

    private WeaponConfigDocument? _doc;
    private string? _sourcePath;
    private string? _runtimePath;
    private string? _currentSection;

    private static readonly string[] TextKeys =
    [
        "id", "name", "type",
        "clip", "reserve", "damage",
        "fire_interval", "range",
        "effect_range0", "effect_range1", "effect_range2",
        "damage_mult0", "damage_mult1", "damage_mult2",
        "reload",
        "crosshair_base_gap", "crosshair_shot_kick",
        "crosshair_move_kick", "crosshair_max_gap",
        "crosshair_recover",
        "view_x", "view_y", "view_z",
        "zoom_fov",
        "pv_model", "pv_anim", "pv_texture",
        "hh_model", "hh_texture", "sound_dir"
    ];

    private static readonly string[] CheckKeys =
    [
        "automatic", "auto_reload", "show_crosshair", "zoom_hide_weapon"
    ];

    public WeaponToolForm()
    {
        Text = "FIRETEAM Weapon Tool";
        StartPosition = FormStartPosition.CenterParent;
        Size = new Size(980, 760);
        MinimumSize = new Size(860, 650);
        BackColor = Color.FromArgb(20, 22, 25);
        ForeColor = Color.FromArgb(235, 238, 241);
        Font = new Font("Segoe UI", 9.5F);

        var split = new SplitContainer
        {
            Dock = DockStyle.Fill,
            SplitterDistance = 220,
            FixedPanel = FixedPanel.Panel1
        };

        _slots.Dock = DockStyle.Fill;
        _slots.BackColor = Color.FromArgb(31, 34, 38);
        _slots.ForeColor = ForeColor;
        _slots.BorderStyle = BorderStyle.FixedSingle;
        _slots.SelectedIndexChanged += (_, _) => LoadSelectedSection();
        split.Panel1.Padding = new Padding(12);
        split.Panel1.Controls.Add(_slots);

        var right = new Panel
        {
            Dock = DockStyle.Fill,
            AutoScroll = true,
            Padding = new Padding(12)
        };

        var toolbar = new FlowLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = true,
            Padding = new Padding(0, 0, 0, 10)
        };

        toolbar.Controls.Add(MakeButton("Save Config", (_, _) => SaveCurrent()));
        toolbar.Controls.Add(MakeButton("Reload", (_, _) => LoadConfig()));
        toolbar.Controls.Add(MakeButton("Open weapons.cfg", (_, _) => OpenConfig()));
        toolbar.Controls.Add(MakeButton("Import CA Attributes", (_, _) => ImportAttributes()));
        right.Controls.Add(toolbar);

        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            ColumnCount = 2,
            Padding = new Padding(0, 50, 0, 15)
        };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 190));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));

        var row = 0;
        AddGroup(grid, ref row, "Identity / gameplay");
        AddText(grid, ref row, "ID", "id");
        AddText(grid, ref row, "Display Name", "name");
        AddText(grid, ref row, "Type", "type");
        AddText(grid, ref row, "Magazine", "clip");
        AddText(grid, ref row, "Reserve", "reserve");
        AddText(grid, ref row, "Damage", "damage");
        AddText(grid, ref row, "Fire Interval", "fire_interval");
        AddText(grid, ref row, "Range", "range");
        AddText(grid, ref row, "Effect Range 0", "effect_range0");
        AddText(grid, ref row, "Effect Range 1", "effect_range1");
        AddText(grid, ref row, "Effect Range 2", "effect_range2");
        AddText(grid, ref row, "Damage Mult 0", "damage_mult0");
        AddText(grid, ref row, "Damage Mult 1", "damage_mult1");
        AddText(grid, ref row, "Damage Mult 2", "damage_mult2");
        AddText(grid, ref row, "Reload Seconds", "reload");
        AddCheck(grid, ref row, "Automatic", "automatic");
        AddCheck(grid, ref row, "Auto Reload", "auto_reload");

        AddGroup(grid, ref row, "Crosshair / scope");
        AddCheck(grid, ref row, "Show Crosshair", "show_crosshair");
        AddText(grid, ref row, "Base Gap", "crosshair_base_gap");
        AddText(grid, ref row, "Shot Kick", "crosshair_shot_kick");
        AddText(grid, ref row, "Move Kick", "crosshair_move_kick");
        AddText(grid, ref row, "Max Gap", "crosshair_max_gap");
        AddText(grid, ref row, "Recover", "crosshair_recover");
        AddText(grid, ref row, "Zoom FOV", "zoom_fov");
        AddCheck(grid, ref row, "Hide Weapon Scoped", "zoom_hide_weapon");

        AddGroup(grid, ref row, "First-person placement");
        AddText(grid, ref row, "View X", "view_x");
        AddText(grid, ref row, "View Y", "view_y");
        AddText(grid, ref row, "View Z", "view_z");

        AddGroup(grid, ref row, "Assets");
        AddText(grid, ref row, "PV Model", "pv_model");
        AddText(grid, ref row, "PV Animation", "pv_anim");
        AddText(grid, ref row, "PV Texture", "pv_texture");
        AddText(grid, ref row, "HH Model", "hh_model");
        AddText(grid, ref row, "HH Texture", "hh_texture");
        AddText(grid, ref row, "Sound Dir", "sound_dir");

        right.Controls.Add(grid);

        _status.Dock = DockStyle.Bottom;
        _status.Height = 32;
        _status.TextAlign = ContentAlignment.MiddleLeft;
        _status.ForeColor = Color.FromArgb(180, 185, 190);
        right.Controls.Add(_status);

        split.Panel2.Controls.Add(right);
        Controls.Add(split);

        Shown += (_, _) => LoadConfig();
    }

    private Button MakeButton(string text, EventHandler onClick)
    {
        var button = new Button
        {
            Text = text,
            AutoSize = true,
            Height = 34,
            FlatStyle = FlatStyle.Flat,
            BackColor = Color.FromArgb(31, 34, 38),
            ForeColor = ForeColor,
            Margin = new Padding(0, 0, 8, 6)
        };
        button.Click += onClick;
        return button;
    }

    private static void AddGroup(TableLayoutPanel grid, ref int row, string title)
    {
        var label = new Label
        {
            Text = title,
            AutoSize = true,
            Font = new Font("Segoe UI Semibold", 11F, FontStyle.Bold),
            ForeColor = Color.FromArgb(227, 151, 27),
            Margin = new Padding(0, 18, 0, 6)
        };
        grid.Controls.Add(label, 0, row);
        grid.SetColumnSpan(label, 2);
        ++row;
    }

    private void AddText(TableLayoutPanel grid, ref int row, string label, string key)
    {
        var title = new Label
        {
            Text = label,
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            Margin = new Padding(0, 7, 8, 7)
        };

        var box = new TextBox
        {
            Anchor = AnchorStyles.Left | AnchorStyles.Right,
            BackColor = Color.FromArgb(31, 34, 38),
            ForeColor = ForeColor,
            BorderStyle = BorderStyle.FixedSingle,
            Margin = new Padding(0, 4, 0, 4)
        };

        _text[key] = box;
        grid.Controls.Add(title, 0, row);
        grid.Controls.Add(box, 1, row);
        ++row;
    }

    private void AddCheck(TableLayoutPanel grid, ref int row, string label, string key)
    {
        var title = new Label
        {
            Text = label,
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            Margin = new Padding(0, 7, 8, 7)
        };

        var check = new CheckBox
        {
            AutoSize = true,
            Anchor = AnchorStyles.Left,
            Margin = new Padding(0, 6, 0, 6)
        };

        _checks[key] = check;
        grid.Controls.Add(title, 0, row);
        grid.Controls.Add(check, 1, row);
        ++row;
    }

    private void LoadConfig()
    {
        try
        {
            (_sourcePath, _runtimePath) = FindConfigPaths();

            var path = _sourcePath ?? _runtimePath;
            if(path is null || !File.Exists(path))
            {
                throw new FileNotFoundException("Could not locate config\\weapons.cfg.");
            }

            _doc = WeaponConfigDocument.Load(path);

            _slots.BeginUpdate();
            _slots.Items.Clear();
            foreach(var section in _doc.Sections)
            {
                var name = _doc.GetValue(section, "name");
                _slots.Items.Add(new WeaponSlotItem(section, string.IsNullOrWhiteSpace(name) ? section : name));
            }
            _slots.EndUpdate();

            if(_slots.Items.Count > 0)
            {
                _slots.SelectedIndex = 0;
            }

            _status.Text = $"Editing: {path}";
        }
        catch(Exception ex)
        {
            MessageBox.Show(ex.Message, "Weapon Tool", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void LoadSelectedSection()
    {
        if(_doc is null || _slots.SelectedItem is not WeaponSlotItem item)
        {
            return;
        }

        _currentSection = item.Section;

        foreach(var key in TextKeys)
        {
            _text[key].Text = _doc.GetValue(_currentSection, key);
        }

        foreach(var key in CheckKeys)
        {
            _checks[key].Checked =
                _doc.GetValue(_currentSection, key) == "1";
        }

        _status.Text = $"Editing {item.DisplayName} ({item.Section})";
    }

    private void SaveCurrent()
    {
        if(_doc is null || _currentSection is null)
        {
            return;
        }

        foreach(var key in TextKeys)
        {
            _doc.SetValue(_currentSection, key, _text[key].Text.Trim());
        }

        foreach(var key in CheckKeys)
        {
            _doc.SetValue(_currentSection, key, _checks[key].Checked ? "1" : "0");
        }

        try
        {
            if(_sourcePath is not null)
            {
                Directory.CreateDirectory(Path.GetDirectoryName(_sourcePath)!);
                _doc.Save(_sourcePath);
            }

            if(_runtimePath is not null)
            {
                Directory.CreateDirectory(Path.GetDirectoryName(_runtimePath)!);
                _doc.Save(_runtimePath);
            }

            _status.Text = "Saved source + current BUILT weapon config.";
        }
        catch(Exception ex)
        {
            MessageBox.Show(ex.Message, "Weapon Tool", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void OpenConfig()
    {
        var path = _sourcePath ?? _runtimePath;
        if(path is null || !File.Exists(path))
        {
            return;
        }

        Process.Start(new ProcessStartInfo(path)
        {
            UseShellExecute = true
        });
    }

    private void ImportAttributes()
    {
        if(_doc is null || _currentSection is null)
        {
            return;
        }

        using var dialog = new OpenFileDialog
        {
            Title = "Import Combat Arms decrypted weapon attributes",
            Filter = "CA attributes (*.txt;*.rar)|*.txt;*.rar|Text files (*.txt)|*.txt|RAR archives (*.rar)|*.rar|All files (*.*)|*.*"
        };

        if(dialog.ShowDialog(this) != DialogResult.OK)
        {
            return;
        }

        try
        {
            var files = ResolveAttributeFiles(dialog.FileName);
            var imported = false;

            foreach(var file in files)
            {
                if(!File.Exists(file))
                    continue;

                var leaf = Path.GetFileName(file);
                if(leaf.Contains("WEAPONS", StringComparison.OrdinalIgnoreCase))
                {
                    imported |= ImportWeaponsText(File.ReadAllText(file));
                }
                else if(leaf.Contains("ITEMS", StringComparison.OrdinalIgnoreCase))
                {
                    imported |= ImportItemsText(File.ReadAllText(file));
                }
                else
                {
                    var text = File.ReadAllText(file);
                    imported |= ImportWeaponsText(text);
                    imported |= ImportItemsText(text);
                }
            }

            if(!imported)
            {
                MessageBox.Show(
                    "No matching Combat Arms weapon block was found for the selected FIRETEAM weapon.",
                    "Weapon Tool",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information);
                return;
            }

            _status.Text = "Imported CA attribute values into the editor. Press Save Config to commit them.";
        }
        catch(Exception ex)
        {
            MessageBox.Show(ex.Message, "Weapon Tool", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private bool ImportWeaponsText(string text)
    {
        var targetName = NormalizeWeaponName(_text["name"].Text);
        var targetId = NormalizeWeaponName(_text["id"].Text);

        foreach(Match match in Regex.Matches(
            text,
            @"(?ms)^\[Weapon\d+\]\s*(.*?)(?=^\[|\z)"))
        {
            var block = match.Groups[1].Value;
            var name = ReadCaValue(block, "Name");
            var normalized = NormalizeWeaponName(name);

            if(normalized.Length == 0 ||
               (normalized != targetName && normalized != targetId))
            {
                continue;
            }

            CopyCa(block, "ShotsPerClip", "clip");
            CopyCa(block, "Range", "range");
            CopyCa(block, "Effectrange0", "effect_range0");
            CopyCa(block, "Effectrange1", "effect_range1");
            CopyCa(block, "Effectrange2", "effect_range2");

            var disabled = ReadCaValue(block, "DisableCrosshair");
            if(int.TryParse(disabled, out var disableCrosshair))
            {
                _checks["show_crosshair"].Checked = disableCrosshair == 0;
            }

            var pos = ReadCaValue(block, "Pos");
            var vector = ParseVector(pos);
            if(vector is not null)
            {
                _text["view_x"].Text = vector.Value.X;
                _text["view_y"].Text = vector.Value.Y;
                _text["view_z"].Text = vector.Value.Z;
            }

            var caPv = ReadCaValue(block, "PVModelNormal");
            var caSkin = ReadCaValue(block, "PVSkin1");
            var caHh = ReadCaValue(block, "HHModel");
            var caHhSkin = ReadCaValue(block, "HHSkin0");

            _status.Text =
                $"CA source: PV={caPv} | Skin={caSkin} | HH={caHh} | HH Skin={caHhSkin}";

            return true;
        }

        return false;
    }

    private bool ImportItemsText(string text)
    {
        var targetName = NormalizeWeaponName(_text["name"].Text);
        var targetId = NormalizeWeaponName(_text["id"].Text);

        foreach(Match match in Regex.Matches(
            text,
            @"(?ms)^\[WeaponItem\d+\]\s*(.*?)(?=^\[|\z)"))
        {
            var block = match.Groups[1].Value;
            var normalizedBlock = NormalizeWeaponName(block);

            if(!normalizedBlock.Contains(targetName) &&
               !normalizedBlock.Contains(targetId))
            {
                continue;
            }

            CopyCa(block, "Damage", "damage");
            CopyCa(block, "ClipQuota", "clip");
            return true;
        }

        return false;
    }

    private void CopyCa(string block, string caKey, string fireteamKey)
    {
        var value = ReadCaValue(block, caKey);
        if(value.Length > 0 && _text.TryGetValue(fireteamKey, out var box))
        {
            box.Text = value;
        }
    }

    private static string ReadCaValue(string block, string key)
    {
        var match = Regex.Match(
            block,
            $@"(?im)^\s*{Regex.Escape(key)}\s*=\s*(.+?)\s*$");

        if(!match.Success)
            return string.Empty;

        var value = match.Groups[1].Value;
        var comment = value.IndexOf("//", StringComparison.Ordinal);
        if(comment >= 0)
            value = value[..comment];

        return value.Trim().Trim('"');
    }

    private static (string X, string Y, string Z)? ParseVector(string value)
    {
        var match = Regex.Match(
            value,
            @"<\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*>");

        if(!match.Success)
            return null;

        return (
            match.Groups[1].Value,
            match.Groups[2].Value,
            match.Groups[3].Value);
    }

    private static IEnumerable<string> ResolveAttributeFiles(string selectedFile)
    {
        if(!selectedFile.EndsWith(".rar", StringComparison.OrdinalIgnoreCase))
        {
            return [selectedFile];
        }

        var sevenZip = FindSevenZip();
        if(sevenZip is null)
        {
            throw new InvalidOperationException(
                "Direct RAR import requires 7-Zip. Install 7-Zip, or extract WEAPONS-DEC.TXT / ITEMS-DEC.TXT and import the text files directly.");
        }

        var temp = Path.Combine(
            Path.GetTempPath(),
            "FireteamWeaponImport",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);

        var start = new ProcessStartInfo(sevenZip)
        {
            UseShellExecute = false,
            CreateNoWindow = true
        };
        start.ArgumentList.Add("e");
        start.ArgumentList.Add("-y");
        start.ArgumentList.Add($"-o{temp}");
        start.ArgumentList.Add(selectedFile);
        start.ArgumentList.Add("ATTRIBUTES/WEAPONS-DEC.TXT");
        start.ArgumentList.Add("ATTRIBUTES/ITEMS-DEC.TXT");

        using var process = Process.Start(start)
            ?? throw new InvalidOperationException("Could not start 7-Zip.");
        process.WaitForExit();

        if(process.ExitCode != 0)
        {
            throw new InvalidOperationException("7-Zip could not extract the selected attribute archive.");
        }

        return Directory.GetFiles(temp, "*-DEC.TXT", SearchOption.AllDirectories);
    }

    private static string? FindSevenZip()
    {
        var candidates = new[]
        {
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "7-Zip", "7z.exe"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "7-Zip", "7z.exe")
        };

        return candidates.FirstOrDefault(File.Exists);
    }

    private static string NormalizeWeaponName(string value)
    {
        return new string(
            value
                .Where(char.IsLetterOrDigit)
                .Select(char.ToLowerInvariant)
                .ToArray());
    }

    private static (string? Source, string? Runtime) FindConfigPaths()
    {
        string? source = null;
        string? runtime = null;

        var current = new DirectoryInfo(AppContext.BaseDirectory);
        for(var i = 0; current is not null && i < 7; ++i, current = current.Parent)
        {
            var sourceCandidate = Path.Combine(current.FullName, "config", "weapons.cfg");
            if(source is null && File.Exists(sourceCandidate))
            {
                source = sourceCandidate;
            }

            var runtimeCandidate = Path.Combine(current.FullName, "BUILT", "config", "weapons.cfg");
            if(runtime is null && File.Exists(runtimeCandidate))
            {
                runtime = runtimeCandidate;
            }

            if(string.Equals(current.Name, "BUILT", StringComparison.OrdinalIgnoreCase))
            {
                var directRuntime = Path.Combine(current.FullName, "config", "weapons.cfg");
                if(File.Exists(directRuntime))
                {
                    runtime = directRuntime;
                }
            }
        }

        return (source, runtime);
    }

    private sealed record WeaponSlotItem(string Section, string DisplayName)
    {
        public override string ToString() => DisplayName;
    }

    private sealed class WeaponConfigDocument
    {
        private readonly List<string> _lines;

        private WeaponConfigDocument(IEnumerable<string> lines)
        {
            _lines = lines.ToList();
        }

        public IEnumerable<string> Sections =>
            _lines
                .Select(line => Regex.Match(line.Trim(), @"^\[(weapon\d+)\]$", RegexOptions.IgnoreCase))
                .Where(match => match.Success)
                .Select(match => match.Groups[1].Value);

        public static WeaponConfigDocument Load(string path) =>
            new(File.ReadAllLines(path));

        public string GetValue(string section, string key)
        {
            var (start, end) = FindSection(section);
            if(start < 0)
                return string.Empty;

            for(var i = start + 1; i < end; ++i)
            {
                var line = _lines[i].Trim();
                if(line.StartsWith("#") || line.StartsWith(";"))
                    continue;

                var equals = line.IndexOf('=');
                if(equals < 0)
                    continue;

                if(string.Equals(line[..equals].Trim(), key, StringComparison.OrdinalIgnoreCase))
                    return line[(equals + 1)..].Trim();
            }

            return string.Empty;
        }

        public void SetValue(string section, string key, string value)
        {
            var (start, end) = FindSection(section);
            if(start < 0)
                return;

            for(var i = start + 1; i < end; ++i)
            {
                var line = _lines[i].Trim();
                var equals = line.IndexOf('=');
                if(equals < 0)
                    continue;

                if(string.Equals(line[..equals].Trim(), key, StringComparison.OrdinalIgnoreCase))
                {
                    _lines[i] = $"{key}={value}";
                    return;
                }
            }

            _lines.Insert(end, $"{key}={value}");
        }

        public void Save(string path) =>
            File.WriteAllLines(path, _lines);

        private (int Start, int End) FindSection(string section)
        {
            var start = -1;

            for(var i = 0; i < _lines.Count; ++i)
            {
                var trimmed = _lines[i].Trim();
                if(trimmed.StartsWith("[") && trimmed.EndsWith("]"))
                {
                    if(start >= 0)
                        return (start, i);

                    if(string.Equals(
                        trimmed,
                        $"[{section}]",
                        StringComparison.OrdinalIgnoreCase))
                    {
                        start = i;
                    }
                }
            }

            return start >= 0
                ? (start, _lines.Count)
                : (-1, -1);
        }
    }
}
