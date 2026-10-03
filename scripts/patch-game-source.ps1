param(
    [Parameter(Mandatory = $true)]
    [string]$LocalRoot
)

$ErrorActionPreference = "Stop"
$encoding = [System.Text.Encoding]::Default
$sealRoot = Join-Path $LocalRoot "imports\sealhunter"

function Read-Source([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing source file: $Path"
    }
    return [System.IO.File]::ReadAllText($Path, $encoding)
}

function Write-Source([string]$Path, [string]$Text) {
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

function Replace-Required([string]$Path, [string]$Old, [string]$New, [string]$Label) {
    $text = Read-Source $Path

    if ($text.Contains($New)) {
        Write-Host "[OK] $Label already patched"
        return
    }

    if (-not $text.Contains($Old)) {
        throw "Could not locate expected code for $Label in $Path"
    }

    $text = $text.Replace($Old, $New)
    Write-Source $Path $text
    Write-Host "[OK] $Label"
}

# Raise listen-server capacity.  This remains experimental until validated with
# real clients, but the SealHunter game code itself does not need to stay at 12.
Replace-Required (Join-Path $sealRoot "cshell\src\ltclientshell.cpp") "request.m_HostInfo.m_dwMaxConnections = 12;" "request.m_HostInfo.m_dwMaxConnections = 24;" "listen server max connections = 24"

$dedicated = Join-Path $sealRoot "ServerApp\Shared\DedicatedServerBase.cpp"
if (Test-Path -LiteralPath $dedicated) {
    Replace-Required $dedicated "m_nMaxPlayers(12)" "m_nMaxPlayers(24)" "dedicated server default max players = 24"
}

# SealHunter used the generic Jupiter/NOLF2 class name AIVolume for its enemy
# spawner.  Cabin Fever contains many real navigation AIVolume objects.
$aiHeader = Join-Path $sealRoot "sshell\src\AIVolume.h"
$text = Read-Source $aiHeader
if (-not $text.Contains("class ZombieSpawner : public BaseClass")) {
    if (-not $text.Contains("class AIVolume : public BaseClass")) {
        throw "Could not locate SealHunter AIVolume class declaration."
    }

    $text = $text.Replace("class AIVolume : public BaseClass", "class ZombieSpawner : public BaseClass")
    $text = $text.Replace("    AIVolume()", "    ZombieSpawner()")
    $text = $text.Replace([char]9 + "~AIVolume()", [char]9 + "~ZombieSpawner()")

    $marker = "#endif // __AIVolume_H__"
    $stub = @"
// Imported Jupiter/NOLF2-style AI navigation volume.
// Intentionally inert during the Cabin Fever bring-up milestone.
class AIVolume : public BaseClass
{
public:
    AIVolume() {}
    ~AIVolume() {}
};

"@
    if (-not $text.Contains($marker)) {
        throw "Could not locate AIVolume header footer."
    }

    $text = $text.Replace($marker, $stub + $marker)
    Write-Source $aiHeader $text
}
Write-Host "[OK] AIVolume header split into ZombieSpawner + inert AIVolume"

$aiCpp = Join-Path $sealRoot "sshell\src\AIVolume.cpp"
$text = Read-Source $aiCpp
if (-not $text.Contains("BEGIN_CLASS(ZombieSpawner)")) {
    if (-not $text.Contains("BEGIN_CLASS(AIVolume)")) {
        throw "Could not locate SealHunter AIVolume registration."
    }

    $text = $text.Replace("BEGIN_CLASS(AIVolume)", "BEGIN_CLASS(ZombieSpawner)")
    $text = $text.Replace("END_CLASS_DEFAULT_FLAGS(AIVolume, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)", "END_CLASS_DEFAULT_FLAGS(ZombieSpawner, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)")
    $text = $text.Replace("AIVolume::", "ZombieSpawner::")

    $registration = "END_CLASS_DEFAULT_FLAGS(ZombieSpawner, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)"
    $compatRegistration = @"
END_CLASS_DEFAULT_FLAGS(ZombieSpawner, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)

// Imported maps use AIVolume for navigation data.  Do not spawn enemies here.
BEGIN_CLASS(AIVolume)
ADD_STRINGPROP_FLAG(Target, "", PF_OBJECTLINK)
ADD_VECTORPROP_FLAG(Dims, PF_DIMS)
END_CLASS_DEFAULT_FLAGS(AIVolume, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)
"@
    $text = $text.Replace($registration, $compatRegistration)
    Write-Source $aiCpp $text
}
Write-Host "[OK] AIVolume implementation collision removed"

$sealCpp = Join-Path $sealRoot "sshell\src\seal.cpp"
Replace-Required $sealCpp '"AIVolume0"' '"ZombieSpawner0"' "legacy seal spawner lookup"
Replace-Required $sealCpp "AIVolume *pAI = (AIVolume*)" "ZombieSpawner *pAI = (ZombieSpawner*)" "legacy seal spawner cast"

# Cabin Fever uses GameStartPoint00 while SealHunter uses GameStartPoint0.
$serverShell = Join-Path $sealRoot "sshell\src\ltservershell.cpp"
$oldStart = 'g_pLTServer->FindNamedObjects("GameStartPoint0", pStartPt);'
$newStart = 'g_pLTServer->FindNamedObjects("GameStartPoint00", pStartPt);' +
    [Environment]::NewLine + [Environment]::NewLine +
    [char]9 + '// SealHunter fallback.' + [Environment]::NewLine +
    [char]9 + 'if (pStartPt.NumObjects() == 0)' + [Environment]::NewLine +
    [char]9 + '{' + [Environment]::NewLine +
    [char]9 + [char]9 + 'g_pLTServer->FindNamedObjects("GameStartPoint0", pStartPt);' + [Environment]::NewLine +
    [char]9 + '}'
Replace-Required $serverShell $oldStart $newStart "GameStartPoint00 compatibility"

Write-Host "[OK] Fireteam FPS gameplay bring-up patch set complete."
