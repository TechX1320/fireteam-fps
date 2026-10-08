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

    void ApplyWallhackRenderStyle(
        bool bEnabled);

    // Dead corpses remain in the world for their animation; do not
    // count them as living enemies when repairing a stalled round.
    bool IsAliveForRound() const { return !m_bDying && m_nHealth > 0; }

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);
    uint32 ObjectMessageFn(HOBJECT hSender, ILTMessage_Read *pMsg);

private:
    enum BehaviorState
    {
        kBehaviorSearch = 0,
        kBehaviorChase,
        kBehaviorAttack,
        kBehaviorLostTarget
    };

    HOBJECT FindNearestPlayer();
    void UpdateZombie(float fDeltaSeconds);
    void AdvanceSmoothMotion();
    void RebuildPath(const LTVector &vTarget);
    bool HasDirectPathToTarget(
        HOBJECT hTarget,
        const LTVector &vFrom,
        const LTVector &vTarget);
    bool CanSeeTarget(
        HOBJECT hTarget,
        const LTVector &vFrom,
        const LTVector &vTarget);
    void CreateInfectedFace();
    void SetZombieAnimation(
        const char *pAnimation,
        bool bLooping);
    void PlayVoiceSound(
        const char *pFilename);
    void PlayAttackVoice();
    bool IsMovementStepClear(
        const LTVector &vPos,
        const LTVector &vDirection,
        float fDistance);
    bool BuildLocalEscapeWaypoint(
        const LTVector &vPos,
        const LTVector &vGoal);

    uint16 m_nHealth;
    float m_fAttackCooldown;
    float m_fAttackAnimationTime;
    float m_fJumpAnimationTime;
    float m_fRepathCooldown;
    float m_fStuckTime;
    float m_fForcePathTime;
    float m_fNoProgressTime;
    float m_fBestProgressDistance;
    float m_fStragglerIdleSeconds;
    float m_fTargetMemory;
    float m_fVoiceCooldown;
    bool m_bHasLastKnownTarget;
    BehaviorState m_eBehaviorState;
    bool m_bDying;
    float m_fDeathTimeRemaining;

    // Expensive AI stays at fUpdateSeconds; collision movement is spread
    // across 1-4 smaller engine updates for smoother replication.
    float m_fLastServerTick;
    float m_fDecisionElapsed;
    uint8 m_nTicksUntilDecision;
    uint8 m_nMotionStepsRemaining;
    LTVector m_vMotionGoal;
    uint32 m_nPathLane;
    uint32 m_nWaypoint;
    LTVector m_vLastPos;
    LTVector m_vLastKnownTargetPos;
    LTVector m_vProgressTarget;
    LTVector m_vStragglerProgressPos;
    LTVector m_vCollisionDims;
    std::vector<LTVector> m_aPath;

    HOBJECT m_hFace;
    HATTACHMENT m_hFaceAttachment;

    FTInfectedDef m_Def;
    bool m_bDefLoaded;
    char m_sCurrentAnimation[64];
};

// Team/global zombie-only visibility powerup. The custom RenderStyle is
// staged from assets-local/RS/ZombieThroughWall.ltb.
float FT_ExtendZombieWallhack(
    float fBaseSeconds);

float FT_GetZombieWallhackRemaining();
bool FT_IsZombieWallhackActive();

#endif