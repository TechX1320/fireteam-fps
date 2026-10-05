#ifndef __FIRETEAM_NAVIGATION_H__
#define __FIRETEAM_NAVIGATION_H__

#include <ltengineobjects.h>
#include <vector>

class AIRegion : public BaseClass
{
public:
    AIRegion();

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);

private:
    void ReadProps(ObjectCreateStruct *pOCS);
    void PrintNavigationSummary();

    char m_sName[64];
    LTVector m_vDims;
};

class AINodePatrol : public BaseClass
{
public:
    AINodePatrol();

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);

private:
    void ReadProps(ObjectCreateStruct *pOCS);

    char m_sName[64];
    char m_sNext[64];
    bool m_bStartDisabled;
};

bool FT_BuildNavigationPath(
    const LTVector &vStart,
    const LTVector &vDestination,
    uint32 nLane,
    std::vector<LTVector> &aWaypoints);

uint32 FT_GetNavigationVolumeCount();

#endif