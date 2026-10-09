#include "FireteamPublicAnnouncer.h"
#include <winhttp.h>
#include <bcrypt.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace
{
    static bool Hex(const char *pText, unsigned nCount)
    {
        if(!pText || strlen(pText) != nCount) return false;
        for(unsigned n = 0; n < nCount; ++n)
        {
            const char c = pText[n];
            if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                 (c >= 'A' && c <= 'F'))) return false;
        }
        return true;
    }

    static void SafeName(char *pOut, size_t nOut, const char *pIn)
    {
        size_t n = 0;
        if(pIn)
        {
            while(pIn[n] && n < nOut - 1)
            {
                const char c = pIn[n];
                if((c >= 'a' && c <= 'z') ||
                   (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == ' ' ||
                   c == '_' || c == '-')
                    pOut[n] = c;
                else
                    pOut[n] = '_';
                ++n;
            }
        }
        pOut[n] = '\0';
    }

    static void ToHex(const unsigned char *pBytes, unsigned nCount,
                      char *pOutput)
    {
        static const char abc[] = "0123456789abcdef";
        for(unsigned n = 0; n < nCount; ++n)
        {
            pOutput[n * 2] = abc[pBytes[n] >> 4];
            pOutput[n * 2 + 1] = abc[pBytes[n] & 15];
        }
        pOutput[nCount * 2] = '\0';
    }
}

FireteamPublicAnnouncer::FireteamPublicAnnouncer()
    : m_hThread(NULL), m_hStopEvent(NULL), m_nPlayers(0),
      m_nHubPort(0), m_bSecure(false), m_nGamePort(0),
      m_nMaxPlayers(0), m_nDifficulty(0)
{
    memset(m_Host, 0, sizeof(m_Host));
    memset(m_Id, 0, sizeof(m_Id));
    memset(m_Key, 0, sizeof(m_Key));
    memset(m_Name, 0, sizeof(m_Name));
    memset(m_Map, 0, sizeof(m_Map));
}

FireteamPublicAnnouncer::~FireteamPublicAnnouncer()
{
    Stop();
}

bool FireteamPublicAnnouncer::ParseEndpoint(const char *pUrl)
{
    if(!pUrl || strlen(pUrl) > 240) return false;
    wchar_t url[256];
    const int count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, pUrl, -1,
        url, sizeof(url) / sizeof(url[0]));
    if(count <= 0) return false;

    URL_COMPONENTS parts;
    memset(&parts, 0, sizeof(parts));
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = m_Host;
    parts.dwHostNameLength = sizeof(m_Host) / sizeof(m_Host[0]) - 1;

    wchar_t path[64] = {};
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = sizeof(path) / sizeof(path[0]) - 1;

    wchar_t extras[8] = {};
    parts.lpszExtraInfo = extras;
    parts.dwExtraInfoLength = sizeof(extras) / sizeof(extras[0]) - 1;

    if(!WinHttpCrackUrl(url, 0, 0, &parts) ||
       parts.dwHostNameLength == 0 || parts.dwHostNameLength > 240)
        return false;
    m_Host[parts.dwHostNameLength] = L'\0';
    // A hub base URL can be scheme://host[:port][/]; never accept
    // credentials, a URL suffix or an insecure Internet endpoint.
    if(parts.dwUrlPathLength > 1 ||
       (parts.dwUrlPathLength == 1 && path[0] != L'/') ||
       parts.dwExtraInfoLength > 0)
        return false;
    m_bSecure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    if(!m_bSecure)
    {
        if(parts.nScheme != INTERNET_SCHEME_HTTP ||
           (_wcsicmp(m_Host, L"localhost") != 0 &&
            wcscmp(m_Host, L"127.0.0.1") != 0))
            return false;
    }
    m_nHubPort = parts.nPort;
    return m_nHubPort != 0;
}

bool FireteamPublicAnnouncer::InitializeIdentity(unsigned nGamePort)
{
    CreateDirectoryA("data", NULL);
    char path[MAX_PATH];
    _snprintf(path, sizeof(path),
              "data\\hub-%u.identity", nGamePort);
    path[sizeof(path) - 1] = '\0';

    FILE *pFile = fopen(path, "rt");
    if(pFile)
    {
        const int read = fscanf(pFile, "%32s %64s", m_Id, m_Key);
        fclose(pFile);
        if(read == 2 && Hex(m_Id, 32) && Hex(m_Key, 64))
            return true;
        // Never silently replace an existing malformed credential.
        return false;
    }

    unsigned char randomData[48];
    if(BCryptGenRandom(NULL, randomData, sizeof(randomData),
                       BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        return false;
    ToHex(randomData, 16, m_Id);
    ToHex(randomData + 16, 32, m_Key);
    SecureZeroMemory(randomData, sizeof(randomData));

    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                              FILE_ATTRIBUTE_HIDDEN, NULL);
    if(file == INVALID_HANDLE_VALUE)
        return false;
    char text[120];
    const int length = _snprintf(text, sizeof(text), "%s %s\n", m_Id, m_Key);
    DWORD written = 0;
    const bool ok = length > 0 &&
        WriteFile(file, text, (DWORD)length, &written, NULL) &&
        written == (DWORD)length;
    CloseHandle(file);
    return ok;
}

bool FireteamPublicAnnouncer::Start(
    const char *pHubUrl, const char *pName, const char *pMap,
    unsigned nGamePort, unsigned nMaxPlayers, unsigned nDifficulty)
{
    if(m_hThread || m_hStopEvent || !ParseEndpoint(pHubUrl) ||
       !InitializeIdentity(nGamePort))
        return false;

    SafeName(m_Name, sizeof(m_Name), pName);
    SafeName(m_Map, sizeof(m_Map), pMap);
    if(!m_Name[0] || !m_Map[0])
        return false;

    m_nGamePort = nGamePort;
    m_nMaxPlayers = nMaxPlayers;
    m_nDifficulty = nDifficulty;
    m_hStopEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if(!m_hStopEvent) return false;

    unsigned threadId = 0;
    const uintptr_t handle = _beginthreadex(
        NULL, 0, &ThreadProc, this, 0, &threadId);
    if(!handle)
    {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = NULL;
        return false;
    }
    m_hThread = (HANDLE)handle;
    return true;
}

void FireteamPublicAnnouncer::UpdatePlayers(int count)
{
    if(count < 0) count = 0;
    if(count > (int)m_nMaxPlayers) count = (int)m_nMaxPlayers;
    InterlockedExchange(&m_nPlayers, count);
}

unsigned __stdcall FireteamPublicAnnouncer::ThreadProc(void *pContext)
{
    return ((FireteamPublicAnnouncer*)pContext)->Run();
}

unsigned FireteamPublicAnnouncer::Run()
{
    // Never block pServer->Update(): WinHTTP is only used on this worker.
    while(WaitForSingleObject(m_hStopEvent, 0) == WAIT_TIMEOUT)
    {
        if(!Post(L"/v1/hosts/heartbeat", false))
            fputs("[WARN] FIRETEAM Hub heartbeat failed; LAN/direct IP still work.\n",
                  stderr);
        if(WaitForSingleObject(m_hStopEvent, 25000) != WAIT_TIMEOUT)
            break;
    }
    Post(L"/v1/hosts/offline", true);
    return 0;
}

bool FireteamPublicAnnouncer::Post(const wchar_t *pEndpoint, bool offline)
{
    char json[620];
    int nBytes = 0;
    if(offline)
        nBytes = _snprintf(json, sizeof(json),
             "{\"id\":\"%s\",\"key\":\"%s\"}",
             m_Id, m_Key);
    else
        nBytes = _snprintf(json, sizeof(json),
             "{\"id\":\"%s\",\"key\":\"%s\",\"name\":\"%s\","
             "\"map\":\"%s\",\"port\":%u,\"players\":%ld,"
             "\"maxPlayers\":%u,\"difficulty\":%u,\"protocol\":\"FT1\"}",
             m_Id, m_Key, m_Name, m_Map, m_nGamePort,
             InterlockedCompareExchange(&m_nPlayers, 0, 0),
             m_nMaxPlayers, m_nDifficulty);
    if(nBytes <= 0 || nBytes >= (int)sizeof(json)) return false;

    HINTERNET session = WinHttpOpen(
        L"FIRETEAM Dedicated/alpha", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if(!session) return false;
    WinHttpSetTimeouts(session, 1500, 2000, 3000, 3000);
    HINTERNET connection = WinHttpConnect(
        session, m_Host, m_nHubPort, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(
        connection, L"POST", pEndpoint, NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        m_bSecure ? WINHTTP_FLAG_SECURE : 0) : NULL;

    bool success = false;
    if(request)
    {
        const wchar_t *pHeaders = L"Content-Type: application/json\r\n";
        DWORD bytes = (DWORD)nBytes;
        if(WinHttpSendRequest(request, pHeaders, (DWORD)-1,
                             json, bytes, bytes, 0) &&
           WinHttpReceiveResponse(request, NULL))
        {
            DWORD status = 0, size = sizeof(status);
            if(WinHttpQueryHeaders(request,
                  WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                  WINHTTP_HEADER_NAME_BY_INDEX, &status, &size,
                  WINHTTP_NO_HEADER_INDEX))
                success = offline ? status == 204 : status == 200;
        }
    }
    SecureZeroMemory(json, sizeof(json));
    if(request) WinHttpCloseHandle(request);
    if(connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return success;
}

void FireteamPublicAnnouncer::Stop()
{
    if(m_hStopEvent) SetEvent(m_hStopEvent);
    if(m_hThread)
    {
        // Worker has short socket timeouts and sends a final "offline".
        // Must join before destroying this object to avoid use-after-free.
        WaitForSingleObject(m_hThread, INFINITE);
        CloseHandle(m_hThread);
        m_hThread = NULL;
    }
    if(m_hStopEvent)
    {
        CloseHandle(m_hStopEvent);
        m_hStopEvent = NULL;
    }
    SecureZeroMemory(m_Key, sizeof(m_Key));
}
