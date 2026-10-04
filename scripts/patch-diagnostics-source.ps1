param(
    [Parameter(Mandatory = $true)]
    [string]$LocalRoot
)

$ErrorActionPreference = "Stop"
$encoding = [System.Text.Encoding]::Default
$sealRoot = Join-Path $LocalRoot "imports\sealhunter"
$engineRoot = Join-Path $LocalRoot "imports\engine"
$sdkRoot = Join-Path $engineRoot "sdk\inc"

function Read-Source([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing source file: $Path"
    }
    return [System.IO.File]::ReadAllText($Path, $encoding)
}

function Write-Source([string]$Path, [string]$Text) {
    $dir = Split-Path -Parent $Path
    if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

function Insert-AfterText([string]$Path, [string]$Needle, [string]$Insertion, [string]$Guard, [string]$Label) {
    $text = Read-Source $Path
    if ($text.Contains($Guard)) {
        Write-Host "[OK] $Label already patched"
        return
    }
    $idx = $text.IndexOf($Needle)
    if ($idx -lt 0) { throw "Could not locate $Label insertion point in $Path" }
    $idx += $Needle.Length
    $text = $text.Insert($idx, $Insertion)
    Write-Source $Path $text
    Write-Host "[OK] $Label"
}

# ---------------------------------------------------------------------------
# Header-only, crash-safe trace logger shared by cshell/server/ClientFX code.
# Opens and closes on every line so the last checkpoint survives a hard crash.
# ---------------------------------------------------------------------------
$traceHeader = @'
#ifndef __FIRETEAM_TRACE_H__
#define __FIRETEAM_TRACE_H__

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>

inline void FTTraceReset()
{
#ifdef _WIN32
    DeleteFileA("fireteam-trace.log");
#else
    remove("fireteam-trace.log");
#endif
}

inline void FTTraceLog(const char* module, const char* format, ...)
{
    char message[2048];
    message[0] = 0;

    va_list args;
    va_start(args, format);
#ifdef _WIN32
    _vsnprintf(message, sizeof(message) - 1, format, args);
#else
    vsnprintf(message, sizeof(message) - 1, format, args);
#endif
    va_end(args);
    message[sizeof(message) - 1] = 0;

    unsigned long tick = 0;
#ifdef _WIN32
    tick = (unsigned long)GetTickCount();
#endif

    FILE* fp = fopen("fireteam-trace.log", "a");
    if(fp)
    {
        fprintf(fp, "%010lu [%s] %s\n", tick, module ? module : "?", message);
        fflush(fp);
        fclose(fp);
    }

#ifdef _WIN32
    char debuggerLine[2300];
    _snprintf(debuggerLine, sizeof(debuggerLine) - 1, "[Fireteam][%s] %s\n", module ? module : "?", message);
    debuggerLine[sizeof(debuggerLine) - 1] = 0;
    OutputDebugStringA(debuggerLine);
#endif
}

#endif
'@
Write-Source (Join-Path $sdkRoot "fireteamtrace.h") $traceHeader
Write-Host "[OK] crash-safe Fireteam trace logger"

# ---------------------------------------------------------------------------
# Client shell diagnostics + ClientFX safe mode.
# ---------------------------------------------------------------------------
$clientH = Join-Path $sealRoot "cshell\src\ltclientshell.h"
$text = Read-Source $clientH
if (-not $text.Contains("m_bClientFXReady")) {
    $needle = "    CClientFXMgr"
    $idx = $text.IndexOf($needle)
    if ($idx -lt 0) { throw "Could not locate ClientFXMgr member." }
    $lineEnd = $text.IndexOf([Environment]::NewLine, $idx)
    if ($lineEnd -lt 0) { $lineEnd = $text.Length }
    $text = $text.Insert($lineEnd, [Environment]::NewLine + "    bool                    m_bClientFXReady;")
    Write-Source $clientH $text
}
Write-Host "[OK] ClientFX readiness state"

$clientCpp = Join-Path $sealRoot "cshell\src\ltclientshell.cpp"
$text = Read-Source $clientCpp

if (-not $text.Contains('#include <fireteamtrace.h>')) {
    $needle = '#include "ltclientshell.h"'
    if (-not $text.Contains($needle)) { throw "Could not locate client shell include anchor." }
    $text = $text.Replace(
        $needle,
        $needle + [Environment]::NewLine +
        '#include <fireteamtrace.h>' + [Environment]::NewLine +
        '#include <ltcrashhandler.h>')
}

if (-not $text.Contains("m_bClientFXReady(false)")) {
    $needle = "m_bInWorld(false),"
    if (-not $text.Contains($needle)) { throw "Could not locate client constructor ClientFX insertion point." }
    $text = $text.Replace($needle, $needle + [Environment]::NewLine + "m_bClientFXReady(false),")
}

# Canonicalize the entire ClientFX init section. This also gives +disableclientfx 1
# as a guaranteed launch path while the source-built plugin is being ported.
$initStartMarker = "    // Initialize Our ClientFX Database"
$initEndMarker = "    //Register console programs"
$initStart = $text.IndexOf($initStartMarker)
$initEnd = $text.IndexOf($initEndMarker, $initStart)
if ($initStart -lt 0 -or $initEnd -lt 0) {
    throw "Could not locate ClientFX init section."
}

$clientFxInit = @"
    // Fireteam ClientFX bring-up.
    FTTraceLog("CLIENT", "06 ClientFX section begin");

    HCONSOLEVAR hDisableClientFX = g_pLTClient->GetConsoleVar("disableclientfx");
    bool bDisableClientFX = hDisableClientFX &&
        (g_pLTClient->GetVarValueFloat(hDisableClientFX) != 0.0f);

    m_bClientFXReady = false;

    if(bDisableClientFX)
    {
        FTTraceLog("CLIENT", "06A ClientFX DISABLED by +disableclientfx 1");
    }
    else
    {
        FTTraceLog("CLIENT", "06A CClientFXDB::Init begin");
        if(!CClientFXDB::GetSingleton().Init(g_pLTClient))
        {
            FTTraceLog("CLIENT", "06X CClientFXDB::Init returned false");
            g_pLTClient->ShutdownWithMessage("Could not init ClientFXDB!");
            return LT_ERROR;
        }
        FTTraceLog("CLIENT", "06B CClientFXDB::Init success");

        FTTraceLog("CLIENT", "06C CClientFXMgr::Init begin");
        if(!m_ClientFXMgr.Init(g_pLTClient))
        {
            FTTraceLog("CLIENT", "06X CClientFXMgr::Init returned false");
            g_pLTClient->ShutdownWithMessage("Could not init ClientFXMgr!");
            return LT_ERROR;
        }
        FTTraceLog("CLIENT", "06D CClientFXMgr::Init success");

        m_ClientFXMgr.SetCamera(m_hCamera);
        m_bClientFXReady = true;
        FTTraceLog("CLIENT", "06E ClientFX ready");
    }

"@
$text = $text.Substring(0, $initStart) + $clientFxInit + $text.Substring($initEnd)

# Gate every post-init ClientFX use so safe mode is actually safe.
$text = $text.Replace(
    "m_ClientFXMgr.ShutdownAllFX();",
    "if(m_bClientFXReady) m_ClientFXMgr.ShutdownAllFX();")
$text = $text.Replace(
    "m_ClientFXMgr.UpdateAllActiveFX( m_bRender );",
    "if(m_bClientFXReady) m_ClientFXMgr.UpdateAllActiveFX( m_bRender );")
$text = $text.Replace(
    "m_ClientFXMgr.RenderAllActiveFX( m_bRender );",
    "if(m_bClientFXReady) m_ClientFXMgr.RenderAllActiveFX( m_bRender );")
$text = $text.Replace(
    "m_ClientFXMgr.OnRendererShutdown();",
    "if(m_bClientFXReady) m_ClientFXMgr.OnRendererShutdown();")
$text = $text.Replace(
    "m_ClientFXMgr.OnSpecialEffectNotify( hObj, pMessage );",
    "if(m_bClientFXReady) m_ClientFXMgr.OnSpecialEffectNotify( hObj, pMessage );")
$text = $text.Replace(
    "m_ClientFXMgr.OnObjectRemove( hObj );",
    "if(m_bClientFXReady) m_ClientFXMgr.OnObjectRemove( hObj );")
$text = $text.Replace(
    "m_ClientFXMgr.SetCamera(NULL);",
    "if(m_bClientFXReady) m_ClientFXMgr.SetCamera(NULL);")

# Add startup trace and minidump handler. Reset only once per process launch.
if (-not $text.Contains("FTDIAG OnEngineInitialized begin")) {
    $sig = "LTRESULT CLTClientShell::OnEngineInitialized(RMode *pMode, LTGUID *pAppGuid)"
    $func = $text.IndexOf($sig)
    $brace = $text.IndexOf("{", $func)
    if ($func -lt 0 -or $brace -lt 0) { throw "Could not locate OnEngineInitialized body." }
    $insert = [Environment]::NewLine +
        "    // FTDIAG OnEngineInitialized begin" + [Environment]::NewLine +
        "    FTTraceReset();" + [Environment]::NewLine +
        "    static LTCrashHandler s_FireteamCrashHandler(true);" + [Environment]::NewLine +
        '    FTTraceLog("CLIENT", "00 OnEngineInitialized begin");'
    $text = $text.Insert($brace + 1, $insert)
}

# Startup checkpoints around the pieces that can fail before Cabin Fever starts.
$checkpoints = @(
    @("result = VerifyClientInterfaces();", 'FTTraceLog("CLIENT", "01 VerifyClientInterfaces returned %d", (int)result);'),
    @("result = InitRenderer(pMode);", 'FTTraceLog("CLIENT", "02 InitRenderer returned %d", (int)result);'),
    @("result = InitSound();", 'FTTraceLog("CLIENT", "03 InitSound returned %d", (int)result);'),
    @("CreateWorldPropObject();", 'FTTraceLog("CLIENT", "04 CreateWorldPropObject complete");'),
    @("result = this->CreateCamera();", 'FTTraceLog("CLIENT", "05 CreateCamera returned %d", (int)result);')
)
foreach($pair in $checkpoints) {
    $needle = $pair[0]
    $logLine = $pair[1]
    if (-not $text.Contains($logLine)) {
        $idx = $text.IndexOf($needle)
        if ($idx -lt 0) { throw "Could not locate diagnostics checkpoint: $needle" }
        $lineEnd = $text.IndexOf([Environment]::NewLine, $idx)
        if ($lineEnd -lt 0) { $lineEnd = $text.Length }
        $text = $text.Insert($lineEnd, [Environment]::NewLine + "    " + $logLine)
    }
}

# Lifecycle diagnostics.
if (-not $text.Contains('FTTraceLog("CLIENT", "20 OnEnterWorld begin")')) {
    $sig = "void CLTClientShell::OnEnterWorld()"
    $func = $text.IndexOf($sig)
    $brace = $text.IndexOf("{", $func)
    if ($func -ge 0 -and $brace -ge 0) {
        $text = $text.Insert($brace + 1, [Environment]::NewLine + '    FTTraceLog("CLIENT", "20 OnEnterWorld begin");')
        $endNeedle = "    m_bInWorld = true;"
        $endIdx = $text.IndexOf($endNeedle, $brace)
        if($endIdx -ge 0) {
            $lineEnd = $text.IndexOf([Environment]::NewLine, $endIdx)
            $text = $text.Insert($lineEnd, [Environment]::NewLine + '    FTTraceLog("CLIENT", "21 OnEnterWorld complete");')
        }
    }
}

if (-not $text.Contains('FTTraceLog("CLIENT", "30 StartNormalGame begin")')) {
    $sig = "LTRESULT CLTClientShell::StartNormalGame()"
    $func = $text.IndexOf($sig)
    if($func -ge 0) {
        $brace = $text.IndexOf("{", $func)
        $text = $text.Insert($brace + 1, [Environment]::NewLine + '    FTTraceLog("CLIENT", "30 StartNormalGame begin");')
    }
}

Write-Source $clientCpp $text
Write-Host "[OK] client startup tracing, minidumps and ClientFX safe mode"

# ---------------------------------------------------------------------------
# Deep ClientFXDB trace. This is the likely current crash boundary.
# ---------------------------------------------------------------------------
$fxDb = Join-Path $engineRoot "clientfx\Shared\ClientFXDB.cpp"
$text = Read-Source $fxDb

if (-not $text.Contains('#include <fireteamtrace.h>')) {
    $firstInclude = $text.IndexOf("#include")
    if($firstInclude -lt 0) { throw "Could not locate ClientFXDB include block." }
    $text = $text.Insert($firstInclude, '#include <fireteamtrace.h>' + [Environment]::NewLine)
}

# Init checkpoints.
if (-not $text.Contains('FTTraceLog("CFXDB", "Init begin")')) {
    $sig = "bool CClientFXDB::Init(ILTClient* pLTClient)"
    $func = $text.IndexOf($sig)
    $brace = $text.IndexOf("{", $func)
    if($func -lt 0 -or $brace -lt 0) { throw "Could not locate ClientFXDB::Init." }
    $text = $text.Insert($brace + 1, [Environment]::NewLine + '    FTTraceLog("CFXDB", "Init begin");')
}

if (-not $text.Contains('FTTraceLog("CFXDB", "LoadFxDll begin")')) {
    $needle = "if( !LoadFxDll() )"
    if(-not $text.Contains($needle)){ throw "Could not locate LoadFxDll call." }
    $text = $text.Replace(
        $needle,
        'FTTraceLog("CFXDB", "LoadFxDll begin");' + [Environment]::NewLine +
        "    " + $needle)
}

if (-not $text.Contains('FTTraceLog("CFXDB", "LoadFxDll success; enumerating ClientFX directory")')) {
    $needle = 'FileEntry *pFiles = pLTClient->GetFileList("ClientFX");'
    if(-not $text.Contains($needle)){ throw "Could not locate ClientFX file enumeration." }
    $text = $text.Replace(
        $needle,
        'FTTraceLog("CFXDB", "LoadFxDll success; enumerating ClientFX directory");' + [Environment]::NewLine +
        "    " + $needle)
}

if (-not $text.Contains('FTTraceLog("CFXDB", "Loading FXF: %s", pEntry->m_pFullFilename)')) {
    $needle = "if( !LoadFxGroups( pLTClient, pEntry->m_pFullFilename ) )"
    if(-not $text.Contains($needle)){ throw "Could not locate LoadFxGroups call." }
    $text = $text.Replace(
        $needle,
        'FTTraceLog("CFXDB", "Loading FXF: %s", pEntry->m_pFullFilename);' + [Environment]::NewLine +
        "                " + $needle)
}

# DLL load checkpoints.
if (-not $text.Contains('FTTraceLog("CFXDB", "LoadFxDll entered")')) {
    $sig = "bool CClientFXDB::LoadFxDll()"
    $func = $text.IndexOf($sig)
    $brace = $text.IndexOf("{", $func)
    if($func -lt 0 -or $brace -lt 0) { throw "Could not locate ClientFXDB::LoadFxDll." }
    $text = $text.Insert($brace + 1, [Environment]::NewLine + '    FTTraceLog("CFXDB", "LoadFxDll entered");')
}

if (-not $text.Contains('FTTraceLog("CFXDB", "LoadLibrary local: %s", sTmp)')) {
    $needle = "m_hDLLInst = ::LoadLibrary(sTmp);"
    if(-not $text.Contains($needle)){ throw "Could not locate local ClientFX LoadLibrary." }
    $text = $text.Replace(
        $needle,
        'FTTraceLog("CFXDB", "LoadLibrary local: %s", sTmp);' + [Environment]::NewLine +
        "        " + $needle + [Environment]::NewLine +
        '        FTTraceLog("CFXDB", "LoadLibrary local result=%p", m_hDLLInst);')
}

if (-not $text.Contains('FTTraceLog("CFXDB", "LoadLibrary temp: %s", sDLLTmpFile)')) {
    $needle = "m_hDLLInst = ::LoadLibrary(sDLLTmpFile);"
    if(-not $text.Contains($needle)){ throw "Could not locate temp ClientFX LoadLibrary." }
    $text = $text.Replace(
        $needle,
        'FTTraceLog("CFXDB", "LoadLibrary temp: %s", sDLLTmpFile);' + [Environment]::NewLine +
        "        " + $needle + [Environment]::NewLine +
        '        FTTraceLog("CFXDB", "LoadLibrary temp result=%p", m_hDLLInst);')
}

if (-not $text.Contains('FTTraceLog("CFXDB", "SetMasterDatabase begin")')) {
    $needle = "pSetMasterFn(GetMasterDatabase());"
    if(-not $text.Contains($needle)){ throw "Could not locate SetMasterDatabase call." }
    $text = $text.Replace(
        $needle,
        'FTTraceLog("CFXDB", "SetMasterDatabase begin");' + [Environment]::NewLine +
        "        " + $needle + [Environment]::NewLine +
        '        FTTraceLog("CFXDB", "SetMasterDatabase complete");')
}

if (-not $text.Contains('FTTraceLog("CFXDB", "fxGetNum begin")')) {
    $needle = "m_nNumEffectTypes = pfnNum();"
    if(-not $text.Contains($needle)){ throw "Could not locate fxGetNum call." }
    $text = $text.Replace(
        $needle,
        'FTTraceLog("CFXDB", "fxGetNum begin");' + [Environment]::NewLine +
        "    " + $needle + [Environment]::NewLine +
        '    FTTraceLog("CFXDB", "fxGetNum returned %u", m_nNumEffectTypes);')
}

if (-not $text.Contains('FTTraceLog("CFXDB", "fxGetRef(%u) begin", nCurrEffect)')) {
    $needle = "m_pEffectTypes[nCurrEffect] = pfnRef(nCurrEffect);"
    if(-not $text.Contains($needle)){ throw "Could not locate fxGetRef call." }
    $text = $text.Replace(
        $needle,
        'FTTraceLog("CFXDB", "fxGetRef(%u) begin", nCurrEffect);' + [Environment]::NewLine +
        "            " + $needle + [Environment]::NewLine +
        '            FTTraceLog("CFXDB", "fxGetRef(%u) = %s", nCurrEffect, m_pEffectTypes[nCurrEffect].m_sName);')
}

Write-Source $fxDb $text
Write-Host "[OK] deep ClientFXDB tracing"

# ---------------------------------------------------------------------------
# Source-built ClientFx.fxd export trace.
# ---------------------------------------------------------------------------
$fxPlugin = Join-Path $engineRoot "clientfx\clientfx.cpp"
$text = Read-Source $fxPlugin

if (-not $text.Contains('#include <fireteamtrace.h>')) {
    $needle = '#include "stdafx.h"'
    if(-not $text.Contains($needle)) { throw "Could not locate ClientFX plugin include anchor." }
    $text = $text.Replace($needle, $needle + [Environment]::NewLine + '#include <fireteamtrace.h>')
}

if (-not $text.Contains('FTTraceLog("CFXPLUGIN", "fxGetNum")')) {
    $needle = "__declspec(dllexport) int fxGetNum()" + [Environment]::NewLine + "{"
    if($text.Contains($needle)) {
        $text = $text.Replace($needle, $needle + [Environment]::NewLine + '    FTTraceLog("CFXPLUGIN", "fxGetNum");')
    }
}

if (-not $text.Contains('FTTraceLog("CFXPLUGIN", "fxGetRef %d", nFx)')) {
    $needle = "__declspec(dllexport) FX_REF fxGetRef(int nFx)" + [Environment]::NewLine + "{"
    if($text.Contains($needle)) {
        $text = $text.Replace($needle, $needle + [Environment]::NewLine + '    FTTraceLog("CFXPLUGIN", "fxGetRef %d", nFx);')
    }
}

if (-not $text.Contains('FTTraceLog("CFXPLUGIN", "fxCreatePropList %d", nFx)')) {
    $needle = "__declspec(dllexport) CBaseFXProps* fxCreatePropList(int nFx)" + [Environment]::NewLine + "{"
    if($text.Contains($needle)) {
        $text = $text.Replace($needle, $needle + [Environment]::NewLine + '    FTTraceLog("CFXPLUGIN", "fxCreatePropList %d", nFx);')
    }
}

Write-Source $fxPlugin $text
Write-Host "[OK] source-built ClientFX plugin export tracing"

Write-Host "[OK] Fireteam diagnostics patch complete."
