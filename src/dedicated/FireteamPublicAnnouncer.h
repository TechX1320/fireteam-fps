#ifndef FIRETEAM_PUBLIC_ANNOUNCER_H
#define FIRETEAM_PUBLIC_ANNOUNCER_H

#include <windows.h>
#include <stdint.h>

// Optional HTTPS presence updates run on a background thread, never on
// Jupiter's real-time world update thread. No third-party host-side app.
class FireteamPublicAnnouncer
{
public:
    FireteamPublicAnnouncer();
    ~FireteamPublicAnnouncer();

    // Explicit opt-in only. URL must be HTTPS, except for loopback tests.
    bool Start(const char *pHubUrl, const char *pName, const char *pMap,
               unsigned nPort, unsigned nMaxPlayers, unsigned nDifficulty);
    void UpdatePlayers(int nCurrent);
    void Stop();

private:
    static unsigned __stdcall ThreadProc(void *pContext);
    unsigned Run();
    bool Post(const wchar_t *pEndpoint, bool bOffline);
    bool InitializeIdentity(unsigned nPort);
    bool ParseEndpoint(const char *pUrl);

    HANDLE m_hThread;
    HANDLE m_hStopEvent;
    volatile LONG m_nPlayers;
    wchar_t m_Host[256];
    unsigned short m_nHubPort;
    bool m_bSecure;
    char m_Id[33];
    char m_Key[65];
    char m_Name[49];
    char m_Map[65];
    unsigned m_nGamePort;
    unsigned m_nMaxPlayers;
    unsigned m_nDifficulty;
};

#endif
