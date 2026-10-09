#ifndef __FIRETEAM_RADAR_HUD_H__
#define __FIRETEAM_RADAR_HUD_H__

class ILTMessage_Read;

// Always game-code rendered. Optional original DTX art can decorate later.
void FT_RadarHudReset();
void FT_RadarHudHandleMessage(ILTMessage_Read *pMessage);
void FT_RenderRadarHud();

#endif
