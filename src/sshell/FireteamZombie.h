#ifndef __FIRETEAM_ZOMBIE_H__
#define __FIRETEAM_ZOMBIE_H__

#include <ltengineobjects.h>
#include "FireteamInfectedDefs.h"
#include <vector>

class FireteamZombie : public BaseClass
{
public:
    FireteamZombie();
    ~FireteamZombie();

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);
    uint32 ObjectMessageFn(HOBJECT hSender, ILTMessage_Read *pMsg);

private:
    HOBJECT FindNearestPlayer();
    void UpdateZombie();
    void RebuildPath(const LTVector &vTarget);
    void CreateInfectedFace();

    uint16 m_nHealth;
    float m_fAttackCooldown;
    float m_fRepathCooldown;
    float m_fStuckTime;
    uint32 m_nPathLane;
    uint32 m_nWaypoint;
    LTVector m_vLastPos;
    std::vector<LTVector> m_aPath;

    HOBJECT m_hFace;
    HATTACHMENT m_hFaceAttachment;

    FTInfectedDef m_Def;
    bool m_bDefLoaded;
};

#endif