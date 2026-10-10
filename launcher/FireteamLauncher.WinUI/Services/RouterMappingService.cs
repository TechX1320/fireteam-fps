using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;

namespace FireteamLauncher.Services;

// Opt-in UPnP IGD mapping. Windows and router API only; no player-installed
// tool. DO NOT use until the dedicated engine is actually listening.
// Never map LAN discovery UDP 27888 or fabricate external reachability.
public sealed class RouterMappingService
{
    private readonly List<(int Port, string Protocol, string Address)> _owned = [];
    private readonly object _gate = new();

    public async Task<string> TryMapGamePortAsync(
        int gamePort, int dedicatedProcessId,
        Func<bool>? hostStillRunning = null,
        CancellationToken cancellationToken = default)
    {
        if(gamePort is < 1 or > 65535 || dedicatedProcessId <= 0)
            return "Invalid dedicated process or game port. No router changes made.";

        var protocols = Array.Empty<string>();
        for(var attempt = 0; attempt < 5; ++attempt)
        {
            cancellationToken.ThrowIfCancellationRequested();
            if(hostStillRunning is not null && !hostStillRunning())
                return "Dedicated process stopped; no router changes made.";
            // Only ports owned by the specific dedicated host PID qualify.
            // A different application listening on that port is NEVER mapped.
            protocols = OwnedNetworkListeners.ProtocolsAtPort(
                gamePort, dedicatedProcessId);
            if(protocols.Length > 0) break;
            await Task.Delay(900, cancellationToken);
        }

        if(protocols.Length == 0)
            return "UPnP not attempted: the dedicated process is not listening " +
                   "on the configured game port using IPv4 UDP/TCP. " +
                   "Run scripts/diagnose-host-connectivity.ps1 while hosting.";

        var ipv4 = DiscoverLocalIpv4();
        if(ipv4 is null)
            return "UPnP not attempted: cannot determine the host's LAN IPv4 address.";

        return await Task.Run(() =>
        {
            var successes = new List<string>();
            var errors = new List<string>();
            foreach(var protocol in protocols)
            {
                if(hostStillRunning is not null && !hostStillRunning())
                    return "Dedicated process stopped; router mapping skipped.";
                try
                {
                    var owned = MapOne(gamePort, protocol, ipv4);
                    successes.Add($"{protocol}/{gamePort} " +
                        (owned ? "mapped" : "already mapped to this PC"));
                }
                catch(Exception ex) when(ex is COMException or
                    InvalidOperationException or NotSupportedException or
                    Microsoft.CSharp.RuntimeBinder.RuntimeBinderException)
                {
                    errors.Add($"{protocol}/{gamePort}: {ex.Message}");
                }
            }
            return (successes.Count > 0
                       ? "UPnP " + string.Join(", ", successes) + ". "
                       : "UPnP mapping unavailable. ") +
                   (errors.Count > 0 ? string.Join("; ", errors) + ". " : "") +
                   "Internet joinability is NOT verified. CGNAT may still prevent joining.";
        }, cancellationToken);
    }

    private static string? DiscoverLocalIpv4()
    {
        try
        {
            // UDP Connect selects the default-route interface; NO packet is
            // transmitted. This is a routing check, not an IP lookup website.
            using var socket = new Socket(
                AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
            socket.Connect(new IPEndPoint(IPAddress.Parse("1.1.1.1"), 53));
            var local = (socket.LocalEndPoint as IPEndPoint)?.Address;
            if(local is null || IPAddress.IsLoopback(local) ||
               local.Equals(IPAddress.Any))
                return null;
            return local.ToString();
        }
        catch(SocketException) { return null; }
    }

    private static dynamic OpenCollection()
    {
        var type = Type.GetTypeFromProgID("HNetCfg.NATUPnP");
        if(type is null)
            throw new NotSupportedException("Windows NATUPnP COM API not available.");
        dynamic nat = Activator.CreateInstance(type)
            ?? throw new NotSupportedException("Cannot create Windows NATUPnP service.");
        dynamic? mappings = nat.StaticPortMappingCollection;
        if(mappings is null)
            throw new NotSupportedException("Router does not expose a UPnP IGD mapping table.");
        return mappings;
    }

    // Returns true only when this launcher CREATED the mapping.
    private bool MapOne(int port, string protocol, string localAddress)
    {
        dynamic mappings = OpenCollection();
        dynamic? previous = null;
        try { previous = mappings.Item(port, protocol); }
        catch(COMException) { /* Not mapped yet. */ }

        if(previous is not null)
        {
            var existingIp = (string)previous.InternalClient;
            var existingPort = (int)previous.InternalPort;
            if(existingIp == localAddress && existingPort == port)
                return false;
            throw new InvalidOperationException(
                "Existing router mapping belongs to another address/port; not overwritten.");
        }

        dynamic created = mappings.Add(port, protocol, port, localAddress,
                                       true, "FIRETEAM AutoHost");
        if(created is null)
            throw new InvalidOperationException("Router did not confirm the mapping.");

        lock(_gate) _owned.Add((port, protocol, localAddress));
        return true;
    }

    public Task<string> RemoveOwnedMappingsAsync() => Task.Run(() =>
    {
        List<(int Port, string Protocol, string Address)> existing;
        lock(_gate)
        {
            existing = _owned.ToList();
            _owned.Clear();
        }

        if(existing.Count == 0)
            return "No launcher-owned router mappings to release.";

        var removed = 0;
        foreach(var (port, protocol, address) in existing)
        {
            try
            {
                dynamic mappings = OpenCollection();
                dynamic? entry = mappings.Item(port, protocol);
                // NEVER remove a router mapping which changed ownership.
                if(entry is not null &&
                   (string)entry.InternalClient == address &&
                   (int)entry.InternalPort == port &&
                   (string)entry.Description == "FIRETEAM AutoHost")
                {
                    mappings.Remove(port, protocol);
                    ++removed;
                }
            }
            catch(Exception ex) when(ex is COMException or
                InvalidOperationException or NotSupportedException or
                Microsoft.CSharp.RuntimeBinder.RuntimeBinderException)
            {
                // One mapping may be removed externally; continue others.
                System.Diagnostics.Debug.WriteLine(
                    "FIRETEAM UPnP cleanup: " + ex.Message);
            }
        }
        return $"Removed {removed} FIRETEAM-owned UPnP mapping(s).";
    });
}
