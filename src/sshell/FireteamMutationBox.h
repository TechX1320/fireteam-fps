#ifndef __FIRETEAM_MUTATION_BOX_H__
#define __FIRETEAM_MUTATION_BOX_H__

#include <ltengineobjects.h>

class FireteamMutationBox : public BaseClass
{
public:
    FireteamMutationBox();

    uint32 EngineMessageFn(
        uint32 messageID,
        void *pData,
        float fData);

private:
    uint32 PreCreate(
        void *pData,
        float fData);

    uint32 InitialUpdate();
    uint32 Update();
    uint32 TouchNotify(
        void *pData,
        float fData);

    void PlayPickupSound();

private:
    float m_fLifeRemaining;
    bool m_bConsumed;
};

// Called by infected death and round controller.
bool FT_MaybeSpawnMutationBoxOnKill(
    const LTVector &vPosition);

uint32 FT_SpawnRoundClearMutationBoxes();

#endif
