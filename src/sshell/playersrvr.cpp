//------------------------------------------------------------------------------//
//
// MODULE	: playersrvr.cpp
//
// PURPOSE	: PlayerSrvr - Implementation
//
// CREATED	: 07/15/2002
//
// (c) 2002 LithTech, Inc.	All Rights Reserved
//
//------------------------------------------------------------------------------//

#include "playersrvr.h"

#include <ltobjectcreate.h>
#include <iltmodel.h>
#include <iltphysics.h>

#include "serverinterfaces.h"
#include "msgids.h"
#include "animids.h"
#include "pickup.h"
#include "projectile.h"

#include "serverutilities.h"
#include <iltphysics.h>
#include <ltobjectcreate.h>
#include <iltcommon.h>
#include "FxFlags.h"
#include "statsmanager.h"
#include <string.h>
#include "FireteamPoisonGas.h"
#include "FireteamExplosiveProjectile.h"


//-----------------------------------------------------------------------------
BEGIN_CLASS(CPlayerSrvr)
END_CLASS_DEFAULT_FLAGS(CPlayerSrvr, BaseClass, LTNULL, LTNULL, CF_HIDDEN)
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



static bool FTPenetrationGeometryFilter(
    HOBJECT hObject,
    void *pUserData)
{
    FTFireFilterData *pData =
        (FTFireFilterData*)pUserData;

    if(pData &&
       (hObject == pData->hPlayer ||
        hObject == pData->hWeapon))
    {
        return false;
    }

    HCLASS hClass =
        g_pLTServer->GetObjectClass(
            hObject);

    HCLASS hZombie =
        g_pLTServer->GetClass(
            "FireteamZombie");
    HCLASS hSeal =
        g_pLTServer->GetClass(
            "Seal");
    HCLASS hPlayer =
        g_pLTServer->GetClass(
            "CPlayerSrvr");

    // Exit-thickness probes are only looking for the far side of the solid
    // geometry. Characters behind the wall must not be mistaken for its exit.
    if(hClass &&
       ((hZombie &&
         g_pLTServer->IsKindOf(
             hClass,
             hZombie)) ||
        (hSeal &&
         g_pLTServer->IsKindOf(
             hClass,
             hSeal)) ||
        (hPlayer &&
         g_pLTServer->IsKindOf(
             hClass,
             hPlayer))))
    {
        return false;
    }

    return true;
}



enum FTFireteamHitRegion
{
    FT_HITREGION_BODY = 0,
    FT_HITREGION_HEAD,
    FT_HITREGION_GROIN
};

static FTFireteamHitRegion FT_GetZombieHitRegion(
    HOBJECT hTarget,
    const LTVector &vHitPoint)
{
    if(!hTarget)
    {
        return FT_HITREGION_BODY;
    }

    LTVector vTargetPos;
    g_pLTServer->GetObjectPos(
        hTarget,
        &vTargetPos);

    LTVector vDims(
        18.0f,
        42.0f,
        18.0f);

    LTVector vModelDims;
    if(g_pLTSCommon->GetModelAnimUserDims(
           hTarget,
           &vModelDims,
           g_pLTServer->GetModelAnimation(
               hTarget)) == LT_OK &&
       vModelDims.y > 10.0f &&
       vModelDims.y < 200.0f)
    {
        vDims =
            vModelDims;
    }

    const float fHeight =
        vDims.y * 2.0f;

    if(fHeight <= 1.0f)
    {
        return FT_HITREGION_BODY;
    }

    const float fBottom =
        vTargetPos.y -
        vDims.y;

    float fNormalizedY =
        (vHitPoint.y -
         fBottom) /
        fHeight;

    if(fNormalizedY < 0.0f)
    {
        fNormalizedY = 0.0f;
    }
    else if(fNormalizedY > 1.0f)
    {
        fNormalizedY = 1.0f;
    }

    // Top ~28% of the collision/model height is the head/neck zone.
    if(fNormalizedY >= 0.72f)
    {
        return FT_HITREGION_HEAD;
    }

    // Combat Arms-style "Nut Shot" region: narrow lower-torso/pelvis band.
    if(fNormalizedY >= 0.42f &&
       fNormalizedY <= 0.52f)
    {
        return FT_HITREGION_GROIN;
    }

    return FT_HITREGION_BODY;
}

static const char* FT_HitRegionName(
    FTFireteamHitRegion eRegion)
{
    switch(eRegion)
    {
        case FT_HITREGION_HEAD:
            return "HEADSHOT";

        case FT_HITREGION_GROIN:
            return "NUTSHOT";

        default:
            return "BODY";
    }
}


//-----------------------------------------------------------------------------
//	CPlayerSrvr::EngineMessageFn(uint32 messageID, void *pData, float fData)
//
//-----------------------------------------------------------------------------
uint32 CPlayerSrvr::EngineMessageFn(uint32 messageID, void *pData, float fData)
{

	switch (messageID)
	{
	case MID_PRECREATE:
		return PreCreate(pData, fData);
    case MID_INITIALUPDATE:
		{
			g_pLTServer->SetNextUpdate(m_hObject, 0.0166f);
			return InitialUpdate(pData, fData);
		}
	case MID_UPDATE:
		{

            if(!m_bAlive)
            {
                m_fRespawnTimer -= 0.25f;
                if(m_fRespawnTimer <= 0.0f)
                {
                    Respawn();
                }
            }
            else
            {
                UpdateHazards();
                CompleteReloadIfReady();
                UpdatePowerups();
            }

            //Do we need to send score stats?

            if(m_bSendStats)
            {
                if(m_iSendStatsCounter == 0)
                {

                    // Get the stats
                    uint32 iNumPlayers = g_pStatsManager->GetNumPlayers();
                    SCORESTRUCT *scoreStruct = new SCORESTRUCT[iNumPlayers];
                    g_pStatsManager->GetPlayerScores(scoreStruct);


                    //Send the stats
	                ILTMessage_Write *pMsg;
	                LTRESULT nResult = g_pLTSCommon->CreateMessage(pMsg);
	                pMsg->IncRef();
	                pMsg->Writeint8(MSG_SERVER_SCORES);
                    pMsg->Writeint8(static_cast<uint8>(iNumPlayers));

                    //Write each entry to the message
                    for(uint8 i = 0; i <iNumPlayers; i++)
                    {
                        pMsg->Writeuint32(scoreStruct[i].iClientID);
                        pMsg->WriteString(scoreStruct[i].sPlayerName);
                        pMsg->Writeuint32(scoreStruct[i].iScore);
                        pMsg->Writefloat(scoreStruct[i].fMoney);
                    }

                    //pMsg->WriteObject(m_hClub);
	                g_pLTServer->SendToClient(pMsg->Read(), m_hClient,  MESSAGE_GUARANTEED);
	                pMsg->DecRef();
                    //Reset the counter
                    m_iSendStatsCounter = (uint8)(2.0f / 0.25f); //desired delay divided by the update frequency
                }else
                {
                    --m_iSendStatsCounter;
                }
            }
            g_pLTServer->SetNextUpdate(m_hObject, 0.25f);


/*
			g_pLTSModel->UpdateModelOBB(m_hClub, &m_WeaponOBB);

			//m_WeaponOBB.m_Pos
			//g_pLTServer->SetObjectPos(m_DebugSphere, &m_WeaponOBB.m_Pos);
			//g_pLTServer->SetNextUpdate(m_hObject, 0.0166f);
			HMODELNODE hNode;
			LTRESULT result = g_pLTSModel->GetNode(m_hClub, "joint1", hNode);
			if(LT_OK != result)
			{
				g_pLTServer->CPrint("(CPlayerSrvr) Error obtaining Node!");
				//return;
			}
			else
			{
				LTransform tTransform;
				g_pLTSModel->GetNodeTransform(m_hClub, hNode, tTransform, true);

				LTVector vPos = tTransform.m_Pos + (tTransform.m_Rot.Up() * m_WeaponOBB.m_Pos.y);

				g_pLTServer->SetObjectPos(m_DebugSphere, &vPos);
				g_pLTServer->SetNextUpdate(m_hObject, 0.0166f);
			}
			*/

		}
		break;
    case MID_MODELSTRINGKEY:
        {
			ArgList* pArgList = (ArgList*)pData;

			char szBuffer[256];
			sprintf(szBuffer, "");

			for ( int i = 0 ; i < pArgList->argc ; i++ )
			{
				//g_pLTServer->CPrint("MODEL KEY: %s", pArgList->argv[i]);
                if(strcmp("Throw", pArgList->argv[i]) == 0)
                {
                    CheckForHit();
                }
                else if(strcmp("FOOTSTEP_KEY_1", pArgList->argv[i]) == 0)
                {
                    PlayClientFX("SnowStep1", m_hObject, LTNULL, LTNULL, 0);
                }
                else if(strcmp("FOOTSTEP_KEY_2", pArgList->argv[i]) == 0)
                {
                    PlayClientFX("SnowStep2", m_hObject, LTNULL, LTNULL, 0);
                }
			}
            return 1;
        }
		break;

	default:
		break;
	}

	// Pass the message along to parent class.
	return BaseClass::EngineMessageFn(messageID, pData, fData);
}



//-----------------------------------------------------------------------------
//	CPlayerSrvr::ObjectMessageFn(HOBJECT hSender, void *pData, float fData)
//
//-----------------------------------------------------------------------------
uint32 CPlayerSrvr::ObjectMessageFn(HOBJECT hSender, ILTMessage_Read *pMsg)
{
	pMsg->SeekTo(0);
	uint32 messageID = pMsg->Readuint32();
	switch(messageID)
	{
        case OBJ_MID_DAMAGE:
            {
                HCLASS hPlayerClass = g_pLTServer->GetClass("CPlayerSrvr");
                HCLASS hSenderClass = hSender ? g_pLTServer->GetObjectClass(hSender) : LTNULL;

                // Co-op invariant: damage sent directly by another player is ignored.
                if(hPlayerClass && hSenderClass && g_pLTServer->IsKindOf(hSenderClass, hPlayerClass))
                {
                    break;
                }

                uint8 nDamage = pMsg->Readuint8();
                ApplyDamage(nDamage);
            }
            break;
		case OBJ_MID_PICKUP:
            {
                uint8 nPickupType   = pMsg->Readuint8();
                float fPickupValue        = pMsg->Readfloat();

                g_pLTServer->CPrint("PlayerSrvr -> OBJ_MID_PICKUP  %d  %f", nPickupType, fPickupValue);
            }
            break;
        case OBJ_MID_KILLSCORE_SNOWMAN:
        case OBJ_MID_KILLSCORE:
            {
                //if it's a snowman don't inc the score.
                m_iScore+= (OBJ_MID_KILLSCORE_SNOWMAN == messageID) ? 0:1; 
                //m_fMoney += 9.95f;
				m_fMoney += pMsg->Readfloat();

                //send score to client
            	ILTMessage_Write *pMessage;
	            LTRESULT nResult = g_pLTSCommon->CreateMessage(pMessage);
	            pMessage->IncRef();
	            pMessage->Writeint8(MSG_CS_SCORE);
	            pMessage->Writeuint32(m_iScore);
                pMessage->Writefloat(m_fMoney);
	            g_pLTServer->SendToClient(pMessage->Read(), m_hClient,  MESSAGE_GUARANTEED);
	            pMessage->DecRef();

                //Play smack talk
                float fRand = g_pLTServer->Random(1.0f, 100.0f);

                g_pLTServer->CPrint("rand: %f", fRand);

                // Pick one and Play it
                if( fRand < 20.0f ) 
                {
					const int NumSmackTalkSounds = 12;
					int Index = g_pLTServer->IntRandom(0, NumSmackTalkSounds - 1);
                    char buf[32];
                    sprintf(buf, "SmackTalk%d", Index);
                    PlayClientFX(buf, m_hObject, LTNULL, LTNULL, 0);
                    g_pLTServer->CPrint("Play SmackTalk FX: %s", buf);

                    PlayClientFX("SmackTalkIcon", m_hObject, LTNULL, LTNULL, 0);
                }
            }
            break;
        default:
            break;
    }

    return BaseClass::ObjectMessageFn(hSender, pMsg);
}



//-----------------------------------------------------------------------------
//	CPlayerSrvr::SetPlayerName(const char* name)
//
//-----------------------------------------------------------------------------
void CPlayerSrvr::SetPlayerName(const char* name)
{
    if(!name)
    {
        m_sName[0] = '\0';
        return;
    }

    strncpy(
        m_sName,
        name,
        sizeof(m_sName) - 1);
    m_sName[sizeof(m_sName) - 1] = '\0';
}



//-----------------------------------------------------------------------------
//	CPlayerSrvr::GetPlayerName()
//
//-----------------------------------------------------------------------------
char* CPlayerSrvr::GetPlayerName()
{
    return m_sName;
}



//-----------------------------------------------------------------------------
//	CPlayerSrvr::PreCreate(void *pData, float fData)
//
//-----------------------------------------------------------------------------
uint32 CPlayerSrvr::PreCreate(void *pData, float fData)
{
	// Let parent class handle it first
	BaseClass::EngineMessageFn(MID_PRECREATE, pData, fData);

	// Cast pData to a ObjectCreateStruct* for convenience
	ObjectCreateStruct* pStruct = (ObjectCreateStruct*)pData;

    pStruct->m_Flags |= FLAG_MODELKEYS;

	// Set the object type to OT_MODEL
	pStruct->m_ObjectType = OT_MODEL;

	// Check to see if this is coming from a world file
	if(fData == PRECREATE_WORLDFILE)
	{
		ReadProps(pStruct);
	}

	// Return default of 1
	return 1;
}



//-----------------------------------------------------------------------------
//	CPlayerSrvr::InitialUpdate(void *pData, float fData)
//
//-----------------------------------------------------------------------------
uint32 CPlayerSrvr::InitialUpdate(void *pData, float fData)
{
    g_pLTServer->GetObjectPos(m_hObject, &m_vSpawnPos);
    g_pLTServer->GetObjectRotation(m_hObject, &m_rSpawnRot);

    // Set up animation trackers
    m_idUpperBodyTracker = ANIM_UPPER;
    g_pLTSModel->AddTracker(m_hObject, m_idUpperBodyTracker);

    m_idLowerBodyTracker = ANIM_LOWER;
    g_pLTSModel->AddTracker(m_hObject, m_idLowerBodyTracker);

    // Set up weight sets

    if ( LT_OK == g_pLTSModel->FindWeightSet(m_hObject, "Upper", m_hWeightUpper) )
    {
        g_pLTSModel->SetWeightSet(m_hObject, m_idUpperBodyTracker, m_hWeightUpper);
    }

    if ( LT_OK == g_pLTSModel->FindWeightSet(m_hObject, "Lower", m_hWeightLower) )
    {
        g_pLTSModel->SetWeightSet(m_hObject, m_idLowerBodyTracker, m_hWeightLower);
    }

    // Get hand socket
    LTRESULT SocketResult = g_pLTSModel->GetSocket(m_hObject, "RightHand", hRightHandSocket);
    if(LT_OK == SocketResult)
    {
        g_pLTServer->CPrint("Got the RightHand socket");
    }

    //Create and attach club
    HCLASS hClass = g_pLTServer->GetClass("BaseClass");
    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;
    ocs.m_Flags = FLAG_VISIBLE | FLAG_FORCECLIENTUPDATE | FLAG_SHADOW;
    ocs.m_Flags2 = FLAG2_DISABLEPREDICTION;
    const FTWeaponDef *pInitialWeapon =
        FT_GetWeaponDef(
            m_WeaponDefs,
            m_nWeaponSlot);

    if(pInitialWeapon)
    {
        FT_CopyWeaponString(
            ocs.m_Filename,
            sizeof(ocs.m_Filename),
            pInitialWeapon->sHHModel);
        FT_CopyWeaponString(
            ocs.m_SkinName,
            sizeof(ocs.m_SkinName),
            pInitialWeapon->sHHTexture);
    }

    BaseClass *pClubObj = (BaseClass*)g_pLTServer->CreateObject(hClass, &ocs);

    if(pClubObj)
    {
        m_hClub = pClubObj->m_hObject;
        //Now attach it

        LTVector vOffset;
        LTVector rOffset;

        vOffset.Init();
        rOffset.Init();
        CreateAttachment(m_hClubAttach, m_hClub, "RightHand", rOffset, vOffset);

		//Get the radius and OBB for this weapon
		uint32 iNumOBBS;
		g_pLTSModel->GetNumModelOBBs(m_hClub, iNumOBBS);

        if(iNumOBBS > 0)
        {
            ModelOBB *pWeaponOBBs = new ModelOBB[iNumOBBS];
            if(g_pLTSModel->GetModelOBBCopy(m_hClub, pWeaponOBBs) == LT_OK)
            {
                // Use the largest OBB as the simple melee volume for now.
                uint32 nLargest = 0;
                float fLargestVolume = -1.0f;

                for(uint32 nOBB = 0; nOBB < iNumOBBS; ++nOBB)
                {
                    float fVolume = pWeaponOBBs[nOBB].m_Size.x *
                                    pWeaponOBBs[nOBB].m_Size.y *
                                    pWeaponOBBs[nOBB].m_Size.z;
                    if(fVolume > fLargestVolume)
                    {
                        fLargestVolume = fVolume;
                        nLargest = nOBB;
                    }
                }

                m_WeaponOBB = pWeaponOBBs[nLargest];
            }
            delete [] pWeaponOBBs;
        }
    }

    return 1;
}



//-----------------------------------------------------------------------------
//	CPlayerSrvr::ReadProps(ObjectCreateStruct* pStruct)
//
//-----------------------------------------------------------------------------
void CPlayerSrvr::ReadProps(ObjectCreateStruct* pStruct)
{
	g_pLTServer->GetPropString("Name", pStruct->m_Filename, MAX_CS_FILENAME_LEN);
}



//-----------------------------------------------------------------------------
//	CPlayerSrvr::PlayAnimation(const char* sAnimName, uint8 nTracker)
//
//-----------------------------------------------------------------------------
void CPlayerSrvr::PlayAnimation(const char* sAnimName, uint8 nTracker, bool bLooping)
{
		HMODELANIM hAnim = g_pLTServer->GetAnimIndex(m_hObject, (char *)sAnimName);
        g_pLTSModel->SetCurAnim(m_hObject, nTracker, hAnim);
        g_pLTSModel->SetLooping(m_hObject, nTracker, bLooping);
}



//-----------------------------------------------------------------------------
//	CPlayerSrvr::CreateAttachment()
//
//-----------------------------------------------------------------------------
void CPlayerSrvr::CreateAttachment(HATTACHMENT &hAttachment, HOBJECT hChildObject, const char* sSocket, LTVector &vRotOffset, LTVector &vPosOffset)
{
	LTVector vOffset;
    LTRotation rOffset;

    vOffset.Init();
    rOffset.Init();
    hAttachment = NULL;

    HMODELSOCKET hSocket;
    LTRESULT SocketResult = g_pLTSModel->GetSocket(m_hObject, sSocket, hSocket);
    if(LT_OK == SocketResult)
    {
        g_pLTServer->CPrint("Got the socket");
    }
	else
    {
        g_pLTServer->CPrint("GetSocket failed: " );
    }

    LTransform tSocketTransform;
    LTRESULT SocketTrResult = g_pLTSModel->GetSocketTransform(m_hObject, hSocket, tSocketTransform, LTTRUE);
    if(LT_OK == SocketTrResult)
    {
        g_pLTServer->CPrint("Got the transform");
    }
	else
    {
        g_pLTServer->CPrint("GetTrasform failed: ");
    }

    // We need to adjust the rotation of our weapon before we make the attachment
    rOffset.Rotate(rOffset.Right(), MATH_DEGREES_TO_RADIANS(vRotOffset.x));
    rOffset.Rotate(rOffset.Up(), MATH_DEGREES_TO_RADIANS(vRotOffset.y));
    rOffset.Rotate(rOffset.Forward(), MATH_DEGREES_TO_RADIANS(vRotOffset.z));

    LTRESULT resultAttachment = g_pLTServer->CreateAttachment(m_hObject, hChildObject, sSocket, &vOffset, &rOffset, &hAttachment);

    if(LT_OK != resultAttachment)
    {
        g_pLTServer->CPrint("Error creating attachment!!");
    }
}



//-----------------------------------------------------------------------------
//	CPlayerSrvr::CheckForHit()
//
//-----------------------------------------------------------------------------
void CPlayerSrvr::CheckForHit()
{
    const FTWeaponDef *pDef = FT_GetWeaponDef(
        m_WeaponDefs,
        m_nWeaponSlot);

    if(!pDef || pDef->eType != FT_WEAPON_MELEE)
    {
        return;
    }

    IntersectQuery qInfo;
    IntersectInfo iInfo;

    LTVector vPos;
    LTRotation rRot;

    g_pLTServer->GetObjectPos(m_hObject, &vPos);
    g_pLTServer->GetObjectRotation(m_hObject, &rRot);

    LTVector vForward = rRot.Forward();
    vPos.y -= 30.0f;

    qInfo.m_From = vPos + vForward;
    qInfo.m_To   = vPos + (vForward * pDef->fRange);
    qInfo.m_Flags = INTERSECT_OBJECTS;

    if(!g_pLTServer->IntersectSegment(&qInfo, &iInfo) ||
       !iInfo.m_hObject ||
       g_pLTSPhysics->IsWorldObject(iInfo.m_hObject) == LT_YES)
    {
        return;
    }

    HCLASS hTarget = g_pLTServer->GetObjectClass(iInfo.m_hObject);
    HCLASS hClassSeal = g_pLTServer->GetClass("Seal");
    HCLASS hClassSnowman = g_pLTServer->GetClass("Snowman");
    HCLASS hClassZombie = g_pLTServer->GetClass("FireteamZombie");

    const bool bSeal =
        hClassSeal && hTarget &&
        g_pLTServer->IsKindOf(hTarget, hClassSeal);

    const bool bSnowman =
        hClassSnowman && hTarget &&
        g_pLTServer->IsKindOf(hTarget, hClassSnowman);

    const bool bZombie =
        hClassZombie && hTarget &&
        g_pLTServer->IsKindOf(hTarget, hClassZombie);

    if(!bSeal && !bSnowman && !bZombie)
    {
        // Fireteam co-op: friendly fire is permanently disabled.
        return;
    }

    if(bSeal)
    {
        PlaySound(1);
    }
    else if(bSnowman)
    {
        PlaySound(2);
    }

    ILTMessage_Write *pMsg = LTNULL;
    if(g_pLTSCommon->CreateMessage(pMsg) == LT_OK && pMsg)
    {
        pMsg->IncRef();
        pMsg->Writeuint32(OBJ_MID_DAMAGE);
        pMsg->Writeuint8(pDef->nDamage);
        g_pLTServer->SendToObject(
            pMsg->Read(),
            m_hObject,
            iInfo.m_hObject,
            0);
        pMsg->DecRef();
    }
}



//-----------------------------------------------------------------------------
//	PickUp::PlaySound()
//
//-----------------------------------------------------------------------------
void CPlayerSrvr::PlaySound(int i)
{
	// Send clientfx message
	uint32 dwFxFlags = 0;

    switch(i)
    {
        case 1:
	    {
		    PlayClientFX("SealImpact", m_hClub, LTNULL, LTNULL, dwFxFlags);
	    }break;
        case 2:
	    {
		    PlayClientFX("SnowmanImpact", m_hClub, LTNULL, LTNULL, dwFxFlags);
	    }break;
        case 3:
	    {
		    PlayClientFX("PlayerImpact", m_hClub, LTNULL, LTNULL, dwFxFlags);
	    }break;


        default:
            break;
    }

}



//-----------------------------------------------------------------------------
//	PickUp::SetClubID()
//
//-----------------------------------------------------------------------------
void CPlayerSrvr::SetClubID()
{
   	// Send over the position of the startpoint so the client can position the player
	ILTMessage_Write *pMsg;
	LTRESULT nResult = g_pLTSCommon->CreateMessage(pMsg);
	pMsg->IncRef();
	pMsg->Writeint8(MSG_CS_MY_CLUB);
    pMsg->WriteObject(m_hClub);
	g_pLTServer->SendToClient(pMsg->Read(), m_hClient,  MESSAGE_GUARANTEED);
	pMsg->DecRef();
}

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
    g_pLTServer->CPrint(
        "Fireteam: %s took %u damage (%u/%u HP).",
        m_sName,
        (uint32)nDamage,
        (uint32)m_nHealth,
        (uint32)m_nMaxHealth);
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
    // PoisonGas world objects stay registered for map compatibility, but
    // environmental damage remains OFF until CA's actual safe/outside volume
    // semantics are reproduced. This prevents indoor spawn from being poisoned.
    m_fPoisonCarry = 0.0f;
}

void CPlayerSrvr::Respawn()
{
    m_bAlive = true;
    m_nHealth = m_nMaxHealth;
    m_fRespawnTimer = 0.0f;
    m_fPoisonCarry = 0.0f;
    m_bReloading = false;
    m_nReloadSlot = 0;
    m_fBottomlessUntil = 0.0f;
    m_fOneHitUntil = 0.0f;
    m_bBottomlessWasActive = false;
    m_bOneHitWasActive = false;

    for(uint8 nSlot = 1; nSlot <= 5; ++nSlot)
    {
        m_nWeaponAmmoInClip[nSlot] = m_WeaponDefs[nSlot].nClipSize;
        m_nWeaponAmmoReserve[nSlot] = m_WeaponDefs[nSlot].nStartReserve;
        m_fNextWeaponShot[nSlot] = 0.0f;
    }

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
    SendPrimaryAmmo();
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
void CPlayerSrvr::NotifyPowerup(
    const char *pText,
    float fSeconds)
{
    if(!m_hClient ||
       !pText ||
       !pText[0])
    {
        return;
    }

    ILTMessage_Write *pMsg = LTNULL;

    if(g_pLTSCommon->CreateMessage(
        pMsg) != LT_OK ||
       !pMsg)
    {
        return;
    }

    pMsg->IncRef();
    pMsg->Writeuint8(
        MSG_SC_POWERUP);
    pMsg->WriteString(
        pText);
    pMsg->Writefloat(
        fSeconds);

    g_pLTServer->SendToClient(
        pMsg->Read(),
        m_hClient,
        MESSAGE_GUARANTEED);

    pMsg->DecRef();
}

void CPlayerSrvr::SendPowerupState()
{
    if(!m_hClient)
    {
        return;
    }

    const float fNow =
        g_pLTServer->GetTime();

    float fBottomless =
        m_bBottomlessWasActive
        ? (m_fBottomlessUntil - fNow)
        : 0.0f;

    float fOneHit =
        m_bOneHitWasActive
        ? (m_fOneHitUntil - fNow)
        : 0.0f;

    if(fBottomless < 0.0f)
        fBottomless = 0.0f;

    if(fOneHit < 0.0f)
        fOneHit = 0.0f;

    ILTMessage_Write *pMsg = LTNULL;

    if(g_pLTSCommon->CreateMessage(
        pMsg) != LT_OK ||
       !pMsg)
    {
        return;
    }

    pMsg->IncRef();
    pMsg->Writeuint8(
        MSG_SC_POWERUP_STATE);
    pMsg->Writefloat(
        fBottomless);
    pMsg->Writefloat(
        fOneHit);

    g_pLTServer->SendToClient(
        pMsg->Read(),
        m_hClient,
        MESSAGE_GUARANTEED);

    pMsg->DecRef();
}

void CPlayerSrvr::GrantAmmoMagazines(
    uint32 nMagazines)
{
    if(nMagazines == 0)
    {
        return;
    }

    for(uint8 nSlot = 1;
        nSlot <= 5;
        ++nSlot)
    {
        const FTWeaponDef *pDef =
            FT_GetWeaponDef(
                m_WeaponDefs,
                nSlot);

        if(!pDef ||
           pDef->eType ==
               FT_WEAPON_MELEE ||
           pDef->nClipSize == 0)
        {
            continue;
        }

        uint32 nAdd =
            (uint32)pDef->nClipSize *
            nMagazines;

        uint32 nReserve =
            (uint32)
                m_nWeaponAmmoReserve[nSlot] +
            nAdd;

        if(nReserve > 65535)
        {
            nReserve = 65535;
        }

        m_nWeaponAmmoReserve[nSlot] =
            (uint16)nReserve;
    }

    SendPrimaryAmmo();
}

void CPlayerSrvr::GrantHealth(
    uint32 nAmount)
{
    if(!m_bAlive ||
       nAmount == 0)
    {
        return;
    }

    uint32 nHealth =
        (uint32)m_nHealth +
        nAmount;

    if(nHealth >
       (uint32)m_nMaxHealth)
    {
        nHealth =
            m_nMaxHealth;
    }

    m_nHealth =
        (uint8)nHealth;

    SendHealth();
}

void CPlayerSrvr::GrantBottomless(
    float fSeconds)
{
    if(fSeconds <= 0.0f)
    {
        return;
    }

    const float fUntil =
        g_pLTServer->GetTime() +
        fSeconds;

    if(fUntil >
       m_fBottomlessUntil)
    {
        m_fBottomlessUntil =
            fUntil;
    }

    const FTWeaponDef *pDef =
        FT_GetWeaponDef(
            m_WeaponDefs,
            m_nWeaponSlot);

    if(pDef &&
       pDef->eType !=
           FT_WEAPON_MELEE &&
       pDef->nClipSize > 0)
    {
        m_nWeaponAmmoInClip[
            m_nWeaponSlot] =
            pDef->nClipSize;

        SendPrimaryAmmo();
    }

    m_bBottomlessWasActive =
        true;

    SendPowerupState();
}

void CPlayerSrvr::GrantOneHit(
    float fSeconds)
{
    if(fSeconds <= 0.0f)
    {
        return;
    }

    const float fUntil =
        g_pLTServer->GetTime() +
        fSeconds;

    if(fUntil >
       m_fOneHitUntil)
    {
        m_fOneHitUntil =
            fUntil;
    }

    m_bOneHitWasActive =
        true;

    SendPowerupState();
}

void CPlayerSrvr::UpdatePowerups()
{
    const float fNow =
        g_pLTServer->GetTime();

    bool bChanged = false;

    if(m_bBottomlessWasActive &&
       fNow >= m_fBottomlessUntil)
    {
        m_bBottomlessWasActive =
            false;
        bChanged = true;
    }

    if(m_bOneHitWasActive &&
       fNow >= m_fOneHitUntil)
    {
        m_bOneHitWasActive =
            false;
        bChanged = true;
    }

    // Do not queue a delayed center-screen "ENDED" announcement. The
    // persistent HUD timer disappears from this authoritative state instead,
    // which also behaves correctly after alt-tab / renderer reactivation.
    if(bChanged)
    {
        SendPowerupState();
    }
}

//-----------------------------------------------------------------------------
// Fireteam loadout / primary weapon.
//-----------------------------------------------------------------------------
void CPlayerSrvr::SetWeaponSlot(uint8 nSlot)
{
    const FTWeaponDef *pDef = FT_GetWeaponDef(m_WeaponDefs, nSlot);
    if(!pDef)
    {
        return;
    }

    m_bReloading = false;
    m_nReloadSlot = 0;
    m_nWeaponSlot = nSlot;
    SendPrimaryAmmo();

    if(!m_hClub)
    {
        return;
    }

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;

    FT_CopyWeaponString(
        ocs.m_Filename,
        sizeof(ocs.m_Filename),
        pDef->sHHModel);
    FT_CopyWeaponString(
        ocs.m_SkinName,
        sizeof(ocs.m_SkinName),
        pDef->sHHTexture);

    g_pLTSCommon->SetObjectFilenames(m_hClub, &ocs);

    g_pLTServer->CPrint(
        "Fireteam weapon: slot %u %s ammo=%u/%u",
        (uint32)m_nWeaponSlot,
        pDef->sName,
        (uint32)m_nWeaponAmmoInClip[m_nWeaponSlot],
        (uint32)m_nWeaponAmmoReserve[m_nWeaponSlot]);
}

void CPlayerSrvr::FirePrimary(
    const LTVector &vFrom,
    const LTVector &vDirection)
{
    if(!m_bAlive)
    {
        return;
    }

    const FTWeaponDef *pDef = FT_GetWeaponDef(
        m_WeaponDefs,
        m_nWeaponSlot);

    if(!pDef || pDef->eType == FT_WEAPON_MELEE)
    {
        return;
    }

    CompleteReloadIfReady();
    if(m_bReloading && m_nReloadSlot == m_nWeaponSlot)
    {
        return;
    }

    const float fNow = g_pLTServer->GetTime();
    if(fNow < m_fNextWeaponShot[m_nWeaponSlot])
    {
        return;
    }

    if(m_nWeaponAmmoInClip[m_nWeaponSlot] == 0)
    {
        SendPrimaryAmmo();

        if(pDef->bAutoReload)
        {
            ReloadWeapon();
        }

        return;
    }

    LTVector vDir = vDirection;
    if(vDir.MagSqr() < 0.0001f)
    {
        return;
    }
    vDir.Normalize();

    LTVector vPlayerPos;
    g_pLTServer->GetObjectPos(
        m_hObject,
        &vPlayerPos);

    // The client fires from the actual first-person camera. The old server
    // prototype replaced that with player-origin +30, which can put the
    // authoritative ray inside a nearby doorframe even though the crosshair
    // has a clear line. Trust the camera position only while it remains close
    // enough to the authoritative player body to be physically plausible.
    LTVector vServerFrom = vPlayerPos;
    vServerFrom.y += 65.0f;

    LTVector vReportedOffset =
        vFrom - vPlayerPos;

    if(vReportedOffset.x >= -32.0f &&
       vReportedOffset.x <= 32.0f &&
       vReportedOffset.z >= -32.0f &&
       vReportedOffset.z <= 32.0f &&
       vReportedOffset.y >= -20.0f &&
       vReportedOffset.y <= 110.0f)
    {
        vServerFrom = vFrom;
    }

    m_fNextWeaponShot[m_nWeaponSlot] =
        fNow + pDef->fFireInterval;

    const bool bBottomless =
        fNow <
        m_fBottomlessUntil;

    if(!bBottomless)
    {
        --m_nWeaponAmmoInClip[
            m_nWeaponSlot];

        SendPrimaryAmmo();

        if(m_nWeaponAmmoInClip[
               m_nWeaponSlot] == 0 &&
           pDef->bAutoReload)
        {
            ReloadWeapon();
        }
    }

    if(pDef->eType == FT_WEAPON_GRENADE ||
       pDef->eType == FT_WEAPON_ROCKET)
    {
        SpawnExplosiveProjectile(
            *pDef,
            vServerFrom,
            vDir);
        return;
    }

    if(pDef->eType != FT_WEAPON_HITSCAN)
    {
        return;
    }

    FTFireFilterData filterData;
    filterData.hPlayer = m_hObject;
    filterData.hWeapon = m_hClub;

    HCLASS hZombie =
        g_pLTServer->GetClass("FireteamZombie");
    HCLASS hSeal =
        g_pLTServer->GetClass("Seal");
    HCLASS hPlayer =
        g_pLTServer->GetClass("CPlayerSrvr");

    LTVector vTraceFrom =
        vServerFrom + (vDir * 4.0f);

    float fRemainingRange =
        pDef->fRange;
    float fTravelled = 0.0f;
    float fPenetrationDamageScale = 1.0f;

    // NOLF2's vector projectile code continues a ray after a shoot-through
    // surface by finding the exit point with a reverse trace. FIRETEAM keeps
    // the same geometry model while exposing weapon penetration in cfg until
    // CA's material/surface table is fully mapped.
    const uint32 kMaxPenetrations = 2;

    for(uint32 nPass = 0;
        nPass <= kMaxPenetrations &&
        fRemainingRange > 1.0f;
        ++nPass)
    {
        IntersectQuery query;
        IntersectInfo info;

        query.m_From = vTraceFrom;
        query.m_To =
            vTraceFrom +
            (vDir * fRemainingRange);
        query.m_Flags =
            INTERSECT_OBJECTS |
            IGNORE_NONSOLID |
            INTERSECT_HPOLY;
        query.m_FilterFn = FTFireFilter;
        query.m_pUserData = &filterData;

        if(!g_pLTServer->IntersectSegment(
            &query,
            &info))
        {
            if(nPass == 0)
            {
                g_pLTServer->CPrint(
                    "Fireteam weapon: %s no hit (%u/%u)",
                    pDef->sName,
                    (uint32)m_nWeaponAmmoInClip[m_nWeaponSlot],
                    (uint32)m_nWeaponAmmoReserve[m_nWeaponSlot]);
            }
            return;
        }

        const float fSegmentDistance =
            (info.m_Point - query.m_From).Mag();

        fTravelled += fSegmentDistance;
        fRemainingRange -= fSegmentDistance;

        HCLASS hTarget = info.m_hObject
            ? g_pLTServer->GetObjectClass(
                info.m_hObject)
            : LTNULL;

        const bool bZombie =
            hZombie && hTarget &&
            g_pLTServer->IsKindOf(
                hTarget,
                hZombie);

        const bool bSeal =
            hSeal && hTarget &&
            g_pLTServer->IsKindOf(
                hTarget,
                hSeal);

        const bool bEnemy =
            bZombie ||
            bSeal;

        if(bEnemy)
        {
            float fDamageMult =
                pDef->fDamageMult0;

            if(pDef->fEffectRange1 > 0.0f &&
               fTravelled > pDef->fEffectRange1)
            {
                fDamageMult =
                    pDef->fDamageMult2;
            }
            else if(
                pDef->fEffectRange0 > 0.0f &&
                fTravelled > pDef->fEffectRange0)
            {
                fDamageMult =
                    pDef->fDamageMult1;
            }

            float fDamage =
                (float)pDef->nDamage *
                fDamageMult *
                fPenetrationDamageScale;

            FTFireteamHitRegion eHitRegion =
                FT_HITREGION_BODY;

            if(bZombie)
            {
                eHitRegion =
                    FT_GetZombieHitRegion(
                        info.m_hObject,
                        info.m_Point);

                if(eHitRegion ==
                   FT_HITREGION_HEAD)
                {
                    fDamage *=
                        2.0f;
                }
                else if(eHitRegion ==
                        FT_HITREGION_GROIN)
                {
                    fDamage *=
                        1.5f;
                }
            }

            if(fDamage < 1.0f &&
               pDef->nDamage > 0)
            {
                fDamage = 1.0f;
            }
            if(fDamage > 255.0f)
            {
                fDamage = 255.0f;
            }

            const bool bOneHit =
                fNow <
                m_fOneHitUntil;

            const uint8 nDamage =
                bOneHit
                ? 255
                : (uint8)(
                    fDamage +
                    0.5f);

            ILTMessage_Write *pDamage = LTNULL;
            if(g_pLTSCommon->CreateMessage(
                pDamage) == LT_OK &&
               pDamage)
            {
                pDamage->IncRef();
                pDamage->Writeuint32(
                    OBJ_MID_DAMAGE);
                pDamage->Writeuint8(
                    nDamage);
                g_pLTServer->SendToObject(
                    pDamage->Read(),
                    m_hObject,
                    info.m_hObject,
                    0);
                pDamage->DecRef();

                g_pLTServer->CPrint(
                    "Fireteam weapon: %s infected hit region=%s damage=%u distance=%.1f penetrations=%u",
                    pDef->sName,
                    FT_HitRegionName(
                        eHitRegion),
                    (uint32)nDamage,
                    fTravelled,
                    nPass);
            }
            return;
        }

        // Players stop bullets but never receive damage.
        if(hPlayer && hTarget &&
           g_pLTServer->IsKindOf(
               hTarget,
               hPlayer))
        {
            return;
        }

        if(pDef->fPenetrationMaxThickness <= 0.0f ||
           nPass >= kMaxPenetrations ||
           fRemainingRange <= 1.0f)
        {
            g_pLTServer->CPrint(
                "Fireteam weapon: %s world/solid hit %.1f",
                pDef->sName,
                fTravelled);
            return;
        }

        const LTVector vEntry =
            info.m_Point;

        // Start just beyond the maximum allowed thickness and trace backwards
        // to the entry point. If another surface is found, that is the exit.
        IntersectQuery exitQuery;
        IntersectInfo exitInfo;

        exitQuery.m_From =
            vEntry +
            (vDir *
             (pDef->fPenetrationMaxThickness +
              2.0f));
        exitQuery.m_To =
            vEntry -
            (vDir * 2.0f);
        exitQuery.m_Flags =
            INTERSECT_OBJECTS |
            IGNORE_NONSOLID |
            INTERSECT_HPOLY;
        exitQuery.m_FilterFn =
            FTPenetrationGeometryFilter;
        exitQuery.m_pUserData =
            &filterData;

        if(!g_pLTServer->IntersectSegment(
            &exitQuery,
            &exitInfo))
        {
            g_pLTServer->CPrint(
                "Fireteam weapon: %s penetration failed - no exit.",
                pDef->sName);
            return;
        }

        const float fThickness =
            (exitInfo.m_Point -
             vEntry).Mag();

        if(fThickness < 0.5f ||
           fThickness >
                pDef->fPenetrationMaxThickness)
        {
            g_pLTServer->CPrint(
                "Fireteam weapon: %s penetration blocked thickness=%.1f max=%.1f.",
                pDef->sName,
                fThickness,
                pDef->fPenetrationMaxThickness);
            return;
        }

        fTravelled += fThickness;
        fRemainingRange -= fThickness;

        if(fRemainingRange <= 1.0f)
        {
            return;
        }

        fPenetrationDamageScale *=
            pDef->fPenetrationDamageMult;

        fRemainingRange *=
            pDef->fPenetrationRangeMult;

        vTraceFrom =
            exitInfo.m_Point +
            (vDir * 3.0f);

        g_pLTServer->CPrint(
            "Fireteam weapon: %s penetrated %.1f units.",
            pDef->sName,
            fThickness);
    }
}


void CPlayerSrvr::SendPrimaryAmmo()
{
    if(!m_hClient)
    {
        return;
    }

    const FTWeaponDef *pDef = FT_GetWeaponDef(
        m_WeaponDefs,
        m_nWeaponSlot);

    if(!pDef)
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
    pMsg->Writeuint16(m_nWeaponAmmoInClip[m_nWeaponSlot]);
    pMsg->Writeuint16(m_nWeaponAmmoReserve[m_nWeaponSlot]);
    g_pLTServer->SendToClient(
        pMsg->Read(),
        m_hClient,
        MESSAGE_GUARANTEED);
    pMsg->DecRef();
}


void CPlayerSrvr::SyncPrimaryAmmo()
{
    CompleteReloadIfReady();
    SendPrimaryAmmo();
}

void CPlayerSrvr::ReloadWeapon()
{
    if(!m_bAlive)
    {
        return;
    }

    const FTWeaponDef *pDef = FT_GetWeaponDef(
        m_WeaponDefs,
        m_nWeaponSlot);

    if(!pDef ||
       pDef->eType == FT_WEAPON_MELEE ||
       pDef->nClipSize == 0 ||
       m_bReloading)
    {
        return;
    }

    if(m_nWeaponAmmoInClip[m_nWeaponSlot] >= pDef->nClipSize ||
       m_nWeaponAmmoReserve[m_nWeaponSlot] == 0)
    {
        return;
    }

    m_bReloading = true;
    m_nReloadSlot = m_nWeaponSlot;
    m_fReloadComplete =
        g_pLTServer->GetTime() + pDef->fReloadSeconds;

    g_pLTServer->CPrint(
        "Fireteam weapon: reloading %s",
        pDef->sName);
}

void CPlayerSrvr::CompleteReloadIfReady()
{
    if(!m_bReloading ||
       g_pLTServer->GetTime() < m_fReloadComplete)
    {
        return;
    }

    const FTWeaponDef *pDef = FT_GetWeaponDef(
        m_WeaponDefs,
        m_nReloadSlot);

    if(!pDef)
    {
        m_bReloading = false;
        m_nReloadSlot = 0;
        return;
    }

    const uint16 nNeeded =
        (pDef->nClipSize > m_nWeaponAmmoInClip[m_nReloadSlot])
        ? (pDef->nClipSize - m_nWeaponAmmoInClip[m_nReloadSlot])
        : 0;

    const uint16 nTransfer =
        (nNeeded < m_nWeaponAmmoReserve[m_nReloadSlot])
        ? nNeeded
        : m_nWeaponAmmoReserve[m_nReloadSlot];

    m_nWeaponAmmoInClip[m_nReloadSlot] += nTransfer;
    m_nWeaponAmmoReserve[m_nReloadSlot] -= nTransfer;

    m_bReloading = false;
    const uint8 nCompletedSlot = m_nReloadSlot;
    m_nReloadSlot = 0;

    if(nCompletedSlot == m_nWeaponSlot)
    {
        SendPrimaryAmmo();
    }
}

void CPlayerSrvr::SpawnExplosiveProjectile(
    const FTWeaponDef &def,
    const LTVector &vFrom,
    const LTVector &vDirection)
{
    HCLASS hProjectileClass =
        g_pLTServer->GetClass("FireteamExplosiveProjectile");

    if(!hProjectileClass)
    {
        g_pLTServer->CPrint(
            "Fireteam explosive: class lookup failed for %s",
            def.sName);
        return;
    }

    LTVector vDir = vDirection;
    vDir.Normalize();

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;
    ocs.m_Pos = vFrom + (vDir * 28.0f);
    ocs.m_Rotation = LTRotation(
        vDir,
        LTVector(0.0f, 1.0f, 0.0f));

    if(def.sProjectileModel[0])
    {
        FT_CopyWeaponString(
            ocs.m_Filename,
            sizeof(ocs.m_Filename),
            def.sProjectileModel);
        FT_CopyWeaponString(
            ocs.m_SkinName,
            sizeof(ocs.m_SkinName),
            def.sProjectileTexture);
    }
    else
    {
        // Definition fallback for older configs.
        FT_CopyWeaponString(
            ocs.m_Filename,
            sizeof(ocs.m_Filename),
            def.sHHModel);
        FT_CopyWeaponString(
            ocs.m_SkinName,
            sizeof(ocs.m_SkinName),
            def.sHHTexture);
    }

    FireteamExplosiveProjectile *pProjectile =
        (FireteamExplosiveProjectile*)g_pLTServer->CreateObject(
            hProjectileClass,
            &ocs);

    if(!pProjectile)
    {
        g_pLTServer->CPrint(
            "Fireteam explosive: failed to create projectile for %s (model=%s)",
            def.sName,
            ocs.m_Filename);
        return;
    }

    LTVector vProjectileScale(
        def.fProjectileScale,
        def.fProjectileScale,
        def.fProjectileScale);
    g_pLTServer->ScaleObject(
        pProjectile->m_hObject,
        &vProjectileScale);

    g_pLTServer->CPrint(
        "Fireteam explosive: spawned %s projectile model=%s scale=%.2f",
        def.sName,
        ocs.m_Filename,
        def.fProjectileScale);

    pProjectile->Configure(
        m_hObject,
        def.nDamage,
        def.fSplashRadius,
        def.fProjectileSpeed,
        def.fFuseSeconds,
        def.eType == FT_WEAPON_ROCKET);
}
