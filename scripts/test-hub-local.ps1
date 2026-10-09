# Safe loopback-only smoke test. Does not touch firewall or router.
param([string]$Url = "http://127.0.0.1:27890")
$ErrorActionPreference = "Stop"
function New-Hex([int]$bytes) {
    $buffer = New-Object byte[] $bytes
    $rng = [Security.Cryptography.RandomNumberGenerator]::Create()
    try { $rng.GetBytes($buffer) } finally { $rng.Dispose() }
    return [BitConverter]::ToString($buffer).Replace("-", "").ToLowerInvariant()
}
function Post-Json($path, $body) {
    Invoke-RestMethod -Uri "$Url$path" -Method Post -ContentType "application/json" -Body ($body | ConvertTo-Json -Compress) -TimeoutSec 6
}
$health = Invoke-RestMethod "$Url/healthz" -TimeoutSec 6
if($health.status -ne "ready") { throw "Hub health failed" }
$hostId = New-Hex 16
$key = New-Hex 32
$payload = @{
    id = $hostId; key = $key; name = "TEST_HOST"; map = "CABINFEVER"
    port = 27889; players = 2; maxPlayers = 24
    difficulty = 4; protocol = "FT1"
}
$reply = Post-Json "/v1/hosts/heartbeat" $payload
if(-not $reply.accepted) { throw "Registration failed" }
$directory = Invoke-RestMethod "$Url/v1/servers" -TimeoutSec 6
$entry = @($directory.servers | Where-Object { $_.name -eq "TEST_HOST" })
if($entry.Count -ne 1 -or $entry[0].players -ne 2 -or $entry[0].verified -ne $false) {
    throw "Directory listing incorrect"
}
$payload.players = 7
$reply = Post-Json "/v1/hosts/heartbeat" $payload
$directory = Invoke-RestMethod "$Url/v1/servers" -TimeoutSec 6
$entry = @($directory.servers | Where-Object { $_.name -eq "TEST_HOST" })
if($entry.Count -ne 1 -or $entry[0].players -ne 7) {
    throw "Heartbeat occupancy not updated"
}
$wrong = $payload.Clone()
$wrong.key = New-Hex 32
$rejected = $false
try { $null = Post-Json "/v1/hosts/heartbeat" $wrong }
catch { $rejected = $true }
if(-not $rejected) { throw "Wrong key was accepted" }
$offline = @{ id = $hostId; key = $key } | ConvertTo-Json -Compress
Invoke-RestMethod -Uri "$Url/v1/hosts/offline" -Method Post -ContentType "application/json" -Body $offline -TimeoutSec 6 | Out-Null
$directory = Invoke-RestMethod "$Url/v1/servers" -TimeoutSec 6
if(@($directory.servers | Where-Object { $_.name -eq "TEST_HOST" }).Count -ne 0) {
    throw "Offline host not removed"
}
Write-Host "[PASS] Hub heartbeat / occupancy / wrong-key protection / offline removal"
