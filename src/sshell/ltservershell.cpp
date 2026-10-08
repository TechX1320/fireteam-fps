//------------------------------------------------------------------------------//
//
// MODULE   : ltservershell.cpp
//
// PURPOSE  : CLTServerShell - Implementation
//
// CREATED  : 03/02/2002
//
// (c) 2002 LithTech, Inc.  All Rights Reserved
//
//------------------------------------------------------------------------------//

// Definition of class
#include "ltservershell.h"

// Global pointers to server interfaces
#include "serverinterfaces.h"

// Msg Ids
#include "msgids.h"

//--Engine includes--

// Object Create Struct definition
#include <ltobjectcreate.h>

// ILTPhysics definition
#include <iltphysics.h>

// ILTCommon definition
#include <iltcommon.h>

#include "playersrvr.h"
#include "FireteamPlayerDefs.h"
#include "FireteamSpawner.h"
#include "FireteamNavigation.h"
#include "FireteamZombie.h"
#include "projectile.h"
#include "statsmanager.h"

#include <time.h>




//-----------------------------------------------------------------------------
SETUP_SERVERSHELL()

// Define our instantiation of the IServerShell interface.
define_interface(CLTServerShell, IServerShell);

CLTServerShell *g_pSShell = NULL;
StatsManager *g_pStatsManager = NULL;



//-----------------------------------------------------------------------------
//  CLTServerShell::CLTServerShell()
//
//-----------------------------------------------------------------------------
CLTServerShell::CLTServerShell()
{
}


//-----------------------------------------------------------------------------
//  CLTServerShell::OnServerInitialized()
//
//-----------------------------------------------------------------------------
LTRESULT CLTServerShell::OnServerInitialized()
{
	g_pLTServer->CPrint("CLTServerShell::OnServerInitialized()");
	LTRESULT result;

	// Set the pointer to our server shell
	g_pSShell = this;

	// Verify that all server interfaces initialized correctly.
	result = VerifyServerInterfaces();
	if (LT_OK != result)
	{
		return result;
	}

    // Init the global stats manager
    g_pStatsManager = new StatsManager();

    g_pLTSPhysics->SetStairHeight(16.0f);

    srand((unsigned int)time(NULL));

	return LT_OK;
}




//-----------------------------------------------------------------------------
//  CLTServerShell::OnServerTerm()
//
//-----------------------------------------------------------------------------
void CLTServerShell::OnServerTerm()
{
	g_pLTServer->CPrint("CLTServerShell::OnServerTerm()");

    // DeAllocate stats manager pointer
    if(g_pStatsManager)
    {
        delete g_pStatsManager;
    }
}



//-----------------------------------------------------------------------------
//  CLTServerShell::SRand(unsigned int uiRand)
//
//-----------------------------------------------------------------------------
void CLTServerShell::SRand(unsigned int uiRand)
{
	srand(uiRand);
}



//----------------------------------------------------------------------------
//  CLTServerShell::RegisterObject(CWorldProperties *pWorldProps)
//
//----------------------------------------------------------------------------
void CLTServerShell::RegisterObject(WorldProperties *pWorldProps)
{
	m_pWorldProperties = pWorldProps;
}



//----------------------------------------------------------------------------
//  CLTServerShell::UnRegisterObject(CWorldProperties *pWorldProps)
//
//----------------------------------------------------------------------------
void CLTServerShell::UnRegisterObject(WorldProperties *pWorldProps)
{
	m_pWorldProperties = NULL;
}



//----------------------------------------------------------------------------
// CLTServerShell::OnClientEnterWorld(HCLIENT hClient)
//
//----------------------------------------------------------------------------
LPBASECLASS CLTServerShell::OnClientEnterWorld(HCLIENT hClient)
{
	g_pLTServer->CPrint("CLTServerShell::OnClientEnterWorld(HCLIENT: %p)", hClient);

	ObjectCreateStruct objCreateStruct;
	HCLASS hClass;
	LTVector vPos, vStartPos;
	LTRotation rStartRot;

    hClass = g_pLTServer->GetClass("CPlayerSrvr");

	if (!hClass)
	{
		assert(!"ERROR: Couldn't get BaseClass!!");
		return NULL;
	}

	objCreateStruct.Clear();
	objCreateStruct.m_ObjectType = OT_MODEL;
    objCreateStruct.m_Flags = FLAG_SOLID | FLAG_CLIENTNONSOLID | FLAG_FORCECLIENTUPDATE | FLAG_YROTATION | FLAG_VISIBLE | FLAG_SHADOW;

    objCreateStruct.m_Flags2 |= FLAG2_PLAYERCOLLIDE;

    FTPlayerDef playerDef;
    const bool bPlayerDefLoaded =
        FT_LoadPlayerDef(
            "config/player.cfg",
            playerDef);

    if(bPlayerDefLoaded)
    {
        FT_CopyPlayerString(
            objCreateStruct.m_Filenames[0],
            MAX_CS_FILENAME_LEN,
            playerDef.sBodyModel);
        FT_CopyPlayerString(
            objCreateStruct.m_Filenames[1],
            MAX_CS_FILENAME_LEN,
            playerDef.sAnimationModel);
        FT_CopyPlayerString(
            objCreateStruct.m_SkinNames[0],
            MAX_CS_FILENAME_LEN,
            playerDef.sSkin0);
        FT_CopyPlayerString(
            objCreateStruct.m_SkinNames[1],
            MAX_CS_FILENAME_LEN,
            playerDef.sSkin1);
    }
    else
    {
        g_pLTServer->CPrint(
            "Fireteam: failed to load config/player.cfg.");
    }

	// Set client's pos and rot according to the "StartPoint", if one exists.
	this->FindStartPoint(vStartPos, rStartRot);
	objCreateStruct.m_Pos = vStartPos;
	objCreateStruct.m_Rotation = rStartRot;

	// Create our client object
	CPlayerSrvr *pClientObj = (CPlayerSrvr*)g_pLTServer->CreateObject(hClass, &objCreateStruct);

	
	if (pClientObj)
	{
        LTVector vTPos;
        g_pLTServer->GetObjectPos(pClientObj->m_hObject, &vTPos);
        g_pLTServer->CPrint("SERVER Player Pos: %f %f %f", vTPos.x, vTPos.y, vTPos.z);

        // Attach our player class to the client
		g_pLTServer->SetClientUserData(hClient, pClientObj);
		HOBJECT hClientHandle = g_pLTServer->ObjectToHandle(pClientObj);
        g_pLTServer->CPrint("ClientHandle: %p", hClientHandle);

        // Get our unique Client ID, and store it
        uint32 clientID = g_pLTServer->GetClientID(hClient);
        pClientObj->SetClientID(clientID);
        g_pLTServer->CPrint("ClientID %d", clientID);

        // Create a player name, and store it
        char name[16];
        memset(name, 0, 15);
        sprintf(name, "Player %d", clientID);
        pClientObj->SetPlayerName(name);

        // Player dimensions are external content too, keeping server/client
        // collision consistent and mod-friendly.
        LTVector vDims;
        if(bPlayerDefLoaded)
        {
            vDims.Init(
                playerDef.fCollisionX,
                playerDef.fCollisionY,
                playerDef.fCollisionZ);
        }
        else
        {
            g_pLTSCommon->GetModelAnimUserDims(
                hClientHandle,
                &vDims,
                g_pLTServer->GetModelAnimation(hClientHandle));
        }
        g_pLTSPhysics->SetObjectDims(
            hClientHandle,
            &vDims,
            0);

        pClientObj->SetClient(hClient);

        g_pStatsManager->RegisterPlayer(pClientObj);
        FT_OnFireteamPlayerJoined(hClient);
	}
	else
	{
		g_pLTServer->CPrint("CLTServerShell::OnClientEnterWorld - Create client object failed" );
	}

	// Send over the position of the startpoint so the client can position the player
	ILTMessage_Write *pMsg;
	LTRESULT nResult = g_pLTSCommon->CreateMessage(pMsg);
	pMsg->IncRef();
	pMsg->Writeint8(MSG_STARTPOINT_POS_ROT);
	pMsg->WriteLTVector(vStartPos);
	pMsg->WriteLTRotation(rStartRot);
	g_pLTServer->SendToClient(pMsg->Read(), hClient,  MESSAGE_GUARANTEED);
	pMsg->DecRef();

	return pClientObj;
}



//----------------------------------------------------------------------------
//  CLTServerShell::OnClientExitWorld(HCLIENT hClient)
//
//----------------------------------------------------------------------------
void CLTServerShell::OnClientExitWorld(HCLIENT hClient)
{
	g_pLTServer->CPrint("CLTServerShell::OnClientExitWorld(%p)", hClient);
    CPlayerSrvr *pBaseClass = (CPlayerSrvr *)g_pLTServer->GetClientUserData(hClient);
	HOBJECT hPlayer = g_pLTServer->ObjectToHandle(pBaseClass);
    g_pLTServer->RemoveObject(hPlayer);
    g_pStatsManager->RemovePlayer(pBaseClass);
}



//----------------------------------------------------------------------------
//  CLTServerShell::OnMessage(HCLIENT hSender, ILTMessage_Read *pMessage)
//
//----------------------------------------------------------------------------
void CLTServerShell::OnMessage(HCLIENT hSender, ILTMessage_Read *pMessage)
{
	CPlayerSrvr *pPlayerClass = (CPlayerSrvr *)g_pLTServer->GetClientUserData(hSender);
	
	HOBJECT hPlayer = g_pLTServer->ObjectToHandle(pPlayerClass);

	pMessage->SeekTo(0);
	uint8 messageID = pMessage->Readuint8();

	switch(messageID)
	{
	case MSG_CS_VELOCITY:
		{
            if(!pPlayerClass ||
               !pPlayerClass->CanControlPlayer())
            {
                break;
            }
			LTVector vVel, vPos;
			vVel = pMessage->ReadLTVector();
			vPos = pMessage->ReadLTVector();
			g_pLTSPhysics->SetVelocity(hPlayer, &vVel);
			LTVector vZero(0.0f, 0.0f, 0.0f);
			g_pLTSPhysics->SetAcceleration(hPlayer, &vZero);
            g_pLTServer->MoveObject(hPlayer, &vPos);

			g_pLTServer->GetObjectPos(hPlayer, &vPos);
			g_pLTServer->SetClientViewPos(hSender, &vPos);
		}
		break;
	case MSG_CS_ROTATION:
		{
            if(!pPlayerClass ||
               !pPlayerClass->CanControlPlayer())
            {
                break;
            }
			LTRotation rRot;
			rRot = pMessage->ReadLTRotation();
			g_pLTServer->SetObjectRotation(hPlayer, &rRot);
		}
		break;
	case MSG_CS_VELANDROT:
		{
            if(!pPlayerClass ||
               !pPlayerClass->CanControlPlayer())
            {
                break;
            }
			LTVector vVel, vPos;
			LTRotation rRot;
			vVel = pMessage->ReadLTVector();
			vPos = pMessage->ReadLTVector();
			rRot = pMessage->ReadLTRotation();
			g_pLTSPhysics->SetVelocity(hPlayer, &vVel);
			LTVector vZero(0.0f, 0.0f, 0.0f);
			g_pLTSPhysics->SetAcceleration(hPlayer, &vZero);

            // Resolve movement through collision instead of teleporting each
            // multiplayer packet directly into world geometry.
            g_pLTServer->MoveObject(hPlayer, &vPos);
            g_pLTServer->SetObjectRotation(hPlayer, &rRot);

            LTVector vResolvedPos;
            g_pLTServer->GetObjectPos(hPlayer, &vResolvedPos);
			g_pLTServer->SetClientViewPos(hSender, &vResolvedPos);
		}
		break;
    case MSG_CS_SHOOT:
        {
            LTVector vFrom = pMessage->ReadLTVector();
            LTVector vDirection = pMessage->ReadLTVector();
            const bool bZoomed =
                pMessage->Readbool();

            if(pPlayerClass)
            {
                pPlayerClass->FirePrimary(
                    vFrom,
                    vDirection,
                    bZoomed);
            }
        }
        break;
    case MSG_CS_ANIM:
        {
            char sAnimName[128];
            pMessage->ReadString(sAnimName, 128);
            uint8 nTrackerID = pMessage->Readuint8();
            bool bLooping = pMessage->Readbool();

            pPlayerClass->PlayAnimation(sAnimName, nTrackerID, bLooping);
        }
        break;
    case MSG_CS_PLAYERNAME:
        {
            char szName[16];
            szName[15] = '\0';
            pMessage->ReadString(szName, 16);
            pPlayerClass->SetPlayerName(szName);
            g_pLTServer->CPrint("%s has entered the game", szName);

	        ILTMessage_Write *pMsg;
        	LTRESULT nResult = g_pLTSCommon->CreateMessage(pMsg);
	        pMsg->IncRef();
            pMsg->Writeuint8(MSG_CS_PLAYERNAME);
            pMsg->WriteString(szName);
            g_pLTServer->SetObjectSFXMessage(hPlayer, pMsg->Read());
            pMsg->DecRef();
        }
        break;
    case MSG_CS_MY_CLUB:
        {
            pPlayerClass->SetClubID();
        }
        break;
    case MSG_CS_WEAPON_SLOT:
        {
            if(pPlayerClass)
            {
                pPlayerClass->SetWeaponSlot(
                    pMessage->Readuint8());
            }
        }
        break;
    case MSG_CS_RELOAD:
        {
            if(pPlayerClass)
            {
                pPlayerClass->ReloadWeapon();
            }
        }
        break;
    case MSG_CS_AMMO_SYNC:
        {
            if(pPlayerClass)
            {
                pPlayerClass->SyncPrimaryAmmo();
            }
        }
        break;
    case MSG_CS_QA_ZOMBIES:
        {
            // Weapon QA is a local/single-client developer workflow. Never
            // expose a normal multiplayer client control over global spawning.
            if(g_pStatsManager &&
               g_pStatsManager->GetNumPlayers() <= 1)
            {
                FT_SetQaZombiesEnabled(
                    pMessage->Readbool());
            }
        }
        break;
    case MSG_CS_QA_SPECTATOR:
        {
            const bool bSpectating =
                pMessage->Readbool();

            // QA freecam can only alter targetability in a single-player
            // developer session. Normal dead-player spectating needs no
            // client authority because lives are server-owned.
            if(pPlayerClass &&
               g_pStatsManager &&
               g_pStatsManager->GetNumPlayers() <= 1)
            {
                pPlayerClass->SetQaSpectating(
                    bSpectating);
            }
        }
        break;
    case MSG_CS_SPECTATOR_POS:
        {
            const LTVector vViewPos =
                pMessage->ReadLTVector();

            if(pPlayerClass &&
               (pPlayerClass->GetLives() == 0 ||
                pPlayerClass->IsQaSpectating()))
            {
                g_pLTServer->SetClientViewPos(
                    hSender,
                    &vViewPos);
            }
        }
        break;
    case MSG_CS_SCORE:
        {
            bool bShowStats = pMessage->Readbool();

			if ( pPlayerClass )
			{
            	// Set whether we should send stats or not.
            	pPlayerClass->SetSendStatsFlag(bShowStats);
			}
        }
        break;
    case MSG_CS_CHAT:
        {
            char szCharStringbuffer[256];
            memset(szCharStringbuffer, 0, 256);
            pMessage->ReadString(szCharStringbuffer, 255);

            char szCharString[512];
            sprintf(szCharString, "%s: %s", pPlayerClass->GetPlayerName(), szCharStringbuffer);

	        ILTMessage_Write *pMsg;
        	LTRESULT nResult = g_pLTSCommon->CreateMessage(pMsg);
	        pMsg->IncRef();
            pMsg->Writeuint8(MSG_SC_CHAT);
            pMsg->WriteString(szCharString);
            g_pLTServer->SendToClient(pMsg->Read(), NULL, MESSAGE_GUARANTEED);
            pMsg->DecRef();
        }
        break;
	default:
		g_pLTServer->CPrint("Server: Received Unknown message");
		break;
	}
}



//----------------------------------------------------------------------------
// CLTServerShell::FindStartPoint(LTVector &vStartPtPos,
//                                LTRotation &rStartPtRot)
//
//----------------------------------------------------------------------------
LTRESULT CLTServerShell::FindStartPoint(LTVector &vStartPtPos, LTRotation &rStartPtRot)
{
	// Get a list of the startpoint objects with a particular name (should be 1)
	ObjArray <HOBJECT, 1> pStartPt;

	g_pLTServer->FindNamedObjects("GameStartPoint00", pStartPt);

	// SealHunter fallback.
	if (pStartPt.NumObjects() == 0)
	{
		g_pLTServer->FindNamedObjects("GameStartPoint0", pStartPt);
	}

	if (pStartPt.NumObjects() == 0)
	{
		// Fall back: Check for objects named "StartPoint".
		g_pLTServer->FindNamedObjects("StartPoint", pStartPt);
		if (pStartPt.NumObjects() == 0)
		{
			vStartPtPos.Init(0.0f, 0.0f, 0.0f);
			rStartPtRot.Init();
			g_pLTServer->CPrint("Can't find StartPoint. Using (%f, %f, %f) instead.",
				vStartPtPos.x,
				vStartPtPos.y,
				vStartPtPos.z);
			return LT_NOTFOUND;
		}
		else
		{
			// Get startpoint's position and rotation
			g_pLTServer->GetObjectPos(pStartPt.GetObject(0), &vStartPtPos);
			g_pLTServer->GetObjectRotation(pStartPt.GetObject(0), &rStartPtRot);
		}
	}
	else
	{
		// Get startpoint's position and rotation
		g_pLTServer->GetObjectPos(pStartPt.GetObject(0), &vStartPtPos);
		g_pLTServer->GetObjectRotation(pStartPt.GetObject(0), &rStartPtRot);
	}

	return LT_OK;
}



//-----------------------------------------------------------------------------
//  CLTServerShell::OnAddClient(HCLIENT hClient)
//
//-----------------------------------------------------------------------------
void CLTServerShell::OnAddClient(HCLIENT hClient)
{
	//g_pLTServer->CPrint("OnAddClient(%p)", hClient);
}



//-----------------------------------------------------------------------------
//  CLTServerShell::OnRemoveClient(HCLIENT hClient)
//
//-----------------------------------------------------------------------------
void CLTServerShell::OnRemoveClient(HCLIENT hClient)
{
	//g_pLTServer->CPrint("OnRemoveClient(%p)", hClient);
}



//-----------------------------------------------------------------------------
//  CLTServerShell::PreStartWorld(bool bSwitchingWorlds)
//
//-----------------------------------------------------------------------------
void CLTServerShell::PreStartWorld(bool bSwitchingWorlds)
{
    // AIVolumes belong to the current world. Never reuse old map handles.
    FT_ResetNavigationCache();
}



//-----------------------------------------------------------------------------
//  CLTServerShell::PostStartWorld()
//
//-----------------------------------------------------------------------------
void CLTServerShell::PostStartWorld()
{
    FT_EnsureFireteamRoundController();
}



//-----------------------------------------------------------------------------
//  CLTServerShell::Update(LTFLOAT timeElapsed)
//
//-----------------------------------------------------------------------------
void CLTServerShell::Update(LTFLOAT timeElapsed)
{
	//g_pLTServer->CPrint("Update(%g)", timeElapsed);
}



//-----------------------------------------------------------------------------
//  CLTServerShell::CacheFiles()
//
//-----------------------------------------------------------------------------
void CLTServerShell::CacheFiles()
{
	//g_pLTServer->CPrint("CacheFiles()");
}



//-----------------------------------------------------------------------------
//  CLTServerShell::FileLoadNotify(const char *pFilename, LTRESULT status)
//
//-----------------------------------------------------------------------------
LTRESULT CLTServerShell::FileLoadNotify(const char *pFilename, LTRESULT status)
{
	//g_pLTServer->CPrint("FileLoadNotify(%s, %d)", pFilename, status);

	return LT_OK;
}



//-----------------------------------------------------------------------------
//  CLTServerShell::OnPlaybackFinish()
//
//-----------------------------------------------------------------------------
void CLTServerShell::OnPlaybackFinish()
{
	//g_pLTServer->CPrint("OnPlaybackFinish()");
}



//-----------------------------------------------------------------------------
//  CLTServerShell::ProcessPacket(ILTMessage_Read *pMsg, uint8 senderAddr[4],
//		uint16 senderPort)
//
//-----------------------------------------------------------------------------
LTRESULT CLTServerShell::ProcessPacket(ILTMessage_Read *pMsg, uint8 senderAddr[4], uint16 senderPort)
{
	//g_pLTServer->CPrint("ProcessPacket(%p, %p, %d)",pMsg, senderAddr, senderPort);

	return LT_OK;
}

