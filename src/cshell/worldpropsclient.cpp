//------------------------------------------------------------------------------//
//
// MODULE   : worldpropclnt.cpp
//
// PURPOSE  : CWorldPropsClnt - Implementation
//
// CREATED  : 02/14/2002
//
// (c) 2002 LithTech, Inc.  All Rights Reserved
//
//------------------------------------------------------------------------------//

//--Project Includes--

// Definition of this class
#include "worldpropsclient.h"

// Global pointers to client interfaces
#include "clientinterfaces.h"

//--Engine Includes--

// ILTClient definition
#include <iltclient.h>

//--Other includes--

// for sprintf
#include <stdio.h>

extern ILTTexInterface* g_pTexInterfaceLT;



//------------------------------------------------------------------------------
//	CWorldPropsClnt::CWorldPropsClnt()
//
//------------------------------------------------------------------------------
CWorldPropsClnt::CWorldPropsClnt()
:
m_nFarZ(10000),
m_vBackgroundColor(1.0f,1.0f,1.0f),
m_bFogEnable(false),
m_vFogColor(1.0f,1.0f,1.0f),
m_nFogNearZ(0),
m_nFogFarZ(5000),
m_bSkyFogEnable(false),
m_nSkyFogNearZ(0),
m_nSkyFogFarZ(5000),
m_bAllSkyPortals(false),
m_bPanSky(false),
m_fPanSkyOffsetX(0.0f),
m_fPanSkyOffsetZ(0.0f),
m_fPanSkyScaleX(1.0f),
m_fPanSkyScaleZ(1.0f),
m_fSkyScale(1.0f)
{
    m_szPanSkyTexture[0] = '\0';
}



//------------------------------------------------------------------------------
//	CWorldPropsClnt::~CWorldPropsClnt()
//
//------------------------------------------------------------------------------
CWorldPropsClnt::~CWorldPropsClnt()
{
}



//------------------------------------------------------------------------------
//	CWorldPropsClnt::UnpackWorldProps(ILTMessage_Read *pMsgProps)
//
//------------------------------------------------------------------------------
void CWorldPropsClnt::UnpackWorldProps(ILTMessage_Read *pMsgProps)
{
	m_nFarZ = pMsgProps->Readuint32();
	m_vBackgroundColor = pMsgProps->ReadLTVector();
	m_bFogEnable = pMsgProps->Readbool();
	m_vFogColor = pMsgProps->ReadLTVector();
	m_nFogNearZ = pMsgProps->Readuint32();
	m_nFogFarZ = pMsgProps->Readuint32();
	m_bSkyFogEnable = pMsgProps->Readbool();
	m_nSkyFogNearZ = pMsgProps->Readuint32();
	m_nSkyFogFarZ = pMsgProps->Readuint32();
	m_fSkyScale = pMsgProps->Readfloat();

    m_bAllSkyPortals = pMsgProps->Readbool();
    m_bPanSky = pMsgProps->Readbool();
    pMsgProps->ReadString(
        m_szPanSkyTexture,
        sizeof(m_szPanSkyTexture));
    m_fPanSkyOffsetX = pMsgProps->Readfloat();
    m_fPanSkyOffsetZ = pMsgProps->Readfloat();
    m_fPanSkyScaleX = pMsgProps->Readfloat();
    m_fPanSkyScaleZ = pMsgProps->Readfloat();

    g_pLTClient->CPrint(
        "Fireteam worldprops(client): FarZ=%d Background=%.1f %.1f %.1f Fog=%u color=%.1f %.1f %.1f near=%u far=%u SkyFog=%u near=%u far=%u SkyScale=%.2f AllSky=%u PanSky=%u tex=%s",
        m_nFarZ,
        m_vBackgroundColor.x,
        m_vBackgroundColor.y,
        m_vBackgroundColor.z,
        m_bFogEnable ? 1u : 0u,
        m_vFogColor.x,
        m_vFogColor.y,
        m_vFogColor.z,
        m_nFogNearZ,
        m_nFogFarZ,
        m_bSkyFogEnable ? 1u : 0u,
        m_nSkyFogNearZ,
        m_nSkyFogFarZ,
        m_fSkyScale,
        m_bAllSkyPortals ? 1u : 0u,
        m_bPanSky ? 1u : 0u,
        m_szPanSkyTexture[0]
            ? m_szPanSkyTexture
            : "<none>");

	this->ApplyWorldProps();
}



//------------------------------------------------------------------------------
//	CWorldPropsClnt::ApplyWorldProps()
//
//------------------------------------------------------------------------------
void CWorldPropsClnt::ApplyWorldProps()
{
	// Set worldprop console variables.
	char buffer[255];

	sprintf(buffer, "FarZ %d", m_nFarZ);
	g_pLTClient->RunConsoleString(buffer);

	sprintf(buffer, "BackgroundColor %d %d %d", (uint8)m_vBackgroundColor.x, (uint8)m_vBackgroundColor.y, (uint8)m_vBackgroundColor.z);
	g_pLTClient->RunConsoleString(buffer);

	sprintf(buffer, "FogEnable %d", m_bFogEnable ? 1 : 0);
	g_pLTClient->RunConsoleString(buffer);

	if (m_bFogEnable)
	{
		// Set fog rgb colors
		sprintf(buffer, "FogR %d", (uint8)m_vFogColor.x);
		g_pLTClient->RunConsoleString(buffer);
		sprintf(buffer, "FogG %d", (uint8)m_vFogColor.y);
		g_pLTClient->RunConsoleString(buffer);
		sprintf(buffer, "FogB %d", (uint8)m_vFogColor.z);
		g_pLTClient->RunConsoleString(buffer);

		sprintf(buffer, "FogNearZ %d; FogFarZ %d", m_nFogNearZ, m_nFogFarZ);
		g_pLTClient->RunConsoleString(buffer);
	}

	sprintf(buffer, "SkyFogEnable %d", m_bSkyFogEnable ? 1 : 0);
	g_pLTClient->RunConsoleString(buffer);
	if (m_bSkyFogEnable)
	{
		sprintf(buffer, "SkyFogNearZ %d; SkyFogFarZ %d", m_nSkyFogNearZ, m_nSkyFogFarZ);
		g_pLTClient->RunConsoleString(buffer);
	}

	sprintf(buffer, "SkyScale %f", m_fSkyScale);
	g_pLTClient->RunConsoleString(buffer);

    sprintf(
        buffer,
        "AllSkyPortals %d",
        m_bAllSkyPortals ? 1 : 0);
    g_pLTClient->RunConsoleString(buffer);

    sprintf(
        buffer,
        "PanSky %d",
        m_bPanSky ? 1 : 0);
    g_pLTClient->RunConsoleString(buffer);

    if(m_szPanSkyTexture[0])
    {
        sprintf(
            buffer,
            "PanSkyTexture %s",
            m_szPanSkyTexture);
        g_pLTClient->RunConsoleString(buffer);
    }

    sprintf(buffer, "PanSkyOffsetX %f", m_fPanSkyOffsetX);
    g_pLTClient->RunConsoleString(buffer);
    sprintf(buffer, "PanSkyOffsetZ %f", m_fPanSkyOffsetZ);
    g_pLTClient->RunConsoleString(buffer);
    sprintf(buffer, "PanSkyScaleX %f", m_fPanSkyScaleX);
    g_pLTClient->RunConsoleString(buffer);
    sprintf(buffer, "PanSkyScaleZ %f", m_fPanSkyScaleZ);
    g_pLTClient->RunConsoleString(buffer);
}



//------------------------------------------------------------------------------
//	CWorldPropsClnt::Update()
//
//------------------------------------------------------------------------------
void CWorldPropsClnt::Update()
{
}



//------------------------------------------------------------------------------
//	CWorldPropsClnt::GetBackgroundColor()
//
//------------------------------------------------------------------------------
LTRGB CWorldPropsClnt::GetBackgroundColor()
{
	LTRGB rgbColor;
	HCONSOLEVAR hCVar;

	rgbColor.r = 0;
	rgbColor.g = 0;
	rgbColor.b = 0;

	if (hCVar = g_pLTClient->GetConsoleVar("BackgroundR"))
	{
		rgbColor.r = (uint8)g_pLTClient->GetVarValueFloat(hCVar);
	}

	if (hCVar = g_pLTClient->GetConsoleVar("BackgroundG"))
	{
		rgbColor.g = (uint8)g_pLTClient->GetVarValueFloat(hCVar);
	}

	if (hCVar = g_pLTClient->GetConsoleVar("BackgroundB"))
	{
		rgbColor.b = (uint8)g_pLTClient->GetVarValueFloat(hCVar);
	}

	rgbColor.a = 255;

	return rgbColor;
}
