namespace FireteamLauncher.Infrastructure;

public sealed class FireteamConfigDocument
{
    private readonly List<string> _lines;

    private FireteamConfigDocument(IEnumerable<string> lines)
    {
        _lines = lines.ToList();
    }

    public static FireteamConfigDocument Load(string path) =>
        new(File.Exists(path) ? File.ReadAllLines(path) : []);

    public IReadOnlyList<string> Sections =>
        _lines
            .Select(line => line.Trim())
            .Where(line =>
                line.Length >= 3 &&
                line[0] == '[' &&
                line[^1] == ']')
            .Select(line =>
                line[1..^1].Trim())
            .Where(section =>
                section.Length > 0)
            .ToList();

    public bool HasSection(string section) => FindSection(section).Start >= 0;

    public void EnsureSection(string section)
    {
        if(HasSection(section))
        {
            return;
        }

        if(_lines.Count > 0 && !string.IsNullOrWhiteSpace(_lines[^1]))
        {
            _lines.Add(string.Empty);
        }

        _lines.Add($"[{section}]");
    }

    public string GetValue(string section, string key, string fallback = "")
    {
        var (start, end) = FindSection(section);
        if(start < 0)
        {
            return fallback;
        }

        for(var i = start + 1; i < end; ++i)
        {
            var line = _lines[i].Trim();
            if(line.StartsWith('#') || line.StartsWith(';'))
            {
                continue;
            }

            var equals = line.IndexOf('=');
            if(equals < 0)
            {
                continue;
            }

            if(string.Equals(
                line[..equals].Trim(),
                key,
                StringComparison.OrdinalIgnoreCase))
            {
                return line[(equals + 1)..].Trim();
            }
        }

        return fallback;
    }

    public bool GetBool(string section, string key, bool fallback = false)
    {
        var value = GetValue(section, key);
        if(value.Length == 0)
        {
            return fallback;
        }

        return value == "1" ||
               value.Equals("true", StringComparison.OrdinalIgnoreCase) ||
               value.Equals("yes", StringComparison.OrdinalIgnoreCase) ||
               value.Equals("on", StringComparison.OrdinalIgnoreCase);
    }

    public void SetValue(string section, string key, string? value)
    {
        EnsureSection(section);
        var (start, end) = FindSection(section);

        for(var i = start + 1; i < end; ++i)
        {
            var line = _lines[i].Trim();
            var equals = line.IndexOf('=');
            if(equals < 0)
            {
                continue;
            }

            if(string.Equals(
                line[..equals].Trim(),
                key,
                StringComparison.OrdinalIgnoreCase))
            {
                _lines[i] = $"{key}={value ?? string.Empty}";
                return;
            }
        }

        _lines.Insert(end, $"{key}={value ?? string.Empty}");
    }

    public bool RemoveSection(string section)
    {
        var (start, end) = FindSection(section);
        if(start < 0)
        {
            return false;
        }

        _lines.RemoveRange(start, end - start);
        return true;
    }

    public void Save(string path)
    {
        var parent = Path.GetDirectoryName(path);
        if(!string.IsNullOrWhiteSpace(parent))
        {
            Directory.CreateDirectory(parent);
        }

        File.WriteAllLines(path, _lines);
    }

    private (int Start, int End) FindSection(string section)
    {
        var start = -1;

        for(var i = 0; i < _lines.Count; ++i)
        {
            var trimmed = _lines[i].Trim();
            if(!trimmed.StartsWith('[') || !trimmed.EndsWith(']'))
            {
                continue;
            }

            if(start >= 0)
            {
                return (start, i);
            }

            if(string.Equals(trimmed, $"[{section}]", StringComparison.OrdinalIgnoreCase))
            {
                start = i;
            }
        }

        return start >= 0 ? (start, _lines.Count) : (-1, -1);
    }
}
