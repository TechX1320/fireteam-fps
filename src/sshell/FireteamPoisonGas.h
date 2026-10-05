#ifndef __FIRETEAM_POISON_GAS_H__
#define __FIRETEAM_POISON_GAS_H__

#include <ltengineobjects.h>

class PoisonGas : public Container
{
public:
    PoisonGas();

    float GetDamage() const { return m_fDamage; }

protected:
    uint32 EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData);

private:
    void ReadProps(ObjectCreateStruct *pOCS);

    float m_fDamage;
    bool  m_bHidden;
};

#endif