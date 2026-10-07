//------------------------------------------------------------------------------//
// 
// MODULE   : msgids.h
// 
// PURPOSE  : Messages IDs - Definition
// 
// CREATED  : 12/10/2002
// 
// (c) 2002 LithTech, Inc.  All Rights Reserved
// 
//------------------------------------------------------------------------------//


#ifndef __MSG_IDS__
#define __MSG_IDS__


//-----------------------------------------------------------------------------
enum EMessageID
{
	
		MSG_STARTPOINT_POS_ROT, 	// server->client
		MSG_WORLD_PROPS,			// server->client
		MSG_LEFT_MOUSECLICK,		// client->server
		MSG_ADD_MODEL,				// client->server
		MSG_MOVE_MODEL, 			// client->server
		MSG_DELETE_MODEL,			// client->server
		MSG_MOVE_UP,				// client->server
		MSG_STOP_MOVING,			// client->server
		MSG_CS_VELOCITY,			// client->server
		MSG_CS_ROTATION,			// client->server
		MSG_CS_VELANDROT,			// client->server
		MSG_PLAYEROBJ,
        MSG_CS_SHOOT,               // client->server
        MSG_CS_ANIM,                // client->server
        MSG_CS_PLAYERNAME,          // client->server >> server->client
        MSG_CS_MY_CLUB,
        MSG_CS_WEAPON_SLOT,         // client->server
        MSG_CS_RELOAD,              // client->server
        MSG_CS_AMMO_SYNC,           // client->server; authoritative HUD reconciliation
        MSG_CS_QA_ZOMBIES,           // client->server; local QA spawn toggle
        MSG_CS_SCORE,
        MSG_SERVER_SCORES,          // server->client
        MSG_CS_CHAT,                // client->server
        MSG_SC_CHAT,                // server->client
        MSG_SC_HEALTH,              // server->client
        MSG_SC_RESPAWN,             // server->client
        MSG_SC_LIGHTGROUP,          // server->client
        MSG_SC_AMMO,                // server->client
        MSG_SC_ROUND,               // server->client
        MSG_SC_POWERUP,             // server->client announcement
        MSG_SC_POWERUP_STATE,       // server->client timed buff state
        MSG_SC_COMBAT_FEEDBACK,     // server->client shooter-only hit/kill feedback
		MSG_LAST_MESSAGE			//last message marker, do not handle
		
};


//-----------------------------------------------------------------------------
// Shooter-only presentation events sent by the authoritative server.
enum EFireteamCombatFeedback
{
    FT_COMBAT_FEEDBACK_HEADSHOT = 1,
    FT_COMBAT_FEEDBACK_NUTSHOT,
    FT_COMBAT_FEEDBACK_FIRSTKILL,
    FT_COMBAT_FEEDBACK_DOUBLEKILL,
    FT_COMBAT_FEEDBACK_MULTIKILL,
    FT_COMBAT_FEEDBACK_ULTRAKILL,
    FT_COMBAT_FEEDBACK_FANTASTIC,
    FT_COMBAT_FEEDBACK_UNBELIEVABLE
};


//-----------------------------------------------------------------------------
enum EObjMessageID
{
    OBJ_MID_PICKUP              = 0,
    OBJ_MID_DAMAGE              = 1,
    OBJ_MID_KILLSCORE           = 2,
    OBJ_MID_KILLSCORE_SNOWMAN   = 3,
    OBJ_MID_DAMAGE_REGIONAL     = 4,
    OBJ_MID_KILLSCORE_INFECTED  = 5,
};


#endif	// __MSG_IDS__
