param(
    [string]$LocalRoot = (Join-Path $PSScriptRoot '..\.local')
)

$ErrorActionPreference = 'Stop'

function Read-Source([string]$Path) {
    return [IO.File]::ReadAllText($Path)
}

function Write-Source([string]$Path, [string]$Text) {
    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }

    [IO.File]::WriteAllText(
        $Path,
        $Text,
        [Text.Encoding]::ASCII)
}

function Insert-AfterLineContaining(
    [string]$Text,
    [string]$Needle,
    [string[]]$Lines,
    [string]$Guard)
{
    if($Guard -and $Text.Contains($Guard)) {
        return $Text
    }

    $all = $Text -split "\r\n|\n|\r"
    $index = -1

    for($i = 0; $i -lt $all.Length; ++$i) {
        if($all[$i].Contains($Needle)) {
            $index = $i
            break
        }
    }

    if($index -lt 0) {
        throw "Could not locate insertion anchor: $Needle"
    }

    $before = $all[0..$index]
    $after = @()

    if(($index + 1) -lt $all.Length) {
        $after = $all[($index + 1)..($all.Length - 1)]
    }

    return [string]::Join(
        [Environment]::NewLine,
        @($before + $Lines + $after))
}

function Insert-BeforeLineContaining(
    [string]$Text,
    [string]$Needle,
    [string[]]$Lines,
    [string]$Guard)
{
    if($Guard -and $Text.Contains($Guard)) {
        return $Text
    }

    $all = $Text -split "\r\n|\n|\r"
    $index = -1

    for($i = 0; $i -lt $all.Length; ++$i) {
        if($all[$i].Contains($Needle)) {
            $index = $i
            break
        }
    }

    if($index -lt 0) {
        throw "Could not locate insertion anchor: $Needle"
    }

    $before = @()
    if($index -gt 0) {
        $before = $all[0..($index - 1)]
    }

    $after = $all[$index..($all.Length - 1)]

    return [string]::Join(
        [Environment]::NewLine,
        @($before + $Lines + $after))
}

function Replace-CppFunctionBody(
    [string]$Text,
    [string]$Signature,
    [string]$NewBody)
{
    $start = $Text.IndexOf($Signature)
    if($start -lt 0) {
        throw "Could not locate C++ function: $Signature"
    }

    $braceStart = $Text.IndexOf("{", $start)
    if($braceStart -lt 0) {
        throw "Could not locate opening brace for: $Signature"
    }

    $depth = 0
    $braceEnd = -1

    for($i = $braceStart; $i -lt $Text.Length; ++$i) {
        if($Text[$i] -eq '{') {
            ++$depth
        }
        elseif($Text[$i] -eq '}') {
            --$depth
            if($depth -eq 0) {
                $braceEnd = $i
                break
            }
        }
    }

    if($braceEnd -lt 0) {
        throw "Could not locate closing brace for: $Signature"
    }

    return $Text.Substring(0, $braceStart) +
        $NewBody +
        $Text.Substring($braceEnd + 1)
}

$sealRoot = Join-Path $LocalRoot 'imports\sealhunter'

$playerH = Join-Path $sealRoot 'cshell\src\playerclnt.h'
$playerCpp = Join-Path $sealRoot 'cshell\src\playerclnt.cpp'
$clientShell = Join-Path $sealRoot 'cshell\src\ltclientshell.cpp'
$msgIds = Join-Path $sealRoot 'shared\src\msgids.h'
$serverH = Join-Path $sealRoot 'sshell\src\playersrvr.h'
$serverCpp = Join-Path $sealRoot 'sshell\src\playersrvr.cpp'

# ---------------------------------------------------------------------------
# NOLF2-style Fireteam crosshair + ammo HUD.
#
# The crosshair follows NOLF2's HUDCrosshair approach: screen-center geometry
# rendered as untextured DrawPrim posts. Ammo follows NOLF2 HUDAmmo's clip /
# reserve text convention.
# ---------------------------------------------------------------------------
$weaponHudH = @'
#ifndef __FIRETEAM_WEAPON_HUD_H__
#define __FIRETEAM_WEAPON_HUD_H__

#include <ltbasedefs.h>

void FT_WeaponHudInit();
void FT_WeaponHudTerm();
void FT_SetPrimaryAmmo(uint16 nClip, uint16 nReserve);
bool FT_ConsumePrimaryAmmoLocal();
void FT_RenderWeaponHud(uint8 nWeaponSlot, bool bFirstPerson);

#endif
'@

$weaponHudCpp = @'
#include "FireteamWeaponHud.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <iltdrawprim.h>
#include <iltfontmanager.h>
#include <stdio.h>

static CUIFont *s_pAmmoFont = LTNULL;
static CUIFormattedPolyString *s_pAmmoText = LTNULL;

static uint16 s_nPrimaryClip = 30;
static uint16 s_nPrimaryReserve = 90;

static void FT_SetupQuad(
    LT_POLYF4 &poly,
    float x,
    float y,
    float width,
    float height,
    uint8 r,
    uint8 g,
    uint8 b,
    uint8 a)
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

static void FT_SetDrawState()
{
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
}

void FT_WeaponHudInit()
{
    if(s_pAmmoFont)
    {
        return;
    }

    s_pAmmoFont = g_pLTCFontManager->CreateFont(
        "fonts/SQR721B.TTF",
        "Square721 BT",
        22,
        36,
        255);

    if(!s_pAmmoFont)
    {
        g_pLTClient->CPrint("Fireteam: failed to create weapon HUD font.");
        return;
    }

    s_pAmmoFont->SetDefCharWidth(6);
    s_pAmmoFont->SetDefColor(0xFFFFFFFF);

    s_pAmmoText = g_pLTCFontManager->CreateFormattedPolyString(
        s_pAmmoFont,
        "30/90");

    if(s_pAmmoText)
    {
        s_pAmmoText->SetColor(0xFFFFFFFF);
    }
}

void FT_WeaponHudTerm()
{
    if(s_pAmmoText)
    {
        g_pLTCFontManager->DestroyPolyString(s_pAmmoText);
        s_pAmmoText = LTNULL;
    }

    if(s_pAmmoFont)
    {
        g_pLTCFontManager->DestroyFont(s_pAmmoFont);
        s_pAmmoFont = LTNULL;
    }
}

void FT_SetPrimaryAmmo(uint16 nClip, uint16 nReserve)
{
    s_nPrimaryClip = nClip;
    s_nPrimaryReserve = nReserve;
}

bool FT_ConsumePrimaryAmmoLocal()
{
    if(s_nPrimaryClip == 0)
    {
        return false;
    }

    --s_nPrimaryClip;
    return true;
}

void FT_RenderWeaponHud(uint8 nWeaponSlot, bool bFirstPerson)
{
    if(!bFirstPerson || nWeaponSlot != 1 ||
       !g_pLTClient || !g_pLTCDrawPrim)
    {
        return;
    }

    uint32 nScreenW = 0;
    uint32 nScreenH = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nScreenW,
        &nScreenH);

    const float cx = (float)nScreenW * 0.5f;
    const float cy = (float)nScreenH * 0.5f;

    // Ported from NOLF2's DrawPrim crosshair construction: four posts
    // surrounding a center gap.
    const float fGap = 6.0f;
    const float fLength = 10.0f;
    const float fThickness = 2.0f;

    LT_POLYF4 crosshair[4];

    FT_SetupQuad(
        crosshair[0],
        cx - fGap - fLength,
        cy - (fThickness * 0.5f),
        fLength,
        fThickness,
        0, 255, 255, 235);

    FT_SetupQuad(
        crosshair[1],
        cx + fGap,
        cy - (fThickness * 0.5f),
        fLength,
        fThickness,
        0, 255, 255, 235);

    FT_SetupQuad(
        crosshair[2],
        cx - (fThickness * 0.5f),
        cy - fGap - fLength,
        fThickness,
        fLength,
        0, 255, 255, 235);

    FT_SetupQuad(
        crosshair[3],
        cx - (fThickness * 0.5f),
        cy + fGap,
        fThickness,
        fLength,
        0, 255, 255, 235);

    FT_SetDrawState();

    g_pLTCDrawPrim->BeginDrawPrim();
    g_pLTCDrawPrim->DrawPrim(crosshair, 4);
    g_pLTCDrawPrim->EndDrawPrim();

    if(!s_pAmmoText)
    {
        FT_WeaponHudInit();
    }

    if(s_pAmmoText)
    {
        char szAmmo[32];

        // NOLF2 HUDAmmo convention: clip / reserve.
        sprintf(
            szAmmo,
            "%u/%u",
            (uint32)s_nPrimaryClip,
            (uint32)s_nPrimaryReserve);

        s_pAmmoText->SetText(szAmmo);
        s_pAmmoText->SetPosition(
            (float)nScreenW - 118.0f,
            (float)nScreenH - 72.0f);
        s_pAmmoText->Render();
    }
}
'@

Write-Source (Join-Path $sealRoot 'cshell\src\FireteamWeaponHud.h') $weaponHudH
Write-Source (Join-Path $sealRoot 'cshell\src\FireteamWeaponHud.cpp') $weaponHudCpp

# ---------------------------------------------------------------------------
# Shared server -> client ammo sync message.
# ---------------------------------------------------------------------------
$text = Read-Source $msgIds

if(-not $text.Contains('MSG_SC_AMMO'))
{
    $text = Insert-AfterLineContaining $text 'MSG_SC_LIGHTGROUP' @(
        '        MSG_SC_AMMO,                // server->client'
    ) ''
}

Write-Source $msgIds $text

# ---------------------------------------------------------------------------
# Client: final AK firing behavior + HUD network/render hooks.
# ---------------------------------------------------------------------------
$text = Read-Source $playerCpp

if(-not $text.Contains('#include "FireteamWeaponHud.h"'))
{
    $anchor = '#include <iltsoundmgr.h>'
    if(-not $text.Contains($anchor))
    {
        throw "Could not locate player client include anchor for weapon HUD."
    }

    $text = $text.Replace(
        $anchor,
        '#include "FireteamWeaponHud.h"' +
        [Environment]::NewLine +
        $anchor)
}

$attackBody = @'
{
    if(m_nWeaponSlot == 1)
    {
        static float s_fNextPrimaryClientShot = 0.0f;

        float fNow = g_pLTClient->GetTime();
        if(fNow < s_fNextPrimaryClientShot)
        {
            return false;
        }

        if(!FT_ConsumePrimaryAmmoLocal())
        {
            return false;
        }

        s_fNextPrimaryClientShot = fNow + 0.10f;

        PlayAttackAnimation("UMFi", m_idUpperBodyTracker);

        HMODELANIM hFire = INVALID_MODEL_ANIM;
        if(m_hViewWeaponObject)
        {
            hFire = g_pLTClient->GetAnimIndex(
                m_hViewWeaponObject,
                (char*)"fire_0");
        }

        PlayViewWeaponAnimation(
            hFire != INVALID_MODEL_ANIM ? "fire_0" : "fire",
            false);

        PlayViewWeaponSound("FIRE.WAV");
        m_bViewWeaponAction = true;
        return true;
    }

    if(m_bAttacking)
    {
        return false;
    }

    PlayAttackAnimation("UMFi", m_idUpperBodyTracker);
    m_bAttacking = true;

    PlayViewWeaponAnimation("fire_0", false);
    PlayViewWeaponSound("FIRE.WAV");
    m_bViewWeaponAction = true;

    return true;
}
'@

$text = Replace-CppFunctionBody $text 'bool CPlayerClnt::Attack()' $attackBody
Write-Source $playerCpp $text

$text = Read-Source $clientShell

if(-not $text.Contains('#include "FireteamWeaponHud.h"'))
{
    if($text.Contains('#include "FireteamHealthHud.h"'))
    {
        $text = $text.Replace(
            '#include "FireteamHealthHud.h"',
            '#include "FireteamHealthHud.h"' +
            [Environment]::NewLine +
            '#include "FireteamWeaponHud.h"')
    }
    elseif($text.Contains('#include "chatgui.h"'))
    {
        $text = $text.Replace(
            '#include "chatgui.h"',
            '#include "chatgui.h"' +
            [Environment]::NewLine +
            '#include "FireteamWeaponHud.h"')
    }
    else
    {
        throw "Could not locate client shell include anchor for weapon HUD."
    }
}

if(-not $text.Contains('FT_WeaponHudInit();'))
{
    $anchor = 'FT_SettingsInit();'
    if(-not $text.Contains($anchor))
    {
        throw "Could not locate Fireteam settings initialization."
    }

    $text = $text.Replace(
        $anchor,
        $anchor +
        [Environment]::NewLine +
        '    FT_WeaponHudInit();')
}

if(-not $text.Contains('FT_WeaponHudTerm();'))
{
    $anchor = 'FT_SettingsTerm();'
    if(-not $text.Contains($anchor))
    {
        throw "Could not locate Fireteam settings termination."
    }

    $text = $text.Replace(
        $anchor,
        'FT_WeaponHudTerm();' +
        [Environment]::NewLine +
        '    ' +
        $anchor)
}

if(-not $text.Contains('case MSG_SC_AMMO:'))
{
    $ammoCase = @(
        '    case MSG_SC_AMMO:',
        '        {',
        '            uint16 nClip = pMessage->Readuint16();',
        '            uint16 nReserve = pMessage->Readuint16();',
        '            FT_SetPrimaryAmmo(nClip, nReserve);',
        '        }',
        '        break;'
    )

    $text = Insert-BeforeLineContaining(
        $text,
        'case MSG_WORLD_PROPS:',
        $ammoCase,
        'case MSG_SC_AMMO:')
}

if(-not $text.Contains('FT_RenderWeaponHud('))
{
    $anchor = '        FT_RenderHealthHud();'
    if(-not $text.Contains($anchor))
    {
        throw "Could not locate health HUD render hook."
    }

    $render = @"
        FT_RenderHealthHud();

        if(m_pPlayer &&
           m_pCamera &&
           !FT_SettingsIsOpen())
        {
            FT_RenderWeaponHud(
                m_pPlayer->GetWeaponSlot(),
                m_pCamera->IsFirstPerson());
        }
"@

    $text = $text.Replace($anchor, $render)
}

Write-Source $clientShell $text

# ---------------------------------------------------------------------------
# Server: Combat Arms AK-47 ammo, range, falloff and shot diagnostics.
# Weapon12 / Ammo1:
#   30 rounds/clip, 120 selection amount -> 30/90 start
#   InstDamage 48
#   Effect ranges 2500 / 3000 / 3500
#   multipliers 1.00 / 0.70 / 0.35
# ---------------------------------------------------------------------------
$text = Read-Source $serverH

if(-not $text.Contains('void                SendPrimaryAmmo();'))
{
    $text = Insert-AfterLineContaining $text 'FirePrimary(const LTVector' @(
        '    void                SendPrimaryAmmo();'
    ) ''
}

if(-not [regex]::IsMatch(
    $text,
    '(?m)^\s*uint16\s+m_nPrimaryAmmoInClip\s*;\s*$'))
{
    $text = Insert-AfterLineContaining $text 'm_fNextPrimaryShot;' @(
        '    uint16              m_nPrimaryAmmoInClip;',
        '    uint16              m_nPrimaryAmmoReserve;'
    ) ''
}

if(-not [regex]::IsMatch(
    $text,
    '(?m)^\s*uint16\s+m_nPrimaryAmmoInClip\s*;\s*$') -or
   -not [regex]::IsMatch(
    $text,
    '(?m)^\s*uint16\s+m_nPrimaryAmmoReserve\s*;\s*$'))
{
    throw "Could not add Fireteam primary ammo members."
}

Write-Source $serverH $text

$text = Read-Source $serverCpp

if(-not $text.Contains('m_nPrimaryAmmoInClip(30)'))
{
    $anchor = 'm_fNextPrimaryShot(0.0f)'
    if(-not $text.Contains($anchor))
    {
        throw "Could not locate primary-shot constructor initializer."
    }

    $text = $text.Replace(
        $anchor,
        $anchor + ',' +
        [Environment]::NewLine +
        '          m_nPrimaryAmmoInClip(30),' +
        [Environment]::NewLine +
        '          m_nPrimaryAmmoReserve(90)')
}

$setWeaponBody = @'
{
    if(nSlot != 1 && nSlot != 3)
    {
        return;
    }

    m_nWeaponSlot = nSlot;

    g_pLTServer->CPrint(
        "Fireteam gun: server weapon slot=%u ammo=%u/%u",
        (uint32)m_nWeaponSlot,
        (uint32)m_nPrimaryAmmoInClip,
        (uint32)m_nPrimaryAmmoReserve);

    SendPrimaryAmmo();

    if(!m_hClub)
    {
        return;
    }

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;

    if(m_nWeaponSlot == 1)
    {
        strncpy(
            ocs.m_Filename,
            "Weapons/primary_m_hh/HH_AK-47.LTB",
            sizeof(ocs.m_Filename) - 1);
        strncpy(
            ocs.m_SkinName,
            "Weapons/primary_t/HH_AK-47.DTX",
            sizeof(ocs.m_SkinName) - 1);
    }
    else
    {
        strncpy(
            ocs.m_Filename,
            "Weapons/melee_m_hh/HH_ML_DF_BOWIEKNIFE_CH.LTB",
            sizeof(ocs.m_Filename) - 1);
        strncpy(
            ocs.m_SkinName,
            "Weapons/melee_t/HH_ML_DF_BOWIEKNIFE_BC.DTX",
            sizeof(ocs.m_SkinName) - 1);
    }

    g_pLTSCommon->SetObjectFilenames(m_hClub, &ocs);
}
'@

$text = Replace-CppFunctionBody(
    $text,
    'void CPlayerSrvr::SetWeaponSlot(uint8 nSlot)',
    $setWeaponBody)

$firePrimaryBody = @'
{
    if(!m_bAlive || m_nWeaponSlot != 1)
    {
        return;
    }

    float fNow = g_pLTServer->GetTime();
    if(fNow < m_fNextPrimaryShot)
    {
        return;
    }

    if(m_nPrimaryAmmoInClip == 0)
    {
        g_pLTServer->CPrint(
            "Fireteam gun: AK-47 dry - reserve=%u",
            (uint32)m_nPrimaryAmmoReserve);
        SendPrimaryAmmo();
        return;
    }

    m_fNextPrimaryShot = fNow + 0.10f;
    --m_nPrimaryAmmoInClip;
    SendPrimaryAmmo();

    LTVector vDir = vDirection;
    if(vDir.MagSqr() < 0.0001f)
    {
        g_pLTServer->CPrint("Fireteam gun: rejected zero-length firing direction.");
        return;
    }

    vDir.Normalize();

    LTVector vPlayerPos;
    g_pLTServer->GetObjectPos(m_hObject, &vPlayerPos);

    LTVector vStart = vFrom;

    if(vStart.DistSqr(vPlayerPos) > (256.0f * 256.0f))
    {
        vStart = vPlayerPos;
        vStart.y += 30.0f;
    }

    IntersectQuery query;
    IntersectInfo info;

    query.m_From = vStart + (vDir * 4.0f);
    query.m_To = query.m_From + (vDir * 3500.0f);
    query.m_Flags =
        INTERSECT_OBJECTS |
        IGNORE_NONSOLID |
        INTERSECT_HPOLY;

    FTFireFilterData filterData;
    filterData.hPlayer = m_hObject;
    filterData.hWeapon = m_hClub;

    query.m_FilterFn = FTFireFilter;
    query.m_pUserData = &filterData;

    if(!g_pLTServer->IntersectSegment(&query, &info))
    {
        g_pLTServer->CPrint(
            "Fireteam gun: AK-47 no hit ammo=%u/%u",
            (uint32)m_nPrimaryAmmoInClip,
            (uint32)m_nPrimaryAmmoReserve);
        return;
    }

    float fDistance = (info.m_Point - query.m_From).Mag();

    if(!info.m_hObject)
    {
        g_pLTServer->CPrint(
            "Fireteam gun: AK-47 world hit distance=%.1f ammo=%u/%u",
            fDistance,
            (uint32)m_nPrimaryAmmoInClip,
            (uint32)m_nPrimaryAmmoReserve);
        return;
    }

    HCLASS hTarget = g_pLTServer->GetObjectClass(info.m_hObject);
    HCLASS hZombie = g_pLTServer->GetClass("FireteamZombie");
    HCLASS hSeal = g_pLTServer->GetClass("Seal");

    bool bDamage =
        (hZombie && hTarget &&
         g_pLTServer->IsKindOf(hTarget, hZombie)) ||
        (hSeal && hTarget &&
         g_pLTServer->IsKindOf(hTarget, hSeal));

    if(!bDamage)
    {
        g_pLTServer->CPrint(
            "Fireteam gun: AK-47 non-enemy hit distance=%.1f ammo=%u/%u",
            fDistance,
            (uint32)m_nPrimaryAmmoInClip,
            (uint32)m_nPrimaryAmmoReserve);
        return;
    }

    uint8 nDamage = 48;

    if(fDistance > 3000.0f)
    {
        nDamage = 17;
    }
    else if(fDistance > 2500.0f)
    {
        nDamage = 34;
    }

    ILTMessage_Write *pDamage = LTNULL;
    if(g_pLTSCommon->CreateMessage(pDamage) == LT_OK &&
       pDamage)
    {
        pDamage->IncRef();
        pDamage->Writeuint32(OBJ_MID_DAMAGE);
        pDamage->Writeuint8(nDamage);

        g_pLTServer->SendToObject(
            pDamage->Read(),
            m_hObject,
            info.m_hObject,
            0);

        pDamage->DecRef();

        g_pLTServer->CPrint(
            "Fireteam gun: AK-47 infected hit damage=%u distance=%.1f ammo=%u/%u",
            (uint32)nDamage,
            fDistance,
            (uint32)m_nPrimaryAmmoInClip,
            (uint32)m_nPrimaryAmmoReserve);
    }
}
'@

$text = Replace-CppFunctionBody(
    $text,
    'void CPlayerSrvr::FirePrimary(',
    $firePrimaryBody)

if(-not $text.Contains('void CPlayerSrvr::SendPrimaryAmmo()'))
{
    $sendAmmo = @'

void CPlayerSrvr::SendPrimaryAmmo()
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
    pMsg->Writeuint8(MSG_SC_AMMO);
    pMsg->Writeuint16(m_nPrimaryAmmoInClip);
    pMsg->Writeuint16(m_nPrimaryAmmoReserve);

    g_pLTServer->SendToClient(
        pMsg->Read(),
        m_hClient,
        MESSAGE_GUARANTEED);

    pMsg->DecRef();
}

'@

    $text += $sendAmmo
}

Write-Source $serverCpp $text

Write-Host "[OK] Fireteam AK-47 CA ammo/damage + NOLF2-style crosshair HUD"
