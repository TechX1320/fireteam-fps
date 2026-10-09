using System.Net;
using System.Net.Sockets;
using System.Text;

namespace FireteamLauncher.Services;

// Passive UDP advertisements are intentionally LAN-only: no public master
// registration, UPnP, port forwarding or NAT traversal is implied.
public sealed record FireteamLanServer(
    string InstanceId,
    string Name,
    string Address,
    string Map,
    int Difficulty,
    int Players,
    int MaxPlayers,
    DateTime LastSeenUtc);

public sealed class LanServerDiscoveryService : IDisposable
{
    public const int ListenPort = 27888;
    private static readonly TimeSpan Expiry = TimeSpan.FromSeconds(9);
    private readonly object _lock = new();
    private readonly Dictionary<string, FireteamLanServer> _servers =
        new(StringComparer.Ordinal);
    private UdpClient? _socket;
    private CancellationTokenSource? _cancel;

    public event Action? Changed;
    public string? StartError { get; private set; }
    public bool IsListening => _socket is not null;

    public void Start()
    {
        if(_socket is not null)
            return;

        try
        {
            var udp = new UdpClient(AddressFamily.InterNetwork);
            udp.Client.SetSocketOption(SocketOptionLevel.Socket,
                SocketOptionName.ReuseAddress, true);
            udp.Client.Bind(new IPEndPoint(IPAddress.Any, ListenPort));
            _socket = udp;
            _cancel = new CancellationTokenSource();
            _ = ReceiveAsync(udp, _cancel.Token);
        }
        catch(Exception ex) when(ex is SocketException or
                                  UnauthorizedAccessException)
        {
            StartError = ex.Message;
            _socket?.Dispose();
            _socket = null;
            _cancel?.Dispose();
            _cancel = null;
        }
    }

    private async Task ReceiveAsync(UdpClient socket, CancellationToken cancel)
    {
        while(!cancel.IsCancellationRequested)
        {
            UdpReceiveResult received;
            try
            {
                received = await socket.ReceiveAsync(cancel);
            }
            catch(OperationCanceledException)
            {
                return;
            }
            catch(ObjectDisposedException)
            {
                return;
            }
            catch(SocketException)
            {
                if(cancel.IsCancellationRequested) return;
                continue;
            }

            if(received.Buffer.Length is < 24 or > 320 ||
               !TryParse(received.Buffer, out var item))
                continue;

            var from = received.RemoteEndPoint.Address;
            if(from.AddressFamily != AddressFamily.InterNetwork ||
               IPAddress.Any.Equals(from) || IPAddress.Broadcast.Equals(from))
                continue;

            var address = $"{from}:{item.Port}";
            lock(_lock)
            {
                if(_servers.TryGetValue(item.Id, out var earlier) &&
                   earlier.Address.StartsWith("127.", StringComparison.Ordinal) &&
                   !IPAddress.IsLoopback(from))
                {
                    // Do not replace a working localhost target with a LAN
                    // adapter address on the host itself.
                    address = earlier.Address;
                }

                _servers[item.Id] = new FireteamLanServer(
                    item.Id, item.Name, address, item.Map,
                    item.Difficulty, item.Players, item.MaxPlayers,
                    DateTime.UtcNow);
            }
            Changed?.Invoke();
        }
    }

    private static bool TryParse(byte[] data,
        out (string Id, string Name, string Map, int Port, int Difficulty,
             int Players, int MaxPlayers) item)
    {
        item = default;
        var text = Encoding.UTF8.GetString(data);
        var fields = text.Split('|', 8);
        if(fields.Length != 8 || fields[0] != "FTLAN1" ||
           fields[1].Length != 16 ||
           !fields[1].All(Uri.IsHexDigit) ||
           !int.TryParse(fields[2], out var port) || port is < 1 or > 65535 ||
           !int.TryParse(fields[3], out var max) || max is < 1 or > 24 ||
           !int.TryParse(fields[4], out var players) ||
           players < 0 || players > max ||
           !int.TryParse(fields[5], out var difficulty) ||
           difficulty is < 1 or > 10 ||
           fields[6].Length is < 1 or > 64 ||
           !fields[6].All(ch => char.IsAsciiLetterOrDigit(ch) || ch is '_' or '-') ||
           fields[7].Length is < 1 or > 64 ||
           fields[7].Any(char.IsControl))
            return false;

        item = (fields[1], fields[7].Trim(), fields[6],
                port, difficulty, players, max);
        return item.Name.Length > 0;
    }

    public IReadOnlyList<FireteamLanServer> Snapshot()
    {
        lock(_lock)
        {
            var now = DateTime.UtcNow;
            foreach(var id in _servers.Where(item =>
                         now - item.Value.LastSeenUtc > Expiry)
                     .Select(item => item.Key).ToArray())
                _servers.Remove(id);
            return _servers.Values.OrderBy(item => item.Name).ToArray();
        }
    }

    public void Dispose()
    {
        _cancel?.Cancel();
        _socket?.Dispose();
        _cancel?.Dispose();
        _cancel = null;
        _socket = null;
    }
}
