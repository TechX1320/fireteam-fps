#include "FireteamLightGroup.h"

#include "serverinterfaces.h"
#include "msgids.h"

#include <iltcommon.h>
#include <iltmessage.h>
#include <ltobjectcreate.h>
#include <string.h>

BEGIN_CLASS(LightGroup)
    ADD_BOOLPROP(StartOn, LTTRUE)
    ADD_COLORPROP(StartColor, 255, 255, 255)
END_CLASS_DEFAULT_FLAGS(LightGroup, Engine_LightGroup, LTNULL, LTNULL, 0)

LightGroup::LightGroup() :
    m_nID(0),
    m_vColor(1.0f, 1.0f, 1.0f),
    m_bOn(true)
{
}

void LightGroup::ReadProps(ObjectCreateStruct *pOCS)
{
    GenericProp prop;

    if(g_pLTServer->GetPropGeneric("StartOn", &prop) == LT_OK)
    {
        m_bOn = (prop.m_Bool != LTFALSE);
    }

    if(g_pLTServer->GetPropGeneric("StartColor", &prop) == LT_OK)
    {
        m_vColor = prop.m_Color / 255.0f;
    }

    if(g_pLTServer->GetPropGeneric("Name", &prop) == LT_OK)
    {
        strncpy(pOCS->m_Name, prop.m_String, sizeof(pOCS->m_Name) - 1);
        pOCS->m_Name[sizeof(pOCS->m_Name) - 1] = '\0';
    }

    g_pLTServer->GetLightGroupID(pOCS->m_Name, &m_nID);
    pOCS->m_Flags |= FLAG_FORCECLIENTUPDATE;

    g_pLTServer->CPrint(
        "Fireteam lighting: group %s id=%u StartOn=%u StartColor=%.2f %.2f %.2f",
        pOCS->m_Name,
        m_nID,
        m_bOn ? 1 : 0,
        m_vColor.x,
        m_vColor.y,
        m_vColor.z);
}

void LightGroup::SendUpdate()
{
    LTVector vAdjustment = m_bOn ? m_vColor : LTVector(0.0f, 0.0f, 0.0f);

    ILTMessage_Write *pMsg = LTNULL;
    if(g_pLTSCommon->CreateMessage(pMsg) != LT_OK || !pMsg)
    {
        return;
    }

    pMsg->IncRef();
    pMsg->Writeuint8(MSG_SC_LIGHTGROUP);
    pMsg->Writeuint32(m_nID);
    pMsg->WriteLTVector(vAdjustment);
    g_pLTServer->SendToClient(pMsg->Read(), LTNULL, MESSAGE_GUARANTEED);
    pMsg->DecRef();
}

uint32 LightGroup::EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData)
{
    switch(messageID)
    {
        case MID_PRECREATE:
        {
            ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;
            if(pOCS && fData == PRECREATE_WORLDFILE)
            {
                ReadProps(pOCS);
            }
        }
        break;

        case MID_INITIALUPDATE:
        {
            SendUpdate();
            // Repeat because initial light messages can arrive before the client world is ready.
            g_pLTServer->SetNextUpdate(m_hObject, 2.0f);
        }
        break;

        case MID_UPDATE:
        {
            SendUpdate();
            g_pLTServer->SetNextUpdate(m_hObject, 2.0f);
        }
        break;

        default:
            break;
    }

    return Engine_LightGroup::EngineMessageFn(messageID, pData, fData);
}