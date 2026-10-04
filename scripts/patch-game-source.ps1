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

# Raise listen-server capacity toward the desired 24-player target.
Replace-Required (Join-Path $sealRoot "cshell\src\ltclientshell.cpp") "request.m_HostInfo.m_dwMaxConnections = 12;" "request.m_HostInfo.m_dwMaxConnections = 24;" "listen server max connections = 24"

$dedicated = Join-Path $sealRoot "ServerApp\Shared\DedicatedServerBase.cpp"
if (Test-Path -LiteralPath $dedicated) {
    Replace-Required $dedicated "m_nMaxPlayers(12)" "m_nMaxPlayers(24)" "dedicated server default max players = 24"
}

# SealHunter used AIVolume as an enemy spawner. Imported Jupiter maps use
# AIVolume as navigation data, so keep those meanings separate.
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
// Intentionally inert during map bring-up.
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

// Imported maps use AIVolume for navigation data. Do not spawn enemies here.
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

# Cabin Fever contains command/volume Trigger objects. SealHunter's sample
# Trigger forced them to be world models, so make imported Trigger objects inert.
$triggerCpp = Join-Path $sealRoot "sshell\src\trigger.cpp"
Replace-Required $triggerCpp "END_CLASS_DEFAULT_FLAGS(Trigger, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD| CF_WORLDMODEL)" "END_CLASS_DEFAULT_FLAGS(Trigger, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)" "Trigger world-model class flag disabled"
Replace-Required $triggerCpp "pStruct->m_ObjectType = OT_WORLDMODEL;" "pStruct->m_ObjectType = OT_NORMAL;" "Trigger object type made inert"
Replace-Required $triggerCpp "pStruct->m_Flags |= FLAG_VISIBLE | FLAG_SOLID | FLAG_BOXPHYSICS;//FLAG_TOUCH_NOTIFY;" "pStruct->m_Flags = 0;" "Trigger collision/visibility disabled"

# Cabin Fever uses GameStartPoint00 while SealHunter uses GameStartPoint0.
$serverShell = Join-Path $sealRoot "sshell\src\ltservershell.cpp"
$oldStart = 'g_pLTServer->FindNamedObjects("GameStartPoint0", pStartPt);'
$newStart = 'g_pLTServer->FindNamedObjects("GameStartPoint00", pStartPt);' +
    [Environment]::NewLine + [Environment]::NewLine +
    [char]9 + '// SealHunter fallback.' + [Environment]::NewLine +
    [char]9 + 'if (pStartPt.NumObjects() == 0)' + [Environment]::NewLine +
    [char]9 + '{' + [Environment]::NewLine +
    [string]([char]9) + [string]([char]9) + 'g_pLTServer->FindNamedObjects("GameStartPoint0", pStartPt);' + [Environment]::NewLine +
    [char]9 + '}'
Replace-Required $serverShell $oldStart $newStart "GameStartPoint00 compatibility"

# +runworld chooses the world but the original sample still waits at its menu.
# +autostart 1 directly starts normal/local mode after initialization.
# Rebuild this block canonically every run so older duplicate patch attempts are cleaned up.
$clientShell = Join-Path $sealRoot "cshell\src\ltclientshell.cpp"
$text = Read-Source $clientShell

# Migrate old branding first.
$text = $text.Replace("Fireteam FPS:", "Fireteam:")

# Remove every previously injected auto-start block. Some older local workspaces
# accumulated two copies during patch evolution, which causes hAutoStart redefinition.
$autoStartPattern = '(?ms)^[ \t]*HCONSOLEVAR[ \t]+hAutoStart[ \t]*=[ \t]*g_pLTClient->GetConsoleVar\("autostart"\);[ \t]*\r?\n[ \t]*if\(hAutoStart[ \t]*&&[ \t]*g_pLTClient->GetVarValueFloat\(hAutoStart\)[ \t]*!=[ \t]*0\.0f\)[ \t]*\r?\n[ \t]*\{[ \t]*\r?\n[ \t]*g_pLTClient->CPrint\("Fireteam(?::| FPS:) auto-starting selected world\.\.\."\);[ \t]*\r?\n[ \t]*result[ \t]*=[ \t]*StartNormalGame\(\);[ \t]*\r?\n[ \t]*\}[ \t]*\r?\n?'
$text = [regex]::Replace($text, $autoStartPattern, "")

$needle = "m_pChatGui->Init();"
$idx = $text.IndexOf($needle)
if ($idx -lt 0) {
    throw "Could not locate client-shell auto-start insertion point."
}

$lineStart = $text.LastIndexOf([Environment]::NewLine, $idx)
if ($lineStart -lt 0) { $lineStart = 0 } else { $lineStart += [Environment]::NewLine.Length }
$lineEnd = $text.IndexOf([Environment]::NewLine, $idx)
if ($lineEnd -lt 0) { $lineEnd = $text.Length }

$originalLine = $text.Substring($lineStart, $lineEnd - $lineStart)
$indent = $originalLine.Substring(0, $originalLine.IndexOf("m_pChatGui->Init();"))

$insert = $originalLine +
    [Environment]::NewLine + [Environment]::NewLine +
    $indent + 'HCONSOLEVAR hAutoStart = g_pLTClient->GetConsoleVar("autostart");' + [Environment]::NewLine +
    $indent + 'if(hAutoStart && g_pLTClient->GetVarValueFloat(hAutoStart) != 0.0f)' + [Environment]::NewLine +
    $indent + '{' + [Environment]::NewLine +
    $indent + '    g_pLTClient->CPrint("Fireteam: auto-starting selected world...");' + [Environment]::NewLine +
    $indent + '    result = StartNormalGame();' + [Environment]::NewLine +
    $indent + '}'

$text = $text.Remove($lineStart, $lineEnd - $lineStart).Insert($lineStart, $insert)
Write-Source $clientShell $text
Write-Host "[OK] command-line normal-game auto-start normalized"

# Launcher-first project: disable SealHunter's splash/menu frontend.
# Be tolerant of both pristine SealHunter source and already-patched local trees.
$text = Read-Source $clientShell

$frontendPatches = @(
    @("m_Gui.Init(15, 18);", "// Fireteam: legacy SealHunter frontend disabled.", "legacy frontend initialization disabled"),
    @("m_Gui.Render();", "// Fireteam: legacy SealHunter frontend render disabled.", "legacy frontend rendering disabled"),
    @("m_Gui.HandleInput(command);", "// Fireteam: launcher owns frontend/menu input.", "legacy frontend input disabled")
)

foreach($patch in $frontendPatches) {
    $old = $patch[0]
    $new = $patch[1]
    $label = $patch[2]

    if ($text.Contains($new)) {
        Write-Host "[OK] $label already patched"
        continue
    }

    $oldBranded = $new.Replace("Fireteam:", "Fireteam FPS:")
    if ($text.Contains($oldBranded)) {
        $text = $text.Replace($oldBranded, $new)
        Write-Host "[OK] $label branding migrated"
        continue
    }

    if ($text.Contains($old)) {
        $text = $text.Replace($old, $new)
        Write-Host "[OK] $label"
        continue
    }

    throw "Could not locate expected code for $label in $clientShell"
}

Write-Source $clientShell $text
Write-Host "[OK] legacy frontend disabled"

# First-person camera bring-up. NOLF2 uses the same basic concept: first-person
# at the player/head offset and chase mode for third-person. Default to FPS and
# keep SealHunter third-person available on C.
$cameraH = Join-Path $sealRoot "cshell\src\camera.h"
$text = Read-Source $cameraH
if (-not $text.Contains("ToggleView()")) {
    $lines = $text -split "\r\n|\n|\r"

    $zoomIndex = -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i].Contains("UpdateZoom(float zoom);")) {
            $zoomIndex = $i
            break
        }
    }
    if ($zoomIndex -lt 0) {
        throw "Could not add camera toggle declarations."
    }

    $lines = @(
        $lines[0..$zoomIndex]
        "    void            ToggleView();"
        "    bool            IsFirstPerson() const { return m_bFirstPerson; }"
        $lines[($zoomIndex + 1)..($lines.Length - 1)]
    )

    $zoomMemberIndex = -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i].Contains("m_fZoom;")) {
            $zoomMemberIndex = $i
            break
        }
    }
    if ($zoomMemberIndex -lt 0) {
        throw "Could not add first-person camera state."
    }

    $lines = @(
        $lines[0..$zoomMemberIndex]
        "    bool            m_bFirstPerson;"
        $lines[($zoomMemberIndex + 1)..($lines.Length - 1)]
    )

    $text = [string]::Join([Environment]::NewLine, $lines)
    Write-Source $cameraH $text
}
Write-Host "[OK] camera first-person declarations"

$cameraCpp = Join-Path $sealRoot "cshell\src\camera.cpp"
$text = Read-Source $cameraCpp

if (-not $text.Contains('#include "clientinterfaces.h"')) {
    $needle = '#include <ltobjectcreate.h>'
    if (-not $text.Contains($needle)) {
        throw "Could not locate camera include insertion point."
    }
    $text = $text.Replace($needle, $needle + [Environment]::NewLine + '#include "clientinterfaces.h"')
}

$text = $text.Replace("#define MAX_PITCH   30.0f", "#define MAX_PITCH   85.0f")

if (-not $text.Contains("m_bFirstPerson(true)")) {
    $needle = "m_fZoom(MIN_ZOOM)"
    if (-not $text.Contains($needle)) {
        throw "Could not locate camera constructor."
    }
    $text = $text.Replace($needle, $needle + "," + [Environment]::NewLine + "m_bFirstPerson(true)")
}

if (-not $text.Contains("Fireteam FPS first-person camera")) {
    $needle = "    rRot.Rotate(rRot.Right(), (m_fPitch * 0.0174533f));"
    if (-not $text.Contains($needle)) {
        throw "Could not locate camera pitch update."
    }

    $insert = "    LTVector vEyeUp = rRot.Up();" + [Environment]::NewLine +
        "    rRot.Rotate(rRot.Right(), (m_fPitch * 0.0174533f));" + [Environment]::NewLine + [Environment]::NewLine +
        "    // Fireteam FPS first-person camera." + [Environment]::NewLine +
        "    if (m_bFirstPerson)" + [Environment]::NewLine +
        "    {" + [Environment]::NewLine +
        "        g_pLTCCommon->SetObjectFlags(hObject, OFT_Flags, 0, FLAG_VISIBLE);" + [Environment]::NewLine +
        "        vPos += vEyeUp * 65.0f;" + [Environment]::NewLine +
        "        vPos += rRot.Forward() * 3.0f;" + [Environment]::NewLine +
        "        g_pLTClient->SetObjectPosAndRotation(m_hObject, &vPos, &rRot);" + [Environment]::NewLine +
        "        return;" + [Environment]::NewLine +
        "    }" + [Environment]::NewLine + [Environment]::NewLine +
        "    g_pLTCCommon->SetObjectFlags(hObject, OFT_Flags, FLAG_VISIBLE, FLAG_VISIBLE);"

    $text = $text.Replace($needle, $insert)
}

if (-not $text.Contains("void CCamera::ToggleView()")) {
    $text += [Environment]::NewLine + [Environment]::NewLine +
        "//----------------------------------------------------------------------------" + [Environment]::NewLine +
        "// CCamera::ToggleView()" + [Environment]::NewLine +
        "//----------------------------------------------------------------------------" + [Environment]::NewLine +
        "void CCamera::ToggleView()" + [Environment]::NewLine +
        "{" + [Environment]::NewLine +
        "    m_bFirstPerson = !m_bFirstPerson;" + [Environment]::NewLine +
        '    g_pLTClient->CPrint("Camera: %s", m_bFirstPerson ? "First Person" : "Third Person");' + [Environment]::NewLine +
        "}" + [Environment]::NewLine
}

Write-Source $cameraCpp $text
Write-Host "[OK] first-person camera implementation"

$clientShell = Join-Path $sealRoot "cshell\src\ltclientshell.cpp"
$text = Read-Source $clientShell

if (-not $text.Contains("m_pCamera->ToggleView();")) {
    $needle = "if(VK_ESCAPE == key)"
    if (-not $text.Contains($needle)) {
        throw "Could not locate in-game key handler."
    }

    $insert = "if('C' == key)" + [Environment]::NewLine +
        "           {" + [Environment]::NewLine +
        "               m_pCamera->ToggleView();" + [Environment]::NewLine +
        "           }" + [Environment]::NewLine +
        "           else if(VK_ESCAPE == key)"

    $text = $text.Replace($needle, $insert)
    Write-Source $clientShell $text
}
Write-Host "[OK] C toggles first/third person"


# Project branding.
$clientShell = Join-Path $sealRoot "cshell\src\ltclientshell.cpp"
$text = Read-Source $clientShell
$text = $text.Replace('"Seal Hunter Server"', '"Fireteam Server"')
$text = $text.Replace('Fireteam FPS:', 'Fireteam:')
Write-Source $clientShell $text

Write-Host "[OK] Fireteam gameplay bring-up patch set complete."
