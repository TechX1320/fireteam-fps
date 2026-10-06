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
#include <math.h>
#include "clientinterfaces.h"

#define MAX_PITCH   (85.0f * (MATH_PI / 180.0f))
#define MIN_ZOOM	200.0f
#define MAX_ZOOM	400.0f



//----------------------------------------------------------------------------
// CCamera::CCamera()
//
//----------------------------------------------------------------------------
CCamera::CCamera() :
m_fPitch(0.0f),
m_fZoom(MIN_ZOOM),
m_bFirstPerson(true),
m_bWeaponZoom(false),
m_fWeaponZoomFovDegrees(24.0f),
m_nViewportWidth(0),
m_nViewportHeight(0)
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

	//	Get our screen dimensions, for the initial camera rectangle
	g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &nWidth, &nHeight);

	//	Initialize our object creation structure
	objCreate.Clear();
	objCreate.m_ObjectType = OT_CAMERA;
	m_hObject = g_pLTClient->CreateObject(&objCreate);

	if (NULL == m_hObject)
		return LT_ERROR;

    m_nViewportWidth = 0;
    m_nViewportHeight = 0;
    RefreshViewport();

	return LT_OK;
}



//----------------------------------------------------------------------------
// CCamera::UpdatePosition(HOBJECT hObject)
//
//----------------------------------------------------------------------------
void CCamera::RefreshViewport()
{
    if(!m_hObject)
    {
        return;
    }

    uint32 nWidth = 0;
    uint32 nHeight = 0;
    g_pLTClient->GetSurfaceDims(
        g_pLTClient->GetScreenSurface(),
        &nWidth,
        &nHeight);

    if(nWidth == 0 || nHeight == 0 ||
       (nWidth == m_nViewportWidth &&
        nHeight == m_nViewportHeight))
    {
        return;
    }

    g_pLTClient->SetCameraRect(
        m_hObject,
        false,
        0,
        0,
        nWidth,
        nHeight);

    const float fFovX =
        m_bWeaponZoom
        ? MATH_DEGREES_TO_RADIANS(m_fWeaponZoomFovDegrees)
        : (MATH_PI / 2.0f);

    // Keep vertical FOV aspect-correct instead of linearly scaling radians.
    const float fAspect =
        (float)nWidth / (float)nHeight;
    const float fFovY =
        2.0f * (float)atan(
            tan(fFovX * 0.5f) / fAspect);

    g_pLTClient->SetCameraFOV(
        m_hObject,
        fFovX,
        fFovY);

    m_nViewportWidth = nWidth;
    m_nViewportHeight = nHeight;

    g_pLTClient->CPrint(
        "Fireteam video: camera viewport %ux%u",
        nWidth,
        nHeight);
}


//----------------------------------------------------------------------------
// CCamera::UpdatePosition(HOBJECT hObject)
//
//----------------------------------------------------------------------------
void CCamera::UpdatePosition(HOBJECT hObject)
{
    RefreshViewport();

    LTVector vPos;
    LTRotation rRot;

    g_pLTClient->GetObjectPos(hObject, &vPos);
    g_pLTClient->GetObjectRotation(hObject, &rRot);

    LTVector vEyeUp = rRot.Up();
    // Mouse axis offsets and player yaw are already radians. Keep pitch in
    // radians too so horizontal/vertical sensitivity use the same units.
    rRot.Rotate(rRot.Right(), m_fPitch);

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
    float fNewPitch = m_fPitch + pitch;

    if(fNewPitch < -MAX_PITCH)
    {
        fNewPitch = -MAX_PITCH;
    }
    else if(fNewPitch > MAX_PITCH)
    {
        fNewPitch = MAX_PITCH;
    }

    m_fPitch = fNewPitch;
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
// Scoped first-person weapon zoom.
//----------------------------------------------------------------------------
void CCamera::ToggleWeaponZoom(float fFovDegrees)
{
    if(!m_bFirstPerson || fFovDegrees <= 0.0f)
    {
        return;
    }

    m_fWeaponZoomFovDegrees = fFovDegrees;
    if(m_fWeaponZoomFovDegrees < 8.0f) m_fWeaponZoomFovDegrees = 8.0f;
    if(m_fWeaponZoomFovDegrees > 70.0f) m_fWeaponZoomFovDegrees = 70.0f;

    m_bWeaponZoom = !m_bWeaponZoom;

    // Force the next RefreshViewport call to update camera FOV immediately.
    m_nViewportWidth = 0;
    m_nViewportHeight = 0;

    g_pLTClient->CPrint(
        "Fireteam scope: %s %.1f deg",
        m_bWeaponZoom ? "ON" : "OFF",
        m_fWeaponZoomFovDegrees);
}

void CCamera::ClearWeaponZoom()
{
    if(!m_bWeaponZoom)
    {
        return;
    }

    m_bWeaponZoom = false;
    m_nViewportWidth = 0;
    m_nViewportHeight = 0;
}

//----------------------------------------------------------------------------
// CCamera::ToggleView()
//----------------------------------------------------------------------------
void CCamera::ToggleView()
{
    m_bFirstPerson = !m_bFirstPerson;
    if(!m_bFirstPerson)
    {
        ClearWeaponZoom();
    }

    m_nViewportWidth = 0;
    m_nViewportHeight = 0;

    g_pLTClient->CPrint("Camera: %s", m_bFirstPerson ? "First Person" : "Third Person");
}
