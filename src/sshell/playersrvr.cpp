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
#include "FireteamSpawnSafety.h"

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
#include <stdlib.h>
#include <math.h>
#include "FireteamPoisonGas.h"
#include "FireteamExplosiveProjectile.h"
#include "FireteamDifficultyDefs.h"
#include "FireteamZombie.h"
#include "FireteamSpawner.h"


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


static float FT_StackedPowerupUntil(
    float fCurrentUntil,
    float fBaseSeconds,
    uint8 &nStack,
    float &fAddedSeconds)
{
    fAddedSeconds = 0.0f;

    if(fBaseSeconds <= 0.0f)
    {
        return fCurrentUntil;
    }

    const float fNow =
        FT_GetRoundCombatTime();

    if(fCurrentUntil <=
       fNow)
    {
        fCurrentUntil =
            fNow;
        nStack =
            0;
    }

    if(nStack < 255)
    {
        ++nStack;
    }

    const uint32 nDifficulty =
        FT_GetActiveDifficultyLevel(
            "config/session.cfg");

    float fFactor =
        1.0f;

    // Normal (4) and below get full-duration stacking. Medium/Hard keep
    // stacking, but follow the requested 30s -> 25s -> 15s style taper.
    if(nDifficulty >= 5 &&
       nStack > 1)
    {
        if(nDifficulty <= 6)
        {
            if(nStack == 2) fFactor = 0.833333f;
            else if(nStack == 3) fFactor = 0.50f;
            else if(nStack == 4) fFactor = 0.333333f;
            else fFactor = 0.20f;
        }
        else if(nDifficulty <= 8)
        {
            if(nStack == 2) fFactor = 0.75f;
            else if(nStack == 3) fFactor = 0.40f;
            else if(nStack == 4) fFactor = 0.25f;
            else fFactor = 0.15f;
        }
        else
        {
            if(nStack == 2) fFactor = 0.50f;
            else if(nStack == 3) fFactor = 0.25f;
            else if(nStack == 4) fFactor = 0.15f;
            else fFactor = 0.10f;
        }
    }

    fAddedSeconds =
        fBaseSeconds *
        fFactor;

    return
        fCurrentUntil +
        fAddedSeconds;
}



static void FT_ApplyWeaponPerturb(
    const FTWeaponDef &def,
    HOBJECT hPlayer,
    bool bZoomed,
    LTVector &vDirection)
{
    if(def.nMaxPerturb <
       def.nMinPerturb)
    {
        return;
    }

    uint32 nPerturb =
        def.nMinPerturb;

    // Existing curated configs predate CA MinPerturb/MaxPerturb import.
    // Keep them playable, but make an unscoped shot from a scoped rifle
    // meaningfully inaccurate until the next attribute import fills the
    // real Combat Arms values.
    if(def.nMaxPerturb == 0 &&
       def.fZoomFovDegrees > 0.0f)
    {
        nPerturb =
            bZoomed
            ? 0
            : 90;
    }

    if(def.fZoomFovDegrees > 0.0f)
    {
        // Scoped rifles are deliberately inaccurate from the hip/no-scope.
        // A real scoped shot receives the CA MinPerturb value.
        nPerturb =
            bZoomed
            ? def.nMinPerturb
            : def.nMaxPerturb;
    }
    else if(hPlayer)
    {
        // NOLF2/CA interpolate between MinPerturb and MaxPerturb using a
        // dynamic perturb factor. FIRETEAM currently derives that factor from
        // player movement until recoil/bloom state is added.
        LTVector vVelocity;
        g_pLTSPhysics->GetVelocity(
            hPlayer,
            &vVelocity);

        vVelocity.y =
            0.0f;

        float fMove =
            vVelocity.Mag() /
            435.0f;

        if(fMove < 0.0f)
            fMove = 0.0f;
        if(fMove > 1.0f)
            fMove = 1.0f;

        nPerturb =
            def.nMinPerturb +
            (uint32)(
                ((float)(
                    def.nMaxPerturb -
                    def.nMinPerturb) *
                 fMove) +
                0.5f);
    }

    if(nPerturb == 0)
    {
        return;
    }

    LTVector vRight(
        -vDirection.z,
        0.0f,
        vDirection.x);

    if(vRight.MagSqr() <
       0.0001f)
    {
        vRight.Init(
            1.0f,
            0.0f,
            0.0f);
    }
    else
    {
        vRight.Normalize();
    }

    LTVector vUp(
        0.0f,
        1.0f,
        0.0f);

    const float fUnitRight =
        ((float)rand() /
         (float)RAND_MAX) *
        2.0f -
        1.0f;

    const float fUnitUp =
        ((float)rand() /
         (float)RAND_MAX) *
        2.0f -
        1.0f;

    const float fRightPerturb =
        fUnitRight *
        (float)nPerturb /
        1000.0f;

    const float fUpPerturb =
        fUnitUp *
        (float)nPerturb /
        1000.0f;

    vDirection +=
        (vRight *
         fRightPerturb);
    vDirection +=
        (vUp *
         fUpPerturb);

    vDirection.Normalize();
}


static bool FT_AreAllFireteamPlayersOutOfLives()
{
    HCLASS hPlayerClass =
        g_pLTServer->GetClass(
            "CPlayerSrvr");

    if(!hPlayerClass)
    {
        return false;
    }

    bool bFoundPlayer = false;

    for(HOBJECT hObject =
            g_pLTServer->GetNextObject(
                LTNULL);
        hObject;
        hObject =
            g_pLTServer->GetNextObject(
                hObject))
    {
        HCLASS hClass =
            g_pLTServer->GetObjectClass(
                hObject);

        if(!hClass ||
           !g_pLTServer->IsKindOf(
                hClass,
                hPlayerClass))
        {
            continue;
        }

        CPlayerSrvr *pPlayer =
            (CPlayerSrvr*)
            g_pLTServer->HandleToObject(
                hObject);

        if(!pPlayer)
        {
            continue;
        }

        bFoundPlayer = true;

        if(pPlayer->HasResumeIdentity() &&
           !pPlayer->IsWaitingForNextRound() &&
           pPlayer->GetLives() > 0)
        {
            return false;
        }
    }

    return bFoundPlayer;
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

    // Tighter kill regions: avoid awarding most upper-torso/pelvis hits.
    if(fNormalizedY >= 0.91f)
    {
        return FT_HITREGION_HEAD;
    }

    if(fNormalizedY >= 0.465f &&
       fNormalizedY <= 0.485f)
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

            if(!m_bResumeVerified)
            {
                g_pLTServer->SetNextUpdate(m_hObject, 0.25f);
                return 1;
            }
            if(!m_bAlive)
            {
                if(m_nLives > 0 && !m_bWaitingNextRound)
                {
                    // Use an absolute server deadline instead of assuming
                    // every object update runs at exactly 250 ms.
                    if(g_pLTServer->GetTime() >= m_fRespawnTimer)
                    {
                        Respawn();
                    }
                }
            }
            else
            {
                UpdateHazards();
                CompleteReloadIfReady();
                UpdatePowerups();
            }

            const float fTraceNow =
                g_pLTServer->GetTime();

            if(fTraceNow >=
               m_fNextPositionTrace)
            {
                LTVector vTracePos;
                g_pLTServer->GetObjectPos(
                    m_hObject,
                    &vTracePos);

                g_pLTServer->CPrint(
                    "Fireteam player trace: %s pos=%.1f %.1f %.1f lives=%u/%u alive=%u.",
                    m_sName,
                    vTracePos.x,
                    vTracePos.y,
                    vTracePos.z,
                    (uint32)m_nLives,
                    (uint32)m_nMaxLives,
                    m_bAlive ? 1 : 0);

                m_fNextPositionTrace =
                    fTraceNow + 5.0f;
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


                    // Send up to the 24-player room limit. Keep the
                    // complete allocation until after the message is built.
                    ILTMessage_Write *pMsg = LTNULL;
                    const LTRESULT nResult = g_pLTSCommon->CreateMessage(pMsg);
                    if(nResult == LT_OK && pMsg)
                    {
                    pMsg->IncRef();
                    pMsg->Writeint8(MSG_SERVER_SCORES);
                    const uint8 nSendCount =
                        (uint8)(iNumPlayers > 24 ? 24 : iNumPlayers);
                    pMsg->Writeuint8(nSendCount);

                    for(uint8 i = 0; i < nSendCount; ++i)
                    {
                        pMsg->Writeuint32(scoreStruct[i].iClientID);
                        pMsg->WriteString(scoreStruct[i].sPlayerName);
                        pMsg->Writeuint32(scoreStruct[i].iScore);
                        pMsg->Writeuint8(scoreStruct[i].iLives);
                        pMsg->Writefloat(scoreStruct[i].fMoney);
                        pMsg->Writeuint32(scoreStruct[i].iShotsFired);
                        pMsg->Writeuint32(scoreStruct[i].iShotsHit);
                        pMsg->Writeuint32(scoreStruct[i].iDeaths);
                        pMsg->Writeuint32(scoreStruct[i].iPowerups);
                        pMsg->Writeuint32(scoreStruct[i].iHeadshotKills);
                    }

                    g_pLTServer->SendToClient(
                        pMsg->Read(), m_hClient, MESSAGE_GUARANTEED);
                    pMsg->DecRef();
                    }
                    delete[] scoreStruct;
                    // Reset the counter
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
        case OBJ_MID_KILLSCORE_INFECTED:
            {
                const bool bSnowman =
                    (OBJ_MID_KILLSCORE_SNOWMAN == messageID);
                const bool bInfected =
                    (OBJ_MID_KILLSCORE_INFECTED == messageID);

                if(!bSnowman)
                {
                    ++m_iScore;
                }

                m_fMoney += pMsg->Readfloat();

                if(bInfected)
                {
                    const uint8 nKillingRegion =
                        pMsg->Readuint8();
                    char szZombieType[32] = {0};
                    pMsg->ReadString(szZombieType, sizeof(szZombieType));
                    RecordZombieTypeKill(
                        szZombieType[0] ? szZombieType : "unknown");

                    if(nKillingRegion ==
                       (uint8)FT_HITREGION_HEAD)
                    {
                        ++m_nHeadshotKills;
                        SendCombatFeedback(
                            FT_COMBAT_FEEDBACK_HEADSHOT);
                    }
                    else if(nKillingRegion ==
                            (uint8)FT_HITREGION_GROIN)
                    {
                        SendCombatFeedback(
                            FT_COMBAT_FEEDBACK_NUTSHOT);
                    }
                }

                if(!bSnowman)
                {
                    const float fNow =
                        g_pLTServer->GetTime();

                    if(m_fLastKillFeedbackTime > 0.0f &&
                       (fNow - m_fLastKillFeedbackTime) <= 2.5f)
                    {
                        if(m_nKillFeedbackChain < 255)
                        {
                            ++m_nKillFeedbackChain;
                        }
                    }
                    else
                    {
                        m_nKillFeedbackChain = 1;
                    }

                    m_fLastKillFeedbackTime =
                        fNow;

                    uint8 nFeedback = 0;

                    if(m_iScore == 1)
                    {
                        nFeedback =
                            FT_COMBAT_FEEDBACK_FIRSTKILL;
                    }
                    else if(m_nKillFeedbackChain == 2)
                    {
                        nFeedback =
                            FT_COMBAT_FEEDBACK_DOUBLEKILL;
                    }
                    else if(m_nKillFeedbackChain == 3)
                    {
                        nFeedback =
                            FT_COMBAT_FEEDBACK_MULTIKILL;
                    }
                    else if(m_nKillFeedbackChain == 4)
                    {
                        nFeedback =
                            FT_COMBAT_FEEDBACK_ULTRAKILL;
                    }
                    else if(m_nKillFeedbackChain == 5)
                    {
                        nFeedback =
                            FT_COMBAT_FEEDBACK_FANTASTIC;
                    }
                    else if(m_nKillFeedbackChain >= 6)
                    {
                        nFeedback =
                            FT_COMBAT_FEEDBACK_UNBELIEVABLE;
                    }

                    if(nFeedback)
                    {
                        SendCombatFeedback(
                            nFeedback);
                    }
                }

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



// Named counters use bounded fixed arrays so no allocations are needed in
// the server's authoritative hit/kill/pickup message hot paths.
static void FT_RecordNamedMatchCounter(
    FTNamedCounter *pCounters, uint8 &nUsed, const char *pId)
{
    if(!pCounters || !pId || !pId[0])
        return;
    for(uint8 n = 0; n < nUsed; ++n)
    {
        if(_stricmp(pCounters[n].sId, pId) == 0)
        {
            ++pCounters[n].nCount;
            return;
        }
    }
    if(nUsed >= FT_MAX_MATCH_CATEGORIES)
        return;
    FTNamedCounter &entry = pCounters[nUsed++];
    strncpy(entry.sId, pId, sizeof(entry.sId) - 1);
    entry.sId[sizeof(entry.sId) - 1] = '\0';
    entry.nCount = 1;
}

void CPlayerSrvr::RecordZombieTypeKill(const char *pType)
{
    FT_RecordNamedMatchCounter(
        m_aZombieTypes, m_nZombieTypeCount, pType);
}

void CPlayerSrvr::RecordPowerupPickup(const char *pType)
{
    ++m_nPowerups;
    FT_RecordNamedMatchCounter(
        m_aPowerupTypes, m_nPowerupTypeCount, pType);
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


void CPlayerSrvr::SetQaSpectating(
    bool bSpectating)
{
    if(m_bQaSpectating ==
       bSpectating)
    {
        return;
    }

    m_bQaSpectating =
        bSpectating;

    if(m_bQaSpectating)
    {
        LTVector vZero(
            0.0f,
            0.0f,
            0.0f);

        g_pLTSPhysics->SetVelocity(
            m_hObject,
            &vZero);

        m_bReloading = false;
        m_nReloadSlot = 0;
    }

    g_pLTServer->CPrint(
        "Fireteam QA spectator: %s %s.",
        m_sName,
        m_bQaSpectating
            ? "ON"
            : "OFF");
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
// Reconnection keeps the original authoritative state, including damage
// and a pending 5s respawn. A client only sends a random lookup capability.
void CPlayerSrvr::SendWaitingStatus(bool bWait)
{
    if(!m_hClient) return;
    ILTMessage_Write *pMsg = LTNULL;
    if(g_pLTSCommon->CreateMessage(pMsg) != LT_OK || !pMsg) return;
    pMsg->IncRef();
    pMsg->Writeuint8(MSG_SC_JOIN_WAIT);
    pMsg->Writebool(bWait);
    g_pLTServer->SendToClient(pMsg->Read(), m_hClient, MESSAGE_GUARANTEED);
    pMsg->DecRef();
}

void CPlayerSrvr::SyncResumePosition()
{
    if(!m_hClient) return;
    LTVector v;
    LTRotation r;
    g_pLTServer->GetObjectPos(m_hObject, &v);
    g_pLTServer->GetObjectRotation(m_hObject, &r);
    ILTMessage_Write *pMsg = LTNULL;
    if(g_pLTSCommon->CreateMessage(pMsg) != LT_OK || !pMsg) return;
    pMsg->IncRef();
    pMsg->Writeuint8(MSG_SC_RESPAWN);
    pMsg->WriteLTVector(v);
    pMsg->WriteLTRotation(r);
    g_pLTServer->SendToClient(pMsg->Read(), m_hClient, MESSAGE_GUARANTEED);
    pMsg->DecRef();
}

void CPlayerSrvr::AuthorizeNewSession(const char *pTicket, bool bWait)
{
    strncpy(m_sResumeToken, pTicket, 32);
    m_sResumeToken[32] = '\0';
    m_bResumeVerified = true;
    m_bWaitingNextRound = bWait;
    if(bWait)
    {
        m_bAlive = false;
        m_nHealth = 0;
        m_fRespawnTimer = 0.0f;
        // Late joiners are not combatants until the NEXT round.
        g_pLTSCommon->SetObjectFlags(m_hObject, OFT_Flags, 0, FLAG_VISIBLE);
    }
    SendHealth();
    SendLives();
    SendWaitingStatus(bWait);
    if(bWait) NotifyPowerup("WAITING FOR NEXT ROUND", 4.0f);
    else SendPowerupState();
}

void CPlayerSrvr::CaptureResumeState(FTPlayerResumeState &state) const
{
    g_pLTServer->GetObjectPos(m_hObject, &state.vPosition);
    g_pLTServer->GetObjectRotation(m_hObject, &state.rRotation);
    state.vRespawnAnchor = m_vSpawnPos;
    state.rRespawnAnchor = m_rSpawnRot;
    state.nHealth = m_nHealth;
    state.nMaxHealth = m_nMaxHealth;
    state.nLives = m_nLives;
    state.nMaxLives = m_nMaxLives;
    state.nWeaponSlot = m_nWeaponSlot;
    state.bAlive = m_bAlive;
    state.bWaitingForRound = m_bWaitingNextRound;
    state.fRespawnTime = m_fRespawnTimer;
    state.fProtectionUntil = m_fRespawnInvulnerableUntil;
    for(uint8 slot = 0; slot < 6; ++slot)
    {
        state.aClip[slot] = m_nWeaponAmmoInClip[slot];
        state.aReserve[slot] = m_nWeaponAmmoReserve[slot];
        strncpy(state.aWeaponIDs[slot], m_WeaponDefs[slot].sId, 31);
        state.aWeaponIDs[slot][31] = '\0';
        state.aShotsBySlot[slot] = m_aShotsBySlot[slot];
    }
    state.nScore = m_iScore;
    state.nShots = m_nAcceptedShots;
    state.nHits = m_nConfirmedHits;
    state.nDeaths = m_nDeaths;
    state.nPowerups = m_nPowerups;
    state.nHeadshots = m_nHeadshotKills;
    state.nDamageTaken = m_nDamageTaken;
    memcpy(state.aZombieTypes, m_aZombieTypes, sizeof(m_aZombieTypes));
    memcpy(state.aPowerupTypes, m_aPowerupTypes, sizeof(m_aPowerupTypes));
    state.nZombieTypes = m_nZombieTypeCount;
    state.nPowerupTypes = m_nPowerupTypeCount;
    state.fMoney = m_fMoney;
    state.fBottomlessUntil = m_fBottomlessUntil;
    state.fOneHitUntil = m_fOneHitUntil;
    state.fGodUntil = m_fGodUntil;
    state.nBottomlessStacks = m_nBottomlessStacks;
    state.nOneHitStacks = m_nOneHitStacks;
    state.nGodStacks = m_nGodStacks;
}

void CPlayerSrvr::RestoreResumeState(
    const char *pTicket, const FTPlayerResumeState &state)
{
    strncpy(m_sResumeToken, pTicket, 32);
    m_sResumeToken[32] = '\0';
    m_bResumeVerified = true;
    m_bWaitingNextRound = state.bWaitingForRound;
    m_bAlive = state.bAlive;
    m_nHealth = state.nHealth;
    m_nMaxHealth = state.nMaxHealth;
    m_nLives = state.nLives;
    m_nMaxLives = state.nMaxLives;
    m_nWeaponSlot = state.nWeaponSlot >= 1 &&
        state.nWeaponSlot <= 5 ? state.nWeaponSlot : 1;
    m_fRespawnTimer = state.fRespawnTime;
    m_fRespawnInvulnerableUntil = state.fProtectionUntil;
    m_iScore = state.nScore;
    m_nAcceptedShots = state.nShots;
    m_nConfirmedHits = state.nHits;
    m_nDeaths = state.nDeaths;
    m_nPowerups = state.nPowerups;
    m_nHeadshotKills = state.nHeadshots;
    m_nDamageTaken = state.nDamageTaken;
    memcpy(m_aZombieTypes, state.aZombieTypes, sizeof(m_aZombieTypes));
    memcpy(m_aPowerupTypes, state.aPowerupTypes, sizeof(m_aPowerupTypes));
    m_nZombieTypeCount = state.nZombieTypes;
    m_nPowerupTypeCount = state.nPowerupTypes;
    m_fMoney = state.fMoney;
    m_fBottomlessUntil = state.fBottomlessUntil;
    m_fOneHitUntil = state.fOneHitUntil;
    m_fGodUntil = state.fGodUntil;
    m_nBottomlessStacks = state.nBottomlessStacks;
    m_nOneHitStacks = state.nOneHitStacks;
    m_nGodStacks = state.nGodStacks;
    m_bReloading = false;
    m_fReloadComplete = 0.0f;

    for(uint8 slot = 0; slot < 6; ++slot)
    {
        m_aShotsBySlot[slot] = state.aShotsBySlot[slot];
        // A changed loadout cannot borrow another weapon's ammunition.
        if(_stricmp(m_WeaponDefs[slot].sId, state.aWeaponIDs[slot]) == 0)
        {
            m_nWeaponAmmoInClip[slot] = state.aClip[slot];
            m_nWeaponAmmoReserve[slot] = state.aReserve[slot];
        }
    }
    // Restore the original safe respawn anchor separately from where the
    // player disconnected, or dying after reconnect would spawn at the quit spot.
    m_vSpawnPos = state.vRespawnAnchor;
    m_rSpawnRot = state.rRespawnAnchor;
    g_pLTServer->TeleportObject(m_hObject, &state.vPosition);
    g_pLTServer->SetObjectRotation(m_hObject, &state.rRotation);
    if(m_bWaitingNextRound)
        g_pLTSCommon->SetObjectFlags(
            m_hObject, OFT_Flags, 0, FLAG_VISIBLE);

    SendHealth();
    SendLives();
    SendPrimaryAmmo();
    SendPowerupState();
    SendWaitingStatus(m_bWaitingNextRound);
    if(m_bAlive)
        SyncResumePosition();
}

void CPlayerSrvr::ActivateForNextRound()
{
    if(!m_bResumeVerified || !m_bWaitingNextRound) return;
    m_bWaitingNextRound = false;
    g_pLTSCommon->SetObjectFlags(
        m_hObject, OFT_Flags, FLAG_VISIBLE, FLAG_VISIBLE);
    SendWaitingStatus(false);
    Respawn();
}

void CPlayerSrvr::ApplyDamage(uint8 nDamage)
{
    if(!m_bResumeVerified ||
       !m_bAlive ||
       m_bQaSpectating ||
       nDamage == 0)
    {
        return;
    }

    if(g_pLTServer->GetTime() < m_fRespawnInvulnerableUntil ||
       FT_GetRoundCombatTime() < m_fGodUntil)
    {
        g_pLTServer->CPrint(
            "Fireteam: %s GOD MODE absorbed %u damage.",
            m_sName,
            (uint32)nDamage);
        return;
    }

    const uint8 nActuallyTaken =
        nDamage >= m_nHealth ? m_nHealth : nDamage;
    m_nDamageTaken += nActuallyTaken;
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
        ++m_nDeaths;
        m_bAlive = false;

        if(m_nLives > 0)
        {
            --m_nLives;
        }

        SendLives();

        LTVector vZero(0.0f, 0.0f, 0.0f);
        g_pLTSPhysics->SetVelocity(m_hObject, &vZero);

        if(m_nLives > 0)
        {
            m_fRespawnTimer = g_pLTServer->GetTime() + 5.0f;

            g_pLTServer->CPrint(
                "Fireteam: %s died. %u/%u lives remain; respawning in 5 seconds.",
                m_sName,
                (uint32)m_nLives,
                (uint32)m_nMaxLives);
        }
        else
        {
            m_fRespawnTimer = 0.0f;

            g_pLTServer->CPrint(
                "Fireteam: %s is OUT OF LIVES.",
                m_sName);

            NotifyPowerup(
                "OUT OF LIVES",
                4.0f);

            if(FT_AreAllFireteamPlayersOutOfLives())
            {
                FT_OnFireteamSquadGameOver();
            }
        }
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
    if(m_nLives == 0 || m_bWaitingNextRound || !m_bResumeVerified)
    {
        return;
    }

    m_bAlive = true;
    m_nHealth = m_nMaxHealth;
    m_fRespawnTimer = 0.0f;
    m_fPoisonCarry = 0.0f;
    m_bReloading = false;
    m_nReloadSlot = 0;
    m_fBottomlessUntil = 0.0f;
    m_fOneHitUntil = 0.0f;
    // Respawn protection uses real server time, independent of power-ups.
    m_fGodUntil = 0.0f;
    m_fRespawnInvulnerableUntil = g_pLTServer->GetTime() + 3.0f;
    m_nBottomlessStacks = 0;
    m_nOneHitStacks = 0;
    m_nGodStacks = 0;
    m_bBottomlessWasActive = false;
    m_bOneHitWasActive = false;
    m_bGodWasActive = false;
    m_fLastKillFeedbackTime = 0.0f;
    m_nKillFeedbackChain = 0;

    for(uint8 nSlot = 1; nSlot <= 5; ++nSlot)
    {
        m_nWeaponAmmoInClip[nSlot] = m_WeaponDefs[nSlot].nClipSize;
        m_nWeaponAmmoReserve[nSlot] = m_WeaponDefs[nSlot].nStartReserve;
        m_fNextWeaponShot[nSlot] = 0.0f;
    }

    // The player's saved point can be next to a pole or low ceiling. Check
    // the complete collider against the compiled DAT geometry on EVERY
    // respawn, not only when initially entering the map.
    LTVector vSafeSpawn = m_vSpawnPos;
    LTVector vDims(16.0f, 38.0f, 16.0f);
    g_pLTSPhysics->GetObjectDims(m_hObject, &vDims);
    LTVector vResolvedSpawn;
    if(FT_ResolveSafePlayerSpawn(m_vSpawnPos, vDims, vResolvedSpawn))
        vSafeSpawn = vResolvedSpawn;

    g_pLTServer->TeleportObject(m_hObject, &vSafeSpawn);
    g_pLTServer->SetObjectRotation(m_hObject, &m_rSpawnRot);
    g_pLTServer->GetObjectPos(m_hObject, &vSafeSpawn);
    m_vSpawnPos = vSafeSpawn;

    LTVector vZero(0.0f, 0.0f, 0.0f);
    g_pLTSPhysics->SetVelocity(m_hObject, &vZero);

    if(m_hClient)
    {
        ILTMessage_Write *pMsg = LTNULL;
        if(g_pLTSCommon->CreateMessage(pMsg) == LT_OK && pMsg)
        {
            pMsg->IncRef();
            pMsg->Writeuint8(MSG_SC_RESPAWN);
            pMsg->WriteLTVector(vSafeSpawn);
            pMsg->WriteLTRotation(m_rSpawnRot);
            g_pLTServer->SendToClient(pMsg->Read(), m_hClient, MESSAGE_GUARANTEED);
            pMsg->DecRef();
        }
    }

    SendHealth();
    SendLives();
    SendPrimaryAmmo();
    SendPowerupState();
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

void CPlayerSrvr::SendLives()
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
    pMsg->Writeuint8(
        MSG_SC_LIVES);
    pMsg->Writeuint8(
        m_nLives);
    pMsg->Writeuint8(
        m_nMaxLives);

    g_pLTServer->SendToClient(
        pMsg->Read(),
        m_hClient,
        MESSAGE_GUARANTEED);

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
        FT_GetRoundCombatTime();

    float fBottomless =
        m_bBottomlessWasActive
        ? (m_fBottomlessUntil - fNow)
        : 0.0f;

    float fOneHit =
        m_bOneHitWasActive
        ? (m_fOneHitUntil - fNow)
        : 0.0f;

    float fGod =
        m_bGodWasActive
        ? (m_fGodUntil - fNow)
        : 0.0f;

    float fWallhack =
        FT_GetZombieWallhackRemaining();

    if(fBottomless < 0.0f)
        fBottomless = 0.0f;

    if(fOneHit < 0.0f)
        fOneHit = 0.0f;

    if(fGod < 0.0f)
        fGod = 0.0f;

    if(fWallhack < 0.0f)
        fWallhack = 0.0f;

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
    pMsg->Writefloat(
        fGod);
    pMsg->Writefloat(
        fWallhack);

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

    RecordPowerupPickup("ammo_resupply");
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

    RecordPowerupPickup("health");
    SendHealth();
}

void CPlayerSrvr::GrantBottomless(
    float fSeconds)
{
    if(fSeconds <= 0.0f)
    {
        return;
    }
    RecordPowerupPickup("bottomless");

    float fAdded =
        0.0f;

    m_fBottomlessUntil =
        FT_StackedPowerupUntil(
            m_fBottomlessUntil,
            fSeconds,
            m_nBottomlessStacks,
            fAdded);

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
    // Cancel an in-progress reload; bottomless ammo is available now.
    m_bReloading = false;
    m_nReloadSlot = 0;

    g_pLTServer->CPrint(
        "Fireteam powerup: %s BOTTOMLESS stack %u +%.1fs.",
        m_sName,
        (uint32)m_nBottomlessStacks,
        fAdded);

    SendPowerupState();
}

void CPlayerSrvr::GrantOneHit(
    float fSeconds)
{
    if(fSeconds <= 0.0f)
    {
        return;
    }
    RecordPowerupPickup("one_hit");

    float fAdded =
        0.0f;

    m_fOneHitUntil =
        FT_StackedPowerupUntil(
            m_fOneHitUntil,
            fSeconds,
            m_nOneHitStacks,
            fAdded);

    m_bOneHitWasActive =
        true;

    g_pLTServer->CPrint(
        "Fireteam powerup: %s ONE HIT stack %u +%.1fs.",
        m_sName,
        (uint32)m_nOneHitStacks,
        fAdded);

    SendPowerupState();
}

void CPlayerSrvr::GrantGodMode(
    float fSeconds)
{
    if(fSeconds <= 0.0f)
    {
        return;
    }
    RecordPowerupPickup("god_mode");

    float fAdded =
        0.0f;

    m_fGodUntil =
        FT_StackedPowerupUntil(
            m_fGodUntil,
            fSeconds,
            m_nGodStacks,
            fAdded);

    m_bGodWasActive =
        true;

    g_pLTServer->CPrint(
        "Fireteam powerup: %s GOD MODE stack %u +%.1fs.",
        m_sName,
        (uint32)m_nGodStacks,
        fAdded);

    SendPowerupState();
}

void CPlayerSrvr::UpdatePowerups()
{
    const float fNow =
        FT_GetRoundCombatTime();

    bool bChanged = false;

    if(m_bBottomlessWasActive &&
       fNow >= m_fBottomlessUntil)
    {
        m_bBottomlessWasActive = false;
        m_nBottomlessStacks = 0;
        bChanged = true;

        // If an Ammo Resupply arrived while Bottomless was active on an
        // empty weapon, load from ACTUAL reserve once the effect ends.
        // No bonus bullets are fabricated, and no reload loop is started.
        const FTWeaponDef *pDef =
            FT_GetWeaponDef(m_WeaponDefs, m_nWeaponSlot);
        if(pDef && pDef->eType != FT_WEAPON_MELEE &&
           pDef->nClipSize > 0 &&
           m_nWeaponAmmoInClip[m_nWeaponSlot] == 0 &&
           m_nWeaponAmmoReserve[m_nWeaponSlot] > 0)
        {
            const uint16 nTake =
                m_nWeaponAmmoReserve[m_nWeaponSlot] < pDef->nClipSize
                ? m_nWeaponAmmoReserve[m_nWeaponSlot] : pDef->nClipSize;
            m_nWeaponAmmoReserve[m_nWeaponSlot] -= nTake;
            m_nWeaponAmmoInClip[m_nWeaponSlot] = nTake;
            SendPrimaryAmmo();
        }
    }

    if(m_bOneHitWasActive &&
       fNow >= m_fOneHitUntil)
    {
        m_bOneHitWasActive =
            false;
        m_nOneHitStacks =
            0;
        bChanged = true;
    }

    if(m_bGodWasActive &&
       fNow >= m_fGodUntil)
    {
        m_bGodWasActive =
            false;
        m_nGodStacks =
            0;
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
    const LTVector &vDirection,
    bool bZoomed)
{
    if(!m_bResumeVerified || !m_bAlive ||
       m_bQaSpectating)
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
    const bool bBottomless = m_bBottomlessWasActive &&
        FT_GetRoundCombatTime() < m_fBottomlessUntil;
    if(fNow < m_fNextWeaponShot[m_nWeaponSlot])
    {
        return;
    }

    // A zero-reserve gun still fires while Bottomless Mag is active.
    // No reserve ammo is fabricated; reloading works again after expiry.
    if(m_nWeaponAmmoInClip[m_nWeaponSlot] == 0 && !bBottomless)
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

    if(pDef->eType ==
       FT_WEAPON_HITSCAN)
    {
        FT_ApplyWeaponPerturb(
            *pDef,
            m_hObject,
            bZoomed,
            vDir);
    }

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
    // Only accepted server-side attacks count. Premature reload shots,
    // untrusted client firing rate and invalid aim directions do not.
    ++m_nAcceptedShots;
    ++m_aShotsBySlot[m_nWeaponSlot];

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

    // Imported CA shotgun metadata supplies 6 or 8 vectors per trigger.
    // All rays are server-authoritative. The center pellet is straight and
    // the remaining pellets form a rotating cone: close-range clusters do
    // more damage, while spread and falloff lower long-range effectiveness.
    uint32 nPellets = pDef->nPellets;
    if(nPellets == 0) nPellets = 1;
    if(nPellets > 12) nPellets = 12;
    const float fShotRotation =
        ((float)rand() / (float)RAND_MAX) * 6.2831853f;
    uint32 nFeedbackSent = 0;

    for(uint32 nPellet = 0; nPellet < nPellets; ++nPellet)
    {
        LTVector vPelletDir = vDir;
        if(nPellets > 1 && nPellet > 0)
        {
            LTVector vRight(-vDir.z, 0.0f, vDir.x);
            if(vRight.MagSqr() < 0.0001f)
                vRight.Init(1.0f, 0.0f, 0.0f);
            else
                vRight.Normalize();

            // Camera-relative up vector, including steep look angles.
            LTVector vUp(
                vRight.y * vDir.z - vRight.z * vDir.y,
                vRight.z * vDir.x - vRight.x * vDir.z,
                vRight.x * vDir.y - vRight.y * vDir.x);
            if(vUp.MagSqr() < 0.0001f)
                vUp.Init(0.0f, 1.0f, 0.0f);
            else
                vUp.Normalize();

            const float fAngle = fShotRotation +
                (float)(nPellet - 1) * 6.2831853f / (float)(nPellets - 1);
            const float fCone = pDef->fPelletSpread;
            vPelletDir += vRight * (cosf(fAngle) * fCone);
            vPelletDir += vUp * (sinf(fAngle) * fCone);
            vPelletDir.Normalize();
        }
        TracePrimaryPellet(*pDef, vServerFrom, vPelletDir,
                           nPellet == 0, nFeedbackSent);
    }
}

// Penetration, hit-region multipliers, obstacles and friendly-fire behavior
// remain the same as the previous proven single-ray implementation.
void CPlayerSrvr::TracePrimaryPellet(
    const FTWeaponDef &def, const LTVector &vServerFrom,
    const LTVector &vDir, bool bLogMiss, uint32 &nFeedbackSent)
{
    const FTWeaponDef *pDef = &def;
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
            if(nPass == 0 && bLogMiss)
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
                m_bOneHitWasActive &&
                FT_GetRoundCombatTime() < m_fOneHitUntil;

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

                if(bZombie)
                {
                    pDamage->Writeuint32(
                        OBJ_MID_DAMAGE_REGIONAL);
                    pDamage->Writeuint8(
                        nDamage);
                    pDamage->Writeuint8(
                        (uint8)eHitRegion);
                }
                else
                {
                    pDamage->Writeuint32(
                        OBJ_MID_DAMAGE);
                    pDamage->Writeuint8(
                        nDamage);
                }

                g_pLTServer->SendToObject(
                    pDamage->Read(),
                    m_hObject,
                    info.m_hObject,
                    0);
                pDamage->DecRef();
                // Counts a confirmed direct hitscan impact, not theoretical
                // pellet/projectile hits or a client-reported hit marker.
                ++m_nConfirmedHits;
                // Shooter-only marker after server collision and damage delivery.
                if(nFeedbackSent < 2)
                {
                    SendCombatFeedback(FT_COMBAT_FEEDBACK_HIT, &info.m_Point);
                    ++nFeedbackSent;
                }

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


void CPlayerSrvr::SendCombatFeedback(
    uint8 nFeedback, const LTVector *pWorldHit)
{
    if(!m_hClient ||
       nFeedback == 0)
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
        MSG_SC_COMBAT_FEEDBACK);
    pMsg->Writeuint8(
        nFeedback);
    if(nFeedback == FT_COMBAT_FEEDBACK_HIT)
    {
        // Only a server-confirmed impact supplies the world-space position.
        // Older kill banners retain their one-byte payload.
        LTVector vImpact = pWorldHit ? *pWorldHit : LTVector(0.0f, 0.0f, 0.0f);
        pMsg->WriteLTVector(vImpact);
    }

    g_pLTServer->SendToClient(
        pMsg->Read(),
        m_hClient,
        MESSAGE_GUARANTEED);

    pMsg->DecRef();
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
    if(m_bBottomlessWasActive &&
       FT_GetRoundCombatTime() < m_fBottomlessUntil)
        return;
    if(!m_bResumeVerified || !m_bAlive ||
       m_bQaSpectating)
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

    // CA-style reload policy: the player's usual animation and timing are
    // presentation-side and remain weapon-specific. Authoritative ammo can
    // never transfer sooner than 125 ms after a validated reload request.
    // A modified client that fires before this time hits the server's
    // m_bReloading guard in FirePrimary: the premature shot deals NO damage.
    // Do not send invented blank bullets or award stats for rejected shots.
    const float kMinimumReloadSeconds = 0.125f;
    m_fReloadComplete =
        g_pLTServer->GetTime() + kMinimumReloadSeconds;

    g_pLTServer->CPrint(
        "Fireteam weapon: %s reload accepted; server minimum %.0fms (client animation %.2fs).",
        pDef->sName,
        kMinimumReloadSeconds * 1000.0f,
        pDef->fReloadSeconds);
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
