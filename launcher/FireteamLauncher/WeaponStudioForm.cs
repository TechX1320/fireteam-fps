using System.Diagnostics;
using System.IO.Compression;
using System.Text;
using System.Text.RegularExpressions;

namespace FireteamLauncher;

public sealed class WeaponStudioForm : Form
{
    private static readonly Color Bg = Color.FromArgb(14, 16, 18);
    private static readonly Color Panel = Color.FromArgb(26, 29, 31);
    private static readonly Color Panel2 = Color.FromArgb(33, 36, 39);
    private static readonly Color Border = Color.FromArgb(57, 61, 65);
    private static readonly Color TextMain = Color.FromArgb(235, 238, 241);
    private static readonly Color TextMuted = Color.FromArgb(155, 161, 166);
    private static readonly Color Green = Color.FromArgb(38, 216, 88);
    private static readonly Color Cyan = Color.FromArgb(44, 184, 224);
    private static readonly Color Red = Color.FromArgb(236, 88, 93);

    private static readonly string[] RuntimeKeys =
    [
        "id", "name", "type",
        "clip", "reserve", "damage",
        "fire_interval", "range",
        "effect_range0", "effect_range1", "effect_range2",
        "damage_mult0", "damage_mult1", "damage_mult2",
        "reload",
        "penetration_max_thickness",
        "penetration_damage_mult",
        "penetration_range_mult",
        "automatic", "auto_reload", "show_crosshair",
        "zoom_fov", "zoom_hide_weapon",
        "crosshair_base_gap", "crosshair_shot_kick",
        "crosshair_move_kick", "crosshair_max_gap",
        "crosshair_recover",
        "view_x", "view_y", "view_z",
        "pv_model", "pv_anim", "pv_texture",
        "hh_model", "hh_texture", "sound_dir"
    ];

    private readonly TextBox _search = new();
    private readonly ListView _weaponList = new();
    private readonly Label _count = new();
    private readonly Label _title = new();
    private readonly Label _subtitle = new();
    private readonly Label _sourceBadge = new();
    private readonly Label _status = new();

    private readonly Dictionary<string, TextBox> _text =
        new(StringComparer.OrdinalIgnoreCase);
    private readonly Dictionary<string, CheckBox> _checks =
        new(StringComparer.OrdinalIgnoreCase);

    private readonly ComboBox _assignSlot = new();
    private readonly CheckBox _enabled = new();
    private readonly Label _support = new();

    private FireteamConfigDocument? _doc;
    private string? _sourcePath;
    private string? _runtimePath;
    private string? _selectedSection;
    private bool _suppressChecks;

    public WeaponStudioForm()
    {
        Text = "FIRETEAM Content Studio — Weapons";
        StartPosition = FormStartPosition.CenterParent;
        MinimumSize = new Size(1060, 720);
        Size = new Size(1280, 820);
        BackColor = Bg;
        ForeColor = TextMain;
        Font = new Font("Segoe UI", 9.5F);

        Controls.Add(BuildRoot());
        Shown += (_, _) => LoadConfig();
    }

    private Control BuildRoot()
    {
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            ColumnCount = 2,
            RowCount = 2,
            BackColor = Bg,
            Padding = new Padding(0)
        };
        root.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 285));
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

        var brand = new Label
        {
            Text = "FIRETEAM",
            AutoSize = true,
            Font = new Font("Segoe UI Semibold", 12F, FontStyle.Bold),
            ForeColor = Green,
            Location = new Point(12, 10)
        };
        top.Controls.Add(brand);

        var crumb = new Label
        {
            Text = "CONTENT STUDIO / WEAPONS",
            AutoSize = true,
            Font = new Font("Segoe UI Semibold", 9F, FontStyle.Bold),
            ForeColor = TextMain,
            Location = new Point(97, 13)
        };
        top.Controls.Add(crumb);

        var actions = new FlowLayoutPanel
        {
            Dock = DockStyle.Right,
            AutoSize = true,
            FlowDirection = FlowDirection.LeftToRight,
            WrapContents = false
        };
        actions.Controls.Add(Button("SAVE", (_, _) => SaveCurrent(), true));
        actions.Controls.Add(Button("AUTO IMPORT CA CATALOG", (_, _) => AutoImportCatalog(), true));
        actions.Controls.Add(Button("RELOAD", (_, _) => LoadConfig()));
        actions.Controls.Add(Button("OPEN CFG", (_, _) => OpenConfig()));
        top.Controls.Add(actions);

        root.Controls.Add(BuildSidebar(), 0, 1);
        root.Controls.Add(BuildEditor(), 1, 1);

        return root;
    }

    private Control BuildSidebar()
    {
        var side = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = Color.FromArgb(17, 19, 21),
            Padding = new Padding(12)
        };

        var section = new Label
        {
            Text = "WEAPON CATALOG",
            Dock = DockStyle.Top,
            Height = 25,
            Font = new Font("Segoe UI Semibold", 8F, FontStyle.Bold),
            ForeColor = Green
        };
        side.Controls.Add(section);

        _search.Dock = DockStyle.Top;
        _search.Height = 32;
        _search.PlaceholderText = "Search weapon name or ID...";
        _search.BackColor = Panel2;
        _search.ForeColor = TextMain;
        _search.BorderStyle = BorderStyle.FixedSingle;
        _search.TextChanged += (_, _) => RefreshList();
        side.Controls.Add(_search);
        _search.BringToFront();

        _count.Dock = DockStyle.Bottom;
        _count.Height = 28;
        _count.ForeColor = TextMuted;
        _count.TextAlign = ContentAlignment.MiddleLeft;
        side.Controls.Add(_count);

        _weaponList.Dock = DockStyle.Fill;
        _weaponList.View = View.Details;
        _weaponList.FullRowSelect = true;
        _weaponList.HideSelection = false;
        _weaponList.CheckBoxes = true;
        _weaponList.BorderStyle = BorderStyle.None;
        _weaponList.BackColor = Panel;
        _weaponList.ForeColor = TextMain;
        _weaponList.Columns.Add("WEAPON", 172);
        _weaponList.Columns.Add("TYPE", 70);
        _weaponList.SelectedIndexChanged += (_, _) => LoadSelection();
        _weaponList.ItemChecked += WeaponListItemChecked;
        side.Controls.Add(_weaponList);
        _weaponList.BringToFront();

        return side;
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
            Height = 86,
            BackColor = Bg
        };

        var lab = new Label
        {
            Text = "WEAPON AUTHORING LAB",
            AutoSize = true,
            ForeColor = Green,
            Font = new Font("Segoe UI Semibold", 8F, FontStyle.Bold),
            Location = new Point(0, 2)
        };
        header.Controls.Add(lab);

        _title.Text = "Select a weapon";
        _title.AutoSize = true;
        _title.Font = new Font("Segoe UI Semibold", 17F, FontStyle.Bold);
        _title.ForeColor = TextMain;
        _title.Location = new Point(0, 21);
        header.Controls.Add(_title);

        _subtitle.Text = "External cfg data — no rebuild required for tuning values.";
        _subtitle.AutoSize = true;
        _subtitle.ForeColor = TextMuted;
        _subtitle.Location = new Point(2, 55);
        header.Controls.Add(_subtitle);

        _sourceBadge.AutoSize = true;
        _sourceBadge.ForeColor = Cyan;
        _sourceBadge.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        _sourceBadge.Location = new Point(690, 25);
        header.Controls.Add(_sourceBadge);

        host.Controls.Add(header);

        var tabs = new TabControl
        {
            Dock = DockStyle.Fill,
            Padding = new Point(14, 6)
        };
        tabs.TabPages.Add(BuildGameplayTab());
        tabs.TabPages.Add(BuildPresentationTab());
        tabs.TabPages.Add(BuildAssetsTab());
        tabs.TabPages.Add(BuildCatalogTab());

        host.Controls.Add(tabs);
        tabs.BringToFront();

        _status.Dock = DockStyle.Bottom;
        _status.Height = 30;
        _status.TextAlign = ContentAlignment.MiddleLeft;
        _status.ForeColor = TextMuted;
        host.Controls.Add(_status);

        return host;
    }

    private TabPage BuildGameplayTab()
    {
        var tab = NewTab("Gameplay");
        var flow = NewFlow();

        var identity = Card("IDENTITY / AVAILABILITY");
        var identityGrid = Grid();
        AddText(identityGrid, "Display name", "name", 0);
        AddText(identityGrid, "ID", "id", 1);
        AddText(identityGrid, "Type", "type", 2);

        _enabled.Text = "Enabled in build catalog";
        _enabled.AutoSize = true;
        _enabled.ForeColor = TextMain;
        _enabled.Margin = new Padding(3, 8, 3, 8);
        identityGrid.Controls.Add(Label("Catalog state"), 0, 3);
        identityGrid.Controls.Add(_enabled, 1, 3);

        _support.AutoSize = true;
        _support.ForeColor = TextMuted;
        identityGrid.Controls.Add(Label("Import support"), 0, 4);
        identityGrid.Controls.Add(_support, 1, 4);

        identity.Controls.Add(identityGrid);
        flow.Controls.Add(identity);

        var ammo = Card("BALLISTICS / AMMO");
        var ammoGrid = Grid();
        AddText(ammoGrid, "Magazine", "clip", 0);
        AddText(ammoGrid, "Reserve", "reserve", 1);
        AddText(ammoGrid, "Damage", "damage", 2);
        AddText(ammoGrid, "Fire interval", "fire_interval", 3);
        AddText(ammoGrid, "Range", "range", 4);
        AddText(ammoGrid, "Reload seconds", "reload", 5);
        AddCheck(ammoGrid, "Automatic", "automatic", 6);
        AddCheck(ammoGrid, "Auto reload", "auto_reload", 7);
        ammo.Controls.Add(ammoGrid);
        flow.Controls.Add(ammo);

        var falloff = Card("RANGE FALLOFF");
        var fallGrid = Grid();
        AddText(fallGrid, "Effect range 0", "effect_range0", 0);
        AddText(fallGrid, "Effect range 1", "effect_range1", 1);
        AddText(fallGrid, "Effect range 2", "effect_range2", 2);
        AddText(fallGrid, "Damage mult 0", "damage_mult0", 3);
        AddText(fallGrid, "Damage mult 1", "damage_mult1", 4);
        AddText(fallGrid, "Damage mult 2", "damage_mult2", 5);
        falloff.Controls.Add(fallGrid);
        flow.Controls.Add(falloff);

        var penetration = Card("VECTOR PENETRATION");
        var penGrid = Grid();
        AddText(penGrid, "Max thickness", "penetration_max_thickness", 0);
        AddText(penGrid, "Damage multiplier", "penetration_damage_mult", 1);
        AddText(penGrid, "Range multiplier", "penetration_range_mult", 2);
        var note = new Label
        {
            Text = "NOLF2-style reverse-trace shoot-through. 0 thickness disables penetration.",
            AutoSize = true,
            ForeColor = TextMuted,
            Margin = new Padding(3, 6, 3, 6)
        };
        penGrid.Controls.Add(note, 1, 3);
        penetration.Controls.Add(penGrid);
        flow.Controls.Add(penetration);

        tab.Controls.Add(flow);
        return tab;
    }

    private TabPage BuildPresentationTab()
    {
        var tab = NewTab("View / HUD");
        var flow = NewFlow();

        var view = Card("FIRST-PERSON PLACEMENT");
        var viewGrid = Grid();
        AddText(viewGrid, "View X", "view_x", 0);
        AddText(viewGrid, "View Y", "view_y", 1);
        AddText(viewGrid, "View Z", "view_z", 2);
        view.Controls.Add(viewGrid);
        flow.Controls.Add(view);

        var scope = Card("SCOPE / CROSSHAIR");
        var scopeGrid = Grid();
        AddCheck(scopeGrid, "Show crosshair", "show_crosshair", 0);
        AddText(scopeGrid, "Zoom FOV", "zoom_fov", 1);
        AddCheck(scopeGrid, "Hide weapon scoped", "zoom_hide_weapon", 2);
        AddText(scopeGrid, "Base gap", "crosshair_base_gap", 3);
        AddText(scopeGrid, "Shot kick", "crosshair_shot_kick", 4);
        AddText(scopeGrid, "Move kick", "crosshair_move_kick", 5);
        AddText(scopeGrid, "Max gap", "crosshair_max_gap", 6);
        AddText(scopeGrid, "Recover", "crosshair_recover", 7);
        scope.Controls.Add(scopeGrid);
        flow.Controls.Add(scope);

        tab.Controls.Add(flow);
        return tab;
    }

    private TabPage BuildAssetsTab()
    {
        var tab = NewTab("Assets");
        var flow = NewFlow();

        var pv = Card("PLAYER VIEW");
        var pvGrid = Grid();
        AddText(pvGrid, "PV model", "pv_model", 0);
        AddText(pvGrid, "PV animation", "pv_anim", 1);
        AddText(pvGrid, "PV texture", "pv_texture", 2);
        pv.Controls.Add(pvGrid);
        flow.Controls.Add(pv);

        var world = Card("WORLD / HELD MODEL");
        var worldGrid = Grid();
        AddText(worldGrid, "HH model", "hh_model", 0);
        AddText(worldGrid, "HH texture", "hh_texture", 1);
        AddText(worldGrid, "Sound directory", "sound_dir", 2);
        world.Controls.Add(worldGrid);
        flow.Controls.Add(world);

        var ca = Card("COMBAT ARMS SOURCE");
        var caGrid = Grid();
        AddReadOnly(caGrid, "CA name", "ca_name", 0);
        AddReadOnly(caGrid, "CA PV model", "ca_pv_model", 1);
        AddReadOnly(caGrid, "CA PV skin", "ca_pv_skin", 2);
        AddReadOnly(caGrid, "CA HH model", "ca_hh_model", 3);
        AddReadOnly(caGrid, "CA HH skin", "ca_hh_skin", 4);
        ca.Controls.Add(caGrid);
        flow.Controls.Add(ca);

        tab.Controls.Add(flow);
        return tab;
    }

    private TabPage BuildCatalogTab()
    {
        var tab = NewTab("Catalog / Loadout");
        var flow = NewFlow();

        var active = Card("ACTIVE FIVE-SLOT LOADOUT");
        var grid = Grid();

        _assignSlot.DropDownStyle = ComboBoxStyle.DropDownList;
        _assignSlot.Items.AddRange(
            ["1 — Primary", "2 — Sidearm", "3 — Melee", "4 — Secondary", "5 — Special"]);
        _assignSlot.SelectedIndex = 0;
        _assignSlot.BackColor = Panel2;
        _assignSlot.ForeColor = TextMain;
        grid.Controls.Add(Label("Target slot"), 0, 0);
        grid.Controls.Add(_assignSlot, 1, 0);

        var apply = Button("COPY SELECTED WEAPON TO SLOT", (_, _) => ApplyToSlot(), true);
        apply.AutoSize = true;
        grid.Controls.Add(apply, 1, 1);

        var note = new Label
        {
            Text =
                "The game runtime still consumes weapon1..weapon5. The catalog can hold hundreds of imported weapons; copy any supported entry into an active slot without changing C++.",
            AutoSize = true,
            MaximumSize = new Size(650, 0),
            ForeColor = TextMuted,
            Margin = new Padding(3, 10, 3, 8)
        };
        grid.Controls.Add(note, 1, 2);
        active.Controls.Add(grid);
        flow.Controls.Add(active);

        var importer = Card("BULK CONTENT IMPORT");
        var importFlow = new FlowLayoutPanel
        {
            Dock = DockStyle.Top,
            AutoSize = true,
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            Padding = new Padding(12)
        };
        importFlow.Controls.Add(new Label
        {
            Text =
                "AUTO IMPORT CA CATALOG reads the decrypted attributes, matches Guns.zip / GunsHH.zip assets, writes catalog.* sections, and extracts matched commercial assets only into ignored assets-local/WeaponImports.",
            AutoSize = true,
            MaximumSize = new Size(700, 0),
            ForeColor = TextMuted,
            Margin = new Padding(0, 0, 0, 12)
        });
        importFlow.Controls.Add(Button("AUTO IMPORT CA CATALOG", (_, _) => AutoImportCatalog(), true));
        importer.Controls.Add(importFlow);
        flow.Controls.Add(importer);

        tab.Controls.Add(flow);
        return tab;
    }

    private void LoadConfig()
    {
        try
        {
            (_sourcePath, _runtimePath) = FindConfigPaths();
            var path = _sourcePath ?? _runtimePath;

            if(path is null || !File.Exists(path))
                throw new FileNotFoundException("Could not locate config\\weapons.cfg.");

            _doc = FireteamConfigDocument.Load(path);
            _selectedSection = null;
            RefreshList();
            _status.Text = $"Editing {path}";
        }
        catch(Exception ex)
        {
            Error(ex.Message);
        }
    }

    private void RefreshList()
    {
        if(_doc is null)
            return;

        var selected = _selectedSection;
        var filter = Normalize(_search.Text);

        _suppressChecks = true;
        _weaponList.BeginUpdate();
        _weaponList.Items.Clear();

        var visible = 0;
        foreach(var section in _doc.Sections)
        {
            var isActive = Regex.IsMatch(section, @"^weapon[1-5]$", RegexOptions.IgnoreCase);
            var isCatalog = section.StartsWith("catalog.", StringComparison.OrdinalIgnoreCase);
            if(!isActive && !isCatalog)
                continue;

            var name = _doc.GetValue(section, "name", section);
            var id = _doc.GetValue(section, "id");
            if(filter.Length > 0 &&
               !Normalize(name).Contains(filter) &&
               !Normalize(id).Contains(filter))
            {
                continue;
            }

            var prefix = isActive
                ? $"SLOT {section[^1]}  "
                : string.Empty;

            var item = new ListViewItem(prefix + name)
            {
                Tag = section,
                Checked = isActive || _doc.GetBool(section, "enabled")
            };
            item.SubItems.Add(_doc.GetValue(section, "type", "?"));

            if(isActive)
            {
                item.ForeColor = Cyan;
            }
            else if(_doc.GetBool(section, "supported"))
            {
                item.ForeColor = TextMain;
            }
            else
            {
                item.ForeColor = TextMuted;
            }

            _weaponList.Items.Add(item);
            ++visible;

            if(string.Equals(section, selected, StringComparison.OrdinalIgnoreCase))
                item.Selected = true;
        }

        _weaponList.EndUpdate();
        _suppressChecks = false;
        _count.Text = $"{visible} visible weapon definitions";

        if(_weaponList.SelectedItems.Count == 0 &&
           _weaponList.Items.Count > 0)
        {
            _weaponList.Items[0].Selected = true;
        }
    }

    private void LoadSelection()
    {
        if(_doc is null || _weaponList.SelectedItems.Count == 0)
            return;

        SaveFieldsToDocument();

        var item = _weaponList.SelectedItems[0];
        _selectedSection = (string)item.Tag!;

        _title.Text = _doc.GetValue(_selectedSection, "name", _selectedSection);
        var id = _doc.GetValue(_selectedSection, "id");
        _subtitle.Text = $"{_selectedSection}  •  {id}";

        var active = Regex.IsMatch(_selectedSection, @"^weapon[1-5]$", RegexOptions.IgnoreCase);
        var source = _doc.GetValue(_selectedSection, "source", active ? "ACTIVE LOADOUT" : "LOCAL / CUSTOM");
        _sourceBadge.Text = source.ToUpperInvariant();

        foreach(var (key, box) in _text)
            box.Text = _doc.GetValue(_selectedSection, key);

        foreach(var (key, check) in _checks)
            check.Checked = _doc.GetBool(_selectedSection, key);

        _enabled.Enabled = !active;
        _enabled.Checked = active || _doc.GetBool(_selectedSection, "enabled");

        var supported = active || _doc.GetBool(_selectedSection, "supported", true);
        _support.Text = supported ? "READY / SUPPORTED" : "REVIEW / UNSUPPORTED";
        _support.ForeColor = supported ? Green : Red;

        _status.Text = active
            ? "Active runtime slot. Save changes to tune immediately."
            : "Catalog definition. Enable it to stage imported assets during build.";
    }

    private void WeaponListItemChecked(object? sender, ItemCheckedEventArgs e)
    {
        if(_suppressChecks || _doc is null || e.Item.Tag is not string section)
            return;

        if(Regex.IsMatch(section, @"^weapon[1-5]$", RegexOptions.IgnoreCase))
        {
            e.Item.Checked = true;
            return;
        }

        _doc.SetValue(section, "enabled", e.Item.Checked ? "1" : "0");

        if(string.Equals(section, _selectedSection, StringComparison.OrdinalIgnoreCase))
            _enabled.Checked = e.Item.Checked;
    }

    private void SaveCurrent()
    {
        try
        {
            SaveFieldsToDocument();
            if(_doc is null)
                return;

            if(_sourcePath is not null)
                _doc.Save(_sourcePath);

            if(_runtimePath is not null)
                _doc.Save(_runtimePath);

            _status.Text = "Saved source + current BUILT weapon config.";
            RefreshList();
        }
        catch(Exception ex)
        {
            Error(ex.Message);
        }
    }

    private void SaveFieldsToDocument()
    {
        if(_doc is null || string.IsNullOrWhiteSpace(_selectedSection))
            return;

        foreach(var (key, box) in _text)
        {
            if(box.ReadOnly)
                continue;
            _doc.SetValue(_selectedSection, key, box.Text.Trim());
        }

        foreach(var (key, check) in _checks)
            _doc.SetValue(_selectedSection, key, check.Checked ? "1" : "0");

        if(_selectedSection.StartsWith("catalog.", StringComparison.OrdinalIgnoreCase))
            _doc.SetValue(_selectedSection, "enabled", _enabled.Checked ? "1" : "0");
    }

    private void ApplyToSlot()
    {
        if(_doc is null || string.IsNullOrWhiteSpace(_selectedSection))
            return;

        SaveFieldsToDocument();

        var slot = _assignSlot.SelectedIndex + 1;
        var destination = $"weapon{slot}";

        _doc.CopySection(_selectedSection, destination, RuntimeKeys);

        _status.Text = $"Copied {_doc.GetValue(_selectedSection, "name")} to active slot {slot}. Press SAVE to commit.";
        _selectedSection = destination;
        RefreshList();
    }

    private void OpenConfig()
    {
        var path = _sourcePath ?? _runtimePath;
        if(path is null || !File.Exists(path))
            return;

        Process.Start(new ProcessStartInfo(path) { UseShellExecute = true });
    }

    private void AutoImportCatalog()
    {
        if(_doc is null)
            return;

        try
        {
            SaveFieldsToDocument();

            var repoRoot = FindRepositoryRoot()
                ?? throw new InvalidOperationException("Could not locate the FIRETEAM repository root.");

            var assetRoot = Path.Combine(repoRoot, "assets-local");
            Directory.CreateDirectory(assetRoot);

            var attributes = FindFirst(assetRoot,
                "*Decrypted*Attribute*.rar",
                "*Attribute*.rar",
                "WEAPONS-DEC.TXT");

            var guns = Directory.GetFiles(assetRoot, "Guns*.zip", SearchOption.TopDirectoryOnly)
                .FirstOrDefault(path => !Path.GetFileName(path).Contains("HH", StringComparison.OrdinalIgnoreCase));

            var gunsHh = Directory.GetFiles(assetRoot, "GunsHH*.zip", SearchOption.TopDirectoryOnly)
                .FirstOrDefault();

            attributes ??= PickFile(
                "Select decrypted Combat Arms attributes",
                "Attributes (*.rar;*.txt)|*.rar;*.txt|All files (*.*)|*.*");

            guns ??= PickFile(
                "Select Combat Arms Guns.zip",
                "ZIP archives (*.zip)|*.zip|All files (*.*)|*.*");

            if(attributes is null || guns is null)
                return;

            if(gunsHh is null)
            {
                using var ask = new OpenFileDialog
                {
                    Title = "Optional: select GunsHH.zip (Cancel to skip)",
                    Filter = "ZIP archives (*.zip)|*.zip|All files (*.*)|*.*"
                };
                if(ask.ShowDialog(this) == DialogResult.OK)
                    gunsHh = ask.FileName;
            }

            var attributeFiles = ResolveAttributeFiles(attributes).ToList();
            var weaponsText = attributeFiles
                .Where(path => Path.GetFileName(path).Contains("WEAPONS", StringComparison.OrdinalIgnoreCase))
                .Select(File.ReadAllText)
                .FirstOrDefault()
                ?? attributeFiles.Select(File.ReadAllText).FirstOrDefault()
                ?? string.Empty;

            var itemsText = attributeFiles
                .Where(path => Path.GetFileName(path).Contains("ITEMS", StringComparison.OrdinalIgnoreCase))
                .Select(File.ReadAllText)
                .FirstOrDefault()
                ?? string.Empty;

            if(weaponsText.Length == 0)
                throw new InvalidOperationException("WEAPONS-DEC data was not found in the selected attributes.");

            using var gunsZip = ZipFile.OpenRead(guns);
            using var hhZip = gunsHh is not null && File.Exists(gunsHh)
                ? ZipFile.OpenRead(gunsHh)
                : null;

            var importer = new CatalogImporter(
                _doc,
                repoRoot,
                gunsZip,
                hhZip,
                itemsText);

            var result = importer.Import(weaponsText);

            _status.Text =
                $"CA catalog import: {result.Imported} definitions, {result.Supported} supported, {result.Assets} asset files matched.";

            SaveCurrent();
            RefreshList();

            MessageBox.Show(
                $"Combat Arms catalog import complete.\n\n" +
                $"Definitions: {result.Imported}\n" +
                $"Auto-supported/enabled: {result.Supported}\n" +
                $"Matched asset files: {result.Assets}\n\n" +
                "Unsupported/problem content stays disabled. The build only stages enabled catalog imports.",
                "FIRETEAM Weapon Studio",
                MessageBoxButtons.OK,
                MessageBoxIcon.Information);
        }
        catch(Exception ex)
        {
            Error(ex.Message);
        }
    }

    private static string? FindFirst(string root, params string[] patterns)
    {
        foreach(var pattern in patterns)
        {
            var found = Directory.GetFiles(root, pattern, SearchOption.AllDirectories)
                .OrderByDescending(File.GetLastWriteTimeUtc)
                .FirstOrDefault();
            if(found is not null)
                return found;
        }
        return null;
    }

    private string? PickFile(string title, string filter)
    {
        using var dialog = new OpenFileDialog
        {
            Title = title,
            Filter = filter
        };
        return dialog.ShowDialog(this) == DialogResult.OK
            ? dialog.FileName
            : null;
    }

    private static IEnumerable<string> ResolveAttributeFiles(string selected)
    {
        if(!selected.EndsWith(".rar", StringComparison.OrdinalIgnoreCase))
            return [selected];

        var sevenZip = FindSevenZip()
            ?? throw new InvalidOperationException(
                "RAR import needs 7-Zip. Install 7-Zip or extract WEAPONS-DEC.TXT / ITEMS-DEC.TXT manually.");

        var temp = Path.Combine(
            Path.GetTempPath(),
            "FireteamCatalogImport",
            Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(temp);

        var start = new ProcessStartInfo(sevenZip)
        {
            UseShellExecute = false,
            CreateNoWindow = true
        };
        start.ArgumentList.Add("x");
        start.ArgumentList.Add("-y");
        start.ArgumentList.Add($"-o{temp}");
        start.ArgumentList.Add(selected);

        using var process = Process.Start(start)
            ?? throw new InvalidOperationException("Could not start 7-Zip.");
        process.WaitForExit();

        if(process.ExitCode != 0)
            throw new InvalidOperationException("7-Zip could not extract the attribute archive.");

        return Directory.GetFiles(temp, "*-DEC.TXT", SearchOption.AllDirectories);
    }

    private static string? FindSevenZip()
    {
        var candidates = new[]
        {
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "7-Zip", "7z.exe"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86), "7-Zip", "7z.exe"),
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "7-Zip", "7zz.exe")
        };
        return candidates.FirstOrDefault(File.Exists);
    }

    private static (string? Source, string? Runtime) FindConfigPaths()
    {
        string? source = null;
        string? runtime = null;

        var current = new DirectoryInfo(AppContext.BaseDirectory);
        for(var i = 0; current is not null && i < 8; ++i, current = current.Parent)
        {
            var src = Path.Combine(current.FullName, "config", "weapons.cfg");
            if(source is null && File.Exists(src) &&
               File.Exists(Path.Combine(current.FullName, "build.cmd")))
                source = src;

            var built = Path.Combine(current.FullName, "BUILT", "config", "weapons.cfg");
            if(runtime is null && File.Exists(built))
                runtime = built;

            if(string.Equals(current.Name, "BUILT", StringComparison.OrdinalIgnoreCase))
            {
                var direct = Path.Combine(current.FullName, "config", "weapons.cfg");
                if(File.Exists(direct))
                    runtime = direct;
            }
        }

        return (source, runtime);
    }

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

    private static string Normalize(string value) =>
        new(value.Where(char.IsLetterOrDigit).Select(char.ToLowerInvariant).ToArray());

    private static TabPage NewTab(string title) =>
        new(title)
        {
            BackColor = Bg,
            ForeColor = TextMain,
            Padding = new Padding(8)
        };

    private static FlowLayoutPanel NewFlow() =>
        new()
        {
            Dock = DockStyle.Fill,
            AutoScroll = true,
            FlowDirection = FlowDirection.TopDown,
            WrapContents = false,
            Padding = new Padding(3)
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

        var title = new Label
        {
            Text = heading,
            Dock = DockStyle.Top,
            Height = 28,
            Font = new Font("Segoe UI Semibold", 8F, FontStyle.Bold),
            ForeColor = Green
        };

        card.Controls.Add(title);
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
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 190));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        return grid;
    }

    private void AddText(TableLayoutPanel grid, string label, string key, int row)
    {
        var box = new TextBox
        {
            Dock = DockStyle.Fill,
            BackColor = Panel2,
            ForeColor = TextMain,
            BorderStyle = BorderStyle.FixedSingle,
            Margin = new Padding(3, 4, 3, 4)
        };
        _text[key] = box;
        grid.Controls.Add(Label(label), 0, row);
        grid.Controls.Add(box, 1, row);
    }

    private void AddReadOnly(TableLayoutPanel grid, string label, string key, int row)
    {
        var box = new TextBox
        {
            Dock = DockStyle.Fill,
            ReadOnly = true,
            BackColor = Color.FromArgb(23, 25, 27),
            ForeColor = TextMuted,
            BorderStyle = BorderStyle.FixedSingle,
            Margin = new Padding(3, 4, 3, 4)
        };
        _text[key] = box;
        grid.Controls.Add(Label(label), 0, row);
        grid.Controls.Add(box, 1, row);
    }

    private void AddCheck(TableLayoutPanel grid, string label, string key, int row)
    {
        var check = new CheckBox
        {
            AutoSize = true,
            ForeColor = TextMain,
            Margin = new Padding(3, 7, 3, 7)
        };
        _checks[key] = check;
        grid.Controls.Add(Label(label), 0, row);
        grid.Controls.Add(check, 1, row);
    }

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

    private void Error(string message)
    {
        _status.Text = message;
        MessageBox.Show(
            message,
            "FIRETEAM Weapon Studio",
            MessageBoxButtons.OK,
            MessageBoxIcon.Error);
    }

    private sealed class CatalogImporter
    {
        private readonly FireteamConfigDocument _doc;
        private readonly string _repoRoot;
        private readonly ZipArchive _guns;
        private readonly ZipArchive? _hh;
        private readonly string _itemsText;
        private int _assets;

        public CatalogImporter(
            FireteamConfigDocument doc,
            string repoRoot,
            ZipArchive guns,
            ZipArchive? hh,
            string itemsText)
        {
            _doc = doc;
            _repoRoot = repoRoot;
            _guns = guns;
            _hh = hh;
            _itemsText = itemsText;
        }

        public ImportResult Import(string weaponsText)
        {
            var imported = 0;
            var supported = 0;

            foreach(Match match in Regex.Matches(
                weaponsText,
                @"(?ms)^\[Weapon\d+\]\s*(.*?)(?=^\[|\z)"))
            {
                var block = match.Groups[1].Value;
                var name = Read(block, "Name");
                if(string.IsNullOrWhiteSpace(name))
                    continue;

                var id = Slug(name);
                if(id.Length == 0)
                    continue;

                var section = $"catalog.{id}";
                var duplicate = 2;
                while(_doc.HasSection(section) &&
                      !string.Equals(
                          _doc.GetValue(section, "ca_name"),
                          name,
                          StringComparison.OrdinalIgnoreCase))
                {
                    section = $"catalog.{id}_{duplicate++}";
                }

                _doc.EnsureSection(section);
                _doc.SetValue(section, "source", "Combat Arms decrypted attributes");
                _doc.SetValue(section, "ca_name", name);
                _doc.SetValue(section, "name", name);
                _doc.SetValue(section, "id", section["catalog.".Length..]);

                var type = InferType(name, block);
                _doc.SetValue(section, "type", type);

                Copy(block, "ShotsPerClip", section, "clip");
                Copy(block, "Range", section, "range");
                Copy(block, "Effectrange0", section, "effect_range0");
                Copy(block, "Effectrange1", section, "effect_range1");
                Copy(block, "Effectrange2", section, "effect_range2");

                var clip = ParseInt(_doc.GetValue(section, "clip"));
                if(clip > 0 && string.IsNullOrWhiteSpace(_doc.GetValue(section, "reserve")))
                    _doc.SetValue(section, "reserve", (clip * 3).ToString());

                _doc.SetValue(section, "damage",
                    FindItemValue(name, "Damage", "0"));
                _doc.SetValue(section, "fire_interval",
                    _doc.GetValue(section, "fire_interval", "0.15"));
                _doc.SetValue(section, "reload",
                    _doc.GetValue(section, "reload", "2.00"));

                _doc.SetValue(section, "damage_mult0", "1.0");
                _doc.SetValue(section, "damage_mult1", "0.70");
                _doc.SetValue(section, "damage_mult2", "0.35");
                _doc.SetValue(section, "auto_reload", type == "melee" ? "0" : "1");

                var fireType = Read(block, "FireType");
                if(fireType.Length > 0)
                {
                    _doc.SetValue(
                        section,
                        "automatic",
                        fireType.Contains("auto", StringComparison.OrdinalIgnoreCase)
                            ? "1"
                            : "0");
                }

                var disableCrosshair = ParseInt(Read(block, "DisableCrosshair"));
                _doc.SetValue(section, "show_crosshair", disableCrosshair != 0 ? "0" : "1");

                var pos = ParseVector(Read(block, "Pos"));
                if(pos is not null)
                {
                    _doc.SetValue(section, "view_x", pos.Value.X);
                    _doc.SetValue(section, "view_y", pos.Value.Y);
                    _doc.SetValue(section, "view_z", pos.Value.Z);
                }

                var caPv = Leaf(Read(block, "PVModelNormal"));
                if(caPv.Length == 0)
                    caPv = Leaf(Read(block, "PVModel"));

                var caSkin = Leaf(Read(block, "PVSkin1"));
                if(caSkin.Length == 0)
                    caSkin = Leaf(Read(block, "PVSkin0"));

                var caHh = Leaf(Read(block, "HHModel"));
                var caHhSkin = Leaf(Read(block, "HHSkin0"));
                if(caHhSkin.Length == 0)
                    caHhSkin = Leaf(Read(block, "HHSkin1"));

                _doc.SetValue(section, "ca_pv_model", caPv);
                _doc.SetValue(section, "ca_pv_skin", caSkin);
                _doc.SetValue(section, "ca_hh_model", caHh);
                _doc.SetValue(section, "ca_hh_skin", caHhSkin);

                var asset = ImportAssets(section, name, caPv, caSkin, caHh, caHhSkin);

                var excluded = IsExplicitlyUnsupported(name, block);
                var isSupported =
                    !excluded &&
                    (type == "hitscan" || type == "melee") &&
                    asset.PvModel.Length > 0;

                _doc.SetValue(section, "supported", isSupported ? "1" : "0");
                _doc.SetValue(section, "enabled", isSupported ? "1" : "0");

                if(isSupported)
                    ++supported;

                ++imported;
            }

            return new ImportResult(imported, supported, _assets);
        }

        private ImportedAssetSet ImportAssets(
            string section,
            string name,
            string caPv,
            string caSkin,
            string caHh,
            string caHhSkin)
        {
            var id = _doc.GetValue(section, "id");
            var tokens = Tokens(name, id, caPv, caHh);

            var pvModel = FindExactOrToken(
                _guns,
                caPv,
                tokens,
                "GUNS_M_PV",
                ".LTB",
                entry =>
                    !entry.Name.Contains("ANIBASE", StringComparison.OrdinalIgnoreCase) &&
                    !entry.Name.StartsWith("ANI_", StringComparison.OrdinalIgnoreCase) &&
                    !entry.Name.Contains("I_INFO", StringComparison.OrdinalIgnoreCase));

            var pvAnim = FindToken(
                _guns,
                tokens,
                "GUNS_M_PV",
                ".LTB",
                entry =>
                    (entry.Name.Contains("ANIBASE", StringComparison.OrdinalIgnoreCase) ||
                     entry.Name.StartsWith("ANI_", StringComparison.OrdinalIgnoreCase)) &&
                    !entry.Name.Contains("I_INFO", StringComparison.OrdinalIgnoreCase));

            var pvSkin = FindExactOrToken(
                _guns,
                caSkin,
                tokens,
                "GUNS_T_PV",
                ".DTX",
                _ => true);

            var hhModel = _hh is null
                ? null
                : FindExactOrToken(
                    _hh,
                    caHh,
                    tokens,
                    "GUNS_M_HH",
                    ".LTB",
                    _ => true);

            var hhSkin = _hh is null
                ? null
                : FindExactOrToken(
                    _hh,
                    caHhSkin,
                    tokens,
                    "GUNS_T_HH",
                    ".DTX",
                    _ => true);

            var soundEntries = _guns.Entries
                .Where(entry =>
                    entry.Name.EndsWith(".WAV", StringComparison.OrdinalIgnoreCase) &&
                    entry.FullName.Contains("GUNS_SND", StringComparison.OrdinalIgnoreCase) &&
                    tokens.Any(token => Normalize(entry.FullName).Contains(token)))
                .Take(32)
                .ToList();

            var imported = new ImportedAssetSet();

            if(pvModel is not null)
                imported.PvModel = Extract(pvModel, id, "pv");
            if(pvAnim is not null)
                imported.PvAnim = Extract(pvAnim, id, "pv");
            if(pvSkin is not null)
                imported.PvTexture = Extract(pvSkin, id, "pv");
            if(hhModel is not null)
                imported.HhModel = Extract(hhModel, id, "hh");
            if(hhSkin is not null)
                imported.HhTexture = Extract(hhSkin, id, "hh");

            if(soundEntries.Count > 0)
            {
                foreach(var entry in soundEntries)
                    Extract(entry, id, "snd");
                imported.SoundDir = $"Weapons/imported/{id}/snd";
            }

            if(imported.PvModel.Length > 0)
                _doc.SetValue(section, "pv_model", imported.PvModel);
            if(imported.PvAnim.Length > 0)
                _doc.SetValue(section, "pv_anim", imported.PvAnim);
            if(imported.PvTexture.Length > 0)
                _doc.SetValue(section, "pv_texture", imported.PvTexture);
            if(imported.HhModel.Length > 0)
                _doc.SetValue(section, "hh_model", imported.HhModel);
            if(imported.HhTexture.Length > 0)
                _doc.SetValue(section, "hh_texture", imported.HhTexture);
            if(imported.SoundDir.Length > 0)
                _doc.SetValue(section, "sound_dir", imported.SoundDir);

            return imported;
        }

        private string Extract(ZipArchiveEntry entry, string id, string bucket)
        {
            var relative = $"Weapons/imported/{id}/{bucket}/{entry.Name}";
            var path = Path.Combine(
                _repoRoot,
                "assets-local",
                "WeaponImports",
                relative.Replace('/', Path.DirectorySeparatorChar));

            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            entry.ExtractToFile(path, true);
            ++_assets;
            return relative;
        }

        private ZipArchiveEntry? FindExactOrToken(
            ZipArchive archive,
            string exactLeaf,
            IReadOnlyList<string> tokens,
            string pathMarker,
            string extension,
            Func<ZipArchiveEntry, bool> predicate)
        {
            if(exactLeaf.Length > 0)
            {
                var exact = archive.Entries.FirstOrDefault(entry =>
                    entry.Name.Equals(exactLeaf, StringComparison.OrdinalIgnoreCase) &&
                    entry.FullName.Contains(pathMarker, StringComparison.OrdinalIgnoreCase) &&
                    predicate(entry));
                if(exact is not null)
                    return exact;
            }

            return FindToken(archive, tokens, pathMarker, extension, predicate);
        }

        private static ZipArchiveEntry? FindToken(
            ZipArchive archive,
            IReadOnlyList<string> tokens,
            string pathMarker,
            string extension,
            Func<ZipArchiveEntry, bool> predicate) =>
            archive.Entries
                .Where(entry =>
                    entry.Name.EndsWith(extension, StringComparison.OrdinalIgnoreCase) &&
                    entry.FullName.Contains(pathMarker, StringComparison.OrdinalIgnoreCase) &&
                    predicate(entry) &&
                    tokens.Any(token => Normalize(entry.Name).Contains(token)))
                .OrderByDescending(entry =>
                    entry.Name.StartsWith("PVMLA_", StringComparison.OrdinalIgnoreCase) ? 3 :
                    entry.Name.StartsWith("CM_HND_", StringComparison.OrdinalIgnoreCase) ? 2 : 1)
                .ThenBy(entry => entry.FullName)
                .FirstOrDefault();

        private void Copy(string block, string caKey, string section, string ftKey)
        {
            var value = Read(block, caKey);
            if(value.Length > 0)
                _doc.SetValue(section, ftKey, value);
        }

        private string FindItemValue(string name, string key, string fallback)
        {
            if(_itemsText.Length == 0)
                return fallback;

            var target = Normalize(name);
            foreach(Match item in Regex.Matches(
                _itemsText,
                @"(?ms)^\[WeaponItem\d+\]\s*(.*?)(?=^\[|\z)"))
            {
                var block = item.Groups[1].Value;
                if(!Normalize(block).Contains(target))
                    continue;

                var value = Read(block, key);
                if(value.Length > 0)
                    return value;
            }
            return fallback;
        }

        private static string InferType(string name, string block)
        {
            var n = Normalize(name + " " + block);
            var melee = new[]
            {
                "knife", "bowie", "machete", "katana", "sickle",
                "axe", "bat", "crowbar", "hammer", "kukri", "melee"
            };
            return melee.Any(n.Contains) ? "melee" : "hitscan";
        }

        private static bool IsExplicitlyUnsupported(string name, string block)
        {
            var n = Normalize(name + " " + block);
            var unsupported = new[]
            {
                "grenade", "frag", "flashbang", "smoke", "mine", "claymore",
                "rocket", "rpg", "law", "m79", "snowball", "zombiehand",
                "infectedhand", "gasgrenade"
            };
            return unsupported.Any(n.Contains);
        }

        private static IReadOnlyList<string> Tokens(params string[] values)
        {
            var result = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var value in values)
            {
                var normalized = Normalize(value);
                if(normalized.Length >= 3)
                    result.Add(normalized);

                foreach(var part in Regex.Split(value, @"[^A-Za-z0-9]+"))
                {
                    var token = Normalize(part);
                    if(token.Length >= 4)
                        result.Add(token);
                }
            }
            return result.OrderByDescending(x => x.Length).ToList();
        }

        private static string Read(string block, string key)
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
            return (match.Groups[1].Value, match.Groups[2].Value, match.Groups[3].Value);
        }

        private static string Leaf(string value)
        {
            if(string.IsNullOrWhiteSpace(value))
                return string.Empty;
            return value.Replace('\\', '/').Split('/').Last();
        }

        private static int ParseInt(string value) =>
            int.TryParse(value, out var parsed) ? parsed : 0;

        private static string Slug(string value)
        {
            var normalized = Regex.Replace(value.ToLowerInvariant(), @"[^a-z0-9]+", "_").Trim('_');
            return normalized.Length > 48 ? normalized[..48] : normalized;
        }

        private sealed class ImportedAssetSet
        {
            public string PvModel { get; set; } = "";
            public string PvAnim { get; set; } = "";
            public string PvTexture { get; set; } = "";
            public string HhModel { get; set; } = "";
            public string HhTexture { get; set; } = "";
            public string SoundDir { get; set; } = "";
        }
    }

    private sealed record ImportResult(int Imported, int Supported, int Assets);
}
