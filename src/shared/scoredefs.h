#ifndef _SCOREDEFS_H_
#define _SCOREDEFS_H_

#include <ltbasedefs.h>

typedef struct ScoreStruct
{
    uint32 iClientID;
    char   sPlayerName[32];
    uint32 iScore;
    uint8  iLives;
    float  fMoney;
}SCORESTRUCT;


#endif //_SCOREDEFS_H_
