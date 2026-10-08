//------------------------------------------------------------------------------//
//
// MODULE   : ltclientshell.cpp
//
// PURPOSE  : CLTClientShell - Implementation
//
// CREATED  : 2002/03/02
//
// (c) 2002 LithTech, Inc.  All Rights Reserved
//
//------------------------------------------------------------------------------//
#include <windows.h>

#include "ltclientshell.h"
#include <fireteamtrace.h>
#include <time.h>
#include <ltcrashhandler.h>
#include "camera.h"
#include "clientinterfaces.h"
#include "msgids.h"
#include "commandids.h"
#include "worldpropsclient.h"
#include "playerclnt.h"
#include "statsgui.h"
#include "chatgui.h"
#include "FireteamSettingsMenu.h"
#include "FireteamLightGroupClient.h"
#include "FireteamHealthHud.h"
#include "FireteamWeaponHud.h"
#include "FireteamRoundHud.h"
#include "FireteamCombatFeedback.h"
#include "FireteamAmbientAudio.h"
#include "FireteamController.h"
#include "FireteamLoadingScreen.h"
// Client-side helper functions
#include "clienthelper.h"

// ClientFX
#include "ClientFXDB.h"

// Engine Includes
#include <iltclient.h>
#include <iltphysics.h>
#include <iltsoundmgr.h>
#include <iltcommon.h>
#include <stdio.h>

#define DEFAULT_GRAVITY     -2000.0f

//-----------------------------------------------------------------------------
//	This macro defines certain DLL exports and variables
SETUP_CLIENTSHELL();

CLTClientShell* g_pCShell = NULL;

static const float g_aQaMoveSteps[] =
{
    0.01f,
    0.05f,
    0.10f,
    0.25f,
    0.50f,
    1.00f
};

static const uint8 g_nQaMoveStepCount =
    (uint8)(
        sizeof(g_aQaMoveSteps) /
        sizeof(g_aQaMoveSteps[0]));

// Define the instantiation of the ClientShell interface.
define_interface(CLTClientShell, IClientShell);



//-------------------------------------------------------------------------------------------------
// Helper functions
//-------------------------------------------------------------------------------------------------


void ServerFn(int argc, char **argv)
{
    g_pLTClient->CPrint("Starting Multiplayer Server...");
    g_pLTClient->CPrint("Result: %d", g_pCShell->StartMultiplayerServer() );
}



void NormalFn(int argc, char **argv)
{
    g_pLTClient->CPrint("NormalFn");
    g_pLTClient->CPrint("Result: %d", g_pCShell->StartNormalGame() );
}



void JoinFn(int argc, char **argv)
{
    g_pLTClient->CPrint("JoinFn");
    if(argc == 1)
    {
		g_pLTClient->CPrint("Connecting to Multiplayer Server... %s", argv[0]);
		g_pLTClient->CPrint("Result: %d", g_pCShell->JoinMultiplayerGame(argv[0]) );
    }
}



void FindServersFn(int argc, char **argv)
{
    g_pLTClient->CPrint("This cvar is disabled");
//    g_pLTClient->CPrint("Finding Servers...");
//    g_pCShell->FindServers();
}



//-------------------------------------------------------------------------------------------------
// CLTClientShell
//-------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------
//	CLTClientShell::CLTClientShell()
//
//------------------------------------------------------------------------------
CLTClientShell::CLTClientShell()
:
m_hCamera(NULL),
m_pCamera(NULL),
m_bRender(true),
m_bFirstUpdate(true),
m_bHavePlayerStartInfo(false),
m_bQaControlDown(false),
m_bQaMenuOpen(false),
m_bQaMenuKeyHeld(false),
m_bQaZombiesEnabled(false),
m_bQaZombieSettingSent(false),
m_bQaSpectator(false),
m_bQaSpectatorSettingSent(false),
m_nQaMoveStepIndex(2),
m_bControllerWasConnected(false),
m_bControllerMenuDown(false),
m_bControllerMenuHoldActivated(false),
m_fControllerMenuDownAt(0.0f),
m_vPlayerStartPos(0.0f, 160.0f, 0.0f),
m_pWorldPropsClnt(NULL),
m_pPlayer(NULL),
m_nGameMode(LOCAL_GAMEMODE_NONE),
m_bInWorld(false),
m_bClientFXReady(false),
m_bShowStats(false)
{
	g_pCShell= this;
	m_rPlayerStartRot.Init();
}



//------------------------------------------------------------------------------
//	CLTClientShell::~CLTClientShell()
//
//------------------------------------------------------------------------------
CLTClientShell::~CLTClientShell()
{
}



//------------------------------------------------------------------------------
//  CLTClientShell::OnEngineInitialized(RMode *pMode, LTGUID *pAppGuid)
//
//------------------------------------------------------------------------------
LTRESULT CLTClientShell::OnEngineInitialized(RMode *pMode, LTGUID *pAppGuid)
{
    // FTDIAG OnEngineInitialized begin
    FTTraceReset();
    static LTCrashHandler s_FireteamCrashHandler(true);
    FTTraceLog("CLIENT", "00 OnEngineInitialized begin");
	LTRESULT result = LT_OK;

	result = VerifyClientInterfaces();
    FTTraceLog("CLIENT", "01 VerifyClientInterfaces=%d", (int)result);
	if (ShutdownIfFailed(result, "Error in VerifyClientInterfaces(). Shutting down.\n"))
	{
		return result;
	}

	result = InitRenderer(pMode);
    FTTraceLog("CLIENT", "02 InitRenderer=%d", (int)result);
	if (ShutdownIfFailed(result, "Error in InitRenderer(). Shutting down.\n"))
	{
		return result;
	}

	result = InitSound();
    FTTraceLog("CLIENT", "03 InitSound=%d", (int)result);
	DebugPrintIfFailed(result, "Error: Couldn't intialize sound system.\n");

	CreateWorldPropObject();
    FTTraceLog("CLIENT", "04 WorldProps created");

    g_pLTClient->CPrint("You must type server, normal or join <ip.ip.ip.ip>");

    result = this->CreateCamera();
    FTTraceLog("CLIENT", "05 CreateCamera=%d", (int)result);

    // Fireteam ClientFX bring-up.
    FTTraceLog("CLIENT", "06 ClientFX section begin");

    HCONSOLEVAR hDisableClientFX = g_pLTClient->GetConsoleVar("disableclientfx");
    bool bDisableClientFX = hDisableClientFX &&
        (g_pLTClient->GetVarValueFloat(hDisableClientFX) != 0.0f);

    m_bClientFXReady = false;

    if(bDisableClientFX)
    {
        FTTraceLog("CLIENT", "06A ClientFX disabled by launch option");
    }
    else
    {
        FTTraceLog("CLIENT", "06A ClientFXDB Init begin");
        if(!CClientFXDB::GetSingleton().Init(g_pLTClient))
        {
            FTTraceLog("CLIENT", "06X ClientFXDB Init failed");
            g_pLTClient->ShutdownWithMessage("Could not init ClientFXDB!");
            return LT_ERROR;
        }
        FTTraceLog("CLIENT", "06B ClientFXDB Init success");

        FTTraceLog("CLIENT", "06C ClientFXMgr Init begin");
        if(!m_ClientFXMgr.Init(g_pLTClient))
        {
            FTTraceLog("CLIENT", "06X ClientFXMgr Init failed");
            g_pLTClient->ShutdownWithMessage("Could not init ClientFXMgr!");
            return LT_ERROR;
        }

        m_ClientFXMgr.SetCamera(m_hCamera);
        m_bClientFXReady = true;
        FTTraceLog("CLIENT", "06D ClientFX ready");
    }
    //Register console programs
	g_pLTClient->RegisterConsoleProgram("Server", ServerFn);
	g_pLTClient->RegisterConsoleProgram("Normal", NormalFn);
	g_pLTClient->RegisterConsoleProgram("Join", JoinFn);
	g_pLTClient->RegisterConsoleProgram("FindServers", FindServersFn);

	// Fireteam: legacy SealHunter frontend disabled.

    // Create and Init the stats gui
    m_pStatsGui = new CStatsGui;
    assert(m_pStatsGui);
    m_pStatsGui->Init();

    // Create and Init the chat gui
    m_pChatGui = new CChatGui;
    assert(m_pChatGui);
    m_pChatGui->Init();

    // Initialize Fireteam presentation/settings before an autostarted world.
    // This lets gamma/volume apply to the loading screen too and prevents
    // server round messages from racing HUD initialization.
    FT_SettingsInit();
    FT_WeaponHudInit();
    FT_RoundHudInit();
    FT_CombatFeedbackInit();
    FT_LoadingScreenInit();

    HCONSOLEVAR hDevSpectator =
        g_pLTClient->GetConsoleVar(
            "devspectator");

    m_bQaSpectator =
        hDevSpectator &&
        g_pLTClient->GetVarValueFloat(
            hDevSpectator) != 0.0f;

    if(m_bQaSpectator)
    {
        g_pLTClient->CPrint(
            "Fireteam QA spectator: requested by +devspectator.");
    }

    HCONSOLEVAR hAutoStart = g_pLTClient->GetConsoleVar("autostart");
    if(hAutoStart && g_pLTClient->GetVarValueFloat(hAutoStart) != 0.0f)
    {
        const char *pMode = "single";
        HCONSOLEVAR hMode =
            g_pLTClient->GetConsoleVar("fireteammode");

        if(hMode)
        {
            const char *pConfiguredMode =
                g_pLTClient->GetVarValueString(hMode);

            if(pConfiguredMode &&
               pConfiguredMode[0])
            {
                pMode = pConfiguredMode;
            }
        }

        g_pLTClient->CPrint(
            "Fireteam: auto-starting mode=%s",
            pMode);

        if(_stricmp(pMode, "host") == 0)
        {
            result = StartMultiplayerServer();
        }
        else if(_stricmp(pMode, "join") == 0 ||
                _stricmp(pMode, "client") == 0)
        {
            const char *pJoinIP = "127.0.0.1";
            HCONSOLEVAR hJoinIP =
                g_pLTClient->GetConsoleVar("joinip");

            if(hJoinIP)
            {
                const char *pConfiguredIP =
                    g_pLTClient->GetVarValueString(hJoinIP);

                if(pConfiguredIP &&
                   pConfiguredIP[0])
                {
                    pJoinIP = pConfiguredIP;
                }
            }

            char szJoinIP[MAX_SGR_STRINGLEN];
            strncpy(
                szJoinIP,
                pJoinIP,
                sizeof(szJoinIP) - 1);
            szJoinIP[sizeof(szJoinIP) - 1] = '\0';

            result = JoinMultiplayerGame(
                szJoinIP);
        }
        else
        {
            result = StartNormalGame();
        }
    }

    return result;
}



//---------------------------------------------------------------------------
//  CLTClientShell::CreateCamera()
//
//---------------------------------------------------------------------------
LTRESULT CLTClientShell::CreateCamera()
{

    LTRESULT result = LT_OK;

    if( m_hCamera )
	{
        return LT_OK;
	}

    if( !m_pCamera )
	{
		m_pCamera = new CCamera;
	}

    if( !m_pCamera )
	{
		return LT_ERROR;
	}

    // Create the CAMERA
    result = m_pCamera->CreateCamera();
	if (ShutdownIfFailed(result, "Error in CreateCamera(). Shutting down.\n"))
	{
		return result;
	}

	m_hCamera = m_pCamera->GetCamera();

    return result;
}



//---------------------------------------------------------------------------
//  CLTClientShell::InitRenderer(RMode *pMode)
//
//---------------------------------------------------------------------------
LTRESULT CLTClientShell::InitRenderer(RMode *pMode)
{
	//	g_pLTClient->CPrint("CLTClientShell::InitRenderer");

	//	Attempt to set the render mode based on pMode.
	//		pMode contains command line parameters or
	//		console vars included in autoexec.cfg. Such as:
	//
	//		screenwidth 800
	//		screenheight 600
	//		bitdepth 32
	//
	//		Or on the command line:
	//
	//		lithtech.exe +screenwidth 800 +screenheight 600 +bitdepth 32

	if (g_pLTClient->SetRenderMode(pMode) != LT_OK)
	{
		g_pLTClient->DebugOut("Error: Couldn't set requested render mode!\n");

		// Try 640x480x16 as a fallback.

		RMode newRMode;
		memset(&newRMode, 0, sizeof(newRMode));

		newRMode.m_Width		= 640;
		newRMode.m_Height		= 480;
		newRMode.m_BitDepth		= 16;

		newRMode.m_bHWTnL	= pMode->m_bHWTnL;
		sprintf(newRMode.m_InternalName, "%s", pMode->m_InternalName);
		sprintf(newRMode.m_Description, "%s", pMode->m_Description);

		// Give it another try.
		g_pLTClient->DebugOut("Attempting to set render mode to 640x480x16...\n");

		if (g_pLTClient->SetRenderMode(&newRMode) != LT_OK)
		{
			// Still didn't work.
			g_pLTClient->DebugOut(
				"Error: Couldn't set to 640x480x16 mode. Shutting down.\n");
			g_pLTClient->ShutdownWithMessage(
				"Error: Couldn't set to 640x480x16 mode. Shutting down.\n");

			return LT_ERROR;
		}
	}

	return LT_OK;
}



//---------------------------------------------------------------------------
//  CLTClientShell::InitSound()
//
//---------------------------------------------------------------------------
LTRESULT CLTClientShell::InitSound()
{
	//	g_pLTClient->CPrint("CLTClientShell::InitSound");

	InitSoundInfo sndInfo;

	sndInfo.Init();

    // Initialize sound with defaults.
	return g_pLTCSoundMgr->InitSound(&sndInfo);
}



//---------------------------------------------------------------------------
//  CLTClientShell::InitGame()
//
//---------------------------------------------------------------------------
LTRESULT CLTClientShell::InitGame(EGameType eType, char* ip)
{
	LTRESULT result;

	const char* pLevelName = NULL;
	StartGameRequest request;
	memset(&request, 0, sizeof(StartGameRequest));

    switch(eType)
    {
	case EGT_HOST:
		{
			request.m_Type = STARTGAME_HOST;
			request.m_HostInfo.m_dwMaxConnections = 24;
            strncpy(request.m_HostInfo.m_sName, "Fireteam Server", MAX_NETSESSIONNAME_LEN);
            request.m_HostInfo.m_Port = 27889;
			g_pLTClient->InitNetworking(NULL, 0);
			SetService();
		}
		break;
	case EGT_CONNECT:
		{
			request.m_Type = STARTGAME_CLIENTTCP;
			strncpy( request.m_TCPAddress, ip, MAX_SGR_STRINGLEN);
            strncat( request.m_TCPAddress, ":27889", MAX_SGR_STRINGLEN);
			g_pLTClient->InitNetworking(NULL, 0);
			SetService();
		}break;
	case EGT_NORMAL:
	default:
		{
			request.m_Type = STARTGAME_NORMAL;
			request.m_HostInfo.m_dwMaxConnections = 1;
		}
		break;
    }

    HCONSOLEVAR hVar = g_pLTClient->GetConsoleVar("runworld");
	pLevelName = g_pLTClient->GetVarValueString(hVar);

	if (pLevelName)
	{
		// (length includes the NULL character)
		LTStrCpy(request.m_WorldName, pLevelName, strlen(pLevelName)+1);
	}
	else
	{
		strcpy(request.m_WorldName, "worlds/demoroom");
	}

    FT_LoadingScreenStart(
        request.m_WorldName);

	result = g_pLTClient->StartGame(&request);

    FT_LoadingScreenStop();

	if (LT_OK != result)
	{
		DebugPrintIfFailed(result, "Start Game Request failed.\n");
		return result;
	}

	return result;
}



//---------------------------------------------------------------------------
//  CLTClientShell::SetService()
//
//---------------------------------------------------------------------------
bool CLTClientShell::SetService()
{
	NetService *pCur, *pListHead;
	HNETSERVICE hNetService;

	pCur      = NULL;
	pListHead = NULL;
	hNetService = NULL;

    if( g_pLTClient->GetServiceList( pListHead ) != LT_OK || !pListHead )
        return false;

	// Find the service specified.
	pCur = pListHead;
	while( pCur )
	{
		if( pCur->m_dwFlags & NETSERVICE_TCPIP )
		{
			hNetService = pCur->m_handle;
			break;
		}

		pCur = pCur->m_pNext;
	}

	// Free the service list.
    g_pLTClient->FreeServiceList( pListHead );

	// Check if tcp not found.
	if( !hNetService )
        return false;

	// Select it.
    if( g_pLTClient->SelectService( hNetService ) != LT_OK )
        return false;

    return true;
}



//---------------------------------------------------------------------------
//  CLTClientShell::OnEnterWorld()
//
//---------------------------------------------------------------------------
void CLTClientShell::OnEnterWorld()
{
    g_pLTClient->CPrint("CLTClientShell::OnEnterWorld()");

    // Clear the input buffer for a clean start
    g_pLTClient->ClearInput();

    if(!m_pPlayer)
    {
        m_pPlayer = new CPlayerClnt();
    }

    m_bInWorld = true;

    // Low-volume Cabin Fever ambience loop supplied as a local mod asset.
    FT_AmbientAudioEnterWorld();

    // NOLF2/Jupiter lighting diagnostics. Do not modify these values here;
    // record them so CA map parity can be fixed from evidence rather than a
    // blanket brightness multiplier.
    float fGammaR = 1.0f;
    float fGammaG = 1.0f;
    float fGammaB = 1.0f;

    HCONSOLEVAR hGamma = g_pLTClient->GetConsoleVar("GammaR");
    if(hGamma) fGammaR = g_pLTClient->GetVarValueFloat(hGamma);
    hGamma = g_pLTClient->GetConsoleVar("GammaG");
    if(hGamma) fGammaG = g_pLTClient->GetVarValueFloat(hGamma);
    hGamma = g_pLTClient->GetConsoleVar("GammaB");
    if(hGamma) fGammaB = g_pLTClient->GetVarValueFloat(hGamma);

    LTVector vCameraLightAdd(0.0f, 0.0f, 0.0f);
    if(m_hCamera)
    {
        g_pLTClient->GetCameraLightAdd(
            m_hCamera,
            &vCameraLightAdd);
    }

    g_pLTClient->CPrint(
        "Fireteam lighting: gamma %.2f %.2f %.2f camera light-add %.2f %.2f %.2f",
        fGammaR,
        fGammaG,
        fGammaB,
        vCameraLightAdd.x,
        vCameraLightAdd.y,
        vCameraLightAdd.z);

}



//---------------------------------------------------------------------------
//  CLTClientShell::OnExitWorld()
//
//---------------------------------------------------------------------------
void CLTClientShell::OnExitWorld()
{
    FT_AmbientAudioExitWorld();

	if (m_pPlayer)
	{
		delete m_pPlayer;
		m_pPlayer = NULL;
	}

    m_bInWorld = false;
    m_bFirstUpdate = true;
    m_bHavePlayerStartInfo = false;
    m_bQaControlDown = false;
    m_bQaMenuOpen = false;
    m_bQaMenuKeyHeld = false;
    m_bQaZombieSettingSent = false;
    m_bQaSpectatorSettingSent = false;
    m_bControllerWasConnected = false;
    m_bControllerMenuDown = false;
    m_bControllerMenuHoldActivated = false;
    FT_ControllerReset();

    FT_WeaponHudSetQaMenu(
        false,
        g_aQaMoveSteps[m_nQaMoveStepIndex],
        m_bQaZombiesEnabled);

    if(m_pCamera)
    {
        m_pCamera->SetFreecamEnabled(
            false);
    }

    FT_RoundHudSetSpectator(
        false,
        false);

    FT_ClearLightGroups();

    // Stop all the client FX
	if(m_bClientFXReady) m_ClientFXMgr.ShutdownAllFX();
}



//---------------------------------------------------------------------------
//  CLTClientShell::OnMessage(ILTMessage_Read* pMessage)
//
//---------------------------------------------------------------------------
void CLTClientShell::OnMessage(ILTMessage_Read* pMessage)
{
    pMessage->SeekTo(0);
    uint8 messageID = pMessage->Readuint8();

    switch (messageID)
    {
    case MSG_STARTPOINT_POS_ROT:
        {
			m_vPlayerStartPos = pMessage->ReadLTVector();
			m_rPlayerStartRot = pMessage->ReadLTRotation();
            m_bHavePlayerStartInfo = true;
            g_pLTClient->CPrint("CLIENT startpoint: %f %f %f", m_vPlayerStartPos.x, m_vPlayerStartPos.y, m_vPlayerStartPos.z);
		}
		break;
    case MSG_CS_MY_CLUB:
        {
            m_pPlayer->SetClub(pMessage->ReadObject());
        }
		break;
    case MSG_CS_SCORE:
        {
            uint32 iScore = pMessage->Readuint32();
            float  fMoney = pMessage->Readfloat();
            m_pPlayer->SetScore(iScore);
            m_pPlayer->SetMoney(fMoney);

            m_Gui.SetStats(iScore, fMoney);
        }
        break;
    case MSG_SC_HEALTH:
        {
            uint8 nHealth = pMessage->Readuint8();
            uint8 nMaxHealth = pMessage->Readuint8();
            FT_SetHealth(nHealth, nMaxHealth);
        }
        break;
    case MSG_SC_LIVES:
        {
            const uint8 nLives =
                pMessage->Readuint8();
            const uint8 nMaxLives =
                pMessage->Readuint8();

            FT_RoundHudSetLives(
                nLives,
                nMaxLives);
        }
        break;
    case MSG_SC_RESPAWN:
        {
            LTVector vRespawn = pMessage->ReadLTVector();
            LTRotation rRespawn = pMessage->ReadLTRotation();

            m_vPlayerStartPos = vRespawn;
            m_rPlayerStartRot = rRespawn;

            if(m_pPlayer && m_pPlayer->GetPlayerObject())
            {
                g_pLTClient->SetObjectPos(m_pPlayer->GetPlayerObject(), &vRespawn);
                g_pLTClient->SetObjectRotation(m_pPlayer->GetPlayerObject(), &rRespawn);

                LTVector vZero(0.0f, 0.0f, 0.0f);
                g_pLTCPhysics->SetVelocity(m_pPlayer->GetPlayerObject(), &vZero);
            }
        }
        break;
    case MSG_SC_LIGHTGROUP:
        {
            uint32 nLightGroupID = pMessage->Readuint32();
            LTVector vAdjustment = pMessage->ReadLTVector();
            FT_QueueLightGroup(nLightGroupID, vAdjustment);
        }
        break;
    case MSG_SC_ROUND:
        {
            FT_RoundHudHandleMessage(pMessage);
        }
        break;
    case MSG_SC_POWERUP:
        {
            char szReward[128];
            szReward[0] = '\0';

            pMessage->ReadString(
                szReward,
                sizeof(szReward));

            const float fSeconds =
                pMessage->Readfloat();

            FT_RoundHudShowAnnouncement(
                szReward,
                fSeconds);
        }
        break;
    case MSG_SC_POWERUP_STATE:
        {
            const float fBottomlessSeconds =
                pMessage->Readfloat();
            const float fOneHitSeconds =
                pMessage->Readfloat();
            const float fGodSeconds =
                pMessage->Readfloat();
            const float fWallhackSeconds =
                pMessage->Readfloat();

            FT_RoundHudSetTimedPowerups(
                fBottomlessSeconds,
                fOneHitSeconds,
                fGodSeconds,
                fWallhackSeconds);
        }
        break;
    case MSG_SC_COMBAT_FEEDBACK:
        {
            FT_CombatFeedbackHandleMessage(
                pMessage);
        }
        break;
    case MSG_SC_AMMO:
        {
            uint16 nClip = pMessage->Readuint16();
            uint16 nReserve = pMessage->Readuint16();
            FT_SetPrimaryAmmo(nClip, nReserve);

            if(m_pPlayer)
            {
                m_pPlayer->SetPrimaryAmmo(
                    nClip,
                    nReserve);
            }

            // Normal Fireteam behavior: an empty magazine with reserve ammo
            // immediately requests a server-authoritative reload. A future
            // Expert mode can deliberately disable this convenience.
            if(m_pPlayer && nClip == 0 && nReserve > 0)
            {
                const FTWeaponDef *pDef = m_pPlayer->GetCurrentWeaponDef();
                if(pDef &&
                   pDef->bAutoReload &&
                   pDef->nClipSize > 0 &&
                   pDef->eType != FT_WEAPON_MELEE &&
                   pDef->fReloadSeconds > 0.0f)
                {
                    // This also starts the local reload animation. The server
                    // may already have begun the same reload after the final
                    // shot; duplicate requests are ignored while reloading.
                    m_pPlayer->ReloadWeapon();
                }
            }
        }
        break;
	case MSG_WORLD_PROPS:
		{
			m_pWorldPropsClnt->UnpackWorldProps(pMessage);
		}
		break;
    case MSG_SERVER_SCORES:
        {
            assert(m_pStatsGui);
            m_pStatsGui->HandleMessage(pMessage);

        }
        break;
    case MSG_SC_CHAT:
        {
            assert(m_pChatGui);
            m_pChatGui->HandleMessage(pMessage);
        }
        break;
	default:
		break;
	}
}



//---------------------------------------------------------------------------
//  CLTClientShell::Update()
//
//---------------------------------------------------------------------------
void CLTClientShell::Update()
{
	LTVector vDims;
	LTVector vPos;
	LTRotation rRot;

    // If the game mode is other than NONE, proceed
    if(m_nGameMode)
    {
        if(IsInWorld())
        {
            if( m_bFirstUpdate )
            {
                // Wait for the guaranteed authoritative startpoint before
                // creating the separate local collision/player object.
                if(!m_bHavePlayerStartInfo)
                {
                    return;
                }

				//Check to see if the client object is available, if not return
                HLOCALOBJ hObject = g_pLTClient->GetClientObject();
                if(!hObject)
				{
					return;
				}

                // Set our client object non solid and pin the local
                // presentation to the authoritative spawn before CreatePlayer
                // samples its position. This removes the alt-tab/load race.
                g_pLTCCommon->SetObjectFlags(hObject, OFT_Flags, FLAG_CLIENTNONSOLID, FLAG_CLIENTNONSOLID);
                g_pLTClient->SetObjectPos(
                    hObject,
                    &m_vPlayerStartPos);
                g_pLTClient->SetObjectRotation(
                    hObject,
                    &m_rPlayerStartRot);
                m_pPlayer->SetClientObject(hObject);
                m_pPlayer->CreatePlayer();
                HLOCALOBJ hPlayer = m_pPlayer->GetPlayerObject();

                if (NULL != hPlayer)
                {
                    LTVector vNewGlobalForce(0, DEFAULT_GRAVITY, 0);
                    g_pLTCPhysics->SetGlobalForce(vNewGlobalForce);
                    g_pLTCPhysics->SetStairHeight(16.0f);

                    //Ask for CLUB ID
                	ILTMessage_Write *pMessage;
	                LTRESULT nResult = g_pLTCCommon->CreateMessage(pMessage);

	                if( LT_OK == nResult)
	                {
		                pMessage->IncRef();
		                pMessage->Writeuint8(MSG_CS_MY_CLUB);
		                g_pLTClient->SendToServer(pMessage->Read(), MESSAGE_GUARANTEED);
		                pMessage->DecRef();
	                }

                    m_bFirstUpdate = false;
                }

                //Set the actual client object invisible.
                g_pLTCCommon->SetObjectFlags(hObject, OFT_Flags, 0, FLAG_VISIBLE);

                // Send our name to the server
                SendPlayerName();
            }


            assert(m_pChatGui);
            m_pChatGui->Update();

            // Poll for user input
            PollInput();

            if(!m_pCamera->IsFreecam())
            {
                //Update our player object
                m_pPlayer->Update();

                // Update our camera
                m_pCamera->UpdatePosition(
                    m_pPlayer->GetPlayerObject(),
                    m_pPlayer->GetEyeHeight());

                const FTWeaponDef *pViewWeapon =
                    m_pPlayer->GetCurrentWeaponDef();

                const bool bShowViewWeapon =
                    m_pCamera->IsFirstPerson() &&
                    !(m_pCamera->IsWeaponZoomed() &&
                      pViewWeapon &&
                      pViewWeapon->bZoomHideWeapon);

                m_pPlayer->UpdateWeaponView(
                    bShowViewWeapon);
            }
            else
            {
                m_pPlayer->UpdateWeaponView(
                    false);
            }
        }
    }

	if( m_bRender )
	{
		if( Render() != LT_OK )
		{
			g_pLTClient->DebugOut("Error in Render()\n");
			g_pLTClient->Shutdown();
		}
	}

    FT_UpdateLightGroups();

	// Update the world properties class
	if( m_pWorldPropsClnt )
	{
		m_pWorldPropsClnt->Update();
	}
}



//---------------------------------------------------------------------------
//  CLTClientShell::Render()
//
//---------------------------------------------------------------------------
LTRESULT CLTClientShell::Render()
{
	LTRESULT result;

    float fFrameTime = g_pLTClient->GetFrameTime();

	//	Clear the screen to prepare for the next draw
	result = g_pLTClient->ClearScreen(NULL, CLEARSCREEN_SCREEN | CLEARSCREEN_RENDER, NULL);
	if (LT_OK != result)
	{
		g_pLTClient->DebugOut("Error in ClearScreen()\n");
		g_pLTClient->Shutdown();
	}

	//	Tell LithTech we are going to be starting our 3d render
	result = g_pLTClient->Start3D();
	if (LT_OK != result)
	{
		g_pLTClient->DebugOut("Error in Start3D()\n");
		g_pLTClient->Shutdown();
	}

	//	Tell LithTech to render the scene using this camera
	result = g_pLTClient->RenderCamera(m_hCamera, fFrameTime);
	if (LT_OK != result)
	{
		g_pLTClient->DebugOut("Error in RenderCamera()\n");
		g_pLTClient->Shutdown();
	}

	//update all the effects status as well as any that might effect the camera
	if(m_bClientFXReady) m_ClientFXMgr.UpdateAllActiveFX( m_bRender );

	// Render the effects
	if(m_bClientFXReady) m_ClientFXMgr.RenderAllActiveFX( m_bRender );

    if(IsInWorld())
    {
        const bool bFreecam =
            m_pCamera &&
            m_pCamera->IsFreecam();

        if(m_pPlayer &&
           !bFreecam)
        {
            FT_RenderPoisonOverlay(
                m_pPlayer->GetPlayerObject());
        }

        if(!bFreecam)
        {
            FT_RenderHealthHud();
            FT_RenderCombatFeedback();
        }

        FT_RenderRoundHud();

        if(m_pPlayer &&
           m_pCamera &&
           !bFreecam &&
           !FT_SettingsIsOpen())
        {
            const FTWeaponDef *pHudWeapon =
                m_pPlayer->GetCurrentWeaponDef();

            FT_RenderWeaponHud(
                m_pPlayer->GetWeaponSlot(),
                m_pCamera->IsFirstPerson(),
                pHudWeapon
                    ? (pHudWeapon->bShowCrosshair &&
                       !m_pCamera->IsWeaponZoomed() &&
                       !m_pChatGui->IsChatInputActive())
                    : false,
                pHudWeapon
                    ? (pHudWeapon->nClipSize > 0 &&
                       !m_pPlayer->IsDevWeaponQa())
                    : false,
                m_pPlayer->IsMoving(),
                m_pCamera->IsWeaponZoomed());
        }
    }

	// Render the gui.
	// Fireteam: legacy SealHunter frontend render disabled.

    if(IsInWorld())
    {
        if(m_bShowStats)
        {
            assert(m_pStatsGui);
            m_pStatsGui->Render();
        }

        assert(m_pChatGui);
        m_pChatGui->Render();
        FT_SettingsRender();
    }

	//	Tell LithTech to finish rendering for this scene
	result = g_pLTClient->End3D(END3D_CANDRAWCONSOLE);
	if (LT_OK != result)
	{
		g_pLTClient->DebugOut("Error in End3D()\n");
		g_pLTClient->Shutdown();
	}

	//	Tell LithTech to flip the screen surface
	result = g_pLTClient->FlipScreen(NULL);
	if (LT_OK != result)
	{
		g_pLTClient->DebugOut("Error in FlipScreen()\n");
		g_pLTClient->Shutdown();
	}

	return LT_OK;
}



//---------------------------------------------------------------------------
//	CLTClientShell::OnEvent(uint32 dwEventID, uint32 dwParam)
//
//---------------------------------------------------------------------------
void CLTClientShell::OnEvent(uint32 dwEventID, uint32 dwParam)
{
    switch(dwEventID)
    {
    case LTEVENT_LOSTFOCUS:
        m_bRender = false;
        m_bQaControlDown = false;
        m_bControllerMenuDown = false;
        m_bControllerMenuHoldActivated = false;
        FT_ControllerReset();
        break;

    case LTEVENT_RENDERTERM:
        {
            m_bRender = false;

            // Let the ClientFx mgr know the renderer is shutting down
            if(m_bClientFXReady) m_ClientFXMgr.OnRendererShutdown();
        }
        break;

    case LTEVENT_GAINEDFOCUS:
        m_bRender = true;
        break;

    case LTEVENT_RENDERINIT:
        m_bRender = true;
        break;

    case LTEVENT_RENDERALMOSTINITTED:
        m_bRender = true;
        break;

    default:
        break;
    }
}



//---------------------------------------------------------------------------
//	CLTClientShell::OnCommandOn()
//
//---------------------------------------------------------------------------
void CLTClientShell::OnCommandOn(int command)
{
	if (m_nGameMode == LOCAL_GAMEMODE_NONE)
	{
		// Fireteam: launcher owns frontend/menu input.
	}


    // Are we currently typing text?
    if(!m_pChatGui->IsChatInputActive())
    {
	    // quit
	    if (command == COMMAND_QUIT)
	    {
            // This is done in OnKeyDown() now
            // g_pLTClient->Shutdown();
        }else
        if(m_bInWorld)
        {
            if(command == COMMAND_CHAT)
            {
                m_pChatGui->SetActive(true);
            }
            else if(command == COMMAND_ALT_ATTACK)
            {
                const FTWeaponDef *pWeaponDef =
                    m_pPlayer
                    ? m_pPlayer->GetCurrentWeaponDef()
                    : LTNULL;

                if(pWeaponDef &&
                   pWeaponDef->fZoomFovDegrees > 0.0f &&
                   m_pCamera)
                {
                    m_pCamera->ToggleWeaponZoom(
                        pWeaponDef->fZoomFovDegrees);
                }
            }
            else
            if (command == COMMAND_SHOWSTATS)
            {
                SetStatsVisible(true);
            }
        }
    }
    else        
    {
        // Don't show stats if we're typing in chat.
        m_bShowStats = false;
    }
}



//---------------------------------------------------------------------------
//  CLTClientShell::OnCommandOff()
//
//---------------------------------------------------------------------------
void CLTClientShell::OnCommandOff(int command)
{
    //
    if (command == COMMAND_SHOWSTATS)
    {
        SetStatsVisible(false);
    }
}



//---------------------------------------------------------------------------
void CLTClientShell::SetStatsVisible(
    bool bVisible)
{
    if(m_bShowStats == bVisible)
    {
        return;
    }

    ILTMessage_Write *pMessage = LTNULL;

    if(g_pLTCCommon->CreateMessage(
           pMessage) == LT_OK &&
       pMessage)
    {
        pMessage->IncRef();
        pMessage->Writeuint8(
            MSG_CS_SCORE);
        pMessage->Writebool(
            bVisible);
        g_pLTClient->SendToServer(
            pMessage->Read(),
            MESSAGE_GUARANTEED);
        pMessage->DecRef();
    }

    m_bShowStats =
        bVisible;
}


//---------------------------------------------------------------------------
//  CLTClientShell::PollInput()
//
//---------------------------------------------------------------------------
LTRESULT CLTClientShell::PollInput()
{
	float offsets[3];
	LTVector vVel, vUp, vRight, vForward, vTemp;
	LTRotation rRot, rTemp;
	float fFrameTime;
	bool bMoved = false;
	bool bRotated = false;

    FTControllerState controller;
    FT_ControllerPoll(
        controller);

    if(controller.bConnected !=
       m_bControllerWasConnected)
    {
        m_bControllerWasConnected =
            controller.bConnected;

        g_pLTClient->CPrint(
            controller.bConnected
                ? "Fireteam controller: Xbox/XInput pad connected."
                : "Fireteam controller: pad disconnected.");
    }

    const float fControllerNow =
        g_pLTClient->GetTime();
    const bool bMenuButtonDown =
        controller.bConnected &&
        (controller.nButtons &
         FT_PAD_MENU) != 0;

    if(bMenuButtonDown &&
       !m_bControllerMenuDown)
    {
        m_bControllerMenuDown = true;
        m_bControllerMenuHoldActivated = false;
        m_fControllerMenuDownAt =
            fControllerNow;
    }

    if(bMenuButtonDown &&
       m_bControllerMenuDown &&
       !m_bControllerMenuHoldActivated &&
       !FT_SettingsIsOpen() &&
       !m_pChatGui->IsChatInputActive() &&
       (fControllerNow -
        m_fControllerMenuDownAt) >= 0.55f)
    {
        // Requested Xbox layout: hold Menu opens text chat.
        m_pChatGui->SetActive(true);
        m_bControllerMenuHoldActivated = true;
        SetStatsVisible(false);
    }

    if(!bMenuButtonDown &&
       m_bControllerMenuDown)
    {
        if(!m_bControllerMenuHoldActivated &&
           !m_pChatGui->IsChatInputActive())
        {
            // Short Menu press opens/closes the existing in-game settings menu.
            FT_SettingsToggle();
        }

        m_bControllerMenuDown = false;
        m_bControllerMenuHoldActivated = false;
    }

    if(controller.nPressed &
       FT_PAD_VIEW)
    {
        SetStatsVisible(true);
    }

    if(controller.nReleased &
       FT_PAD_VIEW)
    {
        SetStatsVisible(false);
    }

    if(FT_SettingsIsOpen())
    {
        if(controller.nPressed & FT_PAD_DPAD_UP)
            FT_SettingsHandleKey(VK_UP);
        if(controller.nPressed & FT_PAD_DPAD_DOWN)
            FT_SettingsHandleKey(VK_DOWN);
        if(controller.nPressed & FT_PAD_DPAD_LEFT)
            FT_SettingsHandleKey(VK_LEFT);
        if(controller.nPressed & FT_PAD_DPAD_RIGHT)
            FT_SettingsHandleKey(VK_RIGHT);
        if(controller.nPressed & FT_PAD_A)
            FT_SettingsHandleKey(VK_RETURN);
        if(controller.nPressed & FT_PAD_B)
            FT_SettingsHandleKey(VK_ESCAPE);

        m_pPlayer->SetControllerMoveAxes(
            0.0f,
            0.0f);
        m_pPlayer->UpdateMoveFlags(0);
        return LT_OK;
    }

    if(m_pChatGui->IsChatInputActive())
    {
        if(controller.nPressed &
           FT_PAD_B)
        {
            m_pChatGui->HandleInputDown(
                VK_ESCAPE,
                0);
        }

        m_pPlayer->SetControllerMoveAxes(
            0.0f,
            0.0f);
        return LT_OK;
    }

    const bool bEliminated =
        FT_RoundHudIsPlayerEliminated();

    const bool bQaFreecam =
        m_bQaSpectator &&
        m_nGameMode ==
            LOCAL_GAMEMODE_NORMAL;

    const bool bSpectating =
        bEliminated ||
        bQaFreecam;

    if(bSpectating)
    {
        if(bQaFreecam &&
           !m_bQaSpectatorSettingSent)
        {
            SendQaSpectatorSetting();
        }

        m_pCamera->ClearWeaponZoom();
        m_pCamera->SetFreecamEnabled(
            true,
            m_pPlayer->GetPlayerObject(),
            m_pPlayer->GetEyeHeight());

        FT_RoundHudSetSpectator(
            true,
            bQaFreecam);

        m_pPlayer->SetControllerMoveAxes(
            0.0f,
            0.0f);
        m_pPlayer->UpdateMoveFlags(
            0);
        m_pPlayer->ReleaseAttackTrigger();

        HLOCALOBJ hPlayer =
            m_pPlayer->GetPlayerObject();

        if(hPlayer)
        {
            LTVector vStop(
                0.0f,
                0.0f,
                0.0f);

            g_pLTCPhysics->SetVelocity(
                hPlayer,
                &vStop);
        }

        float spectatorOffsets[3] =
        {
            0.0f,
            0.0f,
            0.0f
        };

        g_pLTClient->GetAxisOffsets(
            spectatorOffsets);

        const float fSpectatorFrameTime =
            g_pLTClient->GetFrameTime();

        float fForward = 0.0f;
        float fRight = 0.0f;
        float fUp = 0.0f;

        if(g_pLTClient->IsCommandOn(
               COMMAND_MOVE_FORWARD))
        {
            fForward += 1.0f;
        }

        if(g_pLTClient->IsCommandOn(
               COMMAND_MOVE_BACKWARD))
        {
            fForward -= 1.0f;
        }

        if(g_pLTClient->IsCommandOn(
               COMMAND_MOVE_RIGHT))
        {
            fRight += 1.0f;
        }

        if(g_pLTClient->IsCommandOn(
               COMMAND_MOVE_LEFT))
        {
            fRight -= 1.0f;
        }

        if(g_pLTClient->IsCommandOn(
               COMMAND_JUMP))
        {
            fUp += 1.0f;
        }

        if(g_pLTClient->IsCommandOn(
               COMMAND_CROUCH))
        {
            fUp -= 1.0f;
        }

        if(controller.bConnected)
        {
            fForward +=
                controller.fMoveY;
            fRight +=
                controller.fMoveX;

            if(controller.nButtons &
               FT_PAD_A)
            {
                fUp += 1.0f;
            }

            if(controller.nButtons &
               FT_PAD_B)
            {
                fUp -= 1.0f;
            }
        }

        const bool bFast =
            g_pLTClient->IsCommandOn(
                COMMAND_SPRINT) ||
            (controller.bConnected &&
             (controller.nButtons &
              FT_PAD_LTHUMB));

        const float fYaw =
            spectatorOffsets[0] +
            (controller.bConnected
                ? controller.fLookX *
                    fSpectatorFrameTime *
                    2.65f
                : 0.0f);

        const float fPitch =
            spectatorOffsets[1] +
            (controller.bConnected
                ? -controller.fLookY *
                    fSpectatorFrameTime *
                    2.10f
                : 0.0f);

        m_pCamera->UpdateFreecam(
            fForward,
            fRight,
            fUp,
            fYaw,
            fPitch,
            fSpectatorFrameTime,
            bFast);

        SendSpectatorViewPosition();

        return LT_OK;
    }

    if(m_pCamera->IsFreecam())
    {
        m_pCamera->SetFreecamEnabled(
            false);

        FT_RoundHudSetSpectator(
            false,
            false);
    }

    if(FT_RoundHudIsGameOver())
    {
        m_pPlayer->SetControllerMoveAxes(
            0.0f,
            0.0f);
        m_pPlayer->UpdateMoveFlags(
            0);
        return LT_OK;
    }

    if(m_pPlayer->IsDevWeaponQa())
    {
        if(!m_bQaZombieSettingSent)
        {
            SendQaZombieSetting();

            // Weapon QA is an editor workflow, so show the tools immediately
            // on first entry. Insert/F10 can collapse it afterward.
            m_bQaMenuOpen = true;

            FT_WeaponHudSetQaMenu(
                true,
                g_aQaMoveSteps[
                    m_nQaMoveStepIndex],
                m_bQaZombiesEnabled);

            g_pLTClient->CPrint(
                "Fireteam QA tools: OPEN (Insert or F10 toggles panel).");
        }

        const bool bQaMenuKeyDown =
            ((GetAsyncKeyState(
                  VK_INSERT) &
              0x8000) != 0) ||
            ((GetAsyncKeyState(
                  VK_F10) &
              0x8000) != 0);

        if(bQaMenuKeyDown &&
           !m_bQaMenuKeyHeld)
        {
            const bool bWasQaMenuOpen =
                m_bQaMenuOpen;

            m_bQaMenuOpen =
                !m_bQaMenuOpen;

            if(bWasQaMenuOpen &&
               !m_bQaMenuOpen)
            {
                m_pPlayer->SaveDevWeaponView();
            }

            g_pLTClient->ClearInput();

            FT_WeaponHudSetQaMenu(
                m_bQaMenuOpen,
                g_aQaMoveSteps[
                    m_nQaMoveStepIndex],
                m_bQaZombiesEnabled);

            g_pLTClient->CPrint(
                "Fireteam QA tools: %s",
                m_bQaMenuOpen
                    ? "OPEN"
                    : "CLOSED");
        }

        m_bQaMenuKeyHeld =
            bQaMenuKeyDown;
    }
    else
    {
        m_bQaMenuKeyHeld = false;
    }

	fFrameTime = g_pLTClient->GetFrameTime();

    if(controller.bConnected)
    {
        const float kControllerYawSpeed =
            2.65f;
        const float kControllerPitchSpeed =
            2.10f;

        if(controller.fLookX != 0.0f ||
           controller.fLookY != 0.0f)
        {
            bRotated = true;

            m_pPlayer->UpdateRotation(
                controller.fLookX *
                    fFrameTime *
                    kControllerYawSpeed,
                0.0f,
                0.0f);

            m_pCamera->UpdatePitch(
                -controller.fLookY *
                    fFrameTime *
                    kControllerPitchSpeed);
        }

        if(controller.bLeftTriggerPressed)
        {
            const FTWeaponDef *pWeapon =
                m_pPlayer->GetCurrentWeaponDef();

            if(pWeapon &&
               pWeapon->fZoomFovDegrees > 0.0f)
            {
                m_pCamera->ToggleWeaponZoom(
                    pWeapon->fZoomFovDegrees);
            }
        }

        if(controller.bLeftTriggerReleased &&
           m_pCamera->IsWeaponZoomed())
        {
            m_pCamera->ClearWeaponZoom();
        }

        if(controller.nPressed &
           FT_PAD_X)
        {
            m_pPlayer->ReloadWeapon();
        }

        if(controller.nPressed &
           FT_PAD_Y)
        {
            m_pCamera->ClearWeaponZoom();
            m_pPlayer->CycleWeapon(1);
        }

        if(controller.nPressed &
           FT_PAD_RTHUMB)
        {
            // Current server protocol has a dedicated melee loadout slot.
            // R3 equips it and starts the attack; quick-melee can later restore
            // the prior weapon once that protocol exists.
            m_pCamera->ClearWeaponZoom();

            if(m_pPlayer->SelectWeaponSlot(3))
            {
                m_pPlayer->Attack();
            }
        }
    }

    m_pPlayer->SetControllerMoveAxes(
        controller.bConnected
            ? controller.fMoveY
            : 0.0f,
        controller.bConnected
            ? controller.fMoveX
            : 0.0f);

    HLOCALOBJ hPlayer = m_pPlayer->GetPlayerObject();

    g_pLTCPhysics->GetVelocity(hPlayer, &vVel);

	g_pLTClient->GetObjectRotation(hPlayer, &rRot);
	g_pLTCCommon->GetRotationVectors(rRot, vUp, vRight, vForward);

	g_pLTClient->GetAxisOffsets(offsets);

	if ((offsets[0] != 0.0f) ||
		(offsets[1] != 0.0f) ||
		(offsets[2] != 0.0f))
	{
		bRotated = true;
        m_pCamera->UpdatePitch(offsets[1]);
        m_pPlayer->UpdateRotation(offsets[0], 0.0f, 0.0f);
        if(m_pCamera->IsFirstPerson() && offsets[2] != 0.0f)
        {
            m_pCamera->ClearWeaponZoom();
            m_pPlayer->CycleWeapon(offsets[2] > 0.0f ? 1 : -1);
        }
        else
        {
            m_pCamera->UpdateZoom(offsets[2]);
        }
	}

    uint8 dwMoveFlags = 0;
    bool bControllerQuickGrenade = false;

	// left
	if (g_pLTClient->IsCommandOn(COMMAND_MOVE_LEFT))
	{
        dwMoveFlags |= MOVE_LEFT;
	}

	// right
	if (g_pLTClient->IsCommandOn(COMMAND_MOVE_RIGHT))
	{
        dwMoveFlags |= MOVE_RIGHT;
    }

	// move forward
	if ( g_pLTClient->IsCommandOn(COMMAND_MOVE_FORWARD))
    {
        dwMoveFlags |= MOVE_FORWARD;
    }

	// move back
	if (g_pLTClient->IsCommandOn(COMMAND_MOVE_BACKWARD))
    {
        dwMoveFlags |= MOVE_BACKWARD;
    }

    if(controller.bConnected)
    {
        if(controller.nPressed &
           FT_PAD_LB)
        {
            m_pCamera->ClearWeaponZoom();

            bControllerQuickGrenade =
                m_pPlayer->SelectFirstWeaponType(
                    FT_WEAPON_GRENADE);

            if(!bControllerQuickGrenade)
            {
                g_pLTClient->CPrint(
                    "Fireteam controller: LB grenade requested, but no grenade is equipped.");
            }
        }

        if(controller.nButtons &
           FT_PAD_B)
        {
            dwMoveFlags |=
                MOVE_CROUCH;
        }

        if(controller.nButtons &
           FT_PAD_LTHUMB)
        {
            dwMoveFlags |=
                MOVE_SPRINT;
        }

        if(controller.nButtons &
           FT_PAD_A)
        {
            dwMoveFlags |=
                MOVE_JUMP;
        }

        // Reserved in the requested Xbox layout until those gameplay systems
        // exist: RB equipment, D-pad Up mark/light, Left grenade switcher,
        // Down AI scan, Right drop weapon. LB is live as quick grenade.
    }

	// Fireteam weapon fire. The client only sends an action + view ray;
    // the server owns weapon type, ammo, fire timing and damage.
    if (g_pLTClient->IsCommandOn(COMMAND_SHOOT) ||
        (controller.bConnected &&
         controller.bRightTriggerDown) ||
        bControllerQuickGrenade)
    {
        const FTWeaponDef *pWeaponDef = m_pPlayer->GetCurrentWeaponDef();

        if(m_pPlayer->Attack() &&
           pWeaponDef &&
           pWeaponDef->eType != FT_WEAPON_MELEE &&
           !m_pPlayer->IsDevWeaponQa())
        {
            HLOCALOBJ hFireCamera = m_pCamera->GetCamera();
            LTVector vFirePos;
            LTRotation rFireRot;

            g_pLTClient->GetObjectPos(hFireCamera, &vFirePos);
            g_pLTClient->GetObjectRotation(hFireCamera, &rFireRot);
            LTVector vFireDir = rFireRot.Forward();

            ILTMessage_Write *pMessage = LTNULL;
            if(g_pLTCCommon->CreateMessage(pMessage) == LT_OK && pMessage)
            {
                pMessage->IncRef();
                pMessage->Writeuint8(MSG_CS_SHOOT);
                pMessage->WriteLTVector(vFirePos);
                pMessage->WriteLTVector(vFireDir);
                // Server uses this only to choose authoritative CA perturb:
                // scoped shots use MinPerturb, no-scope scoped weapons use
                // MaxPerturb.
                pMessage->Writebool(
                    m_pCamera->IsWeaponZoomed());
                const uint32 nDelivery =
                    (pWeaponDef->eType == FT_WEAPON_GRENADE ||
                     pWeaponDef->eType == FT_WEAPON_ROCKET)
                    ? MESSAGE_GUARANTEED
                    : 0;
                g_pLTClient->SendToServer(pMessage->Read(), nDelivery);
                pMessage->DecRef();
            }
        }
    }
    else
    {
        m_pPlayer->ReleaseAttackTrigger();
    }


    // Right mouse is knife secondary for melee, scope toggle for scoped guns.
    if(g_pLTClient->IsCommandOn(COMMAND_ALT_ATTACK))
    {
        const FTWeaponDef *pAltWeapon =
            m_pPlayer->GetCurrentWeaponDef();

        if(!pAltWeapon ||
           pAltWeapon->fZoomFovDegrees <= 0.0f)
        {
            m_pPlayer->AltAttack();
        }
    }

    if(g_pLTClient->IsCommandOn(COMMAND_CROUCH))
    {
        dwMoveFlags |= MOVE_CROUCH;
    }

    if(g_pLTClient->IsCommandOn(COMMAND_SPRINT))
    {
        dwMoveFlags |= MOVE_SPRINT;
    }

    // jump
	if (g_pLTClient->IsCommandOn(COMMAND_JUMP))
	{
		dwMoveFlags |= MOVE_JUMP;
	}

	// Debug command
    if ( g_pLTClient->IsCommandOn(COMMAND_DEBUG_1))
    {
        HLOCALOBJ hObject = g_pLTClient->GetClientObject();
        LTVector vPos;
        g_pLTClient->GetObjectPos(hObject, &vPos);
        g_pLTClient->CPrint("***DBG*** CLIENTObj pos: %f %f %f", VEC_EXPAND(vPos));
    }

    if(m_pPlayer->IsDevWeaponQa() &&
       (m_bQaMenuOpen ||
        m_bQaControlDown))
    {
        // The QA tool owns the arrow/Ctrl keyboard combinations. Apply this
        // after every movement command is sampled so legacy arrow bindings
        // cannot move the player while editing a view model.
        dwMoveFlags = 0;

        // Some legacy bindings can leave residual velocity behind even after
        // their command flag is suppressed. Pin velocity to zero while the
        // editor panel is open so arrow-key view-model edits never slide the
        // player across the map.
        LTVector vQaStop(
            0.0f,
            0.0f,
            0.0f);

        g_pLTCPhysics->SetVelocity(
            hPlayer,
            &vQaStop);
    }

    m_pPlayer->UpdateMoveFlags(dwMoveFlags);

	return LT_OK;
}



//-----------------------------------------------------------------------------
//  CLTClientShell::SendRotation()
//
//-----------------------------------------------------------------------------
void CLTClientShell::SendRotation(LTRotation &rRot)
{
	ILTMessage_Write *pMessage;
	LTRESULT nResult = g_pLTCCommon->CreateMessage(pMessage);

	if( LT_OK == nResult)
	{
		pMessage->IncRef();
		pMessage->Writeuint8(MSG_CS_ROTATION);
		pMessage->WriteLTRotation(rRot);
		g_pLTClient->SendToServer(pMessage->Read(), 0);
		pMessage->DecRef();
	}
}



//-----------------------------------------------------------------------------
//  CLTClientShell::SendVelocity()
//
//-----------------------------------------------------------------------------
void CLTClientShell::SendVelocityAndPos(LTVector &vVel, LTVector &vPos)
{
	ILTMessage_Write *pMessage;
	LTRESULT nResult = g_pLTCCommon->CreateMessage(pMessage);

	if( LT_OK == nResult)
	{
		pMessage->IncRef();
		pMessage->Writeuint8(MSG_CS_VELOCITY);
		pMessage->WriteLTVector(vVel);
		pMessage->WriteLTVector(vPos);
		g_pLTClient->SendToServer(pMessage->Read(), 0);
		pMessage->DecRef();
	}

}



//-----------------------------------------------------------------------------
//  CLTClientShell::SendVelAndRot()
//
//-----------------------------------------------------------------------------
void CLTClientShell::SendVelPosAndRot(LTVector &vVel, LTVector &vPos, LTRotation &rRot)
{
	ILTMessage_Write *pMessage;
	LTRESULT nResult = g_pLTCCommon->CreateMessage(pMessage);

	if( LT_OK == nResult)
	{
		pMessage->IncRef();
		pMessage->Writeuint8(MSG_CS_VELANDROT);
		pMessage->WriteLTVector(vVel);
		pMessage->WriteLTVector(vPos);
		pMessage->WriteLTRotation(rRot);
		g_pLTClient->SendToServer(pMessage->Read(), 0);
		pMessage->DecRef();
	}

}



//-----------------------------------------------------------------------------
//  CLTClientShell::SendPlayerName()
//
//-----------------------------------------------------------------------------
void CLTClientShell::SendQaZombieSetting()
{
    if(!m_pPlayer ||
       !m_pPlayer->IsDevWeaponQa())
    {
        return;
    }

    ILTMessage_Write *pMessage = LTNULL;
    if(g_pLTCCommon->CreateMessage(pMessage) != LT_OK ||
       !pMessage)
    {
        return;
    }

    pMessage->IncRef();
    pMessage->Writeuint8(
        MSG_CS_QA_ZOMBIES);
    pMessage->Writebool(
        m_bQaZombiesEnabled);

    g_pLTClient->SendToServer(
        pMessage->Read(),
        MESSAGE_GUARANTEED);

    pMessage->DecRef();

    m_bQaZombieSettingSent =
        true;

    g_pLTClient->CPrint(
        "Fireteam QA zombies: %s",
        m_bQaZombiesEnabled
            ? "ON"
            : "OFF");
}


void CLTClientShell::SendQaSpectatorSetting()
{
    if(m_nGameMode !=
           LOCAL_GAMEMODE_NORMAL)
    {
        return;
    }

    ILTMessage_Write *pMessage =
        LTNULL;

    if(g_pLTCCommon->CreateMessage(
           pMessage) != LT_OK ||
       !pMessage)
    {
        return;
    }

    pMessage->IncRef();
    pMessage->Writeuint8(
        MSG_CS_QA_SPECTATOR);
    pMessage->Writebool(
        m_bQaSpectator);

    g_pLTClient->SendToServer(
        pMessage->Read(),
        MESSAGE_GUARANTEED);

    pMessage->DecRef();

    m_bQaSpectatorSettingSent =
        true;

    g_pLTClient->CPrint(
        "Fireteam QA spectator: %s",
        m_bQaSpectator
            ? "ON"
            : "OFF");
}


void CLTClientShell::SendSpectatorViewPosition()
{
    if(!m_pCamera ||
       !m_pCamera->IsFreecam())
    {
        return;
    }

    LTVector vViewPos;

    g_pLTClient->GetObjectPos(
        m_pCamera->GetCamera(),
        &vViewPos);

    ILTMessage_Write *pMessage =
        LTNULL;

    if(g_pLTCCommon->CreateMessage(
           pMessage) != LT_OK ||
       !pMessage)
    {
        return;
    }

    pMessage->IncRef();
    pMessage->Writeuint8(
        MSG_CS_SPECTATOR_POS);
    pMessage->WriteLTVector(
        vViewPos);

    g_pLTClient->SendToServer(
        pMessage->Read(),
        0);

    pMessage->DecRef();
}


//-----------------------------------------------------------------------------
void CLTClientShell::SendPlayerName()
{
    ILTMessage_Write *pMessage;
    LTRESULT nResult =
        g_pLTCCommon->CreateMessage(pMessage);

    if(LT_OK == nResult)
    {
        char szName[32];
        szName[0] = '\0';

        HCONSOLEVAR hPlayerName =
            g_pLTClient->GetConsoleVar("playername");

        if(hPlayerName)
        {
            const char *pConfiguredName =
                g_pLTClient->GetVarValueString(
                    hPlayerName);

            if(pConfiguredName &&
               pConfiguredName[0])
            {
                strncpy(
                    szName,
                    pConfiguredName,
                    sizeof(szName) - 1);
                szName[sizeof(szName) - 1] = '\0';
            }
        }

        if(!szName[0])
        {
            unsigned long nSize =
                sizeof(szName) - 1;

            if(!GetComputerName(
                szName,
                &nSize))
            {
                strcpy(
                    szName,
                    "Player");
            }
        }

        g_pLTClient->CPrint(
            "Fireteam player name: %s",
            szName);

        pMessage->IncRef();
        pMessage->Writeuint8(
            MSG_CS_PLAYERNAME);
        pMessage->WriteString(szName);
        g_pLTClient->SendToServer(
            pMessage->Read(),
            MESSAGE_GUARANTEED);
        pMessage->DecRef();
    }
}



//-----------------------------------------------------------------------------
//  CLTClientShell::GetPlayerStartInfo(LTVector &vStartPos,
//                                     LTRotation &rStartRot)
//
//-----------------------------------------------------------------------------
void CLTClientShell::GetPlayerStartInfo(LTVector &vStartPos, LTRotation &rStartRot)
{
	vStartPos = m_vPlayerStartPos;
	rStartRot = m_rPlayerStartRot;
}



//-----------------------------------------------------------------------------
//  CLTClientShell::CreateWorldPropObject()
//
//-----------------------------------------------------------------------------
void CLTClientShell::CreateWorldPropObject()
{
	m_pWorldPropsClnt = new CWorldPropsClnt;
}



//-----------------------------------------------------------------------------
//	CLTClientShell::DestroyWorldPropObject()
//
//-----------------------------------------------------------------------------
void CLTClientShell::DestroyWorldPropObject()
{
	if( m_pWorldPropsClnt )
	{
		delete m_pWorldPropsClnt;
		m_pWorldPropsClnt = NULL;
	}
}



//-----------------------------------------------------------------------------
//  CLTClientShell::StartMultiplayerServer()
//
//-----------------------------------------------------------------------------
LTRESULT CLTClientShell::StartMultiplayerServer()
{
    m_bFirstUpdate = true;
    m_bHavePlayerStartInfo = false;

    LTRESULT result = this->InitGame(EGT_HOST);

    if(LT_OK == result)
    {
        m_nGameMode = LOCAL_GAMEMODE_HOST;
    }

    return result;
}



//-----------------------------------------------------------------------------
//  CLTClientShell::JoinMultiplayerGame()
//
//-----------------------------------------------------------------------------
LTRESULT CLTClientShell::JoinMultiplayerGame(char* ip)
{
    m_bFirstUpdate = true;
    m_bHavePlayerStartInfo = false;

    LTRESULT result = this->InitGame(EGT_CONNECT, ip);

    if(LT_OK == result)
    {
        m_nGameMode = LOCAL_GAMEMODE_CLIENT;
    }

    return result;

}



//-----------------------------------------------------------------------------
//  CLTClientShell::StartNormalGame()
//
//-----------------------------------------------------------------------------
LTRESULT CLTClientShell::StartNormalGame()
{
    m_bFirstUpdate = true;
    m_bHavePlayerStartInfo = false;

    LTRESULT result = this->InitGame(EGT_NORMAL);

    if(LT_OK == result)
    {
        m_nGameMode = LOCAL_GAMEMODE_NORMAL;
    }

    return result;

}



//-----------------------------------------------------------------------------
//	CLTClientShell::OnLockRenderer()
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnLockRenderer()
{
	//g_pLTClient->CPrint("OnLockRenderer()");
}



//-----------------------------------------------------------------------------
//  CLTClientShell::OnUnLockRenderer()
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnUnLockRenderer()
{
	//g_pLTClient->CPrint("OnUnLockRenderer()");
}



//-----------------------------------------------------------------------------
//  CLTClientShell::OnKeyDown(int key, int rep)
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnKeyDown(int key, int rep)
{
    if(VK_CONTROL == key)
    {
        m_bQaControlDown = true;
    }

    // Fireteam settings key routing
    if(FT_SettingsIsOpen())
    {
        FT_SettingsHandleKey(key);
        return;
    }

    if(m_bInWorld &&
       VK_ESCAPE == key &&
       !m_pChatGui->IsChatInputActive())
    {
        FT_SettingsToggle();
        return;
    }

	//g_pLTClient->CPrint("OnKeyDown(%d,%d)", key, rep);
    if(m_bInWorld)
    {
        if(VK_F8 == key &&
           rep == 0 &&
           m_nGameMode ==
               LOCAL_GAMEMODE_NORMAL &&
           !m_pChatGui->IsChatInputActive())
        {
            m_bQaSpectator =
                !m_bQaSpectator;

            SendQaSpectatorSetting();

            if(!m_bQaSpectator &&
               !FT_RoundHudIsPlayerEliminated() &&
               m_pCamera)
            {
                m_pCamera->SetFreecamEnabled(
                    false);

                FT_RoundHudSetSpectator(
                    false,
                    false);
            }

            return;
        }

        if( VK_F9 == key &&
            rep == 0 )
        {
            HLOCALOBJ hPlayer =
                m_pPlayer->GetPlayerObject();

            if(hPlayer)
            {
                LTVector vPos;
                g_pLTClient->GetObjectPos(
                    hPlayer,
                    &vPos);

                g_pLTClient->CPrint(
                    "Fireteam position: X=%.1f Y=%.1f Z=%.1f",
                    vPos.x,
                    vPos.y,
                    vPos.z);
            }
        }

        if( VK_F12 == key )
        {
            g_pLTClient->CPrint("Creating Cubic Environment Map...");
            g_pLTClient->MakeCubicEnvMap(m_pCamera->GetCamera(), 256, "CubicEnvMap");
        }

        // If chat input is active, then let's give the chat gui a chance to use it
        if(m_pChatGui->IsChatInputActive())
        {
            m_pChatGui->HandleInputDown(key, rep);
        }
        else
        {
           if(m_pPlayer->IsDevWeaponQa() &&
              'Q' == key &&
              rep == 0)
           {
               m_pPlayer->ToggleDevWeaponQuarantine();
           }
           else if(m_pPlayer->IsDevWeaponQa() &&
                   m_bQaMenuOpen &&
                   'Z' == key &&
                   rep == 0)
           {
               m_bQaZombiesEnabled =
                   !m_bQaZombiesEnabled;
               SendQaZombieSetting();

               FT_WeaponHudSetQaMenu(
                   true,
                   g_aQaMoveSteps[
                       m_nQaMoveStepIndex],
                   m_bQaZombiesEnabled);
           }
           else if(m_pPlayer->IsDevWeaponQa() &&
                   m_bQaMenuOpen &&
                   (VK_OEM_PLUS == key ||
                    VK_ADD == key) &&
                   rep == 0)
           {
               if(m_nQaMoveStepIndex + 1 <
                  g_nQaMoveStepCount)
               {
                   ++m_nQaMoveStepIndex;
               }

               FT_WeaponHudSetQaMenu(
                   true,
                   g_aQaMoveSteps[
                       m_nQaMoveStepIndex],
                   m_bQaZombiesEnabled);
           }
           else if(m_pPlayer->IsDevWeaponQa() &&
                   m_bQaMenuOpen &&
                   (VK_OEM_MINUS == key ||
                    VK_SUBTRACT == key) &&
                   rep == 0)
           {
               if(m_nQaMoveStepIndex > 0)
               {
                   --m_nQaMoveStepIndex;
               }

               FT_WeaponHudSetQaMenu(
                   true,
                   g_aQaMoveSteps[
                       m_nQaMoveStepIndex],
                   m_bQaZombiesEnabled);
           }
           else if(m_pPlayer->IsDevWeaponQa() &&
                   m_bQaMenuOpen &&
                   VK_OEM_4 == key)
           {
               m_pPlayer->AdjustDevWeaponScale(
                   -0.05f);
           }
           else if(m_pPlayer->IsDevWeaponQa() &&
                   m_bQaMenuOpen &&
                   VK_OEM_6 == key)
           {
               m_pPlayer->AdjustDevWeaponScale(
                   0.05f);
           }
           else if(m_pPlayer->IsDevWeaponQa() &&
                   m_bQaMenuOpen &&
                   m_bQaControlDown &&
                   'S' == key &&
                   rep == 0)
           {
               m_pPlayer->SaveDevWeaponView();
           }
           else if(m_pPlayer->IsDevWeaponQa() &&
                   m_bQaMenuOpen &&
                   VK_LEFT == key)
           {
               m_pPlayer->AdjustDevWeaponView(
                   -g_aQaMoveSteps[
                       m_nQaMoveStepIndex],
                   0.0f,
                   0.0f);
           }
           else if(m_pPlayer->IsDevWeaponQa() &&
                   m_bQaMenuOpen &&
                   VK_RIGHT == key)
           {
               m_pPlayer->AdjustDevWeaponView(
                   g_aQaMoveSteps[
                       m_nQaMoveStepIndex],
                   0.0f,
                   0.0f);
           }
           else if(m_pPlayer->IsDevWeaponQa() &&
                   m_bQaMenuOpen &&
                   VK_UP == key)
           {
               const float fStep =
                   g_aQaMoveSteps[
                       m_nQaMoveStepIndex];

               m_pPlayer->AdjustDevWeaponView(
                   0.0f,
                   m_bQaControlDown
                       ? 0.0f
                       : fStep,
                   m_bQaControlDown
                       ? fStep
                       : 0.0f);
           }
           else if(m_pPlayer->IsDevWeaponQa() &&
                   m_bQaMenuOpen &&
                   VK_DOWN == key)
           {
               const float fStep =
                   g_aQaMoveSteps[
                       m_nQaMoveStepIndex];

               m_pPlayer->AdjustDevWeaponView(
                   0.0f,
                   m_bQaControlDown
                       ? 0.0f
                       : -fStep,
                   m_bQaControlDown
                       ? -fStep
                       : 0.0f);
           }
           else if(key >= '1' && key <= '5')
           {
               m_pCamera->ClearWeaponZoom();
               m_pPlayer->SelectWeaponSlot((uint8)(key - '0'));
           }
           else if('R' == key)
           {
               m_pPlayer->ReloadWeapon();
           }
           else if('C' == key)
           {
               m_pCamera->ToggleView();
           }
        }
    }
    else if(VK_ESCAPE == key)
    {
        g_pLTClient->Shutdown();
    }
}



//-----------------------------------------------------------------------------
//  CLTClientShell::OnKeyUp(int key)
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnKeyUp(int key)
{
    if(VK_CONTROL == key)
    {
        m_bQaControlDown = false;
    }

	//g_pLTClient->CPrint("OnKeyUp(%d)", key);
        // If chat input is active, then let's give 
    if(m_pChatGui->IsChatInputActive())
    {
        m_pChatGui->HandleInputUp(key);
    }
    else
    {
       // if(  )
    }
}



//-----------------------------------------------------------------------------
//  CLTClientShell::SpecialEffectNotify(HLOCALOBJ hObj, ILTMessage_Read *pMessage)
//
//-----------------------------------------------------------------------------
void CLTClientShell::SpecialEffectNotify(HLOCALOBJ hObj, ILTMessage_Read *pMessage)
{
	//g_pLTClient->CPrint("SpecialEffectNotify(%p,%p)", hObj, pMessage);
	pMessage->SeekTo(0);
    uint8 messageID = pMessage->Readuint8();

    switch(messageID)
    {
    case MSG_CS_PLAYERNAME:
        {
            char szName[16];
            szName[15] = '\0';
            pMessage->ReadString(szName, 16);
            g_pLTClient->CPrint("%s entered the game.", szName);
        }
        break;
    default:
        break;
    }

    // ClientFX: handle this!
	pMessage->SeekTo(0);
	if(m_bClientFXReady) m_ClientFXMgr.OnSpecialEffectNotify( hObj, pMessage );

}



//-----------------------------------------------------------------------------
//  CLTClientShell::OnTouchNotify(HOBJECT hMain, CollisionInfo *pInfo,
//					float forceMag)
//
//-----------------------------------------------------------------------------
LTRESULT CLTClientShell::OnTouchNotify(HOBJECT hMain, CollisionInfo *pInfo,
							float forceMag)
{
	//g_pLTClient->CPrint("OnTouchNotify(%p,%p,%g)", hMain, pInfo, forceMag);

	return LT_OK;
}



//-----------------------------------------------------------------------------
//  CLTClientShell::OnObjectRemove(HLOCALOBJ hObj)
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnObjectRemove(HLOCALOBJ hObj)
{
	//g_pLTClient->CPrint("OnObjectRemove(%p)", hObj);
	if(!hObj)
	{
		return;
	}

	if(m_bClientFXReady) m_ClientFXMgr.OnObjectRemove( hObj );
}



//-----------------------------------------------------------------------------
//  CLTClientShell::OnObjectMove(HLOCALOBJ hObj, bool bTeleport, LTVector *pNewPos)
//
//-----------------------------------------------------------------------------
LTRESULT CLTClientShell::OnObjectMove(HLOCALOBJ hObj, bool bTeleport, LTVector *pNewPos)
{
	//g_pLTClient->CPrint("OnObjectMove(%p,%b,%p)", hObj, bTeleport, pNewPos);

	return LT_OK;
}



//-----------------------------------------------------------------------------
//  CLTClientShell::OnObjectRotate(HLOCALOBJ hObj, bool bTeleport, LTRotation *pNewRot)
//
//-----------------------------------------------------------------------------
LTRESULT CLTClientShell::OnObjectRotate(HLOCALOBJ hObj, bool bTeleport, LTRotation *pNewRot)
{
	//g_pLTClient->CPrint("OnObjectRotate(%p,%d,%p)", hObj, bTeleport, pNewRot);

	return LT_OK;
}



//-----------------------------------------------------------------------------
//  CLTClientShell::OnPlaySound(PlaySoundInfo *pPlaySoundInfo)
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnPlaySound(PlaySoundInfo *pPlaySoundInfo)
{
	//g_pLTClient->CPrint("OnPlaySound(%p)", pPlaySoundInfo);
}



//-----------------------------------------------------------------------------
//  CLTClientShell::OnConsolePrint(CConsolePrintData *pData)
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnConsolePrint(CConsolePrintData *pData)
{
	//g_pLTClient->CPrint("OnConsolePrint(%p)",pData);
}



//-----------------------------------------------------------------------------
//  CLTClientShell::OnModelKey(HLOCALOBJ hObj, ArgList *pArgs)
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnModelKey(HLOCALOBJ hObj, ArgList *pArgs)
{
	//g_pLTClient->CPrint("OnModelKey(%p, %p)", hObj, pArgs);
}



//-----------------------------------------------------------------------------
//  CLTClientShell::ProcessPacket(ILTMessage_Read *pMsg, uint8 senderAddr[4],
//								uint16 senderPort)
//
//-----------------------------------------------------------------------------
LTRESULT CLTClientShell::ProcessPacket(ILTMessage_Read *pMsg, uint8 senderAddr[4],
									uint16 senderPort)
{
	//g_pLTClient->CPrint("ProcessPacket(%p,%p,%d)", pMsg, senderAddr, senderPort);

	return LT_OK;
}



//-----------------------------------------------------------------------------
//  CLTClientShell::OnEngineTerm()
//
//-----------------------------------------------------------------------------
void CLTClientShell::OnEngineTerm()
{
	g_pLTClient->CPrint("CLTClientShell::OnEngineTerm()");

	// Free memory
	if( m_hCamera )
	{
		g_pLTClient->RemoveObject(m_hCamera);
		delete m_pCamera;
		m_pCamera = NULL;
	}

	this->DestroyWorldPropObject();

    // Remove the camera handle from the ClientFXMgr
    if(m_bClientFXReady) m_ClientFXMgr.SetCamera(NULL);

	m_Gui.Term();

    // Dispose of the Stats gui
    assert(m_pStatsGui);
    m_pStatsGui->Term();
    delete m_pStatsGui;
    m_pStatsGui = NULL;

    // Dispose of the Chat gui
    assert(m_pChatGui);
    FT_LoadingScreenTerm();
    FT_CombatFeedbackTerm();
    FT_RoundHudTerm();
    FT_WeaponHudTerm();
    FT_SettingsTerm();
    m_pChatGui->Term();
    delete m_pChatGui;
    m_pChatGui = NULL;

    //LTRESULT res = g_pLTClient->RemoveInternetDriver();
}



//-----------------------------------------------------------------------------
//  CLTClientShell::PreUpdate()
//
//-----------------------------------------------------------------------------
void CLTClientShell::PreUpdate()
{
	//g_pLTClient->CPrint("PreUpdate()");
}



//-----------------------------------------------------------------------------
//  CLTClientShell::PostUpdate()
//
//-----------------------------------------------------------------------------
void CLTClientShell::PostUpdate()
{
	//g_pLTClient->CPrint("PostUpdate()");
}



//-----------------------------------------------------------------------------
//  CLTClientShell::PreLoadWorld(const char *pWorldName)
//
//  Note: this will only get called on a remote client
//
//-----------------------------------------------------------------------------
void CLTClientShell::PreLoadWorld(const char *pWorldName)
{
	g_pLTClient->CPrint("PreLoadWorld(%s)", pWorldName);
}



//-----------------------------------------------------------------------------
//  CLTClientShell::FindServers()
//
//-----------------------------------------------------------------------------
uint CLTClientShell::FindServers(SERVERINFO *pInfo)
{
    uint nServerCount = 0;
    NetSession *pList, *pSession;

    // Clear old values
    for(int j = 0; j < 4; j++)
    {
        memset(pInfo[j].Name, 0, 255);
        memset(pInfo[j].IpAddress, 0, MAX_HOSTIP_LEN - 1);
    }

	
	LTRESULT res = g_pLTClient->AddInternetDriver();
	if(res == LT_ERROR)
    {
        g_pLTClient->CPrint("ERROR: AddInternetDriver()");
        return 0;
    }
	

    SetService();

    // Find the sessions
	if((g_pLTClient->GetSessionList( pList, "*:27889" ) == LT_OK) && pList)
	{
		pSession = pList;
		for(int i = 0; pSession; i++)
		{
            strncpy(pInfo[i].Name, pSession->m_sName, 256);
            strncpy(pInfo[i].IpAddress , pSession->m_HostIP, MAX_HOSTIP_LEN);
            g_pLTClient->CPrint("(%d) %s -> %s:%d", i, pSession->m_sName, pSession->m_HostIP, pSession->m_HostPort);
            g_pLTClient->CPrint("(%d) Ping: %d", i, pSession->m_Ping);
            g_pLTClient->CPrint("(%d) Players: %d/%d", i, pSession->m_dwCurConnections, pSession->m_dwMaxConnections);
            pSession = pSession->m_pNext;
            ++nServerCount;

            // 4 is our hardcoded Max for this sample
            if(nServerCount == 4)
                break;
        }
    }

	res = g_pLTClient->RemoveInternetDriver();

    return nServerCount;

}
