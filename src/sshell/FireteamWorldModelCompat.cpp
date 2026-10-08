#include "serverinterfaces.h"

#include <ltobjectcreate.h>
#include <ltengineobjects.h>
#include <string.h>

// Minimal compatibility for static WorldModel brushes in imported CA maps.
// This restores the engine-owned worldmodel geometry; it deliberately does
// NOT pretend to implement NOLF2/CA damage, activation or trigger behavior.
// RotatingDoor and SlidingDoor need real state machines and are not aliased.
class WorldModel : public BaseClass
{
protected:
    uint32 EngineMessageFn(uint32 nMessage, void *pData, LTFLOAT fData)
    {
        if(nMessage == MID_PRECREATE && pData)
        {
            BaseClass::EngineMessageFn(nMessage, pData, fData);
            ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;

            if(fData == PRECREATE_WORLDFILE)
            {
                // Match the proven SealHunter TransWorldmodel loader:
                // a compiled brush submodel is addressed by its Name prop.
                g_pLTServer->GetPropString(
                    "Name", pOCS->m_Filename, MAX_CS_FILENAME_LEN);
            }

            pOCS->m_ObjectType = OT_WORLDMODEL;
            pOCS->m_Flags |= FLAG_VISIBLE | FLAG_SOLID;
            return 1;
        }

        return BaseClass::EngineMessageFn(nMessage, pData, fData);
    }
};

BEGIN_CLASS(WorldModel)
END_CLASS_DEFAULT_FLAGS(WorldModel, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD | CF_WORLDMODEL)
