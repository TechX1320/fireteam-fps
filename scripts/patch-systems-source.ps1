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


function Insert-BeforeLineContaining([string]$Path, [string]$Needle, [string[]]$NewLines, [string]$Guard, [string]$Label) {
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

    $before = @()
    if ($index -gt 0) {
        $before = $lines[0..($index - 1)]
    }

    $after = $lines[$index..($lines.Length - 1)]
    Write-Source $Path ([string]::Join([Environment]::NewLine, @($before + $NewLines + $after)))
    Write-Host "[OK] $Label"
}

# ---------------------------------------------------------------------------
# Shared Fireteam network messages.
# ---------------------------------------------------------------------------
$msgIds = Join-Path $sealRoot "shared\src\msgids.h"
Insert-AfterLineContaining $msgIds "MSG_SC_CHAT" @(
    "        MSG_SC_HEALTH,              // server->client",
    "        MSG_SC_RESPAWN,             // server->client",
    "        MSG_SC_LIGHTGROUP,          // server->client"
) "MSG_SC_HEALTH" "Fireteam message ids"

# Existing partial workspaces may have health/lightgroup but not respawn.
Insert-AfterLineContaining $msgIds "MSG_SC_HEALTH" @(
    "        MSG_SC_RESPAWN,             // server->client"
) "MSG_SC_RESPAWN" "respawn message id"

# ---------------------------------------------------------------------------
# NOLF2-style LightGroup compatibility.
# ---------------------------------------------------------------------------
$lightGroupH = @'
#ifndef __FIRETEAM_LIGHTGROUP_H__
#define __FIRETEAM_LIGHTGROUP_H__

#include <ltengineobjects.h>

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
            // Repeat because initial light messages can arrive before the client world is ready.
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
Write-Host "[OK] LightGroup server compatibility"

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
Write-Host "[OK] LightGroup client compatibility"

# ---------------------------------------------------------------------------
# Texture-free health HUD.
# ---------------------------------------------------------------------------
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

# ---------------------------------------------------------------------------
# Cabin Fever PoisonGas compatibility.
# ---------------------------------------------------------------------------
$poisonH = @'
#ifndef __FIRETEAM_POISON_GAS_H__
#define __FIRETEAM_POISON_GAS_H__

#include <ltengineobjects.h>

class PoisonGas : public BaseClass
{
public:
    PoisonGas();

    float GetDamage() const { return m_fDamage; }

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);

private:
    void ReadProps(ObjectCreateStruct *pOCS);

    float m_fDamage;
    bool  m_bHidden;
};

#endif
'@

$poisonCpp = @'
#include "FireteamPoisonGas.h"

#include "serverinterfaces.h"

#include <ltobjectcreate.h>
#include <string.h>

BEGIN_CLASS(PoisonGas)
    ADD_BOOLPROP(Hidden, LTFALSE)
    ADD_REALPROP(Viscosity, 0.0f)
    ADD_REALPROP(Friction, 1.0f)
    ADD_VECTORPROP_VAL(Current, 0.0f, 0.0f, 0.0f)
    ADD_REALPROP(Damage, 5.0f)
    ADD_STRINGPROP(DamageType, "POISON")
    ADD_COLORPROP(TintColor, 255.0f, 255.0f, 76.7f)
    ADD_COLORPROP(LightAdd, 0.0f, 0.0f, 0.0f)
    ADD_STRINGPROP(SoundFilter, "UnFiltered")
    ADD_BOOLPROP(CanPlayMovementSounds, LTTRUE)
    ADD_BOOLPROP(FogEnable, LTFALSE)
    ADD_REALPROP(FogFarZ, 300.0f)
    ADD_REALPROP(FogNearZ, -100.0f)
    ADD_COLORPROP(FogColor, 0.0f, 0.0f, 0.0f)
    ADD_STRINGPROP(SurfaceOverride, "Unknown")
    ADD_BOOLPROP(RayHit, LTFALSE)
    ADD_STRINGPROP(PhysicsModel, "Normal")
END_CLASS_DEFAULT_FLAGS(PoisonGas, BaseClass, LTNULL, LTNULL, CF_WORLDMODEL)

PoisonGas::PoisonGas() :
    m_fDamage(5.0f),
    m_bHidden(false)
{
}

void PoisonGas::ReadProps(ObjectCreateStruct *pOCS)
{
    GenericProp prop;

    if(g_pLTServer->GetPropGeneric("Damage", &prop) == LT_OK)
    {
        m_fDamage = prop.m_Float;
    }

    if(g_pLTServer->GetPropGeneric("Hidden", &prop) == LT_OK)
    {
        m_bHidden = (prop.m_Bool != LTFALSE);
    }

    if(g_pLTServer->GetPropGeneric("Name", &prop) == LT_OK)
    {
        strncpy(pOCS->m_Name, prop.m_String, sizeof(pOCS->m_Name) - 1);
        pOCS->m_Name[sizeof(pOCS->m_Name) - 1] = '\0';
    }

    pOCS->m_ObjectType = OT_CONTAINER;
    pOCS->m_Flags |= FLAG_CONTAINER | FLAG_TOUCH_NOTIFY | FLAG_GOTHRUWORLD | FLAG_FORCECLIENTUPDATE;
    pOCS->m_ContainerCode = 240;

    // World-model containers use their object name as the compiled brush filename.
    strncpy(pOCS->m_Filename, pOCS->m_Name, MAX_CS_FILENAME_LEN - 1);
    pOCS->m_Filename[MAX_CS_FILENAME_LEN - 1] = '\0';

    if(m_bHidden)
    {
        pOCS->m_Flags &= ~FLAG_VISIBLE;
    }
}

uint32 PoisonGas::EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData)
{
    if(messageID == MID_PRECREATE)
    {
        ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;
        if(pOCS && fData == PRECREATE_WORLDFILE)
        {
            ReadProps(pOCS);
        }
    }

    return BaseClass::EngineMessageFn(messageID, pData, fData);
}
'@

Write-Source (Join-Path $sealRoot "sshell\src\FireteamPoisonGas.h") $poisonH
Write-Source (Join-Path $sealRoot "sshell\src\FireteamPoisonGas.cpp") $poisonCpp
Write-Host "[OK] PoisonGas container compatibility"

# ---------------------------------------------------------------------------
# Controlled first zombie + Spawner bring-up.
# Only Cabin Fever's Spawner_02_01 auto-spawns ONE placeholder zombie.
# This deliberately prevents the old all-spawners-at-once crash.
# ---------------------------------------------------------------------------
$zombieH = @'
#ifndef __FIRETEAM_ZOMBIE_H__
#define __FIRETEAM_ZOMBIE_H__

#include <ltengineobjects.h>

class FireteamZombie : public BaseClass
{
public:
    FireteamZombie();

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);
    uint32 ObjectMessageFn(HOBJECT hSender, ILTMessage_Read *pMsg);

private:
    HOBJECT FindNearestPlayer();
    void UpdateZombie();

    uint8 m_nHealth;
    float m_fAttackCooldown;
};

#endif
'@

$zombieCpp = @'
#include "FireteamZombie.h"

#include "playersrvr.h"
#include "serverinterfaces.h"
#include "msgids.h"

#include <iltmodel.h>
#include <iltphysics.h>
#include <ltobjectcreate.h>
#include <float.h>
#include <string.h>

BEGIN_CLASS(FireteamZombie)
END_CLASS_DEFAULT_FLAGS(FireteamZombie, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)

FireteamZombie::FireteamZombie() :
    m_nHealth(40),
    m_fAttackCooldown(0.0f)
{
}

HOBJECT FireteamZombie::FindNearestPlayer()
{
    HCLASS hPlayerClass = g_pLTServer->GetClass("CPlayerSrvr");
    if(!hPlayerClass)
    {
        return LTNULL;
    }

    LTVector vMyPos;
    g_pLTServer->GetObjectPos(m_hObject, &vMyPos);

    HOBJECT hBest = LTNULL;
    float fBestDist = FLT_MAX;

    for(HOBJECT hObj = g_pLTServer->GetNextObject(LTNULL);
        hObj;
        hObj = g_pLTServer->GetNextObject(hObj))
    {
        HCLASS hClass = g_pLTServer->GetObjectClass(hObj);
        if(!hClass || !g_pLTServer->IsKindOf(hPlayerClass, hClass))
        {
            continue;
        }

        CPlayerSrvr *pPlayer = (CPlayerSrvr*)g_pLTServer->HandleToObject(hObj);
        if(!pPlayer || !pPlayer->IsAlive())
        {
            continue;
        }

        LTVector vPlayerPos;
        g_pLTServer->GetObjectPos(hObj, &vPlayerPos);
        float fDist = vMyPos.DistSqr(vPlayerPos);

        if(fDist < fBestDist)
        {
            fBestDist = fDist;
            hBest = hObj;
        }
    }

    return hBest;
}

void FireteamZombie::UpdateZombie()
{
    const float kUpdate = 0.10f;
    const float kMoveSpeed = 95.0f;
    const float kAttackRange = 55.0f;

    if(m_fAttackCooldown > 0.0f)
    {
        m_fAttackCooldown -= kUpdate;
    }

    HOBJECT hTarget = FindNearestPlayer();
    if(!hTarget)
    {
        LTVector vStop(0.0f, 0.0f, 0.0f);
        g_pLTSPhysics->SetVelocity(m_hObject, &vStop);
        return;
    }

    LTVector vPos;
    LTVector vTarget;
    g_pLTServer->GetObjectPos(m_hObject, &vPos);
    g_pLTServer->GetObjectPos(hTarget, &vTarget);

    LTVector vToTarget = vTarget - vPos;
    vToTarget.y = 0.0f;
    float fDistance = vToTarget.Mag();

    if(fDistance > 1.0f)
    {
        vToTarget.Normalize();

        LTRotation rLook(vToTarget, LTVector(0.0f, 1.0f, 0.0f));
        g_pLTServer->SetObjectRotation(m_hObject, &rLook);
    }

    if(fDistance > kAttackRange)
    {
        LTVector vVelocity = vToTarget * kMoveSpeed;
        vVelocity.y = -80.0f;
        g_pLTSPhysics->SetVelocity(m_hObject, &vVelocity);
    }
    else
    {
        LTVector vStop(0.0f, 0.0f, 0.0f);
        g_pLTSPhysics->SetVelocity(m_hObject, &vStop);

        if(m_fAttackCooldown <= 0.0f)
        {
            CPlayerSrvr *pPlayer = (CPlayerSrvr*)g_pLTServer->HandleToObject(hTarget);
            if(pPlayer)
            {
                pPlayer->ApplyDamage(10);
            }
            m_fAttackCooldown = 1.0f;
        }
    }
}

uint32 FireteamZombie::EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData)
{
    switch(messageID)
    {
        case MID_PRECREATE:
        {
            ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;
            if(pOCS)
            {
                pOCS->m_ObjectType = OT_MODEL;
                pOCS->m_Flags |= FLAG_SOLID | FLAG_VISIBLE | FLAG_GRAVITY |
                                 FLAG_YROTATION | FLAG_FORCECLIENTUPDATE | FLAG_SHADOW;
                pOCS->m_Flags2 |= FLAG2_PLAYERCOLLIDE;

                strncpy(pOCS->m_Filenames[0], "Models\\HARMGuard.ltb", MAX_CS_FILENAME_LEN - 1);
                strncpy(pOCS->m_Filenames[1], "Models\\playerbase.ltb", MAX_CS_FILENAME_LEN - 1);
                strncpy(pOCS->m_SkinNames[0], "ModelTextures\\HARMPurple.dtx", MAX_CS_FILENAME_LEN - 1);
                strncpy(pOCS->m_SkinNames[1], "ModelTextures\\HARMHeadW1.dtx", MAX_CS_FILENAME_LEN - 1);
            }
        }
        break;

        case MID_INITIALUPDATE:
        {
            HMODELANIM hAnim = g_pLTServer->GetAnimIndex(m_hObject, "LRF");
            if(hAnim != INVALID_MODEL_ANIM)
            {
                g_pLTSModel->SetCurAnim(m_hObject, MAIN_TRACKER, hAnim);
                g_pLTSModel->SetLooping(m_hObject, MAIN_TRACKER, LTTRUE);

                LTVector vDims;
                if(g_pLTSCommon->GetModelAnimUserDims(m_hObject, &vDims, hAnim) == LT_OK)
                {
                    g_pLTSPhysics->SetObjectDims(m_hObject, &vDims, 0);
                }
            }

            g_pLTServer->SetNextUpdate(m_hObject, 0.10f);
        }
        break;

        case MID_UPDATE:
        {
            UpdateZombie();
            g_pLTServer->SetNextUpdate(m_hObject, 0.10f);
        }
        break;

        default:
            break;
    }

    return BaseClass::EngineMessageFn(messageID, pData, fData);
}

uint32 FireteamZombie::ObjectMessageFn(HOBJECT hSender, ILTMessage_Read *pMsg)
{
    pMsg->SeekTo(0);
    uint32 messageID = pMsg->Readuint32();

    if(messageID == OBJ_MID_DAMAGE)
    {
        uint8 nDamage = pMsg->Readuint8();
        m_nHealth = (nDamage >= m_nHealth) ? 0 : (uint8)(m_nHealth - nDamage);

        if(m_nHealth == 0)
        {
            g_pLTServer->CPrint("Fireteam: placeholder zombie killed.");
            g_pLTServer->RemoveObject(m_hObject);
            return 1;
        }
    }

    return BaseClass::ObjectMessageFn(hSender, pMsg);
}
'@

Write-Source (Join-Path $sealRoot "sshell\src\FireteamZombie.h") $zombieH
Write-Source (Join-Path $sealRoot "sshell\src\FireteamZombie.cpp") $zombieCpp

$spawnerH = @'
#ifndef __FIRETEAM_SPAWNER_H__
#define __FIRETEAM_SPAWNER_H__

#include <ltengineobjects.h>

class Spawner : public BaseClass
{
public:
    Spawner();

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);

private:
    void ReadProps(ObjectCreateStruct *pOCS);
    void SpawnTestZombie();

    char m_sName[64];
    bool m_bSpawned;
};

#endif
'@

$spawnerCpp = @'
#include "FireteamSpawner.h"
#include "FireteamZombie.h"

#include "serverinterfaces.h"

#include <ltobjectcreate.h>
#include <string.h>

BEGIN_CLASS(Spawner)
    ADD_STRINGPROP(DefaultSpawn, "")
    ADD_STRINGPROP(Target, "")
    ADD_STRINGPROP(SpawnSound, "")
    ADD_REALPROP(SoundRadius, 500.0f)
    ADD_STRINGPROP(InitialCommand, "")
END_CLASS_DEFAULT_FLAGS(Spawner, BaseClass, LTNULL, LTNULL, 0)

Spawner::Spawner() :
    m_bSpawned(false)
{
    m_sName[0] = '\0';
}

void Spawner::ReadProps(ObjectCreateStruct *pOCS)
{
    GenericProp prop;
    if(g_pLTServer->GetPropGeneric("Name", &prop) == LT_OK)
    {
        strncpy(m_sName, prop.m_String, sizeof(m_sName) - 1);
        m_sName[sizeof(m_sName) - 1] = '\0';

        strncpy(pOCS->m_Name, prop.m_String, sizeof(pOCS->m_Name) - 1);
        pOCS->m_Name[sizeof(pOCS->m_Name) - 1] = '\0';
    }

    pOCS->m_ObjectType = OT_NORMAL;
}

void Spawner::SpawnTestZombie()
{
    if(m_bSpawned)
    {
        return;
    }

    HCLASS hZombieClass = g_pLTServer->GetClass("FireteamZombie");
    if(!hZombieClass)
    {
        g_pLTServer->CPrint("Fireteam: FireteamZombie class not available.");
        return;
    }

    ObjectCreateStruct ocs;
    ocs.Clear();
    g_pLTServer->GetObjectPos(m_hObject, &ocs.m_Pos);
    g_pLTServer->GetObjectRotation(m_hObject, &ocs.m_Rotation);

    BaseClass *pZombie = (BaseClass*)g_pLTServer->CreateObject(hZombieClass, &ocs);
    if(pZombie)
    {
        m_bSpawned = true;
        g_pLTServer->CPrint("Fireteam: spawned ONE placeholder zombie from %s.", m_sName);
    }
}

uint32 Spawner::EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData)
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
            // Controlled bring-up: only one known Cabin Fever spawner is live.
            if(_stricmp(m_sName, "Spawner_02_01") == 0)
            {
                g_pLTServer->SetNextUpdate(m_hObject, 1.0f);
            }
        }
        break;

        case MID_UPDATE:
        {
            SpawnTestZombie();
            g_pLTServer->SetNextUpdate(m_hObject, 0.0f);
        }
        break;

        default:
            break;
    }

    return BaseClass::EngineMessageFn(messageID, pData, fData);
}
'@

Write-Source (Join-Path $sealRoot "sshell\src\FireteamSpawner.h") $spawnerH
Write-Source (Join-Path $sealRoot "sshell\src\FireteamSpawner.cpp") $spawnerCpp
Write-Host "[OK] controlled one-zombie Spawner bring-up"

# ---------------------------------------------------------------------------
# Client shell: health, respawn and LightGroups.
# ---------------------------------------------------------------------------
$clientShell = Join-Path $sealRoot "cshell\src\ltclientshell.cpp"

Insert-AfterLineContaining $clientShell '#include "chatgui.h"' @(
    '#include "FireteamLightGroupClient.h"',
    '#include "FireteamHealthHud.h"'
) '#include "FireteamLightGroupClient.h"' "Fireteam client includes"

$healthCases = @(
    "    case MSG_SC_HEALTH:",
    "        {",
    "            uint8 nHealth = pMessage->Readuint8();",
    "            uint8 nMaxHealth = pMessage->Readuint8();",
    "            FT_SetHealth(nHealth, nMaxHealth);",
    "        }",
    "        break;",
    "    case MSG_SC_RESPAWN:",
    "        {",
    "            LTVector vRespawn = pMessage->ReadLTVector();",
    "            LTRotation rRespawn = pMessage->ReadLTRotation();",
    "",
    "            m_vPlayerStartPos = vRespawn;",
    "            m_rPlayerStartRot = rRespawn;",
    "",
    "            if(m_pPlayer && m_pPlayer->GetPlayerObject())",
    "            {",
    "                g_pLTClient->SetObjectPos(m_pPlayer->GetPlayerObject(), &vRespawn);",
    "                g_pLTClient->SetObjectRotation(m_pPlayer->GetPlayerObject(), &rRespawn);",
    "",
    "                LTVector vZero(0.0f, 0.0f, 0.0f);",
    "                g_pLTCPhysics->SetVelocity(m_pPlayer->GetPlayerObject(), &vZero);",
    "            }",
    "        }",
    "        break;",
    "    case MSG_SC_LIGHTGROUP:",
    "        {",
    "            uint32 nLightGroupID = pMessage->Readuint32();",
    "            LTVector vAdjustment = pMessage->ReadLTVector();",
    "            FT_QueueLightGroup(nLightGroupID, vAdjustment);",
    "        }",
    "        break;"
)
Insert-BeforeLineContaining $clientShell "case MSG_WORLD_PROPS:" $healthCases "case MSG_SC_HEALTH:" "health/respawn/lightgroup client messages"

# Existing partial workspaces may already have health/lightgroup but not respawn.
$respawnCase = @(
    "    case MSG_SC_RESPAWN:",
    "        {",
    "            LTVector vRespawn = pMessage->ReadLTVector();",
    "            LTRotation rRespawn = pMessage->ReadLTRotation();",
    "",
    "            m_vPlayerStartPos = vRespawn;",
    "            m_rPlayerStartRot = rRespawn;",
    "",
    "            if(m_pPlayer && m_pPlayer->GetPlayerObject())",
    "            {",
    "                g_pLTClient->SetObjectPos(m_pPlayer->GetPlayerObject(), &vRespawn);",
    "                g_pLTClient->SetObjectRotation(m_pPlayer->GetPlayerObject(), &rRespawn);",
    "",
    "                LTVector vZero(0.0f, 0.0f, 0.0f);",
    "                g_pLTCPhysics->SetVelocity(m_pPlayer->GetPlayerObject(), &vZero);",
    "            }",
    "        }",
    "        break;"
)
$text = Read-Source $clientShell
if (-not $text.Contains("case MSG_SC_RESPAWN:")) {
    if ($text.Contains("case MSG_SC_LIGHTGROUP:")) {
        Insert-BeforeLineContaining $clientShell "case MSG_SC_LIGHTGROUP:" $respawnCase "case MSG_SC_RESPAWN:" "respawn client message"
    } else {
        Insert-BeforeLineContaining $clientShell "case MSG_WORLD_PROPS:" $respawnCase "case MSG_SC_RESPAWN:" "respawn client message"
    }
}

Insert-BeforeLineContaining $clientShell "// Update the world properties class" @(
    "    FT_UpdateLightGroups();",
    ""
) "FT_UpdateLightGroups();" "LightGroup client update"

Insert-BeforeLineContaining $clientShell "// Stop all the client FX" @(
    "    FT_ClearLightGroups();",
    ""
) "FT_ClearLightGroups();" "LightGroup client cleanup"

Insert-BeforeLineContaining $clientShell "// Render the gui." @(
    "    if(IsInWorld())",
    "    {",
    "        FT_RenderHealthHud();",
    "    }",
    ""
) "FT_RenderHealthHud();" "health HUD render"

Write-Host "[OK] client health/respawn/lightgroup hooks"

# ---------------------------------------------------------------------------
# Server-authoritative health, poison damage, death/respawn.
# Friendly fire is explicitly disabled.
# ---------------------------------------------------------------------------
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
        "          m_nMaxHealth(100)," + [Environment]::NewLine +
        "          m_bAlive(true)," + [Environment]::NewLine +
        "          m_fRespawnTimer(0.0f)," + [Environment]::NewLine +
        "          m_fPoisonCarry(0.0f)")
}

# Migrate partial health constructor from an earlier patch.
if ($text.Contains("m_nMaxHealth(100)") -and -not $text.Contains("m_bAlive(true)")) {
    $text = $text.Replace(
        "          m_nMaxHealth(100)",
        "          m_nMaxHealth(100)," + [Environment]::NewLine +
        "          m_bAlive(true)," + [Environment]::NewLine +
        "          m_fRespawnTimer(0.0f)," + [Environment]::NewLine +
        "          m_fPoisonCarry(0.0f)")
}

# Public player health API.
if (-not $text.Contains("ApplyDamage(uint8 nDamage)")) {
    $lines = $text -split "\r\n|\n|\r"
    $index = -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i].Contains("GetName(){")) {
            $index = $i
            break
        }
    }
    if ($index -lt 0) {
        throw "Could not locate public player accessor section."
    }

    $before = $lines[0..$index]
    $after = $lines[($index + 1)..($lines.Length - 1)]
    $newLines = @(
        "    void                ApplyDamage(uint8 nDamage);",
        "    bool                IsAlive() const { return m_bAlive; }"
    )
    $text = [string]::Join([Environment]::NewLine, @($before + $newLines + $after))
}

# SetClient should immediately sync health.
$lines = $text -split "\r\n|\n|\r"
for ($i = 0; $i -lt $lines.Length; $i++) {
    if ($lines[$i].Contains("SetClient(HCLIENT hClient)")) {
        $lines[$i] = "    void                SetClient(HCLIENT hClient){ m_hClient = hClient; SendHealth(); }"
        break
    }
}
$text = [string]::Join([Environment]::NewLine, $lines)

# Private methods.
if (-not $text.Contains("void                Respawn();")) {
    $lines = $text -split "\r\n|\n|\r"
    $index = -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i].Contains("PlaySound(int i);")) {
            $index = $i
            break
        }
    }
    if ($index -lt 0) {
        throw "Could not locate player private method insertion point."
    }

    $before = $lines[0..$index]
    $after = $lines[($index + 1)..($lines.Length - 1)]
    $newLines = @(
        "    void                SendHealth();",
        "    void                Respawn();",
        "    void                UpdateHazards();"
    )
    $text = [string]::Join([Environment]::NewLine, @($before + $newLines + $after))
}

# Avoid duplicate SendHealth declaration from partial patch.
$lines = $text -split "\r\n|\n|\r"
$seenSendHealth = $false
$out = @()
foreach($line in $lines) {
    if($line.Contains("void                SendHealth();")) {
        if($seenSendHealth) { continue }
        $seenSendHealth = $true
    }
    $out += $line
}
$text = [string]::Join([Environment]::NewLine, $out)

# Private members.
if (-not $text.Contains("m_fRespawnTimer;")) {
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
        "    // Fireteam health/death",
        "    uint8               m_nHealth;",
        "    uint8               m_nMaxHealth;",
        "    bool                m_bAlive;",
        "    float               m_fRespawnTimer;",
        "    float               m_fPoisonCarry;",
        "    LTVector            m_vSpawnPos;",
        "    LTRotation          m_rSpawnRot;"
    )
    $text = [string]::Join([Environment]::NewLine, @($before + $newMembers + $after))
}

Write-Source $playerH $text

$playerCpp = Join-Path $sealRoot "sshell\src\playersrvr.cpp"

Insert-AfterLineContaining $playerCpp '#include "statsmanager.h"' @(
    '#include "FireteamPoisonGas.h"'
) '#include "FireteamPoisonGas.h"' "PoisonGas player include"

Insert-BeforeLineContaining $playerCpp "// Set up animation trackers" @(
    "    g_pLTServer->GetObjectPos(m_hObject, &m_vSpawnPos);",
    "    g_pLTServer->GetObjectRotation(m_hObject, &m_rSpawnRot);",
    ""
) "GetObjectPos(m_hObject, &m_vSpawnPos);" "player spawn transform storage"

$healthUpdate = @(
    "            if(!m_bAlive)",
    "            {",
    "                m_fRespawnTimer -= 0.25f;",
    "                if(m_fRespawnTimer <= 0.0f)",
    "                {",
    "                    Respawn();",
    "                }",
    "            }",
    "            else",
    "            {",
    "                UpdateHazards();",
    "            }",
    ""
)
Insert-BeforeLineContaining $playerCpp "//Do we need to send score stats?" $healthUpdate "UpdateHazards();" "player health/death update"

$text = Read-Source $playerCpp

# Migrate the earlier experimental direct-health handler if it exists.
$oldPartialDamage = @"
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
if ($text.Contains($oldPartialDamage)) {
    $text = $text.Replace($oldPartialDamage, @"
        case OBJ_MID_DAMAGE:
            {
                uint8 nDamage = pMsg->Readuint8();
                ApplyDamage(nDamage);
            }
            break;
"@)
    Write-Source $playerCpp $text
}

$text = Read-Source $playerCpp
if (-not $text.Contains("case OBJ_MID_DAMAGE:")) {
    $damageCase = @(
        "        case OBJ_MID_DAMAGE:",
        "            {",
        "                uint8 nDamage = pMsg->Readuint8();",
        "                ApplyDamage(nDamage);",
        "            }",
        "            break;"
    )
    Insert-BeforeLineContaining $playerCpp "case OBJ_MID_PICKUP:" $damageCase "case OBJ_MID_DAMAGE:" "player damage handler"
}

# If an old PvP experiment exists, remove it before enforcing co-op-only damage.
$text = Read-Source $playerCpp
$oldPvp = @"
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
"@
if ($text.Contains($oldPvp)) {
    $text = $text.Replace($oldPvp, "")
}
$text = [regex]::Replace(
    $text,
    '(?m)^[ \t]*HCLASS hClassPlayer = g_pLTServer->GetClass\("CPlayerSrvr"\);[ \t]*\r?\n?',
    '')
Write-Source $playerCpp $text

Insert-AfterLineContaining $playerCpp 'HCLASS hClassSnowman = g_pLTServer->GetClass("Snowman");' @(
    '                HCLASS hClassZombie = g_pLTServer->GetClass("FireteamZombie");'
) 'GetClass("FireteamZombie")' "zombie melee target class"

$text = Read-Source $playerCpp
if (-not $text.Contains("Fireteam placeholder zombie melee damage.")) {
    if (-not $text.Contains("PlaySound(3);")) {
        throw "Could not locate the generic SealHunter melee fallback."
    }

    $zombieDamage = 'if(hClassZombie && g_pLTServer->IsKindOf(hClassZombie, hTarget))' + [Environment]::NewLine +
        '                    {' + [Environment]::NewLine +
        '                        // Fireteam placeholder zombie melee damage.' + [Environment]::NewLine +
        '                        ILTMessage_Write *pMsg;' + [Environment]::NewLine +
        '                        g_pLTSCommon->CreateMessage(pMsg);' + [Environment]::NewLine +
        '                        pMsg->IncRef();' + [Environment]::NewLine +
        '                        pMsg->Writeuint32(OBJ_MID_DAMAGE);' + [Environment]::NewLine +
        '                        pMsg->Writeuint8(20);' + [Environment]::NewLine +
        '                        g_pLTServer->SendToObject(pMsg->Read(), m_hObject, iInfo.m_hObject, 0);' + [Environment]::NewLine +
        '                        pMsg->DecRef();' + [Environment]::NewLine +
        '                    }' + [Environment]::NewLine +
        '                    else' + [Environment]::NewLine +
        '                    {' + [Environment]::NewLine +
        '                        // Co-op mode: friendly fire is permanently disabled.' + [Environment]::NewLine +
        '                    }'

    $text = $text.Replace("PlaySound(3);", $zombieDamage)
    Write-Source $playerCpp $text
}
Write-Host "[OK] Bowie damages zombies only; friendly fire disabled"

$text = Read-Source $playerCpp

if (-not $text.Contains("void CPlayerSrvr::ApplyDamage(uint8 nDamage)")) {
    $text += @"

//-----------------------------------------------------------------------------
// Fireteam player health/death/respawn.
//-----------------------------------------------------------------------------
void CPlayerSrvr::ApplyDamage(uint8 nDamage)
{
    if(!m_bAlive || nDamage == 0)
    {
        return;
    }

    m_nHealth = (nDamage >= m_nHealth) ? 0 : (uint8)(m_nHealth - nDamage);
    SendHealth();

    if(m_nHealth == 0)
    {
        m_bAlive = false;
        m_fRespawnTimer = 2.0f;

        LTVector vZero(0.0f, 0.0f, 0.0f);
        g_pLTSPhysics->SetVelocity(m_hObject, &vZero);

        g_pLTServer->CPrint("Fireteam: %s died. Respawning in 2 seconds.", m_sName);
    }
}

void CPlayerSrvr::UpdateHazards()
{
    HOBJECT aContainers[16];
    uint32 nContainers = g_pLTServer->GetObjectContainers(m_hObject, aContainers, 16);
    HCLASS hPoisonClass = g_pLTServer->GetClass("PoisonGas");

    bool bInPoison = false;

    for(uint32 i = 0; i < nContainers; ++i)
    {
        HCLASS hClass = g_pLTServer->GetObjectClass(aContainers[i]);
        if(!hPoisonClass || !hClass || !g_pLTServer->IsKindOf(hPoisonClass, hClass))
        {
            continue;
        }

        PoisonGas *pGas = (PoisonGas*)g_pLTServer->HandleToObject(aContainers[i]);
        if(!pGas)
        {
            continue;
        }

        bInPoison = true;
        m_fPoisonCarry += pGas->GetDamage() * 0.25f;
    }

    if(!bInPoison)
    {
        m_fPoisonCarry = 0.0f;
        return;
    }

    if(m_fPoisonCarry >= 1.0f)
    {
        uint8 nDamage = (uint8)m_fPoisonCarry;
        m_fPoisonCarry -= (float)nDamage;
        ApplyDamage(nDamage);
    }
}

void CPlayerSrvr::Respawn()
{
    m_bAlive = true;
    m_nHealth = m_nMaxHealth;
    m_fRespawnTimer = 0.0f;
    m_fPoisonCarry = 0.0f;

    g_pLTServer->TeleportObject(m_hObject, &m_vSpawnPos);
    g_pLTServer->SetObjectRotation(m_hObject, &m_rSpawnRot);

    LTVector vZero(0.0f, 0.0f, 0.0f);
    g_pLTSPhysics->SetVelocity(m_hObject, &vZero);

    if(m_hClient)
    {
        ILTMessage_Write *pMsg = LTNULL;
        if(g_pLTSCommon->CreateMessage(pMsg) == LT_OK && pMsg)
        {
            pMsg->IncRef();
            pMsg->Writeuint8(MSG_SC_RESPAWN);
            pMsg->WriteLTVector(m_vSpawnPos);
            pMsg->WriteLTRotation(m_rSpawnRot);
            g_pLTServer->SendToClient(pMsg->Read(), m_hClient, MESSAGE_GUARANTEED);
            pMsg->DecRef();
        }
    }

    SendHealth();
}

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

# Existing partial patch may already have SendHealth at EOF. Remove duplicate old function if needed.
$firstSend = $text.IndexOf("void CPlayerSrvr::SendHealth()")
if ($firstSend -ge 0) {
    $secondSend = $text.IndexOf("void CPlayerSrvr::SendHealth()", $firstSend + 1)
    if ($secondSend -ge 0) {
        # Keep the newer final implementation; remove the earlier function block.
        $brace = $text.IndexOf("{", $firstSend)
        $depth = 0
        $end = -1
        for($i = $brace; $i -lt $text.Length; $i++) {
            if($text[$i] -eq '{') { $depth++ }
            elseif($text[$i] -eq '}') {
                $depth--
                if($depth -eq 0) {
                    $end = $i + 1
                    break
                }
            }
        }
        if($end -gt $firstSend) {
            $text = $text.Remove($firstSend, $end - $firstSend)
        }
    }
}

Write-Source $playerCpp $text
Write-Host "[OK] health, poison, respawn and no-friendly-fire server logic"

Write-Host "[OK] Fireteam systems patch complete."
