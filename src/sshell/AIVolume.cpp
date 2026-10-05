//------------------------------------------------------------------------------//
//
// MODULE	: AIVolume.cpp
//
// PURPOSE	: AIVolume - Implementation
//
// CREATED	: 10/23/2002
//
// (c) 2002 LithTech, Inc.	All Rights Reserved
//
//------------------------------------------------------------------------------//

#include "AIVolume.h"

#include <ltobjectcreate.h>
#include <iltmessage.h>
#include <iltcommon.h>
#include <iltphysics.h>
#include "msgids.h"
#include "serverinterfaces.h"
#include "seal.h"
#include "statsmanager.h"



// Maximum number of seals on the screen at a given time
#define MAX_SEALS	10


//-----------------------------------------------------------------------------
BEGIN_CLASS(ZombieSpawner)
ADD_STRINGPROP_FLAG(Target, "", PF_OBJECTLINK)
ADD_VECTORPROP_FLAG(Dims, PF_DIMS)
END_CLASS_DEFAULT_FLAGS(ZombieSpawner, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)

// Imported maps use AIVolume for navigation data.  Do not spawn enemies here.
BEGIN_CLASS(AIVolume)
ADD_VECTORPROP_VAL_FLAG(Dims, 32.0f, 8.0f, 32.0f, PF_DIMS)
ADD_STRINGPROP_FLAG(Region, "", PF_OBJECTLINK)
ADD_BOOLPROP(Lit, LTTRUE)
ADD_STRINGPROP_FLAG(LightSwitchNode, "", PF_OBJECTLINK)
ADD_BOOLPROP(PreferredPath, LTFALSE)
END_CLASS_DEFAULT_FLAGS(AIVolume, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)
AIVolume::AIVolume() :
    m_vDims(0.0f, 0.0f, 0.0f),
    m_bLit(true),
    m_bPreferredPath(false)
{
    m_sName[0] = '\0';
    m_sRegion[0] = '\0';
    m_sLightSwitchNode[0] = '\0';
}

void AIVolume::ReadNavigationProps(ObjectCreateStruct *pOCS)
{
    GenericProp prop;

    if(g_pLTServer->GetPropGeneric("Name", &prop) == LT_OK)
    {
        strncpy(m_sName, prop.m_String, sizeof(m_sName) - 1);
        m_sName[sizeof(m_sName) - 1] = '\0';
        strncpy(pOCS->m_Name, m_sName, sizeof(pOCS->m_Name) - 1);
        pOCS->m_Name[sizeof(pOCS->m_Name) - 1] = '\0';
    }

    if(g_pLTServer->GetPropGeneric("Dims", &prop) == LT_OK)
    {
        m_vDims = prop.m_Vec;
    }

    if(g_pLTServer->GetPropGeneric("Region", &prop) == LT_OK)
    {
        strncpy(m_sRegion, prop.m_String, sizeof(m_sRegion) - 1);
        m_sRegion[sizeof(m_sRegion) - 1] = '\0';
    }

    if(g_pLTServer->GetPropGeneric("LightSwitchNode", &prop) == LT_OK)
    {
        strncpy(m_sLightSwitchNode, prop.m_String, sizeof(m_sLightSwitchNode) - 1);
        m_sLightSwitchNode[sizeof(m_sLightSwitchNode) - 1] = '\0';
    }

    if(g_pLTServer->GetPropGeneric("Lit", &prop) == LT_OK)
    {
        m_bLit = (prop.m_Bool != LTFALSE);
    }

    if(g_pLTServer->GetPropGeneric("PreferredPath", &prop) == LT_OK)
    {
        m_bPreferredPath = (prop.m_Bool != LTFALSE);
    }

    pOCS->m_ObjectType = OT_NORMAL;
    pOCS->m_Flags = 0;
}

uint32 AIVolume::EngineMessageFn(uint32 messageID, void *pData, float fData)
{
    if(messageID == MID_PRECREATE)
    {
        ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;
        if(pOCS && fData == PRECREATE_WORLDFILE)
        {
            ReadNavigationProps(pOCS);
        }
    }
    else if(messageID == MID_INITIALUPDATE)
    {
        g_pLTServer->SetNextUpdate(m_hObject, 0.0f);
    }

    return BaseClass::EngineMessageFn(messageID, pData, fData);
}

bool AIVolume::Contains2D(const LTVector& vPos) const
{
    LTVector vCenter;
    g_pLTServer->GetObjectPos(m_hObject, &vCenter);

    return (vPos.x >= (vCenter.x - m_vDims.x)) &&
           (vPos.x <= (vCenter.x + m_vDims.x)) &&
           (vPos.z >= (vCenter.z - m_vDims.z)) &&
           (vPos.z <= (vCenter.z + m_vDims.z));
}



//-----------------------------------------------------------------------------
//	ZombieSpawner::EngineMessageFn(uint32 messageID, void *pData, float fData)
//
//-----------------------------------------------------------------------------
uint32 ZombieSpawner::EngineMessageFn(uint32 messageID, void *pData, float fData)
{
	switch (messageID)
	{
	case MID_PRECREATE:
		return PreCreate(pData, fData);
    //case MID_TOUCHNOTIFY:
    //   return TouchNotify(pData, fData);
    case MID_INITIALUPDATE:
        {
            g_pLTServer->SetNextUpdate(m_hObject, 5.0f);
        }
        break;
		
    case MID_UPDATE:
        {
            uint32 iNumPlayers = g_pStatsManager->GetNumPlayers();

			// if there isn't any players then have a least one seal spawned 
			if ( !iNumPlayers )
			{
				if ( !m_NumSeals )
				{
					Spawn();
				}
				
				g_pLTServer->SetNextUpdate(m_hObject, 10.0f);
				
			}
			else
			{
				if (m_NumSeals < MAX_SEALS)
				{
					//spawn a new one
            		Spawn();
				}
				
				g_pLTServer->SetNextUpdate(m_hObject, 10.0f/(static_cast<float>(iNumPlayers)));
            }
        }
        break;

	default:
		break;
	}

	// Pass the message along to parent class.
	return BaseClass::EngineMessageFn(messageID, pData, fData);
}



//-----------------------------------------------------------------------------
//	ZombieSpawner::PreCreate(void *pData, float fData)
//
//-----------------------------------------------------------------------------
uint32 ZombieSpawner::PreCreate(void *pData, float fData)
{
	// Let parent class handle it first
	BaseClass::EngineMessageFn(MID_PRECREATE, pData, fData);

	// Cast pData to a ObjectCreateStruct* for convenience
	ObjectCreateStruct* pStruct = (ObjectCreateStruct*)pData;

	// Set the object type to OT_NORMAL
	pStruct->m_ObjectType = OT_NORMAL;//OT_WORLDMODEL;

    pStruct->m_Flags = 0;//FLAG_VISIBLE | FLAG_SOLID | FLAG_BOXPHYSICS;//FLAG_TOUCH_NOTIFY;

	// Check to see if this is coming from a world file
	if(fData == PRECREATE_WORLDFILE)
	{
		ReadProps(pStruct);
	}

	// Return default of 1
	return 1;
}



//-----------------------------------------------------------------------------
//	ZombieSpawner::TouchNotify(void *pData, float fData)
//
//-----------------------------------------------------------------------------
uint32 ZombieSpawner::TouchNotify(void *pData, float fData)
{
    g_pLTServer->CPrint("(AIVolume) Touch!");
    if( m_hTarget == NULL)
    {
        /*
        //Find target object
        ObjArray<HOBJECT, 1> objArray; //The code below will crash if it returns more than 1 with the same name
        uint32 dwNumObjects = 0;
        g_pLTServer->FindNamedObjects(m_sTarget, objArray, &dwNumObjects);
        if( dwNumObjects )
        {
            m_hTarget = objArray.GetObject(0);
            PingTarget();
        }
        */
    }
    else
    {
        //PingTarget();
    }



    return 1;
}



//-----------------------------------------------------------------------------
//	ZombieSpawner::PingTarget()
//
//-----------------------------------------------------------------------------
void ZombieSpawner::PingTarget()
{
    /*
	ILTMessage_Write *pMsg;
	LTRESULT nResult = g_pLTSCommon->CreateMessage(pMsg);
	pMsg->IncRef();
	pMsg->Writeint8(MSG_OBJ_PINGOBJECT);
    g_pLTServer->SendToObject(pMsg->Read(), m_hObject, m_hTarget, 0);
	pMsg->DecRef();
    */

}



//-----------------------------------------------------------------------------
//	ZombieSpawner::ReadProps(ObjectCreateStruct* pStruct)
//
//-----------------------------------------------------------------------------
void ZombieSpawner::ReadProps(ObjectCreateStruct* pStruct)
{
	g_pLTServer->GetPropString("Name", pStruct->m_Filename, MAX_CS_FILENAME_LEN);
    g_pLTServer->GetPropVector("Dims", &m_vDims);
}



//-----------------------------------------------------------------------------
//	Spawn
//
//-----------------------------------------------------------------------------
void ZombieSpawner::Spawn()
{
    LTVector vPos;
    LTRotation rRot;
    g_pLTServer->GetObjectPos(m_hObject, &vPos);
    g_pLTServer->GetObjectRotation(m_hObject, &rRot);

    ObjectCreateStruct ocs;
    ocs.m_ObjectType = OT_MODEL;
    strcpy(ocs.m_Filename, "Models/seal.ltb");
    strcpy(ocs.m_SkinName, "ModelTextures/seal.dtx");

    LTFLOAT x = g_pLTServer->Random(-m_vDims.x, m_vDims.x);
    LTFLOAT z = g_pLTServer->Random(-m_vDims.z, m_vDims.z);

    ocs.m_Pos.y = vPos.y;

    ocs.m_Pos.x = vPos.x + x;
    ocs.m_Pos.z = vPos.z + z;

    HCLASS hClass = g_pLTServer->GetClass("Seal");

    if (g_pLTServer->CreateObject(hClass, &ocs) != LTNULL)
	{
		++m_NumSeals;
	}
}
