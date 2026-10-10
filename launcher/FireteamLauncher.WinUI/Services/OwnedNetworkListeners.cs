using System.Net;
using System.Runtime.InteropServices;

namespace FireteamLauncher.Services;

// Windows IP Helper API. Resolve sockets by OwningPid before asking a router
// to make ANY port public. Checking only the port could map an unrelated app.
internal static class OwnedNetworkListeners
{
    private const int AfInet = 2;
    private const uint MoreData = 122;
    private const int MaxTableBytes = 4 * 1024 * 1024;

    private enum UdpTableClass : int
    {
        Basic = 0,
        OwnerPid = 1
    }

    private enum TcpTableClass : int
    {
        BasicListener = 0,
        BasicConnections = 1,
        BasicAll = 2,
        OwnerPidListener = 3
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct UdpOwnerRow
    {
        public uint LocalAddress;
        public uint LocalPort;
        public uint OwningPid;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct TcpOwnerRow
    {
        public uint State;
        public uint LocalAddress;
        public uint LocalPort;
        public uint RemoteAddress;
        public uint RemotePort;
        public uint OwningPid;
    }

    [DllImport("iphlpapi.dll", EntryPoint = "GetExtendedUdpTable")]
    private static extern uint GetExtendedUdpTable(
        IntPtr buffer, ref uint bytes, bool order, int family,
        UdpTableClass tableClass, uint reserved);

    [DllImport("iphlpapi.dll", EntryPoint = "GetExtendedTcpTable")]
    private static extern uint GetExtendedTcpTable(
        IntPtr buffer, ref uint bytes, bool order, int family,
        TcpTableClass tableClass, uint reserved);

    public static string[] ProtocolsAtPort(int port, int processId)
    {
        if(!OperatingSystem.IsWindows() || processId <= 0 ||
           port is < 1 or > 65535)
            return [];

        var detected = new List<string>(2);
        if(HasUdpListener(port, processId))
            detected.Add("UDP");
        if(HasTcpListener(port, processId))
            detected.Add("TCP");
        return detected.ToArray();
    }

    private static int ReadPort(uint networkPort) =>
        unchecked((ushort)IPAddress.NetworkToHostOrder(
            unchecked((short)networkPort)));

    private static bool HasUdpListener(int port, int pid)
    {
        uint bytes = 0;
        if(GetExtendedUdpTable(IntPtr.Zero, ref bytes, false,
               AfInet, UdpTableClass.OwnerPid, 0) != MoreData ||
           bytes < 4 || bytes > MaxTableBytes)
            return false;

        var buffer = Marshal.AllocHGlobal(checked((int)bytes));
        try
        {
            if(GetExtendedUdpTable(buffer, ref bytes, false,
                   AfInet, UdpTableClass.OwnerPid, 0) != 0)
                return false;

            var rows = Marshal.ReadInt32(buffer);
            var stride = Marshal.SizeOf<UdpOwnerRow>();
            if(rows < 0 || rows > (bytes - 4) / stride)
                return false;

            for(var index = 0; index < rows; ++index)
            {
                var row = Marshal.PtrToStructure<UdpOwnerRow>(
                    IntPtr.Add(buffer, 4 + index * stride));
                if(row.OwningPid == (uint)pid &&
                   ReadPort(row.LocalPort) == port)
                    return true;
            }
        }
        finally { Marshal.FreeHGlobal(buffer); }
        return false;
    }

    private static bool HasTcpListener(int port, int pid)
    {
        uint bytes = 0;
        if(GetExtendedTcpTable(IntPtr.Zero, ref bytes, false,
               AfInet, TcpTableClass.OwnerPidListener, 0) != MoreData ||
           bytes < 4 || bytes > MaxTableBytes)
            return false;

        var buffer = Marshal.AllocHGlobal(checked((int)bytes));
        try
        {
            if(GetExtendedTcpTable(buffer, ref bytes, false,
                   AfInet, TcpTableClass.OwnerPidListener, 0) != 0)
                return false;

            var rows = Marshal.ReadInt32(buffer);
            var stride = Marshal.SizeOf<TcpOwnerRow>();
            if(rows < 0 || rows > (bytes - 4) / stride)
                return false;

            for(var index = 0; index < rows; ++index)
            {
                var row = Marshal.PtrToStructure<TcpOwnerRow>(
                    IntPtr.Add(buffer, 4 + index * stride));
                if(row.OwningPid == (uint)pid &&
                   ReadPort(row.LocalPort) == port)
                    return true;
            }
        }
        finally { Marshal.FreeHGlobal(buffer); }
        return false;
    }
}
