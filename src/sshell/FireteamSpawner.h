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
    bool IsPerimeterSpawner() const;
    bool SpawnZombie();
    bool SpawnCrawlerSeal();
    void ResetRoundController();
    void CollectPerimeterSpawners();
    void StartNextRound();
    void UpdateRoundController();

    char m_sName[64];
};

void FT_OnFireteamPlayerJoined(HCLIENT hClient);
// Restage a genuinely stranded final infected without counting a kill.
// Keeps the same zombie object, health, and required number of kills.
bool FT_IsFinalLivingInfected(HOBJECT hZombie);
bool FT_TryRecoverFinalInfected(
    HOBJECT hZombie,
    HOBJECT hTarget,
    LTVector &vResult);

void FT_OnFireteamEnemyKilled();
void FT_OnFireteamSquadGameOver();
void FT_SetQaZombiesEnabled(
    bool bEnabled);
bool FT_AreQaZombiesEnabled();
void FT_EnsureFireteamRoundController();

#endif