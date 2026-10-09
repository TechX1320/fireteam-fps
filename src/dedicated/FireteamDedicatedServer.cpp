// Standalone headless FIRETEAM host using the Jupiter ServerInterface.
// No renderer, player client shell or NOLF2 ServerApp/MFC dependency.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <server_interface.h>
#include <ltbasedefs.h>
#include <ltmodule.h>

#include "FireteamGameGuid.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Minimal, opt-in-free LAN advertisements for the dedicated HOST only.
// Broadcast packets stay inside the local network. This is NOT Internet
// master-server registration and does not create router/NAT mappings.
static const unsigned short kFtLanDiscoveryPort = 27888;
class FireteamLanAdvertiser
{
public:
    FireteamLanAdvertiser()
        : m_Socket(INVALID_SOCKET), m_bWinsock(false),
          m_LastAnnouncement(0), m_nInstance(0) { }

    bool Open()
    {
        WSADATA data;
        if(WSAStartup(MAKEWORD(2, 2), &data) != 0)
            return false;
        m_bWinsock = true;
        m_Socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if(m_Socket == INVALID_SOCKET)
            return false;
        BOOL broadcast = TRUE;
        if(setsockopt(m_Socket, SOL_SOCKET, SO_BROADCAST,
                      (const char*)&broadcast, sizeof(broadcast)) != 0)
            return false;
        m_nInstance = ((ULONGLONG)GetCurrentProcessId() << 32) |
                      (ULONGLONG)GetTickCount();
        return true;
    }

    void Close()
    {
        if(m_Socket != INVALID_SOCKET)
        {
            closesocket(m_Socket);
            m_Socket = INVALID_SOCKET;
        }
        if(m_bWinsock)
        {
            WSACleanup();
            m_bWinsock = false;
        }
    }

    // Called on the normal 15ms dedicated tick, but actually transmits
    // once per 2s. Uses actual Jupiter GetNumClients (not an estimate).
    void Update(ServerInterface *pServer, const char *pName,
                const char *pMap, uint32 nGamePort, uint32 nMaxPlayers,
                uint32 nDifficulty)
    {
        if(m_Socket == INVALID_SOCKET || !pServer)
            return;
        const ULONGLONG now = GetTickCount64();
        if(m_LastAnnouncement && now - m_LastAnnouncement < 2000)
            return;
        m_LastAnnouncement = now;

        char sCleanName[65];
        size_t i = 0;
        for(; i < sizeof(sCleanName) - 1 && pName && pName[i]; ++i)
        {
            char c = pName[i];
            sCleanName[i] = (c == '|' || c == '\r' || c == '\n' ||
                             (unsigned char)c < 32) ? ' ' : c;
        }
        sCleanName[i] = '\0';

        int nClients = pServer->GetNumClients();
        if(nClients < 0) nClients = 0;
        if(nClients > (int)nMaxPlayers) nClients = (int)nMaxPlayers;

        char packet[280];
        // Delimited, bounded and versioned so the launcher can reject
        // untrusted/malformed LAN packets. No passwords or personal data.
        _snprintf(packet, sizeof(packet),
                  "FTLAN1|%08lX%08lX|%u|%u|%d|%u|%s|%s",
                  (unsigned long)(m_nInstance >> 32),
                  (unsigned long)(m_nInstance & 0xffffffff),
                  (unsigned)nGamePort, (unsigned)nMaxPlayers, nClients,
                  (unsigned)nDifficulty, pMap, sCleanName);
        packet[sizeof(packet) - 1] = '\0';

        sockaddr_in address;
        memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = htons(kFtLanDiscoveryPort);
        address.sin_addr.s_addr = INADDR_BROADCAST;
        sendto(m_Socket, packet, (int)strlen(packet), 0,
               (sockaddr*)&address, sizeof(address));

        // Broadcast may not loop back on the hosting PC: announce to its
        // local launcher explicitly without needing an Internet path.
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        sendto(m_Socket, packet, (int)strlen(packet), 0,
               (sockaddr*)&address, sizeof(address));
    }

private:
    SOCKET m_Socket;
    bool m_bWinsock;
    ULONGLONG m_LastAnnouncement;
    ULONGLONG m_nInstance;
};

static unsigned ReadLanDifficulty()
{
    FILE *pFile = fopen("config/session.cfg", "rt");
    if(!pFile) return 4;
    unsigned nDifficulty = 4;
    char line[160];
    while(fgets(line, sizeof(line), pFile))
    {
        unsigned nRead = 0;
        if(sscanf(line, "difficulty=%u", &nRead) == 1 &&
           nRead >= 1 && nRead <= 10)
            nDifficulty = nRead;
    }
    fclose(pFile);
    return nDifficulty;
}

static volatile LONG s_nStopRequested = 0;

static BOOL WINAPI OnConsoleControl(DWORD nControl)
{
    if(nControl == CTRL_C_EVENT ||
       nControl == CTRL_BREAK_EVENT ||
       nControl == CTRL_CLOSE_EVENT)
    {
        InterlockedExchange(&s_nStopRequested, 1);
        return TRUE;
    }
    return FALSE;
}

class FireteamDedicatedHandler : public ServerAppHandler
{
public:
    virtual LTRESULT ConsoleOutputFn(const char *pText)
    {
        if(pText)
            puts(pText);
        return LT_OK;
    }

    virtual LTRESULT OutOfMemory()
    {
        fputs("FIRETEAM: dedicated server ran out of memory.\n", stderr);
        InterlockedExchange(&s_nStopRequested, 1);
        return LT_OK;
    }
};

static bool IsSafeMap(const char *pText)
{
    if(!pText || !pText[0])
        return false;
    for(const char *p = pText; *p; ++p)
    {
        if(!((*p >= 'A' && *p <= 'Z') ||
             (*p >= 'a' && *p <= 'z') ||
             (*p >= '0' && *p <= '9') ||
             *p == '_' || *p == '-'))
            return false;
    }
    return true;
}

static void PrintUsage()
{
    puts("FIRETEAM Dedicated Server (experimental)");
    puts("Usage: FireteamDedicatedServer.exe [--map CABINFEVER] [--port 27889]");
    puts("       [--name \"Fireteam Server\"] [--max-players 24]");
    puts("Press Ctrl+C to stop. Start from a complete FIRETEAM BUILT directory.");
}

int main(int argc, char **argv)
{
    char sMap[96] = "CABINFEVER";
    char sName[96] = "FIRETEAM Dedicated";
    uint32 nPort = 27889;
    uint32 nMaxPlayers = 24;

    for(int i = 1; i < argc; ++i)
    {
        if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            PrintUsage();
            return 0;
        }
        if(i + 1 >= argc)
        {
            PrintUsage();
            return 2;
        }

        const char *pValue = argv[++i];
        if(strcmp(argv[i - 1], "--map") == 0)
        {
            strncpy(sMap, pValue, sizeof(sMap) - 1);
            sMap[sizeof(sMap) - 1] = '\0';
        }
        else if(strcmp(argv[i - 1], "--name") == 0)
        {
            strncpy(sName, pValue, sizeof(sName) - 1);
            sName[sizeof(sName) - 1] = '\0';
        }
        else if(strcmp(argv[i - 1], "--port") == 0)
        {
            nPort = (uint32)atoi(pValue);
        }
        else if(strcmp(argv[i - 1], "--max-players") == 0)
        {
            nMaxPlayers = (uint32)atoi(pValue);
        }
        else
        {
            PrintUsage();
            return 2;
        }
    }

    if(!IsSafeMap(sMap) || nPort == 0 || nPort > 65535 ||
       nMaxPlayers == 0 || nMaxPlayers > 24)
    {
        fputs("Invalid map, port or player count.\n", stderr);
        return 2;
    }

    if(GetFileAttributesA("Engine.REZ") == INVALID_FILE_ATTRIBUTES ||
       GetFileAttributesA("rez") == INVALID_FILE_ATTRIBUTES ||
       GetFileAttributesA("rez\\object.lto") == INVALID_FILE_ATTRIBUTES)
    {
        fputs("Missing Engine.REZ, rez directory, or rez\\object.lto. Run build.cmd first.\n", stderr);
        return 2;
    }

    char sMapFile[MAX_PATH];
    _snprintf(sMapFile, sizeof(sMapFile), "rez\\Worlds\\%s.DAT", sMap);
    sMapFile[sizeof(sMapFile) - 1] = '\0';
    if(GetFileAttributesA(sMapFile) == INVALID_FILE_ATTRIBUTES)
    {
        fprintf(stderr, "Missing staged map: %s\n", sMapFile);
        return 2;
    }

    HMODULE hServerDll = LoadLibraryA("server.dll");
    if(!hServerDll)
    {
        fprintf(stderr, "Unable to load Jupiter server.dll (Win32 error %lu).\n",
                GetLastError());
        return 3;
    }

    typedef SI_CREATESTATUS (__cdecl *CreateServerProc)(
        int, LTGUID &, ServerInterface **);
    typedef void (__cdecl *DeleteServerProc)();

    CreateServerProc pCreateServer =
        reinterpret_cast<CreateServerProc>(
            GetProcAddress(hServerDll, "CreateServer"));
    DeleteServerProc pDeleteServer =
        reinterpret_cast<DeleteServerProc>(
            GetProcAddress(hServerDll, "DeleteServer"));

    if(!pCreateServer || !pDeleteServer)
    {
        fputs("server.dll does not expose the Jupiter server interface.\n", stderr);
        FreeLibrary(hServerDll);
        return 3;
    }

    ServerInterface *pServer = NULL;
    LTGUID gameGuid = FT_GetGameGuid();
    const SI_CREATESTATUS nStatus =
        pCreateServer(SI_VERSION, gameGuid, &pServer);
    if(nStatus != SI_OK || !pServer)
    {
        fprintf(stderr, "CreateServer failed (%u).\n", (unsigned)nStatus);
        FreeLibrary(hServerDll);
        return 3;
    }

    // Match NOLF2/ServerApp/ServerDlg.cpp: the executable must register its
    // LithTech interface database with server.dll before loading the world
    // and object.lto. Otherwise VerifyServerInterfaces may find null holders.
    TSetMasterFn pSetMaster =
        reinterpret_cast<TSetMasterFn>(
            GetProcAddress(hServerDll, "SetMasterDatabase"));
    if(pSetMaster)
    {
        pSetMaster(GetMasterDatabase());
        puts("[OK] Jupiter master interface database registered.");
    }
    else
    {
        puts("[WARN] server.dll has no SetMasterDatabase export.");
    }

    FireteamDedicatedHandler handler;
    bool bReady = false;

    do
    {
        if(pServer->SetAppHandler(&handler) != LT_OK)
        {
            fputs("SetAppHandler failed.\n", stderr);
            break;
        }

        const char *pResources[] = { "Engine.REZ", "rez" };
        if(!pServer->AddResources(pResources, 2))
        {
            fputs("AddResources failed.\n", stderr);
            break;
        }

        if(!pServer->InitNetworking(NULL, 0))
        {
            fputs("InitNetworking failed.\n", stderr);
            break;
        }

        NetService *pServices = NULL;
        if(!pServer->GetServiceList(pServices) || !pServices)
        {
            fputs("No Jupiter network service available.\n", stderr);
            break;
        }

        HNETSERVICE hTcpService = NULL;
        for(NetService *p = pServices; p; p = p->m_pNext)
        {
            if(p->m_dwFlags & NETSERVICE_TCPIP)
            {
                hTcpService = p->m_handle;
                break;
            }
        }
        pServer->FreeServiceList(pServices);

        if(!hTcpService || !pServer->SelectService(hTcpService))
        {
            fputs("TCP/IP network driver unavailable.\n", stderr);
            break;
        }

        NetHost host;
        memset(&host, 0, sizeof(host));
        host.m_Port = (uint16)nPort;
        host.m_dwMaxConnections = nMaxPlayers;
        strncpy(host.m_sName, sName, sizeof(host.m_sName) - 1);

        if(!pServer->HostGame(&host))
        {
            fputs("HostGame failed: port already in use?\n", stderr);
            break;
        }

        // FIRETEAM's server shell does not consume NOLF2's ServerGameOptions.
        // Do not send a fabricated 4-byte "version" pretending to be that
        // structure. StartWorld also replaces game info with request data.
        if(!pServer->SetGameInfo(NULL, 0))
        {
            fputs("SetGameInfo failed.\n", stderr);
            break;
        }

        if(!pServer->LoadBinaries())
        {
            fputs("LoadBinaries failed (object.lto / game resources).\n", stderr);
            break;
        }

        StartGameRequest request;
        memset(&request, 0, sizeof(request));
        request.m_Type = STARTGAME_HOST;
        request.m_HostInfo = host;
        _snprintf(request.m_WorldName, sizeof(request.m_WorldName),
                  "Worlds/%s", sMap);
        request.m_WorldName[sizeof(request.m_WorldName) - 1] = '\0';

        if(!pServer->StartWorld(&request))
        {
            char sError[256] = {};
            pServer->GetErrorString(sError, sizeof(sError));
            fprintf(stderr, "StartWorld failed for %s: %s\n",
                    request.m_WorldName, sError);
            break;
        }

        bReady = true;
        printf("FIRETEAM dedicated server running: %s on port %u (max %u)\n",
               sMap, (unsigned)nPort, (unsigned)nMaxPlayers);
        printf("Local client test: 127.0.0.1:%u\n", (unsigned)nPort);
        puts("Use Ctrl+C to shut down.");

        FireteamLanAdvertiser lanAdvertiser;
        if(lanAdvertiser.Open())
            puts("FIRETEAM LAN discovery: advertising to launchers on UDP 27888.");
        else
            puts("[WARN] LAN discovery unavailable; direct IP still works.");
        const unsigned lanDifficulty = ReadLanDifficulty();

        SetConsoleCtrlHandler(OnConsoleControl, TRUE);
        while(InterlockedCompareExchange(&s_nStopRequested, 0, 0) == 0)
        {
            if(!pServer->Update(0))
            {
                char sError[256] = {};
                pServer->GetErrorString(sError, sizeof(sError));
                fprintf(stderr, "Server update failed: %s\n", sError);
                bReady = false;
                break;
            }
            lanAdvertiser.Update(pServer, sName, sMap, nPort,
                                 nMaxPlayers, lanDifficulty);
            // Launcher requests graceful shutdown through a local file.
            // The server checks it on its normal update loop; no forced kill.
            if(GetFileAttributesA("data\\server-stop.request") != INVALID_FILE_ATTRIBUTES)
            {
                DeleteFileA("data\\server-stop.request");
                InterlockedExchange(&s_nStopRequested, 1);
                puts("FIRETEAM: launcher stop requested.");
            }
            Sleep(15);
        }
        SetConsoleCtrlHandler(OnConsoleControl, FALSE);
        lanAdvertiser.Close();
    } while(false);

    pServer->SetAppHandler(NULL);
    pDeleteServer();
    FreeLibrary(hServerDll);

    if(!bReady)
    {
        fputs("FIRETEAM dedicated host stopped due to initialization/update failure.\n", stderr);
        return 4;
    }

    puts("FIRETEAM dedicated server stopped.");
    return 0;
}
