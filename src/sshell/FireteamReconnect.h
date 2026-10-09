#ifndef __FIRETEAM_RECONNECT_H__
#define __FIRETEAM_RECONNECT_H__

class CPlayerSrvr;

// Identity is a random client capability, not a name or IP address.
// All HP/lives/stats/position data always comes from the running server.
bool FT_ClaimReconnect(CPlayerSrvr *pPlayer, const char *pTicket);
void FT_ParkReconnect(CPlayerSrvr *pPlayer);
void FT_ResetReconnect();

#endif
