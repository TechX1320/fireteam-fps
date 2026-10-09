// ----------------------------------------------------------------------- //
//
// MODULE  : CStatsGui.cpp
//
// PURPOSE : CStatsGui - Implementation
//
// CREATED : 07/18/2003
//
// (c) 2003 LithTech, Inc.  All Rights Reserved
//
// ----------------------------------------------------------------------- //
#include <windows.h>

#include "statsgui.h"
#include "clientinterfaces.h"
#include <iltdrawprim.h>
#include <iltmessage.h>

//------------------------------------------------------------------------------
//	CStatsGui::CStatsGui()
//
//------------------------------------------------------------------------------
CStatsGui::CStatsGui():
m_hBackDrop(NULL),
m_pFont(NULL),
m_pStatsString_Title(NULL),
m_pStatsString_Playername(NULL),
m_pStatsString_Sealswhacked(NULL),
m_pStatsString_Moneyearned(NULL),
m_pScores(NULL),
m_iNumPlayers(0)
{
    for(int i = 0; i < 4; ++i)
        m_pStatsString_Extra[i] = LTNULL;
}


//------------------------------------------------------------------------------
//	CStatsGui::~CStatsGui()
//
//------------------------------------------------------------------------------
CStatsGui::~CStatsGui()
{

}


//------------------------------------------------------------------------------
//	CStatsGui::Init()
//
//------------------------------------------------------------------------------
LTRESULT CStatsGui::Init()
{
    if (NULL == m_hBackDrop)
	{
		LTRESULT result = g_pLTCTexInterface->CreateTextureFromName(m_hBackDrop, "tex/ui/backdrop.dtx");

		if (LT_OK != result)
		{
			g_pLTClient->CPrint("Error!!! Failed to load %s", "tex/ui/backdrop.dtx");
			return false;
		}
	}

    //Init the font
	const char *pFontFilename = "fonts/SQR721B.TTF";
	const char *pFontFace = "Square721 BT";
	uint8 ptSize = 18;
    m_pFont = g_pLTCFontManager->CreateFont(pFontFilename, pFontFace, ptSize, 33, 255);

	if (m_pFont)
	{
		uint32 w, h;
		g_pLTCTexInterface->GetTextureDims(m_pFont->GetTexture(), w, h);
		g_pLTClient->CPrint("Created font <%s> using a %dX%d texture.", pFontFilename, w, h);

        m_pFont->SetDefCharWidth((ptSize/4));
        m_pFont->SetDefColor(0xFF0081DF);
	}
    else
    {
        g_pLTClient->CPrint("Failed to create font: ", pFontFilename);
    }

	if (LTNULL == m_pStatsString_Title)
	{
		m_pStatsString_Title = g_pLTCFontManager->CreateFormattedPolyString(m_pFont, "");
		if (LTNULL == m_pStatsString_Title)
		{
        	g_pLTClient->CPrint("Error creating m_pStatsString_Playername!");
			return LT_ERROR;
		}
	}
    m_pStatsString_Title->SetText("");


	if (LTNULL == m_pStatsString_Playername)
	{
		m_pStatsString_Playername = g_pLTCFontManager->CreateFormattedPolyString(m_pFont, "");
		if (LTNULL == m_pStatsString_Playername)
		{
        	g_pLTClient->CPrint("Error creating m_pStatsString_Playername!");
			return LT_ERROR;
		}
	}
    m_pStatsString_Playername->SetText("");


	if (LTNULL == m_pStatsString_Sealswhacked)
	{
		m_pStatsString_Sealswhacked = g_pLTCFontManager->CreateFormattedPolyString(m_pFont, "");
		if (LTNULL == m_pStatsString_Sealswhacked)
		{
        	g_pLTClient->CPrint("Error creating m_pStatsString_Playername!");
			return LT_ERROR;
		}
	}
    m_pStatsString_Sealswhacked->SetText("");


	if (LTNULL == m_pStatsString_Moneyearned)
	{
		m_pStatsString_Moneyearned = g_pLTCFontManager->CreateFormattedPolyString(m_pFont, "");
		if (LTNULL == m_pStatsString_Moneyearned)
		{
        	g_pLTClient->CPrint("Error creating m_pStatsString_Playername!");
			return LT_ERROR;
		}
	}
    m_pStatsString_Moneyearned->SetText("");


    for(int n = 0; n < 4; ++n)
    {
        m_pStatsString_Extra[n] =
            g_pLTCFontManager->CreateFormattedPolyString(m_pFont, "");
        if(!m_pStatsString_Extra[n])
            return LT_ERROR;
    }

    /*
    uint32 nFontWidth = static_cast<uint32>(m_pStatsString_Playername->GetWidth());
    uint32 nFontHeight = static_cast<uint32>(m_pStatsString_Playername->GetHeight());
    uint32 nWidth, nHeight;
    g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &nWidth, &nHeight);
    m_pStatsString_Playername->SetPosition(static_cast<float>((nWidth/2) - (nFontWidth/2)),
                               static_cast<float>((nHeight/2) - (nFontHeight/2)));
    */

    return LT_OK;
}


//------------------------------------------------------------------------------
//	CStatsGui::Term()
//
//------------------------------------------------------------------------------
LTRESULT CStatsGui::Term()
{

    if(LTNULL != m_hBackDrop)
    {
        g_pLTCTexInterface->ReleaseTextureHandle(m_hBackDrop);
        m_hBackDrop = NULL;
    }

    if(LTNULL != m_pFont)
    {

        g_pLTCFontManager->DestroyFont(m_pFont);
    }

	if (LTNULL != m_pStatsString_Title)
	{
		g_pLTCFontManager->DestroyPolyString(m_pStatsString_Title);
	}

	if (LTNULL != m_pStatsString_Playername)
	{
		g_pLTCFontManager->DestroyPolyString(m_pStatsString_Playername);
	}

	if (LTNULL != m_pStatsString_Sealswhacked)
	{
		g_pLTCFontManager->DestroyPolyString(m_pStatsString_Sealswhacked);
	}

	if (LTNULL != m_pStatsString_Moneyearned)
	{
		g_pLTCFontManager->DestroyPolyString(m_pStatsString_Moneyearned);
	}

    for(int n = 0; n < 4; ++n)
    {
        if(m_pStatsString_Extra[n])
        {
            g_pLTCFontManager->DestroyPolyString(m_pStatsString_Extra[n]);
            m_pStatsString_Extra[n] = LTNULL;
        }
    }

	if (LTNULL != m_pScores)
	{
		delete[] m_pScores;
        m_pScores = NULL;
	}


    return LT_OK;
}


//------------------------------------------------------------------------------
//	CStatsGui::Render()
//
//------------------------------------------------------------------------------
LTRESULT CStatsGui::Render()
{
    if(!m_pStatsString_Title || !m_pStatsString_Playername ||
       !m_pStatsString_Sealswhacked || !m_pStatsString_Moneyearned)
        return LT_ERROR;
    for(int n = 0; n < 4; ++n)
        if(!m_pStatsString_Extra[n])
            return LT_ERROR;

    // If we haven't created this texture yet, then return.
	if (!m_hBackDrop)
	{
		g_pLTClient->CPrint("Error: m_hBackDrop is invalid!!!");
		return LT_ERROR;
	}

	// Get screen dims.
	uint32 nScreenW, nScreenH;
	g_pLTClient->GetSurfaceDims(g_pLTClient->GetScreenSurface(), &nScreenW, &nScreenH);

    // The 2003 background was 512x256 regardless of player count.
    // Use a responsive, centered results panel with proper pixel columns.
    const float fPanelWidth =
        nScreenW > 1060 ? 1020.0f : (float)nScreenW - 40.0f;
    const float fPanelHeight =
        nScreenH > 700 ? 630.0f : (float)nScreenH - 60.0f;
    const float left = ((float)nScreenW - fPanelWidth) * 0.5f;
    const float right = left + fPanelWidth;
    const float top = ((float)nScreenH - fPanelHeight) * 0.5f;
    const float bottom = top + fPanelHeight;
    const float fScale = fPanelWidth / 1020.0f;

	// Set up the verts (clockwise from upper left).
	LT_POLYFT4 poly;
	poly.verts[0].x = left;
	poly.verts[0].y = top;
	poly.verts[0].z = SCREEN_NEAR_Z;
	poly.verts[0].u = 0.0f;
	poly.verts[0].v = 0.0f;

	poly.verts[1].x = right;
	poly.verts[1].y = top;
	poly.verts[1].z = SCREEN_NEAR_Z;
	poly.verts[1].u = 1.0f;
	poly.verts[1].v = 0.0f;

	poly.verts[2].x = right;
	poly.verts[2].y = bottom;
	poly.verts[2].z = SCREEN_NEAR_Z;
	poly.verts[2].u = 1.0f;
	poly.verts[2].v = 1.0f;

	poly.verts[3].x = left;
	poly.verts[3].y = bottom;
	poly.verts[3].z = SCREEN_NEAR_Z;
	poly.verts[3].u = 0.0f;
	poly.verts[3].v = 1.0f;

	// Set up color and alpha.
	poly.rgba.r = 255;
	poly.rgba.g = 255;
	poly.rgba.b = 255;
    poly.rgba.a = 210; // Solid readable scoreboard on dark zombie maps.

	// Set which texture to use.
	g_pLTCDrawPrim->SetTexture(m_hBackDrop);

	// Set up the drawprim render state.
	g_pLTCDrawPrim->SetTransformType(DRAWPRIM_TRANSFORM_SCREEN);
	g_pLTCDrawPrim->SetColorOp(DRAWPRIM_MODULATE);
	g_pLTCDrawPrim->SetAlphaBlendMode(DRAWPRIM_BLEND_MOD_SRCALPHA);
	g_pLTCDrawPrim->SetZBufferMode(DRAWPRIM_NOZ);
	g_pLTCDrawPrim->SetAlphaTestMode(DRAWPRIM_NOALPHATEST);
	g_pLTCDrawPrim->SetClipMode(DRAWPRIM_FASTCLIP);
	g_pLTCDrawPrim->SetFillMode(DRAWPRIM_FILL);
	g_pLTCDrawPrim->SetCullMode(DRAWPRIM_CULL_NONE);
	g_pLTCDrawPrim->SetCamera(NULL);

	// Draw the Image (just 1 quad).
	g_pLTCDrawPrim->DrawPrim(&poly, 1);

    // Each data column is positioned independently. Never use spaces to
    // align proportional-font headings and numbers across resolutions.
    const float fDataTop = top + 78.0f;
    m_pStatsString_Title->SetPosition(left + 26.0f, top + 22.0f);
    m_pStatsString_Playername->SetPosition(left + 26.0f, fDataTop);
    m_pStatsString_Sealswhacked->SetPosition(left + 350.0f * fScale, fDataTop);
    m_pStatsString_Extra[0]->SetPosition(left + 455.0f * fScale, fDataTop);
    m_pStatsString_Extra[1]->SetPosition(left + 570.0f * fScale, fDataTop);
    m_pStatsString_Extra[2]->SetPosition(left + 665.0f * fScale, fDataTop);
    m_pStatsString_Extra[3]->SetPosition(left + 770.0f * fScale, fDataTop);
    m_pStatsString_Moneyearned->SetPosition(left + 918.0f * fScale, fDataTop);

    m_pStatsString_Title->Render();
    m_pStatsString_Playername->Render();
    m_pStatsString_Sealswhacked->Render();
    for(int n = 0; n < 4; ++n)
        m_pStatsString_Extra[n]->Render();
    m_pStatsString_Moneyearned->Render();

    return LT_OK;
}


//------------------------------------------------------------------------------
//	CStatsGui::HandleMessage(ILTMessage_Read* pMessage)
//
//------------------------------------------------------------------------------
LTRESULT CStatsGui::HandleMessage(ILTMessage_Read* pMessage)
{
    m_iNumPlayers = pMessage->Readuint8();

    if(m_iNumPlayers > 0)
    {
        if(LTNULL != m_pScores)
        {
            delete[] m_pScores;
            m_pScores = NULL;
        }

        m_pScores = new SCORESTRUCT[m_iNumPlayers];
        assert(m_pScores);

        //g_pLTClient->CPrint("Server Scores:");

        for(int i = 0; i < m_iNumPlayers; i++)
        {
            m_pScores[i].iClientID = pMessage->Readuint32();
            pMessage->ReadString(m_pScores[i].sPlayerName , 32);
            m_pScores[i].iScore = pMessage->Readuint32();
            m_pScores[i].iLives = pMessage->Readuint8();
            m_pScores[i].fMoney = pMessage->Readfloat();
            m_pScores[i].iShotsFired = pMessage->Readuint32();
            m_pScores[i].iShotsHit = pMessage->Readuint32();
            m_pScores[i].iDeaths = pMessage->Readuint32();
            m_pScores[i].iPowerups = pMessage->Readuint32();
            m_pScores[i].iHeadshotKills = pMessage->Readuint32();
            //g_pLTClient->CPrint("(%d) %d - %s - %d - $%.2f", i, iClientID, sName, iScore, fMoney);
        }

        // Fireteam scoreboard ranks the squad by infected kills.
        SortStats();

        // Recalculate the poly string
        RecalcStatsString();
    }

    return LT_OK;
}


//------------------------------------------------------------------------------
//	LTRESULT CStatsGui::SortStats()
//
//------------------------------------------------------------------------------
LTRESULT CStatsGui::SortStats()
{
    if(!m_pScores || m_iNumPlayers < 2)
    {
        return LT_OK;
    }

    // Selection sort is plenty for a 24-player maximum and keeps this module
    // compatible with the original Jupiter-era toolchain style.
    for(int i = 0; i < m_iNumPlayers - 1; ++i)
    {
        int nBest = i;

        for(int j = i + 1; j < m_iNumPlayers; ++j)
        {
            if(m_pScores[j].iScore >
               m_pScores[nBest].iScore)
            {
                nBest = j;
            }
        }

        if(nBest != i)
        {
            SCORESTRUCT temp = m_pScores[i];
            m_pScores[i] = m_pScores[nBest];
            m_pScores[nBest] = temp;
        }
    }

    return LT_OK;
}


//------------------------------------------------------------------------------
//  LTRESULT CStatsGui::RecalcStatsString()
//
//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
LTRESULT CStatsGui::RecalcStatsString()
{
    if(!m_pStatsString_Title || !m_pStatsString_Playername ||
       !m_pStatsString_Sealswhacked || !m_pStatsString_Moneyearned)
        return LT_ERROR;
    for(int n = 0; n < 4; ++n)
        if(!m_pStatsString_Extra[n]) return LT_ERROR;

    char sNames[1280] = "PLAYER\n\n";
    char sKills[512] = "KILLS\n\n";
    char sShots[512] = "SHOTS\n\n";
    char sHits[512] = "HITS\n\n";
    char sDeaths[512] = "DEATHS\n\n";
    char sPowerups[512] = "POWERUPS\n\n";
    char sLives[512] = "LIVES\n\n";

    for(int i = 0; i < m_iNumPlayers && m_pScores; ++i)
    {
        char line[96];
        _snprintf(line, sizeof(line) - 1, "%s\n", m_pScores[i].sPlayerName);
        line[sizeof(line) - 1] = '\0';
        strncat(sNames, line, sizeof(sNames) - strlen(sNames) - 1);
        _snprintf(line, sizeof(line) - 1, "%u\n", m_pScores[i].iScore);
        line[sizeof(line) - 1] = '\0';
        strncat(sKills, line, sizeof(sKills) - strlen(sKills) - 1);
        _snprintf(line, sizeof(line) - 1, "%u\n", m_pScores[i].iShotsFired);
        line[sizeof(line) - 1] = '\0';
        strncat(sShots, line, sizeof(sShots) - strlen(sShots) - 1);
        _snprintf(line, sizeof(line) - 1, "%u\n", m_pScores[i].iShotsHit);
        line[sizeof(line) - 1] = '\0';
        strncat(sHits, line, sizeof(sHits) - strlen(sHits) - 1);
        _snprintf(line, sizeof(line) - 1, "%u\n", m_pScores[i].iDeaths);
        line[sizeof(line) - 1] = '\0';
        strncat(sDeaths, line, sizeof(sDeaths) - strlen(sDeaths) - 1);
        _snprintf(line, sizeof(line) - 1, "%u\n", m_pScores[i].iPowerups);
        line[sizeof(line) - 1] = '\0';
        strncat(sPowerups, line, sizeof(sPowerups) - strlen(sPowerups) - 1);
        _snprintf(line, sizeof(line) - 1, "%u\n", (uint32)m_pScores[i].iLives);
        line[sizeof(line) - 1] = '\0';
        strncat(sLives, line, sizeof(sLives) - strlen(sLives) - 1);
    }

    m_pStatsString_Title->SetText("FIRETEAM  /  SQUAD PERFORMANCE");
    m_pStatsString_Title->SetColor(0xFFFFB000);
    m_pStatsString_Playername->SetText(sNames);
    m_pStatsString_Sealswhacked->SetText(sKills);
    m_pStatsString_Extra[0]->SetText(sShots);
    m_pStatsString_Extra[1]->SetText(sHits);
    m_pStatsString_Extra[2]->SetText(sDeaths);
    m_pStatsString_Extra[3]->SetText(sPowerups);
    m_pStatsString_Moneyearned->SetText(sLives);

    m_pStatsString_Playername->SetColor(0xFFFFFFFF);
    m_pStatsString_Sealswhacked->SetColor(0xFFFFFFFF);
    for(int n = 0; n < 4; ++n)
        m_pStatsString_Extra[n]->SetColor(0xFFFFFFFF);
    m_pStatsString_Moneyearned->SetColor(0xFFFFFFFF);
    return LT_OK;
}
