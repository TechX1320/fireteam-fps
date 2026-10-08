//------------------------------------------------------------------------------//
//
// MODULE   : camera.h
//
// PURPOSE  : CCamera - Definition
//
// CREATED  : 6/27/2002
//
// (c) 2002 LithTech, Inc.  All Rights Reserved
//
//------------------------------------------------------------------------------//


#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "ltclientshell.h"

// Engine includes
#include <ltbasedefs.h>



//-----------------------------------------------------------------------------
class CCamera
{
public:

	CCamera();

	~CCamera()
	{
	}

	LTRESULT 		CreateCamera();
	HOBJECT 		GetCamera()		{ return m_hObject; }

    void    		UpdatePosition(HOBJECT hObject, float fEyeHeight);
    void            RefreshViewport();
    void    		UpdatePitch(float pitch);
    void    		UpdateZoom(float zoom);
    void            ToggleView();

    void            ToggleWeaponZoom(float fFovDegrees);
    void            ClearWeaponZoom();
    void            SetFreecamEnabled(
                        bool bEnabled,
                        HOBJECT hSource = LTNULL,
                        float fEyeHeight = 0.0f);
    void            UpdateFreecam(
                        float fForward,
                        float fRight,
                        float fUp,
                        float fYaw,
                        float fPitch,
                        float fFrameTime,
                        bool bFast);
    bool            IsWeaponZoomed() const { return m_bWeaponZoom; }
    bool            IsFirstPerson() const { return m_bFirstPerson; }
    bool            IsFreecam() const { return m_bFreecam; }

private:

	HLOCALOBJ 		m_hObject;
    float     		m_fPitch;
    float     		m_fZoom;
    bool            m_bFirstPerson;
    bool            m_bWeaponZoom;
    bool            m_bFreecam;
    float           m_fWeaponZoomFovDegrees;
    LTVector        m_vFreecamPos;
    LTRotation      m_rFreecamRot;
    uint32          m_nViewportWidth;
    uint32          m_nViewportHeight;
};


#endif // __CAMERA_H__
