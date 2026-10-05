#include "FireteamPoisonGas.h"

#include "serverinterfaces.h"

#include <ltobjectcreate.h>
#include <string.h>

BEGIN_CLASS(PoisonGas)
    ADD_BOOLPROP(Hidden, LTFALSE)
    ADD_REALPROP(Viscosity, 0.0f)
    ADD_REALPROP(Friction, 1.0f)
    ADD_VECTORPROP_VAL(Current, 0.0f, 0.0f, 0.0f)
    ADD_REALPROP(Damage, 5.0f)
    ADD_STRINGPROP(DamageType, "POISON")
    ADD_COLORPROP(TintColor, 255.0f, 255.0f, 76.7f)
    ADD_COLORPROP(LightAdd, 0.0f, 0.0f, 0.0f)
    ADD_STRINGPROP(SoundFilter, "UnFiltered")
    ADD_BOOLPROP(CanPlayMovementSounds, LTTRUE)
    ADD_BOOLPROP(FogEnable, LTFALSE)
    ADD_REALPROP(FogFarZ, 300.0f)
    ADD_REALPROP(FogNearZ, -100.0f)
    ADD_COLORPROP(FogColor, 0.0f, 0.0f, 0.0f)
    ADD_STRINGPROP(SurfaceOverride, "Unknown")
    ADD_BOOLPROP(RayHit, LTFALSE)
    ADD_STRINGPROP(PhysicsModel, "Normal")
END_CLASS_DEFAULT_FLAGS(PoisonGas, Container, LTNULL, LTNULL, CF_WORLDMODEL)

PoisonGas::PoisonGas() :
    m_fDamage(5.0f),
    m_bHidden(false)
{
}

void PoisonGas::ReadProps(ObjectCreateStruct *pOCS)
{
    GenericProp prop;

    if(g_pLTServer->GetPropGeneric("Damage", &prop) == LT_OK)
    {
        m_fDamage = prop.m_Float;
    }

    if(g_pLTServer->GetPropGeneric("Hidden", &prop) == LT_OK)
    {
        m_bHidden = (prop.m_Bool != LTFALSE);
    }

    if(g_pLTServer->GetPropGeneric("Name", &prop) == LT_OK)
    {
        strncpy(pOCS->m_Name, prop.m_String, sizeof(pOCS->m_Name) - 1);
        pOCS->m_Name[sizeof(pOCS->m_Name) - 1] = '\0';
    }

    // Container::EngineMessageFn already set OT_CONTAINER, FLAG_CONTAINER,
    // and the compiled brush filename from Name. Use a private Fireteam code
    // so both server and client can identify Cabin Fever poison volumes.
    pOCS->m_Flags |= FLAG_TOUCH_NOTIFY | FLAG_GOTHRUWORLD | FLAG_FORCECLIENTUPDATE;
    pOCS->m_ContainerCode = 240;

    if(m_bHidden)
    {
        pOCS->m_Flags &= ~FLAG_VISIBLE;
    }
}

uint32 PoisonGas::EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData)
{
    // Let Jupiter's built-in Container class establish the BSP/container object first.
    uint32 nResult = Container::EngineMessageFn(messageID, pData, fData);

    if(messageID == MID_PRECREATE)
    {
        ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;
        if(pOCS && fData == PRECREATE_WORLDFILE)
        {
            ReadProps(pOCS);
        }
    }

    return nResult;
}