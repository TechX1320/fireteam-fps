#ifndef __FIRETEAM_GAME_GUID_H__
#define __FIRETEAM_GAME_GUID_H__

#include <ltbasedefs.h>

// Stable FIRETEAM network identity shared by listen hosts, joiners and the
// headless server. This is a FIRETEAM-only value, not the original NOLF2 GUID.
inline LTGUID FT_GetGameGuid()
{
    LTGUID guid = {
        0x46495245, 0x5445, 0x414d,
        { 0x9c, 0x32, 0x6f, 0x21, 0xbe, 0x9d, 0x01, 0x01 }
    };
    return guid;
}

#endif
