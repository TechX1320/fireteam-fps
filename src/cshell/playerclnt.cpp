//------------------------------------------------------------------------------//
//
// MODULE   : playerclnt.cpp
//
// PURPOSE  : CPlayerClnt - Implementation
//
// CREATED  : 07/15/2002
//
// (c) 2002 LithTech, Inc.  All Rights Reserved
//
//------------------------------------------------------------------------------//

#include "playerclnt.h"

#include <iltclient.h>
#include <ltobjectcreate.h>
#include <iltmessage.h>
#include <iltphysics.h>
#include <iltcommon.h>
#include <iltmodel.h>
#include <iltsoundmgr.h>
#include <stdio.h>

#include "clientinterfaces.h"
#include "commandids.h"
#include "msgids.h"
#include "animids.h"
#include "FireteamWeaponHud.h"

#define MOVEMENT_RATE 2000.0f
#define WALK_MAX_SPEED 435.0f
#define SPRINT_MAX_SPEED 600.0f
#define CROUCH_MAX_SPEED 190.0f
#define JUMP_TIME 0.18f
#define JUMP_VELOCITY 400.0f
#define DEFAULT_LEASHTIME 2.0f
#define FT_WEAPON_QA_QUARANTINE_FILE "config/weapon-quarantine.txt"

static bool FT_IsWeaponQaQuarantined(
    const char *pSection)
{
    if(!pSection ||
       !pSection[0])
    {
        return false;
    }

    FILE *pFile =
        fopen(
            FT_WEAPON_QA_QUARANTINE_FILE,
            "rt");

    if(!pFile)
    {
        return false;
    }

    char sLine[256];
    bool bFound = false;

    while(fgets(
        sLine,
        sizeof(sLine),
        pFile))
    {
        char *pLine =
            FT_TrimWeaponLine(
                sLine);

        if(!pLine[0] ||
           pLine[0] == '#' ||
           pLine[0] == ';')
        {
            continue;
        }

        if(_stricmp(
               pLine,
               pSection) == 0)
        {
            bFound = true;
            break;
        }
    }

    fclose(pFile);
    return bFound;
}

static bool FT_SetWeaponQaQuarantined(
    const char *pSection,
    bool bQuarantined)
{
    if(!pSection ||
       _strnicmp(
           pSection,
           "catalog.",
           8) != 0)
    {
        return false;
    }

    const char *pTempPath =
        "config/weapon-quarantine.tmp";

    FILE *pOutput =
        fopen(
            pTempPath,
            "wt");

    if(!pOutput)
    {
        return false;
    }

    FILE *pInput =
        fopen(
            FT_WEAPON_QA_QUARANTINE_FILE,
            "rt");

    bool bFound = false;
    bool bWroteAny = false;
    char sLine[256];

    if(pInput)
    {
        while(fgets(
            sLine,
            sizeof(sLine),
            pInput))
        {
            char *pLine =
                FT_TrimWeaponLine(
                    sLine);

            if(!pLine[0] ||
               pLine[0] == '#' ||
               pLine[0] == ';')
            {
                continue;
            }

            if(_stricmp(
                   pLine,
                   pSection) == 0)
            {
                bFound = true;

                if(!bQuarantined)
                {
                    continue;
                }
            }

            fprintf(
                pOutput,
                "%s\n",
                pLine);
            bWroteAny = true;
        }

        fclose(pInput);
    }

    if(bQuarantined &&
       !bFound)
    {
        fprintf(
            pOutput,
            "%s\n",
            pSection);
        bWroteAny = true;
    }

    if(!bWroteAny)
    {
        fprintf(
            pOutput,
            "# FIRETEAM Weapon QA quarantine. One catalog section per line.\n");
    }

    fclose(pOutput);

    remove(
        FT_WEAPON_QA_QUARANTINE_FILE);

    if(rename(
           pTempPath,
           FT_WEAPON_QA_QUARANTINE_FILE) != 0)
    {
        remove(
            pTempPath);
        return false;
    }

    return true;
}



//----------------------------------------------------------------------------
// CPlayerClnt::CPlayerClnt()
//
//-----------------------------------------------------------------------------
CPlayerClnt::CPlayerClnt() :
m_dwInputFlags(0),
m_fYaw(0.0f),
m_fPitch(0.0f),
m_fRoll(0.0f),
m_hClientObject(NULL),
m_hObject(NULL),
//m_hCurAnim(NULL),
m_bAttacking(false),
m_hClubObject(NULL),
m_hViewWeaponObject(NULL),
m_bViewWeaponAction(false),
m_nViewAttackVariant(0),
m_nWeaponSlot(1),
m_fNextPrimaryClientShot(0.0f),
m_bSemiAutoTriggerHeld(false),
m_bReloading(false),
m_fReloadComplete(0.0f),
m_pDevWeaponDefs(NULL),
m_nDevWeaponCount(0),
m_nDevWeaponIndex(0),
m_bDevWeaponQa(false),
m_bPlayerDefLoaded(false),
m_bCrouching(false),
m_bIsJumping(false),
m_fCurrentJumpRadians(0.0f),
m_fJumpTimeRemaining(0.0f),
m_DebugSphere(NULL),
m_bUseLeashing(true),
m_fLeashingDelay(0.0f)
{
    if(!FT_LoadWeaponDefs("config/weapons.cfg", m_WeaponDefs))
    {
        g_pLTClient->CPrint("Fireteam: failed to load config/weapons.cfg.");
    }

    HCONSOLEVAR hDevWeaponQa =
        g_pLTClient->GetConsoleVar(
            "devweaponqa");

    m_bDevWeaponQa =
        hDevWeaponQa &&
        g_pLTClient->GetVarValueFloat(
            hDevWeaponQa) != 0.0f;

    if(m_bDevWeaponQa)
    {
        const uint32 nCatalogCount =
            FT_CountCatalogWeaponDefs(
                "config/weapons.cfg");

        m_pDevWeaponDefs =
            new FTWeaponDef[nCatalogCount + 5];

        if(m_pDevWeaponDefs)
        {
            uint32 nActiveCount = 0;

            for(uint8 nSlot = 1;
                nSlot <= 5;
                ++nSlot)
            {
                const FTWeaponDef *pActive =
                    FT_GetWeaponDef(
                        m_WeaponDefs,
                        nSlot);

                if(pActive)
                {
                    m_pDevWeaponDefs[nActiveCount] =
                        *pActive;
                    m_pDevWeaponDefs[nActiveCount].nSlot =
                        1;
                    ++nActiveCount;
                }
            }

            const uint32 nLoadedCatalog =
                FT_LoadCatalogWeaponDefs(
                    "config/weapons.cfg",
                    m_pDevWeaponDefs + nActiveCount,
                    nCatalogCount);

            m_nDevWeaponCount =
                nActiveCount +
                nLoadedCatalog;

            g_pLTClient->CPrint(
                "Fireteam weapon QA: loaded %u test entries (%u active + %u catalog). Mouse wheel cycles every entry regardless of launcher Enabled state.",
                m_nDevWeaponCount,
                nActiveCount,
                nLoadedCatalog);
        }
    }

    m_bPlayerDefLoaded =
        FT_LoadPlayerDef(
            "config/player.cfg",
            m_PlayerDef);

    if(!m_bPlayerDefLoaded)
    {
        g_pLTClient->CPrint(
            "Fireteam: failed to load config/player.cfg.");
    }
}



//----------------------------------------------------------------------------
// CPlayerClnt::~CPlayerClnt()
//
//-----------------------------------------------------------------------------
CPlayerClnt::~CPlayerClnt()
{
    if(m_pDevWeaponDefs)
    {
        delete [] m_pDevWeaponDefs;
        m_pDevWeaponDefs = NULL;
        m_nDevWeaponCount = 0;
    }

    if(m_hViewWeaponObject)
    {
        g_pLTClient->RemoveObject(m_hViewWeaponObject);
        m_hViewWeaponObject = NULL;
    }
}



//----------------------------------------------------------------------------
// void CPlayerClnt::SetHObject(HLOCALOBJ hObj)
//
//-----------------------------------------------------------------------------
void CPlayerClnt::SetClientObject(HLOCALOBJ hObj)
{
	m_hClientObject = hObj;
}



//----------------------------------------------------------------------------
// void CPlayerClnt::SpecialEffectMessage(ILTMessage_Read *pMessage)
//
//-----------------------------------------------------------------------------
void CPlayerClnt::SpecialEffectMessage(ILTMessage_Read *pMessage)
{
	// Nothing to read currently
}



//----------------------------------------------------------------------------
// void CPlayerClnt::CreatePlayer()
//
//-----------------------------------------------------------------------------
void CPlayerClnt::CreatePlayer()
{
    LTVector vPos;
    LTRotation rRot;

    g_pLTClient->GetObjectPos(m_hClientObject, &vPos);
    g_pLTClient->GetObjectRotation(m_hClientObject, &rRot);

	ObjectCreateStruct objCreateStruct;
	objCreateStruct.Clear();

    objCreateStruct.m_ObjectType = OT_MODEL;
    objCreateStruct.m_Flags = FLAG_SOLID | FLAG_GRAVITY | FLAG_STAIRSTEP | FLAG_VISIBLE | FLAG_SHADOW;
	objCreateStruct.m_Flags2 = FLAG2_PLAYERCOLLIDE;

    objCreateStruct.m_Pos = vPos;
    objCreateStruct.m_Rotation = rRot;

    if(m_bPlayerDefLoaded)
    {
        FT_CopyPlayerString(
            objCreateStruct.m_Filenames[0],
            MAX_CS_FILENAME_LEN,
            m_PlayerDef.sBodyModel);
        FT_CopyPlayerString(
            objCreateStruct.m_Filenames[1],
            MAX_CS_FILENAME_LEN,
            m_PlayerDef.sAnimationModel);
        FT_CopyPlayerString(
            objCreateStruct.m_SkinNames[0],
            MAX_CS_FILENAME_LEN,
            m_PlayerDef.sSkin0);
        FT_CopyPlayerString(
            objCreateStruct.m_SkinNames[1],
            MAX_CS_FILENAME_LEN,
            m_PlayerDef.sSkin1);
    }

    m_hObject = g_pLTClient->CreateObject(&objCreateStruct);

    CreateViewWeapon();

    if(m_hObject)
    {
        // Set up animation trackers
        m_idUpperBodyTracker = ANIM_UPPER;
        g_pLTCModel->AddTracker(m_hObject, m_idUpperBodyTracker);

        m_idLowerBodyTracker = ANIM_LOWER;
        g_pLTCModel->AddTracker(m_hObject, m_idLowerBodyTracker);

        // Set up weight sets

        if ( LT_OK == g_pLTCModel->FindWeightSet(m_hObject, "Upper", m_hWeightUpper) )
        {
            g_pLTCModel->SetWeightSet(m_hObject, m_idUpperBodyTracker, m_hWeightUpper);
        }

        if ( LT_OK == g_pLTCModel->FindWeightSet(m_hObject, "Lower", m_hWeightLower) )
        {
            g_pLTCModel->SetWeightSet(m_hObject, m_idLowerBodyTracker, m_hWeightLower);
        }
    }
}



//----------------------------------------------------------------------------
// void CPlayerClnt::UpdateMouseMovement(float yaw, float pitch, float roll)
//
//-----------------------------------------------------------------------------
void CPlayerClnt::UpdateRotation(float yaw, float pitch, float roll)
{
    m_fYaw += yaw;
    m_fPitch += pitch;
    m_fRoll += roll;
}

bool CPlayerClnt::IsMoving()
{
    if(!m_hObject)
    {
        return false;
    }

    LTVector vVelocity;
    g_pLTCPhysics->GetVelocity(
        m_hObject,
        &vVelocity);

    return ((vVelocity.x * vVelocity.x) +
            (vVelocity.z * vVelocity.z)) > 100.0f;
}



//----------------------------------------------------------------------------
// void CPlayerClnt::Update()
//
//-----------------------------------------------------------------------------
void CPlayerClnt::Update()
{
    if(m_bReloading &&
       g_pLTClient->GetTime() >= m_fReloadComplete)
    {
        m_bReloading = false;
        m_fReloadComplete = 0.0f;

        // The server owns the actual ammo transfer. Ask it to reconcile the
        // HUD at the exact end of the local reload instead of waiting for the
        // next shot/input to expose the completed magazine.
        ILTMessage_Write *pAmmoSync = LTNULL;
        if(g_pLTCCommon->CreateMessage(pAmmoSync) == LT_OK &&
           pAmmoSync)
        {
            pAmmoSync->IncRef();
            pAmmoSync->Writeuint8(MSG_CS_AMMO_SYNC);
            g_pLTClient->SendToServer(
                pAmmoSync->Read(),
                MESSAGE_GUARANTEED);
            pAmmoSync->DecRef();
        }
    }

    LTVector vZero(0.0f, 0.0f, 0.0f);
    g_pLTCPhysics->SetAcceleration(m_hObject, &vZero);

    // Calculate rotation
    LTVector vUp, vRight, vForward;
    LTRotation rRot;
	g_pLTCCommon->SetupEuler(rRot, m_fPitch, m_fYaw, m_fRoll);
	g_pLTClient->SetObjectRotation(m_hObject, &rRot);

    // Calulate movement
    LTVector vPos, vVel;

    UpdateMovement();

    if (m_bAttacking)
	{
    	UpdateAttacking(m_idUpperBodyTracker);
	}

    g_pLTClient->GetObjectPos(m_hObject, &vPos);
    g_pLTCPhysics->GetVelocity(m_hObject, &vVel);

    // Send our information to the server
    g_pCShell->SendVelPosAndRot(vVel, vPos, rRot);

    // Input flags are accumulated by event callbacks during the frame.
    // Always clear them after movement is consumed. This must happen before
    // any optional weapon/club early-return or a missing hand-held object can
    // leave W/A/S/D latched forever.
    m_dwInputFlags = 0;

    // update club position
    if(!m_hClubObject)
    {
        return;
    }

    HMODELSOCKET hSocket;
    LTRESULT SocketResult = g_pLTCModel->GetSocket(m_hObject, "RightHand", hSocket);
    if (LT_OK != SocketResult)
	{
        g_pLTClient->CPrint("GetSocket failed: " );
    }

    LTransform tSocketTransform;
    LTRESULT SocketTrResult = g_pLTCModel->GetSocketTransform(m_hObject, hSocket, tSocketTransform, LTTRUE);
    if (LT_OK != SocketTrResult)
    {
        g_pLTClient->CPrint("GetTrasform failed: ");
    }

    g_pLTClient->SetObjectPos(m_hClubObject, &tSocketTransform.m_Pos);
    g_pLTClient->SetObjectRotation(m_hClubObject, &tSocketTransform.m_Rot);
}



//----------------------------------------------------------------------------
// void CPlayerClnt::UpdateMovement()
//
//----------------------------------------------------------------------------
void CPlayerClnt::UpdateMovement()
{
    LTVector vPos, vVel;
    LTRotation rRot;
    g_pLTClient->GetObjectPos(m_hObject, &vPos);
    g_pLTCPhysics->GetVelocity(m_hObject, &vVel);
    g_pLTClient->GetObjectRotation(m_hObject, &rRot);

    m_bCrouching =
        (m_dwInputFlags & MOVE_CROUCH) != 0;

    float fForward = 0.0f;
    float fRight = 0.0f;

    if(m_dwInputFlags & MOVE_FORWARD)  fForward += 1.0f;
    if(m_dwInputFlags & MOVE_BACKWARD) fForward -= 1.0f;
    if(m_dwInputFlags & MOVE_RIGHT)    fRight += 1.0f;
    if(m_dwInputFlags & MOVE_LEFT)     fRight -= 1.0f;

    LTVector vMove =
        (rRot.Forward() * fForward) +
        (rRot.Right() * fRight);
    vMove.y = 0.0f;

    if(vMove.MagSqr() > 0.0001f)
    {
        vMove.Normalize();

        float fMaxSpeed = WALK_MAX_SPEED;
        if(m_bCrouching)
        {
            fMaxSpeed = CROUCH_MAX_SPEED;
        }
        else if(m_dwInputFlags & MOVE_SPRINT)
        {
            fMaxSpeed = SPRINT_MAX_SPEED;
        }

        // Direct target velocity gives predictable modern WASD response and
        // keeps diagonal movement at the same speed as straight movement.
        vVel.x = vMove.x * fMaxSpeed;
        vVel.z = vMove.z * fMaxSpeed;

        if(fForward > 0.0f)
            PlayMovementAnimation("LRF", m_idLowerBodyTracker);
        else if(fForward < 0.0f)
            PlayMovementAnimation("LRB", m_idLowerBodyTracker);
        else if(fRight < 0.0f)
            PlayMovementAnimation("LRL", m_idLowerBodyTracker);
        else
            PlayMovementAnimation("LRR", m_idLowerBodyTracker);
    }
    else
    {
        vVel.x = 0.0f;
        vVel.z = 0.0f;

        if(!m_bIsJumping)
        {
            PlayMovementAnimation("LSt", m_idLowerBodyTracker);
        }

        if(!m_bAttacking)
        {
            PlayMovementAnimation("LSt", m_idUpperBodyTracker);
        }
    }

    if(m_dwInputFlags & MOVE_JUMP)
    {
        if(!m_bIsJumping && !m_bCrouching)
        {
            CollisionInfo cInfo;
            g_pLTCPhysics->GetStandingOn(m_hObject, &cInfo);
            if(cInfo.m_hObject)
            {
                m_bIsJumping = true;
                m_fJumpTimeRemaining = JUMP_TIME;
                m_fCurrentJumpRadians = 0.0f;
                PlayMovementAnimation("LJT", m_idLowerBodyTracker);
            }
        }
    }

    g_pLTCPhysics->SetVelocity(m_hObject, &vVel);

    RecalculateBoundingBox();

    if(m_bIsJumping)
    {
        UpdateJump();
    }

    RecalculatePosition();
}


//----------------------------------------------------------------------------
// void CPlayerClnt::RecalculateBoundingBox()
//
//-----------------------------------------------------------------------------
void CPlayerClnt::RecalculateBoundingBox()
{
    if(!m_bPlayerDefLoaded)
    {
        return;
    }

    LTVector vDims(
        m_PlayerDef.fCollisionX,
        m_PlayerDef.fCollisionY,
        m_PlayerDef.fCollisionZ);
    g_pLTCPhysics->SetObjectDims(
        m_hObject,
        &vDims,
        0);
}



//----------------------------------------------------------------------------
// void CPlayerClnt::RecalculatePosition()
//
//-----------------------------------------------------------------------------
void CPlayerClnt::RecalculatePosition()
{
	LTVector curPos, newPos;

	ILTClientPhysics* pCPhysics = (ILTClientPhysics*)g_pLTCPhysics;

	MoveInfo info;
	info.m_Offset.Init();
	info.m_hObject = m_hObject;
	info.m_dt = g_pLTClient->GetFrameTime();
	pCPhysics->UpdateMovement(&info);

	g_pLTClient->GetObjectPos(m_hObject, &curPos);
	newPos = curPos + info.m_Offset;
	pCPhysics->MoveObject(m_hObject, &newPos, 0);

    if ( !m_hClientObject )
    {
        return;
    }

    // Leash our self if to far away.
    // -DLK - disable leashing for a short bit.

    if(m_bUseLeashing)
    {
        // Make sure we aren't too far away from our client.
        LTVector vClientPos;
        g_pLTClient->GetObjectPos(m_hClientObject, &vClientPos);

        float fDist = vClientPos.DistSqr(newPos);

        if( fDist > 1000.0f )
        {
            m_fLeashingDelay += g_pLTClient->GetFrameTime();
            //g_pLTClient->SetObjectPos(m_hObject, &vClientPos);
        }
        else
        {
            m_fLeashingDelay = 0.0f;
        }

        // If the player has been out of the bounds of the leash for DEFAULT_LEASHTIME
        // apply the leash.
        if(m_fLeashingDelay >= DEFAULT_LEASHTIME)
        {
            g_pLTClient->SetObjectPos(m_hObject, &vClientPos);
            m_fLeashingDelay = 0.0f;
        }


    }


}



//----------------------------------------------------------------------------
// void CPlayerClnt::UpdateJump()
//
//-----------------------------------------------------------------------------
void CPlayerClnt::UpdateJump()
{
	// Update our position from the velocity
	ILTClientPhysics* pCPhysics = (ILTClientPhysics*)g_pLTCPhysics;

	MoveInfo info;
	info.m_Offset.Init();
	info.m_hObject = m_hObject;
	info.m_dt = g_pLTClient->GetFrameTime();
	pCPhysics->UpdateMovement(&info);

	// Get current resultant spacial information
	LTVector vPos, vVel;
	LTRotation rRot;
    g_pLTClient->GetObjectPos(m_hObject, &vPos);
    g_pLTCPhysics->GetVelocity(m_hObject, &vVel);
    g_pLTClient->GetObjectRotation(m_hObject, &rRot);

	// Get current frame time
	LTFLOAT fFrameTime = g_pLTClient->GetFrameTime();

	if(m_fJumpTimeRemaining >= JUMP_TIME / 2.0f)
	{
		vVel.y = JUMP_VELOCITY;
	}

	m_fJumpTimeRemaining -= fFrameTime;

	if(m_fJumpTimeRemaining < 0.0f)
	{
		m_bIsJumping = false;
	}

    g_pLTCPhysics->SetVelocity(m_hObject, &vVel);
}



//----------------------------------------------------------------------------
// void CPlayerClnt::Attack()
//
//-----------------------------------------------------------------------------
bool CPlayerClnt::Attack()
{
    if(m_bReloading)
    {
        return false;
    }

    const FTWeaponDef *pDef = GetCurrentWeaponDef();
    if(!pDef)
    {
        return false;
    }

    if(pDef->eType != FT_WEAPON_MELEE)
    {
        const float fNow = g_pLTClient->GetTime();
        if(fNow < m_fNextPrimaryClientShot)
        {
            return false;
        }

        if(!pDef->bAutomatic && m_bSemiAutoTriggerHeld)
        {
            return false;
        }

        if(!pDef->bAutomatic)
        {
            m_bSemiAutoTriggerHeld = true;
        }

        m_fNextPrimaryClientShot = fNow + pDef->fFireInterval;

        const char *pFireAnim = pDef->sAnimFire;

        HMODELANIM hFire = INVALID_MODEL_ANIM;
        if(!pFireAnim[0] &&
           m_hViewWeaponObject)
        {
            hFire = g_pLTClient->GetAnimIndex(
                m_hViewWeaponObject,
                (char*)"fire_0");
        }

        m_bViewWeaponAction =
            PlayViewWeaponAnimation(
                pFireAnim[0]
                    ? pFireAnim
                    : (hFire != INVALID_MODEL_ANIM
                        ? "fire_0"
                        : "fire"),
                false);
        PlayViewWeaponSound("FIRE.WAV");
        FT_WeaponHudOnShot(m_nWeaponSlot);
        return true;
    }

    if(m_bAttacking)
    {
        return false;
    }

    PlayAttackAnimation("UMFi", m_idUpperBodyTracker);
    m_bAttacking = true;
    m_bViewWeaponAction =
        PlayViewWeaponAnimation(
            pDef->sAnimFire[0]
                ? pDef->sAnimFire
                : "fire_0",
            false);
    PlayViewWeaponSound("FIRE.WAV");
    return true;
}




//----------------------------------------------------------------------------
// bool CPlayerClnt::AltAttack()
//
//----------------------------------------------------------------------------
bool CPlayerClnt::AltAttack()
{
    if(m_bReloading)
    {
        return false;
    }

    if(m_nWeaponSlot != 3 || m_bAttacking)
    {
        return false;
    }

    PlayAttackAnimation("UMFi", m_idUpperBodyTracker);
    m_bAttacking = true;

    const FTWeaponDef *pDef =
        GetCurrentWeaponDef();

    m_bViewWeaponAction =
        PlayViewWeaponAnimation(
            pDef && pDef->sAnimAltFire[0]
                ? pDef->sAnimAltFire
                : "fire_1",
            false);
    PlayViewWeaponSound("FIRE.WAV");

    return true;
}


//----------------------------------------------------------------------------
// Fireteam five-slot loadout.
// Slot content is entirely definition-driven by config/weapons.cfg.
//----------------------------------------------------------------------------
bool CPlayerClnt::SelectWeaponSlot(uint8 nSlot)
{
    if(m_bDevWeaponQa)
    {
        return false;
    }

    const FTWeaponDef *pDef = FT_GetWeaponDef(m_WeaponDefs, nSlot);
    if(!pDef)
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
    m_bSemiAutoTriggerHeld = false;
    m_bReloading = false;
    m_fReloadComplete = 0.0f;
    m_fNextPrimaryClientShot = 0.0f;

    CreateViewWeapon();

    ILTMessage_Write *pMessage = LTNULL;
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
        pDef->sName);

    return true;
}

void CPlayerClnt::CycleWeapon(int nDirection)
{
    if(nDirection == 0)
    {
        return;
    }

    if(m_bDevWeaponQa)
    {
        CycleDevWeapon(
            nDirection);
        return;
    }

    int nSlot = (int)m_nWeaponSlot;
    for(int nTry = 0; nTry < 5; ++nTry)
    {
        nSlot += (nDirection > 0) ? 1 : -1;
        if(nSlot > 5) nSlot = 1;
        if(nSlot < 1) nSlot = 5;

        if(FT_GetWeaponDef(m_WeaponDefs, (uint8)nSlot))
        {
            SelectWeaponSlot((uint8)nSlot);
            return;
        }
    }
}

void CPlayerClnt::CycleDevWeapon(int nDirection)
{
    if(!m_pDevWeaponDefs ||
       m_nDevWeaponCount == 0 ||
       nDirection == 0)
    {
        return;
    }

    int nNext =
        (int)m_nDevWeaponIndex +
        (nDirection > 0
            ? 1
            : -1);

    if(nNext >= (int)m_nDevWeaponCount)
    {
        nNext = 0;
    }
    else if(nNext < 0)
    {
        nNext =
            (int)m_nDevWeaponCount - 1;
    }

    m_nDevWeaponIndex =
        (uint32)nNext;
    m_nWeaponSlot = 1;
    m_bAttacking = false;
    m_bViewWeaponAction = false;
    m_bSemiAutoTriggerHeld = false;
    m_bReloading = false;
    m_fReloadComplete = 0.0f;
    m_fNextPrimaryClientShot = 0.0f;

    CreateViewWeapon();

    const FTWeaponDef *pDef =
        GetCurrentWeaponDef();

    if(pDef)
    {
        const bool bQuarantined =
            FT_IsWeaponQaQuarantined(
                pDef->sSection);

        g_pLTClient->CPrint(
            "Fireteam weapon QA [%u/%u]: %s%s | section=%s | model=%s | texture=%s | view=<%.2f, %.2f, %.2f>",
            m_nDevWeaponIndex + 1,
            m_nDevWeaponCount,
            bQuarantined
                ? "[DISABLED] "
                : "",
            pDef->sName,
            pDef->sSection,
            pDef->sPVModel,
            pDef->sPVTexture,
            pDef->fViewX,
            pDef->fViewY,
            pDef->fViewZ);
    }
}


//----------------------------------------------------------------------------
void CPlayerClnt::ToggleDevWeaponQuarantine()
{
    if(!m_bDevWeaponQa)
    {
        return;
    }

    const FTWeaponDef *pDef =
        GetCurrentWeaponDef();

    if(!pDef ||
       _strnicmp(
           pDef->sSection,
           "catalog.",
           8) != 0)
    {
        g_pLTClient->CPrint(
            "Fireteam weapon QA: active loadout entries cannot be quarantined here. Scroll to the catalog copy first.");
        return;
    }

    const bool bWasQuarantined =
        FT_IsWeaponQaQuarantined(
            pDef->sSection);

    const bool bQuarantined =
        !bWasQuarantined;

    if(!FT_SetWeaponQaQuarantined(
           pDef->sSection,
           bQuarantined))
    {
        g_pLTClient->CPrint(
            "Fireteam weapon QA: failed to update quarantine file for %s.",
            pDef->sName);
        return;
    }

    FT_WeaponHudSetQaQuarantined(
        bQuarantined);

    g_pLTClient->CPrint(
        "Fireteam weapon QA: %s %s (%s).",
        pDef->sName,
        bQuarantined
            ? "DISABLED"
            : "ENABLED",
        pDef->sSection);
}


//----------------------------------------------------------------------------
bool CPlayerClnt::ReloadWeapon()
{
    const FTWeaponDef *pDef = GetCurrentWeaponDef();
    if(!pDef ||
       pDef->nClipSize == 0 ||
       pDef->eType == FT_WEAPON_MELEE ||
       m_bReloading)
    {
        return false;
    }

    m_bReloading = true;
    m_fReloadComplete =
        g_pLTClient->GetTime() +
        (pDef->fReloadSeconds > 0.0f
            ? pDef->fReloadSeconds
            : 0.10f);

    ILTMessage_Write *pMessage = LTNULL;
    if(!m_bDevWeaponQa &&
       g_pLTCCommon->CreateMessage(pMessage) == LT_OK &&
       pMessage)
    {
        pMessage->IncRef();
        pMessage->Writeuint8(MSG_CS_RELOAD);
        g_pLTClient->SendToServer(
            pMessage->Read(),
            MESSAGE_GUARANTEED);
        pMessage->DecRef();
    }

    HMODELANIM hReload = INVALID_MODEL_ANIM;
    if(!pDef->sAnimReload[0] &&
       m_hViewWeaponObject)
    {
        hReload = g_pLTClient->GetAnimIndex(
            m_hViewWeaponObject,
            (char*)"reload");
    }

    m_bViewWeaponAction =
        PlayViewWeaponAnimation(
            pDef->sAnimReload[0]
                ? pDef->sAnimReload
                : (hReload != INVALID_MODEL_ANIM
                    ? "reload"
                    : "reload_0"),
            false);
    PlayViewWeaponSound("RELOAD.WAV");
    return true;
}

//----------------------------------------------------------------------------
// void CPlayerClnt::UpdateAttacking()
//
//-----------------------------------------------------------------------------
void CPlayerClnt::UpdateAttacking(uint8 nTracker)
{
	HMODELANIM hCurAnim;
    g_pLTCModel->GetCurAnim(m_hObject, nTracker, hCurAnim);

	uint32 nAnimLen, nAnimTime;
    g_pLTCModel->GetCurAnimLength(m_hObject, nTracker, nAnimLen);
    g_pLTCModel->GetCurAnimTime(m_hObject, nTracker, nAnimTime);

    if(nAnimTime >= nAnimLen)
    {
        PlayMovementAnimation("LSt", m_idUpperBodyTracker);
        m_bAttacking = false;

		HideAttachClientFX();
	}
}



//----------------------------------------------------------------------------
// void CPlayerClnt::PlayAnimation(const char* sAnimName)
//
//-----------------------------------------------------------------------------
void CPlayerClnt::PlayMovementAnimation(const char* sAnimName, uint8 nTracker)
{
	HMODELANIM hAnim = g_pLTClient->GetAnimIndex(m_hObject, (char *)sAnimName);

    HMODELANIM hCurAnim;
    g_pLTCModel->GetCurAnim(m_hObject, nTracker, hCurAnim);
    if(hAnim != hCurAnim)
    {
        g_pLTCModel->SetCurAnim(m_hObject, nTracker, hAnim);
        g_pLTCModel->SetLooping(m_hObject, nTracker, LTTRUE);

        uint32 nAnimLen;
        g_pLTCModel->GetCurAnimLength(m_hObject, nTracker, nAnimLen);

        // Send animation to server
        ILTMessage_Write *pMessage;
        LTRESULT nResult = g_pLTCCommon->CreateMessage(pMessage);

        if( LT_OK == nResult)
        {
            pMessage->IncRef();
            pMessage->Writeuint8(MSG_CS_ANIM);
            pMessage->WriteString(sAnimName);
            pMessage->Writeuint8(nTracker);
            pMessage->Writebool(true);
            g_pLTClient->SendToServer(pMessage->Read(), 0);
            pMessage->DecRef();
        }
    }
}



//----------------------------------------------------------------------------
// void CPlayerClnt::PlayAttackAnimation(const char* sAnimName)
//
//-----------------------------------------------------------------------------
void CPlayerClnt::PlayAttackAnimation(const char* sAnimName, uint8 nTracker)
{
	HMODELANIM hAnim = g_pLTClient->GetAnimIndex(m_hObject, (char *)sAnimName);

    HMODELANIM hCurAnim;
    g_pLTCModel->GetCurAnim(m_hObject, nTracker, hCurAnim);
    if(hAnim != hCurAnim)
    {
        g_pLTCModel->SetCurAnim(m_hObject, nTracker, hAnim);
        g_pLTCModel->SetLooping(m_hObject, nTracker, LTFALSE);

        // Send animation to server
        ILTMessage_Write *pMessage;
        LTRESULT nResult = g_pLTCCommon->CreateMessage(pMessage);

        if( LT_OK == nResult)
        {
            pMessage->IncRef();
            pMessage->Writeuint8(MSG_CS_ANIM);
            pMessage->WriteString(sAnimName);
            pMessage->Writeuint8(nTracker);
            pMessage->Writebool(false);
            g_pLTClient->SendToServer(pMessage->Read(), 0);
            pMessage->DecRef();
        }
        else
        {
            g_pLTClient->CPrint("Failed to create message!");
        }

		ShowAttachClientFX();
	}
}



//----------------------------------------------------------------------------
// void CPlayerClnt::SetClub()
//
//-----------------------------------------------------------------------------
void CPlayerClnt::SetClub(HOBJECT hObj)
{
	m_hClubObject = hObj;

	CreateAttachClientFX();
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPlayerClnt::ShowAttachClientFX
//
//	PURPOSE:	Show all the view attach client fx
//
// ----------------------------------------------------------------------- //

void CPlayerClnt::ShowAttachClientFX()
{
	for ( CLIENTFX_LINK_NODE* pCurr = m_AttachClientFX.m_pNext; pCurr; pCurr = pCurr->m_pNext )
	{
		if ( pCurr->m_Link.IsValid() )
		{
			pCurr->m_Link.GetInstance()->Show();
		}
		else
		{
			// when we hit 0, there are no more
			return;
		}
	}
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPlayerClnt::HideAttachClientFX
//
//	PURPOSE:	Hide all the view attach client fx
//
// ----------------------------------------------------------------------- //

void CPlayerClnt::HideAttachClientFX()
{
	for ( CLIENTFX_LINK_NODE* pCurr = m_AttachClientFX.m_pNext; pCurr; pCurr = pCurr->m_pNext )
	{
		if ( pCurr->m_Link.IsValid() )
		{
			pCurr->m_Link.GetInstance()->Hide();
		}
		else
		{
			// when we hit 0, there are no more
			return;
		}
	}
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPlayerClnt::CreateAttachClientFX
//
//	PURPOSE:	Create the player attach effect.
//
// ----------------------------------------------------------------------- //

void CPlayerClnt::CreateAttachClientFX()
{
	RemoveAttachClientFX();

	if (LTNULL != m_hClubObject)
	{
		const char *pClientFXName = "PlayerPolyTrail";
		CLIENTFX_CREATESTRUCT fxInit( pClientFXName, FXFLAG_LOOP, m_hClubObject );

		CLIENTFX_LINK_NODE* pNewNode = new CLIENTFX_LINK_NODE;

		if(pNewNode)
		{
			g_pClientFXMgr->CreateClientFX( &pNewNode->m_Link, fxInit, true );
			if ( pNewNode->m_Link.IsValid() )
			{
				// start out hidden
				pNewNode->m_Link.GetInstance()->Hide();

				// add the link to the list
				m_AttachClientFX.AddToEnd(pNewNode);
			}
		}
	}
}



// ----------------------------------------------------------------------- //
//
//	ROUTINE:	CPlayerClnt::RemoveAttachClientFX
//
//	PURPOSE:	Destroys all the player attach client fx
//
// ----------------------------------------------------------------------- //

void CPlayerClnt::RemoveAttachClientFX()
{
	ASSERT( 0 != g_pClientFXMgr );

	for ( CLIENTFX_LINK_NODE* pCurr = m_AttachClientFX.m_pNext; pCurr; pCurr = pCurr->m_pNext )
	{
		if ( pCurr->m_Link.IsValid() )
		{
			g_pClientFXMgr->ShutdownClientFX( &pCurr->m_Link );
		}
	}
}

//----------------------------------------------------------------------------
// Fireteam FPS Bowie knife player-view model.
//----------------------------------------------------------------------------
void CPlayerClnt::CreateViewWeapon()
{
    if(m_hViewWeaponObject)
    {
        g_pLTClient->RemoveObject(m_hViewWeaponObject);
        m_hViewWeaponObject = LTNULL;
    }

    const FTWeaponDef *pDef = GetCurrentWeaponDef();
    if(!pDef)
    {
        return;
    }

    FT_WeaponHudSetWeaponDefinition(
        1,
        pDef);

    if(m_bDevWeaponQa)
    {
        FT_WeaponHudSetQaProgress(
            m_nDevWeaponIndex + 1,
            m_nDevWeaponCount);
        FT_WeaponHudSetQaQuarantined(
            FT_IsWeaponQaQuarantined(
                pDef->sSection));
    }
    else
    {
        FT_WeaponHudSetQaProgress(
            0,
            0);
        FT_WeaponHudSetQaQuarantined(
            false);
    }

    char sLocalModelPath[256];
    sprintf(
        sLocalModelPath,
        "rez/%s",
        pDef->sPVModel);

    FILE *pModelFile = fopen(sLocalModelPath, "rb");
    if(!pModelFile)
    {
        g_pLTClient->CPrint(
            "Fireteam: missing staged PV model for %s: %s",
            pDef->sName,
            pDef->sPVModel);
        return;
    }
    fclose(pModelFile);

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;
    ocs.m_Flags = FLAG_VISIBLE | FLAG_REALLYCLOSE;
    ocs.m_Flags2 = FLAG2_DYNAMICDIRLIGHT;
    ocs.m_Pos.Init(pDef->fViewX, pDef->fViewY, pDef->fViewZ);

    FT_CopyWeaponString(
        ocs.m_Filenames[0],
        MAX_CS_FILENAME_LEN,
        pDef->sPVModel);

    if(pDef->sPVAnim[0])
    {
        char sLocalAnimPath[256];
        sprintf(
            sLocalAnimPath,
            "rez/%s",
            pDef->sPVAnim);

        FILE *pAnimFile = fopen(sLocalAnimPath, "rb");
        if(pAnimFile)
        {
            fclose(pAnimFile);
            FT_CopyWeaponString(
                ocs.m_Filenames[1],
                MAX_CS_FILENAME_LEN,
                pDef->sPVAnim);
        }
        else
        {
            g_pLTClient->CPrint(
                "Fireteam: %s has no staged animation companion; using model animations only.",
                pDef->sName);
        }
    }

    if(m_bPlayerDefLoaded &&
       m_PlayerDef.sFirstPersonHandTexture[0])
    {
        FT_CopyWeaponString(
            ocs.m_SkinNames[0],
            MAX_CS_FILENAME_LEN,
            m_PlayerDef.sFirstPersonHandTexture);
    }

    for(uint32 nSkin = 1; nSkin < MAX_MODEL_TEXTURES; ++nSkin)
    {
        FT_CopyWeaponString(
            ocs.m_SkinNames[nSkin],
            MAX_CS_FILENAME_LEN,
            pDef->sPVTexture);
    }

    FT_CopyWeaponString(
        ocs.m_RenderStyleNames[0],
        MAX_CS_FILENAME_LEN,
        "RenderStyles\\DEFAULT.LTB");

    m_hViewWeaponObject = g_pLTClient->CreateObject(&ocs);

    // Some CA archives contain multiple token-matched animation companions.
    // If a staged companion is not compatible with the chosen PV model, retry
    // with the weapon model alone instead of leaving the slot invisible.
    if(!m_hViewWeaponObject && ocs.m_Filenames[1][0])
    {
        g_pLTClient->CPrint(
            "Fireteam: %s PV failed with animation companion %s; retrying model only.",
            pDef->sName,
            ocs.m_Filenames[1]);
        ocs.m_Filenames[1][0] = '\0';
        m_hViewWeaponObject = g_pLTClient->CreateObject(&ocs);
    }

    if(!m_hViewWeaponObject)
    {
        g_pLTClient->CPrint(
            "Fireteam: failed to create %s player-view model: %s",
            pDef->sName,
            pDef->sPVModel);
        return;
    }

    HMODELANIM hSelect = INVALID_MODEL_ANIM;

    if(!pDef->sAnimSelect[0])
    {
        hSelect =
            g_pLTClient->GetAnimIndex(
                m_hViewWeaponObject,
                (char*)"select");
    }

    m_bViewWeaponAction =
        PlayViewWeaponAnimation(
            pDef->sAnimSelect[0]
                ? pDef->sAnimSelect
                : (hSelect != INVALID_MODEL_ANIM
                    ? "select"
                    : "select_0"),
            false);
    PlayViewWeaponSound("SELECT.WAV");
}

bool CPlayerClnt::PlayViewWeaponAnimation(const char* sAnimName, bool bLooping)
{
    if(!m_hViewWeaponObject || !sAnimName)
    {
        return false;
    }

    HMODELANIM hAnim =
        g_pLTClient->GetAnimIndex(
            m_hViewWeaponObject,
            (char*)sAnimName);

    if(hAnim == INVALID_MODEL_ANIM)
    {
        g_pLTClient->CPrint(
            "Fireteam: weapon animation missing: %s",
            sAnimName);
        return false;
    }

    g_pLTCModel->SetCurAnim(
        m_hViewWeaponObject,
        MAIN_TRACKER,
        hAnim);
    g_pLTCModel->SetLooping(
        m_hViewWeaponObject,
        MAIN_TRACKER,
        bLooping ? LTTRUE : LTFALSE);
    return true;
}

void CPlayerClnt::PlayViewWeaponSound(const char* sFilename)
{
    if(!sFilename || !sFilename[0])
    {
        return;
    }

    const FTWeaponDef *pDef = GetCurrentWeaponDef();
    if(!pDef || !pDef->sSoundDir[0])
    {
        return;
    }

    PlaySoundInfo psi;
    PLAYSOUNDINFO_INIT(psi);
    psi.m_dwFlags = PLAYSOUND_LOCAL;

    sprintf(
        psi.m_szSoundName,
        "%s/%s",
        pDef->sSoundDir,
        sFilename);

    HLTSOUND hSound = LTNULL;
    g_pLTCSoundMgr->PlaySound(&psi, hSound);
}
void CPlayerClnt::UpdateViewWeaponAnimation()
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
        const FTWeaponDef *pDef =
            GetCurrentWeaponDef();

        HMODELANIM hIdle = INVALID_MODEL_ANIM;

        if((!pDef ||
            !pDef->sAnimIdle[0]) &&
           m_hViewWeaponObject)
        {
            hIdle =
                g_pLTClient->GetAnimIndex(
                    m_hViewWeaponObject,
                    (char*)"idle_0");
        }

        PlayViewWeaponAnimation(
            pDef && pDef->sAnimIdle[0]
                ? pDef->sAnimIdle
                : (hIdle != INVALID_MODEL_ANIM
                    ? "idle_0"
                    : "idle"),
            true);

        m_bViewWeaponAction = false;
    }
}

void CPlayerClnt::UpdateWeaponView(bool bFirstPerson)
{
    if(m_hViewWeaponObject)
    {
        g_pLTCCommon->SetObjectFlags(
            m_hViewWeaponObject,
            OFT_Flags,
            bFirstPerson ? FLAG_VISIBLE : 0,
            FLAG_VISIBLE);

        if(bFirstPerson)
        {
            // FLAG_REALLYCLOSE models use camera-relative coordinates. NOLF2
            // updates the player-view weapon position continuously; do the same
            // so the authored Combat Arms PV offset is not lost after creation.
            LTVector vViewPos(0.0f, 0.0f, 0.0f);
            const FTWeaponDef *pDef = GetCurrentWeaponDef();
            if(pDef)
            {
                vViewPos.Init(
                    pDef->fViewX,
                    pDef->fViewY,
                    pDef->fViewZ);
            }

            g_pLTClient->SetObjectPos(m_hViewWeaponObject, &vViewPos);
            UpdateViewWeaponAnimation();
        }
    }

    if(m_hClubObject)
    {
        g_pLTCCommon->SetObjectFlags(
            m_hClubObject,
            OFT_Flags,
            bFirstPerson ? 0 : FLAG_VISIBLE,
            FLAG_VISIBLE);
    }
}