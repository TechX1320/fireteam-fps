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
    $dir = Split-Path -Parent $Path
    if ($dir) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

function Insert-AfterLineContaining([string]$Path, [string]$Needle, [string[]]$NewLines, [string]$Guard, [string]$Label) {
    $text = Read-Source $Path
    if ($text.Contains($Guard)) {
        Write-Host "[OK] $Label already patched"
        return
    }

    $lines = $text -split "\r\n|\n|\r"
    $index = -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i].Contains($Needle)) {
            $index = $i
            break
        }
    }

    if ($index -lt 0) {
        throw "Could not locate $Needle for $Label in $Path"
    }

    $before = $lines[0..$index]
    $after = @()
    if (($index + 1) -lt $lines.Length) {
        $after = $lines[($index + 1)..($lines.Length - 1)]
    }

    Write-Source $Path ([string]::Join([Environment]::NewLine, @($before + $NewLines + $after)))
    Write-Host "[OK] $Label"
}

# Shared protocol ids.
$msgIds = Join-Path $sealRoot "shared\src\msgids.h"
Insert-AfterLineContaining $msgIds "MSG_SC_CHAT" @(
    "        MSG_SC_HEALTH,              // server->client",
    "        MSG_SC_LIGHTGROUP,          // server->client"
) "MSG_SC_HEALTH" "health/lightgroup message ids"

# Minimal static LightGroup implementation based on NOLF2's approach.
$lightGroupH = @'
#ifndef __FIRETEAM_LIGHTGROUP_H__
#define __FIRETEAM_LIGHTGROUP_H__

#include <ltengineobjects.h>
#include <iltmessage.h>

class LightGroup : public Engine_LightGroup
{
public:
    LightGroup();

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);

private:
    void ReadProps(ObjectCreateStruct *pOCS);
    void SendUpdate();

    uint32   m_nID;
    LTVector m_vColor;
    bool     m_bOn;
};

#endif
'@

$lightGroupCpp = @'
#include "FireteamLightGroup.h"

#include "serverinterfaces.h"
#include "msgids.h"

#include <iltcommon.h>
#include <iltmessage.h>
#include <ltobjectcreate.h>
#include <string.h>

BEGIN_CLASS(LightGroup)
    ADD_BOOLPROP(StartOn, LTTRUE)
    ADD_COLORPROP(StartColor, 255, 255, 255)
END_CLASS_DEFAULT_FLAGS(LightGroup, Engine_LightGroup, LTNULL, LTNULL, 0)

LightGroup::LightGroup() :
    m_nID(0),
    m_vColor(1.0f, 1.0f, 1.0f),
    m_bOn(true)
{
}

void LightGroup::ReadProps(ObjectCreateStruct *pOCS)
{
    GenericProp prop;

    if(g_pLTServer->GetPropGeneric("StartOn", &prop) == LT_OK)
    {
        m_bOn = (prop.m_Bool != LTFALSE);
    }

    if(g_pLTServer->GetPropGeneric("StartColor", &prop) == LT_OK)
    {
        m_vColor = prop.m_Color / 255.0f;
    }

    if(g_pLTServer->GetPropGeneric("Name", &prop) == LT_OK)
    {
        strncpy(pOCS->m_Name, prop.m_String, sizeof(pOCS->m_Name) - 1);
        pOCS->m_Name[sizeof(pOCS->m_Name) - 1] = '\0';
    }

    g_pLTServer->GetLightGroupID(pOCS->m_Name, &m_nID);
    pOCS->m_Flags |= FLAG_FORCECLIENTUPDATE;
}

void LightGroup::SendUpdate()
{
    LTVector vAdjustment = m_bOn ? m_vColor : LTVector(0.0f, 0.0f, 0.0f);

    ILTMessage_Write *pMsg = LTNULL;
    if(g_pLTSCommon->CreateMessage(pMsg) != LT_OK || !pMsg)
    {
        return;
    }

    pMsg->IncRef();
    pMsg->Writeuint8(MSG_SC_LIGHTGROUP);
    pMsg->Writeuint32(m_nID);
    pMsg->WriteLTVector(vAdjustment);
    g_pLTServer->SendToClient(pMsg->Read(), LTNULL, MESSAGE_GUARANTEED);
    pMsg->DecRef();
}

uint32 LightGroup::EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData)
{
    switch(messageID)
    {
        case MID_PRECREATE:
        {
            ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;
            if(pOCS && fData == PRECREATE_WORLDFILE)
            {
                ReadProps(pOCS);
            }
        }
        break;

        case MID_INITIALUPDATE:
        {
            SendUpdate();
            g_pLTServer->SetNextUpdate(m_hObject, 2.0f);
        }
        break;

        case MID_UPDATE:
        {
            SendUpdate();
            g_pLTServer->SetNextUpdate(m_hObject, 2.0f);
        }
        break;

        default:
            break;
    }

    return Engine_LightGroup::EngineMessageFn(messageID, pData, fData);
}
'@

Write-Source (Join-Path $sealRoot "sshell\src\FireteamLightGroup.h") $lightGroupH
Write-Source (Join-Path $sealRoot "sshell\src\FireteamLightGroup.cpp") $lightGroupCpp
Write-Host "[OK] LightGroup server class"

$lightClientH = @'
#ifndef __FIRETEAM_LIGHTGROUP_CLIENT_H__
#define __FIRETEAM_LIGHTGROUP_CLIENT_H__

#include <ltbasetypes.h>
#include <ltvector.h>

void FT_QueueLightGroup(uint32 nID, const LTVector &vAdjustment);
void FT_UpdateLightGroups();
void FT_ClearLightGroups();

#endif
'@

$lightClientCpp = @'
#include "FireteamLightGroupClient.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <map>

static std::map<uint32, LTVector> s_BaseColors;
static std::map<uint32, LTVector> s_PendingAdjustments;

void FT_QueueLightGroup(uint32 nID, const LTVector &vAdjustment)
{
    s_PendingAdjustments[nID] = vAdjustment;
    FT_UpdateLightGroups();
}

void FT_UpdateLightGroups()
{
    std::map<uint32, LTVector>::iterator it = s_PendingAdjustments.begin();

    while(it != s_PendingAdjustments.end())
    {
        uint32 nID = it->first;
        LTVector vAdjustment = it->second;
        LTVector vBaseColor;

        std::map<uint32, LTVector>::iterator baseIt = s_BaseColors.find(nID);
        if(baseIt == s_BaseColors.end())
        {
            if(g_pLTClient->GetLightGroupColor(nID, &vBaseColor) != LT_OK)
            {
                ++it;
                continue;
            }

            s_BaseColors[nID] = vBaseColor;
        }
        else
        {
            vBaseColor = baseIt->second;
        }

        g_pLTClient->SetLightGroupColor(nID, vBaseColor * vAdjustment);

        std::map<uint32, LTVector>::iterator eraseIt = it;
        ++it;
        s_PendingAdjustments.erase(eraseIt);
    }
}

void FT_ClearLightGroups()
{
    std::map<uint32, LTVector>::iterator it = s_BaseColors.begin();
    for(; it != s_BaseColors.end(); ++it)
    {
        g_pLTClient->SetLightGroupColor(it->first, it->second);
    }

    s_BaseColors.clear();
    s_PendingAdjustments.clear();
}
'@

Write-Source (Join-Path $sealRoot "cshell\src\FireteamLightGroupClient.h") $lightClientH
Write-Source (Join-Path $sealRoot "cshell\src\FireteamLightGroupClient.cpp") $lightClientCpp
Write-Host "[OK] LightGroup client manager"

$healthHudH = @'
#ifndef __FIRETEAM_HEALTH_HUD_H__
#define __FIRETEAM_HEALTH_HUD_H__

#include <ltbasedefs.h>

void FT_SetHealth(uint8 nHealth, uint8 nMaxHealth);
void FT_RenderHealthHud();

#endif
'@

$healthHudCpp = @'
#include "FireteamHealthHud.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <iltdrawprim.h>

static uint8 s_nHealth = 100;
static uint8 s_nMaxHealth = 100;

static void SetupQuad(LT_POLYF4 &poly, float x, float y, float width, float height,
                      uint8 r, uint8 g, uint8 b, uint8 a)
{
    poly.rgba.r = r;
    poly.rgba.g = g;
    poly.rgba.b = b;
    poly.rgba.a = a;

    poly.verts[0].x = x;
    poly.verts[0].y = y;
    poly.verts[0].z = SCREEN_NEAR_Z;

    poly.verts[1].x = x + width;
    poly.verts[1].y = y;
    poly.verts[1].z = SCREEN_NEAR_Z;

    poly.verts[2].x = x + width;
    poly.verts[2].y = y + height;
    poly.verts[2].z = SCREEN_NEAR_Z;

    poly.verts[3].x = x;
    poly.verts[3].y = y + height;
    poly.verts[3].z = SCREEN_NEAR_Z;
}

void FT_SetHealth(uint8 nHealth, uint8 nMaxHealth)
{
    s_nMaxHealth = nMaxHealth ? nMaxHealth : 1;
    s_nHealth = (nHealth > s_nMaxHealth) ? s_nMaxHealth : nHealth;
}

void FT_RenderHealthHud()
{
    if(!g_pLTClient || !g_pLTCDrawPrim)
    {
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &nScreenW, &nScreenH);

    const float fWidth = 240.0f;
    const float fHeight = 16.0f;
    const float fX = 28.0f;
    const float fY = (float)nScreenH - 42.0f;
    const float fRatio = (float)s_nHealth / (float)s_nMaxHealth;

    LT_POLYF4 background;
    LT_POLYF4 health;

    SetupQuad(background, fX, fY, fWidth, fHeight, 20, 20, 20, 220);
    SetupQuad(health, fX + 2.0f, fY + 2.0f, (fWidth - 4.0f) * fRatio, fHeight - 4.0f,
              210, 36, 36, 255);

    g_pLTCDrawPrim->SetTexture(LTNULL);
    g_pLTCDrawPrim->SetTransformType(DRAWPRIM_TRANSFORM_SCREEN);
    g_pLTCDrawPrim->SetColorOp(DRAWPRIM_NOCOLOROP);
    g_pLTCDrawPrim->SetAlphaBlendMode(DRAWPRIM_BLEND_MOD_SRCALPHA);
    g_pLTCDrawPrim->SetZBufferMode(DRAWPRIM_NOZ);
    g_pLTCDrawPrim->SetAlphaTestMode(DRAWPRIM_NOALPHATEST);
    g_pLTCDrawPrim->SetClipMode(DRAWPRIM_FASTCLIP);
    g_pLTCDrawPrim->SetFillMode(DRAWPRIM_FILL);
    g_pLTCDrawPrim->SetCullMode(DRAWPRIM_CULL_NONE);
    g_pLTCDrawPrim->SetCamera(LTNULL);

    g_pLTCDrawPrim->DrawPrim(&background, 1);
    if(s_nHealth > 0)
    {
        g_pLTCDrawPrim->DrawPrim(&health, 1);
    }
}
'@

Write-Source (Join-Path $sealRoot "cshell\src\FireteamHealthHud.h") $healthHudH
Write-Source (Join-Path $sealRoot "cshell\src\FireteamHealthHud.cpp") $healthHudCpp
Write-Host "[OK] health HUD"

# Client shell hooks.
$clientShell = Join-Path $sealRoot "cshell\src\ltclientshell.cpp"
$text = Read-Source $clientShell

if (-not $text.Contains('#include "FireteamLightGroupClient.h"')) {
    $needle = '#include "chatgui.h"'
    if (-not $text.Contains($needle)) {
        throw "Could not locate client shell include insertion point."
    }

    $text = $text.Replace(
        $needle,
        $needle + [Environment]::NewLine +
        '#include "FireteamLightGroupClient.h"' + [Environment]::NewLine +
        '#include "FireteamHealthHud.h"')
}

if (-not $text.Contains("case MSG_SC_HEALTH:")) {
    $needle = "    case MSG_WORLD_PROPS:"
    if (-not $text.Contains($needle)) {
        throw "Could not locate client message insertion point."
    }

    $cases = @"
    case MSG_SC_HEALTH:
        {
            uint8 nHealth = pMessage->Readuint8();
            uint8 nMaxHealth = pMessage->Readuint8();
            FT_SetHealth(nHealth, nMaxHealth);
        }
        break;
    case MSG_SC_LIGHTGROUP:
        {
            uint32 nLightGroupID = pMessage->Readuint32();
            LTVector vAdjustment = pMessage->ReadLTVector();
            FT_QueueLightGroup(nLightGroupID, vAdjustment);
        }
        break;
"@

    $text = $text.Replace($needle, $cases + $needle)
}

if (-not $text.Contains("FT_UpdateLightGroups();")) {
    $needle = "    // Update the world properties class"
    if (-not $text.Contains($needle)) {
        throw "Could not locate client update tail."
    }

    $text = $text.Replace(
        $needle,
        "    FT_UpdateLightGroups();" + [Environment]::NewLine + [Environment]::NewLine + $needle)
}

if (-not $text.Contains("FT_ClearLightGroups();")) {
    $needle = "    // Stop all the client FX"
    if (-not $text.Contains($needle)) {
        throw "Could not locate OnExitWorld cleanup point."
    }

    $text = $text.Replace(
        $needle,
        "    FT_ClearLightGroups();" + [Environment]::NewLine + [Environment]::NewLine + $needle)
}

if (-not $text.Contains("FT_RenderHealthHud();")) {
    $needle = "    // Render the gui."
    if (-not $text.Contains($needle)) {
        throw "Could not locate HUD render insertion point."
    }

    $hud = "    if(IsInWorld())" + [Environment]::NewLine +
        "    {" + [Environment]::NewLine +
        "        FT_RenderHealthHud();" + [Environment]::NewLine +
        "    }" + [Environment]::NewLine + [Environment]::NewLine

    $text = $text.Replace($needle, $hud + $needle)
}

Write-Source $clientShell $text
Write-Host "[OK] client system hooks"

# Player server health.
$playerH = Join-Path $sealRoot "sshell\src\playersrvr.h"
$text = Read-Source $playerH

if (-not $text.Contains("m_nHealth(100)")) {
    $needle = "          m_iSendStatsCounter(0)"
    if (-not $text.Contains($needle)) {
        throw "Could not locate player constructor stat initializer."
    }

    $text = $text.Replace(
        $needle,
        $needle + "," + [Environment]::NewLine +
        "          m_nHealth(100)," + [Environment]::NewLine +
        "          m_nMaxHealth(100)")
}

if (-not $text.Contains("SendHealth(); }")) {
    $lines = $text -split "\r\n|\n|\r"
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i].Contains("SetClient(HCLIENT hClient)")) {
            $lines[$i] = "    void                SetClient(HCLIENT hClient){ m_hClient = hClient; SendHealth(); }"
            break
        }
    }
    $text = [string]::Join([Environment]::NewLine, $lines)
}

if (-not $text.Contains("void                SendHealth();")) {
    $lines = $text -split "\r\n|\n|\r"
    $index = -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i].Contains("PlaySound(int i);")) {
            $index = $i
            break
        }
    }
    if ($index -lt 0) {
        throw "Could not locate PlaySound declaration for health sync."
    }

    $before = $lines[0..$index]
    $after = $lines[($index + 1)..($lines.Length - 1)]
    $text = [string]::Join([Environment]::NewLine, @($before + "    void                SendHealth();" + $after))
}

if (-not $text.Contains("m_nMaxHealth;")) {
    $lines = $text -split "\r\n|\n|\r"
    $index = -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i].Contains("m_iSendStatsCounter;")) {
            $index = $i
            break
        }
    }
    if ($index -lt 0) {
        throw "Could not locate player stat member insertion point."
    }

    $before = $lines[0..$index]
    $after = $lines[($index + 1)..($lines.Length - 1)]
    $newMembers = @(
        "",
        "    // Fireteam health",
        "    uint8               m_nHealth;",
        "    uint8               m_nMaxHealth;"
    )
    $text = [string]::Join([Environment]::NewLine, @($before + $newMembers + $after))
}

Write-Source $playerH $text

$playerCpp = Join-Path $sealRoot "sshell\src\playersrvr.cpp"
$text = Read-Source $playerCpp

if (-not $text.Contains("Fireteam: player %s reached 0 HP.")) {
    $needle = "        case OBJ_MID_PICKUP:"
    if (-not $text.Contains($needle)) {
        throw "Could not locate player object-message switch."
    }

    $damageCase = @"
        case OBJ_MID_DAMAGE:
            {
                uint8 nDamage = pMsg->Readuint8();
                m_nHealth = (nDamage >= m_nHealth) ? 0 : (uint8)(m_nHealth - nDamage);
                SendHealth();

                if(m_nHealth == 0)
                {
                    g_pLTServer->CPrint("Fireteam: player %s reached 0 HP.", m_sName);
                }
            }
            break;
"@

    $text = $text.Replace($needle, $damageCase + $needle)
}

if (-not $text.Contains('GetClass("CPlayerSrvr")')) {
    $needle = '                HCLASS hClassSnowman = g_pLTServer->GetClass("Snowman");'
    if (-not $text.Contains($needle)) {
        throw "Could not locate melee class list."
    }

    $text = $text.Replace(
        $needle,
        $needle + [Environment]::NewLine +
        '                HCLASS hClassPlayer = g_pLTServer->GetClass("CPlayerSrvr");')
}

if (-not $text.Contains("Fireteam player melee damage.")) {
    $needle = "                else" + [Environment]::NewLine +
        "                {" + [Environment]::NewLine +
        "                    PlaySound(3);" + [Environment]::NewLine +
        "                }"

    if (-not $text.Contains($needle)) {
        throw "Could not locate generic melee impact block."
    }

    $replacement = @"
                else if(iInfo.m_hObject != m_hObject && g_pLTServer->IsKindOf(hClassPlayer, hTarget))
                {
                    // Fireteam player melee damage.
                    ILTMessage_Write *pMsg;
                    g_pLTSCommon->CreateMessage(pMsg);
                    pMsg->IncRef();
                    pMsg->Writeuint32(OBJ_MID_DAMAGE);
                    pMsg->Writeuint8(15);
                    g_pLTServer->SendToObject(pMsg->Read(), m_hObject, iInfo.m_hObject, 0);
                    pMsg->DecRef();
                }
                else
                {
                    PlaySound(3);
                }
"@

    $text = $text.Replace($needle, $replacement)
}

if (-not $text.Contains("void CPlayerSrvr::SendHealth()")) {
    $text += @"

//-----------------------------------------------------------------------------
// CPlayerSrvr::SendHealth()
//
//-----------------------------------------------------------------------------
void CPlayerSrvr::SendHealth()
{
    if(!m_hClient)
    {
        return;
    }

    ILTMessage_Write *pMsg = LTNULL;
    if(g_pLTSCommon->CreateMessage(pMsg) != LT_OK || !pMsg)
    {
        return;
    }

    pMsg->IncRef();
    pMsg->Writeuint8(MSG_SC_HEALTH);
    pMsg->Writeuint8(m_nHealth);
    pMsg->Writeuint8(m_nMaxHealth);
    g_pLTServer->SendToClient(pMsg->Read(), m_hClient, MESSAGE_GUARANTEED);
    pMsg->DecRef();
}
"@
}

Write-Source $playerCpp $text
Write-Host "[OK] server health foundation"

Write-Host "[OK] Fireteam systems patch complete."
