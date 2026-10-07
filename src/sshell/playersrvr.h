//------------------------------------------------------------------------------//
//
// MODULE   : playersrvr.h
//
// PURPOSE  : CPlayerSrvr - Definition
//
// CREATED  : 07/15/2002
//
// (c) 2002 LithTech, Inc.  All Rights Reserved
//
//------------------------------------------------------------------------------//

#ifndef __PLAYERSRVR_H__
#define __PLAYERSRVR_H__


// Engine includes
#include <ltbasedefs.h>
#include <ltengineobjects.h>
#include "FireteamWeaponDefs.h"



//-----------------------------------------------------------------------------
class CPlayerSrvr : public BaseClass
{
public:

    CPlayerSrvr()
		: m_hClub(NULL),
    	  m_hClient(NULL),
    	  m_iScore(0),
    	  m_fMoney(0.0f),
		  m_DebugSphere(NULL),
          m_bSendStats(false),
          m_iSendStatsCounter(0),
          m_nHealth(100),
          m_nMaxHealth(100),
          m_bAlive(true),
          m_fRespawnTimer(0.0f),
          m_fPoisonCarry(0.0f),
          m_nWeaponSlot(1),
          m_bReloading(false),
          m_nReloadSlot(0),
          m_fReloadComplete(0.0f),
          m_fBottomlessUntil(0.0f),
          m_fOneHitUntil(0.0f),
          m_fGodUntil(0.0f),
          m_nBottomlessStacks(0),
          m_nOneHitStacks(0),
          m_nGodStacks(0),
          m_bBottomlessWasActive(false),
          m_bOneHitWasActive(false),
          m_bGodWasActive(false),
          m_fLastKillFeedbackTime(0.0f),
          m_nKillFeedbackChain(0)
    {
        FT_LoadWeaponDefs("config/weapons.cfg", m_WeaponDefs);

        for(uint8 nSlot = 0; nSlot < 6; ++nSlot)
        {
            m_fNextWeaponShot[nSlot] = 0.0f;
            m_nWeaponAmmoInClip[nSlot] = m_WeaponDefs[nSlot].nClipSize;
            m_nWeaponAmmoReserve[nSlot] = m_WeaponDefs[nSlot].nStartReserve;
        }
    }

	~CPlayerSrvr()
    {
        if(m_hClub)
        {
			g_pLTServer->RemoveObject(m_hClub);
			m_hClub = NULL;
        }
    }

	// EngineMessageFn handlers
	uint32				EngineMessageFn(uint32 messageID, void *pData, float fData);
    uint32  			ObjectMessageFn(HOBJECT hSender, ILTMessage_Read *pMsg);

    //
    void    			SetPlayerName(const char* name);
    char*   			GetPlayerName();

    void    			PlayAnimation(const char* sAnimName, uint8 nTracker, bool bLooping);
    void                SetClient(HCLIENT hClient){ m_hClient = hClient; SendHealth(); SendPrimaryAmmo(); SendPowerupState(); }
    void    			SetClubID();
    void                SetWeaponSlot(uint8 nSlot);
    void                FirePrimary(
                            const LTVector &vFrom,
                            const LTVector &vDirection,
                            bool bZoomed);
    void                ReloadWeapon();
    void                SyncPrimaryAmmo();
    void                SetClientID(uint32 id){ m_iClientID = id; }
    uint32              GetClientID(){ return m_iClientID; }
    uint32              GetScore(){ return m_iScore; }
    void                SetSendStatsFlag(bool bSend)
                                        { 
                                            m_bSendStats = bSend; 
                                            m_iSendStatsCounter = 0;
                                        }
    float               GetMoney(){ return m_fMoney; }
    char*               GetName(){ return m_sName; }
    void                ApplyDamage(uint8 nDamage);
    bool                IsAlive() const { return m_bAlive; }

    // Fireteam Mutation Box rewards.
    void                GrantAmmoMagazines(uint32 nMagazines);
    void                GrantHealth(uint32 nAmount);
    void                GrantBottomless(float fSeconds);
    void                GrantOneHit(float fSeconds);
    void                GrantGodMode(float fSeconds);
    void                NotifyPowerup(const char *pText, float fSeconds);
    void                SyncPowerupState(){ SendPowerupState(); }

private:

	uint32				PreCreate(void *pData, float fData);
	uint32				InitialUpdate(void *pData, float fData);
	void				ReadProps(ObjectCreateStruct* pStruct);
    void        		CreateAttachment(HATTACHMENT &hAttachment, HOBJECT hChildObject, const char* sSocket,
                    	             	 LTVector &vRotOffset, LTVector &vPosOffset);
    void 				CheckForHit();
    void 				PlaySound(int i);
    void                SendHealth();
    void                SendPrimaryAmmo();
    void                CompleteReloadIfReady();
    void                SpawnExplosiveProjectile(
                            const FTWeaponDef &def,
                            const LTVector &vFrom,
                            const LTVector &vDirection);
    void                Respawn();
    void                UpdateHazards();
    void                UpdatePowerups();
    void                SendPowerupState();
    void                SendCombatFeedback(uint8 nFeedback);

private:

    char                m_sName[32];

    ANIMTRACKERID       m_idUpperBodyTracker;
    ANIMTRACKERID       m_idLowerBodyTracker;

    HMODELWEIGHTSET     m_hWeightUpper;
    HMODELWEIGHTSET     m_hWeightLower;

    HMODELSOCKET        hRightHandSocket;

    HATTACHMENT         m_hClubAttach;
	HOBJECT             m_hClub;
	ModelOBB			m_WeaponOBB;
    uint8               m_nWeaponSlot;
    FTWeaponDef         m_WeaponDefs[6];
    float               m_fNextWeaponShot[6];
    uint16              m_nWeaponAmmoInClip[6];
    uint16              m_nWeaponAmmoReserve[6];
    bool                m_bReloading;
    uint8               m_nReloadSlot;
    float               m_fReloadComplete;

    float               m_fBottomlessUntil;
    float               m_fOneHitUntil;
    float               m_fGodUntil;
    uint8               m_nBottomlessStacks;
    uint8               m_nOneHitStacks;
    uint8               m_nGodStacks;
    bool                m_bBottomlessWasActive;
    bool                m_bOneHitWasActive;
    bool                m_bGodWasActive;

    HCLIENT             m_hClient;

    // Stats
    uint32              m_iScore;
    float               m_fMoney;
    uint32              m_iClientID;
    float               m_fLastKillFeedbackTime;
    uint8               m_nKillFeedbackChain;

    // Stat send toggle and delay counter
    bool                m_bSendStats;
    uint8               m_iSendStatsCounter;

    // Fireteam health/death
    uint8               m_nHealth;
    uint8               m_nMaxHealth;
    bool                m_bAlive;
    float               m_fRespawnTimer;
    float               m_fPoisonCarry;
    LTVector            m_vSpawnPos;
    LTRotation          m_rSpawnRot;

	HOBJECT				m_DebugSphere;
};


#endif // __PLAYERSRVR_H__
