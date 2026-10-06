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
    bool            IsWeaponZoomed() const { return m_bWeaponZoom; }
    bool            IsFirstPerson() const { return m_bFirstPerson; }

private:

	HLOCALOBJ 		m_hObject;
    float     		m_fPitch;
    float     		m_fZoom;
    bool            m_bFirstPerson;
    bool            m_bWeaponZoom;
    float           m_fWeaponZoomFovDegrees;
    uint32          m_nViewportWidth;
    uint32          m_nViewportHeight;
};


#endif // __CAMERA_H__
