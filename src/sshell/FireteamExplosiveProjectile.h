#ifndef __FIRETEAM_EXPLOSIVE_PROJECTILE_H__
#define __FIRETEAM_EXPLOSIVE_PROJECTILE_H__

#include <ltengineobjects.h>

class FireteamExplosiveProjectile : public BaseClass
{
public:
    FireteamExplosiveProjectile();

    void Configure(
        HOBJECT hOwner,
        uint8 nDamage,
        float fSplashRadius,
        float fSpeed,
        float fFuseSeconds,
        bool bRocket);

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);

private:
    void UpdateProjectile();
    void HandleTouch(HOBJECT hObject);
    void Explode();

    HOBJECT m_hOwner;
    uint8   m_nDamage;
    float   m_fSplashRadius;
    float   m_fSpeed;
    float   m_fFuseSeconds;
    float   m_fExplodeAt;
    bool    m_bRocket;
    bool    m_bExploded;
    bool    m_bHaveLastPos;
    LTVector m_vLastPos;
};

#endif
