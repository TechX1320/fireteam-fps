#include "FireteamSoundFX.h"
#include "serverinterfaces.h"

#include <ltobjectcreate.h>
#include <string.h>

BEGIN_CLASS(SoundFX)
    ADD_BOOLPROP(StartOn, LTTRUE)
    ADD_STRINGPROP_FLAG(Sound, "", PF_FILENAME)
    ADD_LONGINTPROP(Priority, 0)
    ADD_REALPROP_FLAG(OuterRadius, 500.0f, PF_RADIUS)
    ADD_REALPROP_FLAG(InnerRadius, 100.0f, PF_RADIUS)
    ADD_LONGINTPROP(Volume, 100)
    ADD_REALPROP(PitchShift, 1.0f)
    ADD_BOOLPROP(Ambient, LTTRUE)
    ADD_BOOLPROP(Loop, LTTRUE)
    ADD_BOOLPROP(PlayAttached, LTFALSE)
    ADD_STRINGPROP_FLAG(Filter, "UnFiltered", PF_STATICLIST)
END_CLASS_DEFAULT_FLAGS(
    SoundFX,
    BaseClass,
    LTNULL,
    LTNULL,
    CF_ALWAYSLOAD)

SoundFX::SoundFX() :
    m_bStartOn(true),
    m_bAmbient(true),
    m_bLooping(true),
    m_bAttached(false),
    m_nPriority(0),
    m_nVolume(100),
    m_fOuterRadius(500.0f),
    m_fInnerRadius(100.0f),
    m_fPitchShift(1.0f),
    m_hSound(LTNULL)
{
    m_sSound[0] = '\0';
}

SoundFX::~SoundFX()
{
    StopSound();
}

void SoundFX::StopSound()
{
    if(m_hSound)
    {
        g_pLTServer->SoundMgr()->KillSound(
            m_hSound);
        m_hSound = LTNULL;
    }
}

void SoundFX::ReadProps(
    ObjectCreateStruct *pOCS)
{
    GenericProp prop;

    if(g_pLTServer->GetPropGeneric(
           "StartOn",
           &prop) == LT_OK)
    {
        m_bStartOn =
            prop.m_Bool != LTFALSE;
    }

    if(g_pLTServer->GetPropGeneric(
           "Sound",
           &prop) == LT_OK)
    {
        strncpy(
            m_sSound,
            prop.m_String,
            sizeof(m_sSound) - 1);

        m_sSound[
            sizeof(m_sSound) - 1] =
            '\0';
    }

    if(g_pLTServer->GetPropGeneric(
           "Priority",
           &prop) == LT_OK)
    {
        long nPriority =
            prop.m_Long;

        if(nPriority < 0)
            nPriority = 0;
        if(nPriority > 255)
            nPriority = 255;

        m_nPriority =
            (uint8)nPriority;
    }

    if(g_pLTServer->GetPropGeneric(
           "OuterRadius",
           &prop) == LT_OK)
    {
        m_fOuterRadius =
            prop.m_Float;
    }

    if(g_pLTServer->GetPropGeneric(
           "InnerRadius",
           &prop) == LT_OK)
    {
        m_fInnerRadius =
            prop.m_Float;
    }

    if(g_pLTServer->GetPropGeneric(
           "Volume",
           &prop) == LT_OK)
    {
        long nVolume =
            prop.m_Long;

        if(nVolume < 0)
            nVolume = 0;
        if(nVolume > 100)
            nVolume = 100;

        m_nVolume =
            (uint8)nVolume;
    }

    if(g_pLTServer->GetPropGeneric(
           "PitchShift",
           &prop) == LT_OK)
    {
        m_fPitchShift =
            prop.m_Float;
    }

    if(g_pLTServer->GetPropGeneric(
           "Ambient",
           &prop) == LT_OK)
    {
        m_bAmbient =
            prop.m_Bool != LTFALSE;
    }

    if(g_pLTServer->GetPropGeneric(
           "Loop",
           &prop) == LT_OK)
    {
        m_bLooping =
            prop.m_Bool != LTFALSE;
    }

    if(g_pLTServer->GetPropGeneric(
           "PlayAttached",
           &prop) == LT_OK)
    {
        m_bAttached =
            prop.m_Bool != LTFALSE;
    }

    pOCS->m_ObjectType =
        OT_NORMAL;
    pOCS->m_Flags = 0;
}

void SoundFX::PlayConfiguredSound()
{
    StopSound();

    if(!m_sSound[0])
    {
        return;
    }

    PlaySoundInfo soundInfo;
    PLAYSOUNDINFO_INIT(
        soundInfo);

    soundInfo.m_dwFlags =
        PLAYSOUND_GETHANDLE;

    if(m_bLooping)
    {
        soundInfo.m_dwFlags |=
            PLAYSOUND_LOOP;
    }

    if(m_bAttached)
    {
        soundInfo.m_dwFlags |=
            PLAYSOUND_ATTACHED;
        soundInfo.m_hObject =
            m_hObject;
    }

    if(m_bAmbient)
    {
        soundInfo.m_dwFlags |=
            PLAYSOUND_AMBIENT;
    }
    else
    {
        soundInfo.m_dwFlags |=
            PLAYSOUND_3D;
    }

    if(m_nVolume < 100)
    {
        soundInfo.m_dwFlags |=
            PLAYSOUND_CTRL_VOL;
        soundInfo.m_nVolume =
            m_nVolume;
    }

    if(m_fPitchShift != 1.0f)
    {
        soundInfo.m_dwFlags |=
            PLAYSOUND_CTRL_PITCH;
        soundInfo.m_fPitchShift =
            m_fPitchShift;
    }

    soundInfo.m_nPriority =
        m_nPriority;
    soundInfo.m_fOuterRadius =
        m_fOuterRadius;
    soundInfo.m_fInnerRadius =
        m_fInnerRadius;

    g_pLTServer->GetObjectPos(
        m_hObject,
        &soundInfo.m_vPosition);

    strncpy(
        soundInfo.m_szSoundName,
        m_sSound,
        sizeof(soundInfo.m_szSoundName) - 1);

    soundInfo.m_szSoundName[
        sizeof(soundInfo.m_szSoundName) - 1] =
        '\0';

    const LTRESULT result =
        g_pLTServer->SoundMgr()->PlaySound(
            &soundInfo,
            m_hSound);

    if(result != LT_OK)
    {
        m_hSound = LTNULL;

        g_pLTServer->CPrint(
            "Fireteam sound: failed to play %s.",
            m_sSound);
    }
}

uint32 SoundFX::EngineMessageFn(
    uint32 messageID,
    void *pData,
    LTFLOAT fData)
{
    switch(messageID)
    {
        case MID_PRECREATE:
        {
            ObjectCreateStruct *pOCS =
                (ObjectCreateStruct*)pData;

            if(pOCS &&
               fData ==
                   PRECREATE_WORLDFILE)
            {
                ReadProps(
                    pOCS);
            }
        }
        break;

        case MID_INITIALUPDATE:
        {
            if(m_bStartOn)
            {
                PlayConfiguredSound();
            }

            g_pLTServer->SetNextUpdate(
                m_hObject,
                0.0f);
        }
        break;

        default:
            break;
    }

    return BaseClass::EngineMessageFn(
        messageID,
        pData,
        fData);
}
