#include "FireteamAmbientAudio.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <iltsoundmgr.h>
#include <string.h>

static HLTSOUND s_hZombieAmbience = LTNULL;

void FT_AmbientAudioExitWorld()
{
    if(s_hZombieAmbience &&
       g_pLTCSoundMgr)
    {
        g_pLTCSoundMgr->KillSound(
            s_hZombieAmbience);
    }

    s_hZombieAmbience = LTNULL;
}

void FT_AmbientAudioEnterWorld()
{
    FT_AmbientAudioExitWorld();

    if(!g_pLTCSoundMgr)
    {
        return;
    }

    PlaySoundInfo info;
    PLAYSOUNDINFO_INIT(info);

    info.m_dwFlags =
        PLAYSOUND_LOCAL |
        PLAYSOUND_LOOP |
        PLAYSOUND_GETHANDLE |
        PLAYSOUND_CTRL_VOL;
    info.m_nVolume = 42;

    strncpy(
        info.m_szSoundName,
        "Snd/Fireteam/ZombieAmbience.wav",
        sizeof(info.m_szSoundName) - 1);
    info.m_szSoundName[
        sizeof(info.m_szSoundName) - 1] =
        '\0';

    if(g_pLTCSoundMgr->PlaySound(
           &info,
           s_hZombieAmbience) != LT_OK)
    {
        s_hZombieAmbience = LTNULL;

        if(g_pLTClient)
        {
            g_pLTClient->CPrint(
                "Fireteam ambience: ZombieAmbience.wav not staged/decodable; continuing without loop.");
        }
    }
    else if(g_pLTClient)
    {
        g_pLTClient->CPrint(
            "Fireteam ambience: ZombieAmbience.wav loop started at volume 42.");
    }
}
