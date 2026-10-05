//------------------------------------------------------------------------------//
//
// MODULE   : camera.cpp
//
// PURPOSE  : CCamera - Implementation
//
// CREATED  : 6/27/2002
//
// (c) 2002 LithTech, Inc.  All Rights Reserved
//
//------------------------------------------------------------------------------//

#include "camera.h"

#include <iltclient.h>
#include <ltobjectcreate.h>
#include "clientinterfaces.h"

#define MAX_PITCH   85.0f
#define MIN_ZOOM	200.0f
#define MAX_ZOOM	400.0f



//----------------------------------------------------------------------------
// CCamera::CCamera()
//
//----------------------------------------------------------------------------
CCamera::CCamera() :
m_fPitch(0.0f),
m_fZoom(MIN_ZOOM),
m_bFirstPerson(true)
{
}



//----------------------------------------------------------------------------
// LTRESULT CCamera::CreateCamera()
//
//----------------------------------------------------------------------------
LTRESULT CCamera::CreateCamera()
{
	uint32 nWidth, nHeight;
	ObjectCreateStruct objCreate;

	//	Get our screen dimensions, for the camera rectangle
	g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &nWidth, &nHeight);

	//	Initialize our object creation structure
	objCreate.Clear();
	objCreate.m_ObjectType = OT_CAMERA;
	m_hObject = g_pLTClient->CreateObject(&objCreate);

	if (NULL == m_hObject)
		return LT_ERROR;

	g_pLTClient->SetCameraRect(m_hObject, false, 0, 0, nWidth, nHeight);

	//	This is an fov of 90 degrees
	float fFovX = MATH_PI/2.0f;
	float fFovY = (fFovX * nHeight) / nWidth;
	g_pLTClient->SetCameraFOV(m_hObject, fFovX, fFovY);

	return LT_OK;
}



//----------------------------------------------------------------------------
// CCamera::UpdatePosition(HOBJECT hObject)
//
//----------------------------------------------------------------------------
void CCamera::UpdatePosition(HOBJECT hObject)
{
    LTVector vPos;
    LTRotation rRot;

    g_pLTClient->GetObjectPos(hObject, &vPos);
    g_pLTClient->GetObjectRotation(hObject, &rRot);

    LTVector vEyeUp = rRot.Up();
    rRot.Rotate(rRot.Right(), (m_fPitch * 0.0174533f));

    // Fireteam FPS first-person camera.
    if (m_bFirstPerson)
    {
        g_pLTCCommon->SetObjectFlags(hObject, OFT_Flags, 0, FLAG_VISIBLE);
        vPos += vEyeUp * 65.0f;
        vPos += rRot.Forward() * 3.0f;
        g_pLTClient->SetObjectPosAndRotation(m_hObject, &vPos, &rRot);
        return;
    }

    g_pLTCCommon->SetObjectFlags(hObject, OFT_Flags, FLAG_VISIBLE, FLAG_VISIBLE);
    vPos += rRot.Forward() * -m_fZoom;
    vPos += rRot.Up() * 50.0f;

    g_pLTClient->SetObjectPosAndRotation(m_hObject, &vPos, &rRot);
}



//----------------------------------------------------------------------------
// CCamera::UpdatePitch(float _pitch)
//
//----------------------------------------------------------------------------
void CCamera::UpdatePitch(float pitch)
{
    if ((m_fPitch + pitch) < -MAX_PITCH)
    {
        m_fPitch = -MAX_PITCH;
    }
    else if ((m_fPitch + pitch) > MAX_PITCH)
    {
        m_fPitch = MAX_PITCH;
    }
    else
    {
		m_fPitch += pitch * 5.0f;
    }
}



//----------------------------------------------------------------------------
// CCamera::UpdateZoom(float zoom)
//
//----------------------------------------------------------------------------
void CCamera::UpdateZoom(float zoom)
{
    m_fZoom -= zoom;

	if(m_fZoom < MIN_ZOOM)
	{
		m_fZoom = MIN_ZOOM;
	}

	if(m_fZoom > MAX_ZOOM)
	{
		m_fZoom = MAX_ZOOM;
	}
}


//----------------------------------------------------------------------------
// CCamera::ToggleView()
//----------------------------------------------------------------------------
void CCamera::ToggleView()
{
    m_bFirstPerson = !m_bFirstPerson;
    g_pLTClient->CPrint("Camera: %s", m_bFirstPerson ? "First Person" : "Third Person");
}
