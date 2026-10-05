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

    void    		UpdatePosition(HOBJECT hObject);
    void    		UpdatePitch(float pitch);
    void    		UpdateZoom(float zoom);
    void            ToggleView();
    bool            IsFirstPerson() const { return m_bFirstPerson; }

private:

	HLOCALOBJ 		m_hObject;
    float     		m_fPitch;
    float     		m_fZoom;
    bool            m_bFirstPerson;
};


#endif // __CAMERA_H__
