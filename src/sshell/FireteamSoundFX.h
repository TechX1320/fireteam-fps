#ifndef __FIRETEAM_SOUND_FX_H__
#define __FIRETEAM_SOUND_FX_H__

#include <ltengineobjects.h>
#include <iltsoundmgr.h>

class SoundFX : public BaseClass
{
public:
    SoundFX();
    ~SoundFX();

protected:
    uint32 EngineMessageFn(
        uint32 messageID,
        void *pData,
        LTFLOAT fData);

private:
    void ReadProps(
        ObjectCreateStruct *pOCS);
    void PlayConfiguredSound();
    void StopSound();

    bool m_bStartOn;
    bool m_bAmbient;
    bool m_bLooping;
    bool m_bAttached;

    char m_sSound[256];

    uint8 m_nPriority;
    uint8 m_nVolume;

    float m_fOuterRadius;
    float m_fInnerRadius;
    float m_fPitchShift;

    HLTSOUND m_hSound;
};

#endif
