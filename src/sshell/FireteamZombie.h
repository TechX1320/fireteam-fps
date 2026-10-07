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
    bool HasDirectPathToTarget(
        HOBJECT hTarget,
        const LTVector &vFrom,
        const LTVector &vTarget);
    void CreateInfectedFace();
    void SetZombieAnimation(
        const char *pAnimation,
        bool bLooping);
    bool IsMovementStepClear(
        const LTVector &vPos,
        const LTVector &vDirection,
        float fDistance);

    uint16 m_nHealth;
    float m_fAttackCooldown;
    float m_fRepathCooldown;
    float m_fStuckTime;
    float m_fForcePathTime;
    bool m_bDying;
    float m_fDeathTimeRemaining;
    uint32 m_nPathLane;
    uint32 m_nWaypoint;
    LTVector m_vLastPos;
    LTVector m_vCollisionDims;
    std::vector<LTVector> m_aPath;

    HOBJECT m_hFace;
    HATTACHMENT m_hFaceAttachment;

    FTInfectedDef m_Def;
    bool m_bDefLoaded;
    char m_sCurrentAnimation[64];
};

#endif