#ifndef __FIRETEAM_LIGHTGROUP_H__
#define __FIRETEAM_LIGHTGROUP_H__

#include <ltengineobjects.h>

class LightGroup : public Engine_LightGroup
{
public:
    LightGroup();

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);

private:
    void ReadProps(ObjectCreateStruct *pOCS);
    void SendUpdate();

    uint32   m_nID;
    LTVector m_vColor;
    bool     m_bOn;
};

#endif