param(
    [string]$LocalRoot = (Join-Path $PSScriptRoot '..\.local')
)

$ErrorActionPreference = 'Stop'

function Read-Source([string]$Path) {
    return [IO.File]::ReadAllText($Path)
}

function Write-Source([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, [Text.Encoding]::ASCII)
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
    $after = if($index + 1 -lt $all.Length) {
        $all[($index + 1)..($all.Length - 1)]
    } else {
        @()
    }

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
$serverShell = Join-Path $sealRoot 'sshell\src\ltservershell.cpp'

# ---------------------------------------------------------------------------
# Client loadout API.
# ---------------------------------------------------------------------------
$text = Read-Source $playerH

$text = Insert-AfterLineContaining $text 'AltAttack();' @(
    '    bool                SelectWeaponSlot(uint8 nSlot);',
    '    void                CycleWeapon(int nDirection);',
    '    uint8               GetWeaponSlot() const { return m_nWeaponSlot; }'
) 'SelectWeaponSlot(uint8 nSlot)' 

$text = Insert-AfterLineContaining $text 'm_nViewAttackVariant;' @(
    '    uint8               m_nWeaponSlot;'
) 'm_nWeaponSlot;'

Write-Source $playerH $text

$text = Read-Source $playerCpp

if(-not $text.Contains('#include <stdio.h>')) {
    $anchor = '#include <iltsoundmgr.h>'
    if(-not $text.Contains($anchor)) {
        throw "Could not locate player client include anchor."
    }

    $text = $text.Replace(
        $anchor,
        $anchor + [Environment]::NewLine + '#include <stdio.h>')
}

if(-not $text.Contains('m_nWeaponSlot(3)')) {
    $anchor = 'm_nViewAttackVariant(0),'
    if(-not $text.Contains($anchor)) {
        throw "Could not locate view-weapon constructor initializer."
    }

    $text = $text.Replace(
        $anchor,
        $anchor + [Environment]::NewLine + 'm_nWeaponSlot(3),')
}

$attackBody = @'
{
    if(m_bAttacking)
    {
        return false;
    }

    PlayAttackAnimation("UMFi", m_idUpperBodyTracker);
    m_bAttacking = true;

    if(m_nWeaponSlot == 1)
    {
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
    }
    else
    {
        PlayViewWeaponAnimation("fire_0", false);
        PlayViewWeaponSound("FIRE.WAV");
        m_bViewWeaponAction = true;
    }

    return true;
}
'@

$text = Replace-CppFunctionBody $text 'bool CPlayerClnt::Attack()' $attackBody

$altBody = @'
{
    if(m_nWeaponSlot != 3 || m_bAttacking)
    {
        return false;
    }

    PlayAttackAnimation("UMFi", m_idUpperBodyTracker);
    m_bAttacking = true;

    PlayViewWeaponAnimation("fire_1", false);
    PlayViewWeaponSound("FIRE.WAV");
    m_bViewWeaponAction = true;

    return true;
}
'@

$text = Replace-CppFunctionBody $text 'bool CPlayerClnt::AltAttack()' $altBody

$createBody = @'
{
    if(m_hViewWeaponObject)
    {
        g_pLTClient->RemoveObject(m_hViewWeaponObject);
        m_hViewWeaponObject = NULL;
    }

    if(m_nWeaponSlot == 1)
    {
        FILE *pAK = fopen(
            "rez\\Weapons\\primary_m_pv\\AK47_PV.LTB",
            "rb");

        if(!pAK)
        {
            g_pLTClient->CPrint(
                "Fireteam: AK-47 PV asset is not staged; falling back to Bowie.");
            m_nWeaponSlot = 3;
        }
        else
        {
            fclose(pAK);
        }
    }

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;
    ocs.m_Flags = FLAG_VISIBLE | FLAG_REALLYCLOSE;
    ocs.m_Flags2 = FLAG2_DYNAMICDIRLIGHT;
    ocs.m_Pos.Init(0.0f, 0.0f, 0.0f);

    if(m_nWeaponSlot == 1)
    {
        strcpy(
            ocs.m_Filenames[0],
            "Weapons\\primary_m_pv\\AK47_PV.LTB");

        FILE *pAni = fopen(
            "rez\\Weapons\\primary_m_pv\\AK47_ANI.LTB",
            "rb");

        if(pAni)
        {
            fclose(pAni);
            strcpy(
                ocs.m_Filenames[1],
                "Weapons\\primary_m_pv\\AK47_ANI.LTB");
        }

        strcpy(
            ocs.m_SkinNames[0],
            "Characters\\male\\hands\\CM_HND_NM_SPECIAL_BC.DTX");

        for(uint32 nSkin = 1; nSkin < MAX_MODEL_TEXTURES; ++nSkin)
        {
            strcpy(
                ocs.m_SkinNames[nSkin],
                "Weapons\\primary_t\\AK47_PV.DTX");
        }
    }
    else
    {
        strcpy(
            ocs.m_Filenames[0],
            "Weapons\\melee_m_pv\\CM_HND_NM_DF_BOWIEKNIFE_CH.LTB");
        strcpy(
            ocs.m_Filenames[1],
            "Weapons\\melee_m_pv\\ANI_G_BOWIEKNIFE_CH.LTB");

        strcpy(
            ocs.m_SkinNames[0],
            "Characters\\male\\hands\\CM_HND_NM_SPECIAL_BC.DTX");

        for(uint32 nSkin = 1; nSkin < MAX_MODEL_TEXTURES; ++nSkin)
        {
            strcpy(
                ocs.m_SkinNames[nSkin],
                "Weapons\\melee_t\\PV_ML_DF_BOWIEKNIFE_BC.DTX");
        }
    }

    strcpy(
        ocs.m_RenderStyleNames[0],
        "RenderStyles\\DEFAULT.LTB");

    m_hViewWeaponObject = g_pLTClient->CreateObject(&ocs);

    if(!m_hViewWeaponObject && m_nWeaponSlot == 1)
    {
        g_pLTClient->CPrint(
            "Fireteam: AK-47 model failed to create; reverting to Bowie.");
        m_nWeaponSlot = 3;
        CreateViewWeapon();
        return;
    }

    if(!m_hViewWeaponObject)
    {
        g_pLTClient->CPrint(
            "Fireteam: view weapon model failed to load.");
        return;
    }

    if(m_nWeaponSlot == 1)
    {
        HMODELANIM hSelect = g_pLTClient->GetAnimIndex(
            m_hViewWeaponObject,
            (char*)"select");

        PlayViewWeaponAnimation(
            hSelect != INVALID_MODEL_ANIM ? "select" : "select_0",
            false);
    }
    else
    {
        PlayViewWeaponAnimation("select", false);
    }

    PlayViewWeaponSound("SELECT.WAV");
    m_bViewWeaponAction = true;
}
'@

$text = Replace-CppFunctionBody $text 'void CPlayerClnt::CreateViewWeapon()' $createBody

$soundBody = @'
{
    if(!sFilename || !sFilename[0])
    {
        return;
    }

    PlaySoundInfo psi;
    PLAYSOUNDINFO_INIT(psi);
    psi.m_dwFlags = PLAYSOUND_LOCAL;

    if(m_nWeaponSlot == 1)
    {
        sprintf(
            psi.m_szSoundName,
            "Weapons\\primary_snd\\AK47\\%s",
            sFilename);
    }
    else
    {
        sprintf(
            psi.m_szSoundName,
            "Weapons\\melee_snd\\BOWIE_KNIFE\\%s",
            sFilename);
    }

    HLTSOUND hSound = NULL;
    g_pLTCSoundMgr->PlaySound(&psi, hSound);
}
'@

$text = Replace-CppFunctionBody $text 'void CPlayerClnt::PlayViewWeaponSound(const char* sFilename)' $soundBody

$updateBody = @'
{
    if(!m_hViewWeaponObject || !m_bViewWeaponAction)
    {
        return;
    }

    uint32 nAnimLen = 0;
    uint32 nAnimTime = 0;
    g_pLTCModel->GetCurAnimLength(
        m_hViewWeaponObject,
        MAIN_TRACKER,
        nAnimLen);
    g_pLTCModel->GetCurAnimTime(
        m_hViewWeaponObject,
        MAIN_TRACKER,
        nAnimTime);

    if(nAnimLen > 0 && nAnimTime >= nAnimLen)
    {
        HMODELANIM hIdle = g_pLTClient->GetAnimIndex(
            m_hViewWeaponObject,
            (char*)"idle_0");

        PlayViewWeaponAnimation(
            hIdle != INVALID_MODEL_ANIM ? "idle_0" : "idle",
            true);

        m_bViewWeaponAction = false;
    }
}
'@

$text = Replace-CppFunctionBody $text 'void CPlayerClnt::UpdateViewWeaponAnimation()' $updateBody

if(-not $text.Contains('bool CPlayerClnt::SelectWeaponSlot(uint8 nSlot)')) {
    $marker = '//----------------------------------------------------------------------------' + [Environment]::NewLine +
        '// void CPlayerClnt::UpdateAttacking()'

    $index = $text.IndexOf($marker)
    if($index -lt 0) {
        throw "Could not locate loadout method insertion point."
    }

    $methods = @'

//----------------------------------------------------------------------------
// Fireteam loadout slots.
// 1 = primary (AK-47), 3 = melee (Bowie). 2/4/5 intentionally empty.
//----------------------------------------------------------------------------
bool CPlayerClnt::SelectWeaponSlot(uint8 nSlot)
{
    if(nSlot != 1 && nSlot != 3)
    {
        return false;
    }

    if(m_nWeaponSlot == nSlot && m_hViewWeaponObject)
    {
        return true;
    }

    m_nWeaponSlot = nSlot;
    m_bAttacking = false;
    m_bViewWeaponAction = false;

    CreateViewWeapon();

    ILTMessage_Write *pMessage;
    if(g_pLTCCommon->CreateMessage(pMessage) == LT_OK && pMessage)
    {
        pMessage->IncRef();
        pMessage->Writeuint8(MSG_CS_WEAPON_SLOT);
        pMessage->Writeuint8(m_nWeaponSlot);
        g_pLTClient->SendToServer(
            pMessage->Read(),
            MESSAGE_GUARANTEED);
        pMessage->DecRef();
    }

    g_pLTClient->CPrint(
        "Fireteam: weapon slot %u - %s",
        (uint32)m_nWeaponSlot,
        m_nWeaponSlot == 1 ? "AK-47" : "Bowie");

    return true;
}

void CPlayerClnt::CycleWeapon(int nDirection)
{
    if(nDirection == 0)
    {
        return;
    }

    SelectWeaponSlot(m_nWeaponSlot == 1 ? 3 : 1);
}


'@

    $text = $text.Insert($index, $methods)
}

$text = $text.Replace(
    'Fireteam: Bowie animation missing: %s',
    'Fireteam: weapon animation missing: %s')

Write-Source $playerCpp $text

# ---------------------------------------------------------------------------
# Shared message id for slot synchronization.
# ---------------------------------------------------------------------------
$text = Read-Source $msgIds

if(-not $text.Contains('MSG_CS_WEAPON_SLOT')) {
    $text = Insert-AfterLineContaining $text 'MSG_CS_MY_CLUB,' @(
        '        MSG_CS_WEAPON_SLOT,         // client->server'
    ) 'MSG_CS_WEAPON_SLOT'
}

Write-Source $msgIds $text

# ---------------------------------------------------------------------------
# Client input: number keys, mouse wheel, and a camera-derived firing ray.
# ---------------------------------------------------------------------------
$text = Read-Source $clientShell

if(-not $text.Contains("m_pPlayer->SelectWeaponSlot(1);")) {
    $anchor = "if('C' == key)"
    if(-not $text.Contains($anchor)) {
        throw "Could not locate camera debug key handler."
    }

    $replacement = @"
if('1' == key)
           {
               m_pPlayer->SelectWeaponSlot(1);
           }
           else if('3' == key)
           {
               m_pPlayer->SelectWeaponSlot(3);
           }
           else if('C' == key)
"@

    $text = $text.Replace($anchor, $replacement)
}

if(-not $text.Contains('m_pPlayer->CycleWeapon(')) {
    $anchor = '        m_pCamera->UpdateZoom(offsets[2]);'
    if(-not $text.Contains($anchor)) {
        throw "Could not locate mouse-wheel camera zoom."
    }

    $replacement = @"
        if(m_pCamera->IsFirstPerson() && offsets[2] != 0.0f)
        {
            m_pPlayer->CycleWeapon(offsets[2] > 0.0f ? 1 : -1);
        }
        else
        {
            m_pCamera->UpdateZoom(offsets[2]);
        }
"@

    $text = $text.Replace($anchor, $replacement)
}

if(-not $text.Contains('pMessage->Writeuint8(MSG_CS_SHOOT);')) {
    $shootPattern = 'g_pLTClient->IsCommandOn\s*\(\s*COMMAND_SHOOT\s*\)'
    $shootMatch = [regex]::Match($text, $shootPattern)

    if(-not $shootMatch.Success) {
        throw "Could not locate COMMAND_SHOOT input branch."
    }

    $ifStart = $text.LastIndexOf("if", $shootMatch.Index)
    if($ifStart -lt 0) {
        throw "Could not locate COMMAND_SHOOT if statement."
    }

    $braceStart = $text.IndexOf("{", $shootMatch.Index)
    if($braceStart -lt 0) {
        throw "Could not locate COMMAND_SHOOT opening brace."
    }

    $depth = 0
    $braceEnd = -1
    for($i = $braceStart; $i -lt $text.Length; ++$i) {
        if($text[$i] -eq '{') {
            ++$depth
        }
        elseif($text[$i] -eq '}') {
            --$depth
            if($depth -eq 0) {
                $braceEnd = $i
                break
            }
        }
    }

    if($braceEnd -lt 0) {
        throw "Could not locate COMMAND_SHOOT closing brace."
    }

    # Include the indentation immediately before the if, but leave any
    # preceding comment/marker intact.
    $lineStart = $text.LastIndexOf([Environment]::NewLine, $ifStart)
    if($lineStart -lt 0) {
        $lineStart = 0
    }
    else {
        $lineStart += [Environment]::NewLine.Length
    }

    $newShoot = @"
    if (g_pLTClient->IsCommandOn(COMMAND_SHOOT))
    {
        if(m_pPlayer->Attack() &&
           m_pPlayer->GetWeaponSlot() == 1)
        {
            HLOCALOBJ hFireCamera = m_pCamera->GetCamera();
            LTVector vFirePos;
            LTRotation rFireRot;

            g_pLTClient->GetObjectPos(
                hFireCamera,
                &vFirePos);
            g_pLTClient->GetObjectRotation(
                hFireCamera,
                &rFireRot);

            LTVector vFireDir = rFireRot.Forward();

            ILTMessage_Write *pMessage;
            if(g_pLTCCommon->CreateMessage(pMessage) == LT_OK &&
               pMessage)
            {
                pMessage->IncRef();
                pMessage->Writeuint8(MSG_CS_SHOOT);
                pMessage->WriteLTVector(vFirePos);
                pMessage->WriteLTVector(vFireDir);
                g_pLTClient->SendToServer(
                    pMessage->Read(),
                    0);
                pMessage->DecRef();
            }
        }
    }
"@

    $text = $text.Substring(0, $lineStart) +
        $newShoot +
        $text.Substring($braceEnd + 1)
}

Write-Source $clientShell $text

# ---------------------------------------------------------------------------
# Server-side selected weapon + server authoritative AK hitscan.
# ---------------------------------------------------------------------------
$text = Read-Source $serverH

$text = Insert-AfterLineContaining $text 'SetClubID();' @(
    '    void                SetWeaponSlot(uint8 nSlot);',
    '    void                FirePrimary(const LTVector &vFrom, const LTVector &vDirection);'
) 'SetWeaponSlot(uint8 nSlot)'

if(-not $text.Contains('m_nWeaponSlot;')) {
    $text = Insert-AfterLineContaining $text 'm_WeaponOBB;' @(
        '    uint8               m_nWeaponSlot;',
        '    float               m_fNextPrimaryShot;'
    ) 'm_nWeaponSlot;'
}

if(-not $text.Contains('m_nWeaponSlot(3)')) {
    $anchor = '          m_fPoisonCarry(0.0f)'
    if(-not $text.Contains($anchor)) {
        throw "Could not locate Fireteam health constructor tail."
    }

    $text = $text.Replace(
        $anchor,
        $anchor + "," + [Environment]::NewLine +
        "          m_nWeaponSlot(3)," + [Environment]::NewLine +
        "          m_fNextPrimaryShot(0.0f)")
}

Write-Source $serverH $text

$text = Read-Source $serverCpp

if(-not $text.Contains('#include <string.h>')) {
    $anchor = '#include "statsmanager.h"'
    if(-not $text.Contains($anchor)) {
        throw "Could not locate server player include anchor."
    }

    $text = $text.Replace(
        $anchor,
        $anchor + [Environment]::NewLine + '#include <string.h>')
}

if(-not $text.Contains('struct FTFireFilterData')) {
    $classEnd = 'END_CLASS_DEFAULT_FLAGS(CPlayerSrvr, BaseClass, LTNULL, LTNULL, CF_HIDDEN)'
    if(-not $text.Contains($classEnd)) {
        throw "Could not locate CPlayerSrvr class registration."
    }

    $filterCode = @'

struct FTFireFilterData
{
    HOBJECT hPlayer;
    HOBJECT hWeapon;
};

static bool FTFireFilter(HOBJECT hObject, void *pUserData)
{
    FTFireFilterData *pData = (FTFireFilterData*)pUserData;
    if(!pData)
    {
        return true;
    }

    return hObject != pData->hPlayer &&
           hObject != pData->hWeapon;
}

'@

    $text = $text.Replace(
        $classEnd,
        $classEnd + $filterCode)
}

if(-not $text.Contains('void CPlayerSrvr::SetWeaponSlot(uint8 nSlot)')) {
    $append = @'

//-----------------------------------------------------------------------------
// Fireteam loadout / primary weapon.
//-----------------------------------------------------------------------------
void CPlayerSrvr::SetWeaponSlot(uint8 nSlot)
{
    if(nSlot != 1 && nSlot != 3)
    {
        return;
    }

    m_nWeaponSlot = nSlot;

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

void CPlayerSrvr::FirePrimary(
    const LTVector &vFrom,
    const LTVector &vDirection)
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

    m_fNextPrimaryShot = fNow + 0.10f;

    LTVector vDir = vDirection;
    if(vDir.MagSqr() < 0.0001f)
    {
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
    query.m_To = vStart + (vDir * 20000.0f);
    query.m_Flags =
        INTERSECT_OBJECTS |
        IGNORE_NONSOLID |
        INTERSECT_HPOLY;

    FTFireFilterData filterData;
    filterData.hPlayer = m_hObject;
    filterData.hWeapon = m_hClub;

    query.m_FilterFn = FTFireFilter;
    query.m_pUserData = &filterData;

    if(!g_pLTServer->IntersectSegment(&query, &info) ||
       !info.m_hObject)
    {
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
        // Fireteam never damages another player.
        return;
    }

    ILTMessage_Write *pDamage;
    if(g_pLTSCommon->CreateMessage(pDamage) == LT_OK &&
       pDamage)
    {
        pDamage->IncRef();
        pDamage->Writeuint32(OBJ_MID_DAMAGE);

        // Temporary DEV damage until the AK-47 attribute record is wired
        // directly into Fireteam's weapon data.
        pDamage->Writeuint8(12);

        g_pLTServer->SendToObject(
            pDamage->Read(),
            m_hObject,
            info.m_hObject,
            0);

        pDamage->DecRef();
    }
}

'@

    $text += $append
}

Write-Source $serverCpp $text

$text = Read-Source $serverShell

if(-not $text.Contains('case MSG_CS_SHOOT:')) {
    $anchor = '    case MSG_CS_ANIM:'
    if(-not $text.Contains($anchor)) {
        throw "Could not locate server animation message handler."
    }

    $handler = @"
    case MSG_CS_SHOOT:
        {
            LTVector vFrom = pMessage->ReadLTVector();
            LTVector vDirection = pMessage->ReadLTVector();

            if(pPlayerClass)
            {
                pPlayerClass->FirePrimary(
                    vFrom,
                    vDirection);
            }
        }
        break;
"@

    $text = $text.Replace(
        $anchor,
        $handler + $anchor)
}

if(-not $text.Contains('case MSG_CS_WEAPON_SLOT:')) {
    $anchor = '    case MSG_CS_SCORE:'
    if(-not $text.Contains($anchor)) {
        throw "Could not locate server score message handler."
    }

    $handler = @"
    case MSG_CS_WEAPON_SLOT:
        {
            if(pPlayerClass)
            {
                pPlayerClass->SetWeaponSlot(
                    pMessage->Readuint8());
            }
        }
        break;
"@

    $text = $text.Replace(
        $anchor,
        $handler + $anchor)
}

Write-Source $serverShell $text

Write-Host "[OK] Fireteam loadout slots: 1 AK-47 / 3 Bowie / wheel cycle"
