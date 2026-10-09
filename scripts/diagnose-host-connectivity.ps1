# Read-only FIRETEAM host networking diagnostics. No router changes.
# Run this while the dedicated server is running:
# powershell -NoProfile -ExecutionPolicy Bypass -File scripts\diagnose-host-connectivity.ps1
param(
    [ValidateRange(1,65534)]
    [int]$GamePort = 27889,
    [switch]$CheckExternalIp
)

$ErrorActionPreference = "Stop"
Write-Host "FIRETEAM - Host connectivity diagnostics (READ ONLY)"
Write-Host "Local socket inspection; does NOT enable UPnP or change Windows Firewall."
Write-Host "Game port: $GamePort   LAN discovery port: 27888"
Write-Host ""

$ports = @($GamePort, ($GamePort + 1), 27888) | Sort-Object -Unique
function Format-Sockets($items, [string]$protocol) {
    $matching = @($items | Where-Object { $ports -contains $_.LocalPort } |
        Sort-Object LocalPort, OwningProcess)
    if($matching.Count -eq 0) {
        Write-Host "[NO MATCH] No local $protocol endpoints on requested ports."
        return
    }
    foreach($entry in $matching) {
        $processName = "unknown"
        try {
            $processName = (Get-Process -Id $entry.OwningProcess -ErrorAction Stop).ProcessName
        } catch { }
        Write-Host ("[{0}] Local={1}:{2} PID={3} Process={4}" -f
            $protocol, $entry.LocalAddress, $entry.LocalPort, $entry.OwningProcess, $processName)
    }
}
try {
    Format-Sockets (Get-NetUDPEndpoint -ErrorAction Stop) "UDP"
    Format-Sockets (Get-NetTCPConnection -State Listen -ErrorAction Stop) "TCP LISTEN"
} catch {
    Write-Host "[WARN] Cannot inspect sockets: $($_.Exception.Message)"
    Write-Host "As an alternative, run: netstat -ano | findstr :$GamePort"
}
Write-Host ""
Write-Host "Default IPv4 gateways:"
try {
    $routes = @(Get-NetRoute -AddressFamily IPv4 -DestinationPrefix "0.0.0.0/0" -ErrorAction Stop |
        Sort-Object RouteMetric, InterfaceMetric |
        Select-Object -First 4)
    foreach($r in $routes) {
        Write-Host ("  Gateway={0} InterfaceIndex={1}" -f $r.NextHop, $r.InterfaceIndex)
    }
} catch {
    Write-Host "[WARN] Cannot read routes: $($_.Exception.Message)"
}

Write-Host ""
Write-Host "Windows UPnP IGD read-only capability probe:"
try {
    $nat = New-Object -ComObject "HNetCfg.NATUPnP" -ErrorAction Stop
    $mappings = $nat.StaticPortMappingCollection
    if($null -eq $mappings) {
        Write-Host "[UNKNOWN] Router did not expose the UPnP mapping collection."
    } else {
        Write-Host "[PRESENT] UPnP mapping collection is accessible. Not proof of external reachability."
        foreach($protocol in @("UDP", "TCP")) {
            try {
                $entry = $mappings.Item($GamePort, $protocol)
                if($null -ne $entry) {
                    Write-Host ("[MAPPING] {0}/{1} -> {2}:{3} Description={4}" -f
                        $GamePort, $protocol, $entry.InternalClient, $entry.InternalPort, $entry.Description)
                }
            } catch {
                Write-Host "[NO MAPPING] $protocol/$GamePort is not currently mapped via this UPnP interface."
            }
        }
    }
} catch {
    Write-Host "[UNKNOWN] UPnP COM query unavailable: $($_.Exception.Message)"
}
Write-Host "UPnP collection absence is inconclusive: NAT-PMP, PCP or manual settings may still work."

if($CheckExternalIp) {
    Write-Host ""
    Write-Host "Optional public IP request (contacts api.ipify.org):"
    try {
        $ip = (Invoke-RestMethod -Uri "https://api4.ipify.org" -TimeoutSec 6).Trim()
        Write-Host "  Internet-observed public IPv4: $ip"
    } catch {
        Write-Host "[WARN] Public-IP request failed: $($_.Exception.Message)"
    }
    Write-Host "Compare against your ROUTER's WAN/Internet IPv4, not the PC's LAN IPv4."
    Write-Host "Different WAN vs Internet IPv4 can indicate double NAT or carrier-grade NAT."
} else {
    Write-Host ""
    Write-Host "Skipped any external service request. Add -CheckExternalIp ONLY if you consent."
}
Write-Host ""
Write-Host "A socket + UPnP result is NOT a connectivity test. Verify a join from outside the LAN."
Write-Host "Redact local/public IP addresses before sharing full diagnostic output."
