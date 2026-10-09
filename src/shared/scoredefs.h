#ifndef _SCOREDEFS_H_
#define _SCOREDEFS_H_

#include <ltbasedefs.h>

// Compact server-owned counters. Type IDs come from authored infected and
// reward definitions, never from client-submitted totals.
#define FT_MAX_MATCH_CATEGORIES 16
typedef struct FTNamedCounter
{
    char sId[32];
    uint32 nCount;
} FTNamedCounter;

typedef struct ScoreStruct
{
    uint32 iClientID;
    char   sPlayerName[32];
    uint32 iScore;           // Existing authoritative kill count.
    uint8  iLives;
    float  fMoney;
    uint32 iShotsFired;      // Accepted ranged trigger pulls.
    uint32 iShotsHit;        // Confirmed direct hitscan hits.
    uint32 iDeaths;
    uint32 iPowerups;
    uint32 iHeadshotKills;
} SCORESTRUCT;


#endif //_SCOREDEFS_H_
