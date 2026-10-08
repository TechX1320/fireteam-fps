#include "FireteamLightGroupClient.h"
#include "clientinterfaces.h"

#include <iltclient.h>
#include <map>

static std::map<uint32, LTVector> s_BaseColors;
static std::map<uint32, LTVector> s_PendingAdjustments;
static bool s_bPrintedGlobalLightScale = false;

void FT_QueueLightGroup(uint32 nID, const LTVector &vAdjustment)
{
    s_PendingAdjustments[nID] = vAdjustment;
    FT_UpdateLightGroups();
}

void FT_UpdateLightGroups()
{
    // A/B diagnostic for Cabin Fever's torn-looking wall lamps. Authored
    // lightmaps remain untouched; only compatibility overrides are skipped.
    // +FTLightGroupOverrides 0 shows the compiled DAT's native LightGroup
    // colors. If lamps still tear, investigate their sprite/DTX render style.
    HCONSOLEVAR hOverrides =
        g_pLTClient->GetConsoleVar("FTLightGroupOverrides");
    const bool bAllowOverrides =
        !hOverrides || g_pLTClient->GetVarValueFloat(hOverrides) != 0.0f;
    if(!bAllowOverrides)
    {
        for(std::map<uint32, LTVector>::iterator p = s_BaseColors.begin();
            p != s_BaseColors.end(); ++p)
            g_pLTClient->SetLightGroupColor(p->first, p->second);
        s_BaseColors.clear();
        s_PendingAdjustments.clear();
        return;
    }

    if(!s_bPrintedGlobalLightScale)
    {
        LTVector vGlobalScale;
        g_pLTClient->GetGlobalLightScale(&vGlobalScale);
        g_pLTClient->CPrint(
            "Fireteam lighting: global scale %.2f %.2f %.2f",
            vGlobalScale.x,
            vGlobalScale.y,
            vGlobalScale.z);
        s_bPrintedGlobalLightScale = true;
    }

    std::map<uint32, LTVector>::iterator it = s_PendingAdjustments.begin();

    while(it != s_PendingAdjustments.end())
    {
        uint32 nID = it->first;
        LTVector vAdjustment = it->second;
        LTVector vBaseColor;

        std::map<uint32, LTVector>::iterator baseIt = s_BaseColors.find(nID);
        if(baseIt == s_BaseColors.end())
        {
            if(g_pLTClient->GetLightGroupColor(nID, &vBaseColor) != LT_OK)
            {
                ++it;
                continue;
            }

            s_BaseColors[nID] = vBaseColor;
            g_pLTClient->CPrint(
                "Fireteam lighting: group %u base %.2f %.2f %.2f adjustment %.2f %.2f %.2f",
                nID,
                vBaseColor.x,
                vBaseColor.y,
                vBaseColor.z,
                vAdjustment.x,
                vAdjustment.y,
                vAdjustment.z);
        }
        else
        {
            vBaseColor = baseIt->second;
        }

        g_pLTClient->SetLightGroupColor(nID, vBaseColor * vAdjustment);

        std::map<uint32, LTVector>::iterator eraseIt = it;
        ++it;
        s_PendingAdjustments.erase(eraseIt);
    }
}

void FT_ClearLightGroups()
{
    std::map<uint32, LTVector>::iterator it = s_BaseColors.begin();
    for(; it != s_BaseColors.end(); ++it)
    {
        g_pLTClient->SetLightGroupColor(it->first, it->second);
    }

    s_BaseColors.clear();
    s_PendingAdjustments.clear();
    s_bPrintedGlobalLightScale = false;
}