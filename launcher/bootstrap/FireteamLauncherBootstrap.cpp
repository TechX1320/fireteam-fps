#include <windows.h>
#include <string>
#include <vector>

static std::wstring GetRootDirectory()
{
    wchar_t path[MAX_PATH];
    DWORD len = GetModuleFileNameW(NULL, path, MAX_PATH);
    if(len == 0 || len >= MAX_PATH)
    {
        return L".";
    }

    std::wstring root(path, len);
    const size_t slash = root.find_last_of(L"\\/");
    if(slash != std::wstring::npos)
    {
        root.resize(slash);
    }

    return root;
}

int WINAPI wWinMain(
    HINSTANCE,
    HINSTANCE,
    PWSTR,
    int)
{
    const std::wstring root = GetRootDirectory();
    const std::wstring target =
        root + L"\\Launcher\\App\\FireteamLauncher.exe";

    const DWORD attrs =
        GetFileAttributesW(target.c_str());

    if(attrs == INVALID_FILE_ATTRIBUTES ||
       (attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        MessageBoxW(
            NULL,
            L"FIRETEAM Launcher runtime is missing.\n\n"
            L"Expected:\nLauncher\\App\\FireteamLauncher.exe\n\n"
            L"Run build.cmd again.",
            L"FIRETEAM Launcher",
            MB_OK | MB_ICONERROR);
        return 2;
    }

    std::wstring command =
        L"\"" + target + L"\"";

    std::vector<wchar_t> commandLine(
        command.begin(),
        command.end());

    commandLine.push_back(L'\0');

    STARTUPINFOW si = {};
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi = {};

    if(!CreateProcessW(
        target.c_str(),
        commandLine.data(),
        NULL,
        NULL,
        FALSE,
        0,
        NULL,
        root.c_str(),
        &si,
        &pi))
    {
        wchar_t message[512];
        wsprintfW(
            message,
            L"FIRETEAM Launcher could not start.\n\n"
            L"Windows error: %lu\n\n"
            L"Runtime:\n%s",
            GetLastError(),
            target.c_str());

        MessageBoxW(
            NULL,
            message,
            L"FIRETEAM Launcher",
            MB_OK | MB_ICONERROR);
        return 3;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
