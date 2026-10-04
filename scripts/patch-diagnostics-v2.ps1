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
    if (-not (Test-Path -LiteralPath $Path)) { throw "Missing source file: $Path" }
    return [System.IO.File]::ReadAllText($Path, $encoding)
}

function Write-Source([string]$Path, [string]$Text) {
    $dir = Split-Path -Parent $Path
    if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

$traceHeader = @'
#ifndef __FIRETEAM_TRACE_H__
#define __FIRETEAM_TRACE_H__
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

inline void FTTraceReset() { DeleteFileA("fireteam-trace.log"); }

inline void FTTraceLog(const char* module, const char* format, ...)
{
    char message[2048];
    message[0] = 0;

    va_list args;
    va_start(args, format);
    _vsnprintf(message, sizeof(message) - 1, format, args);
    va_end(args);
    message[sizeof(message) - 1] = 0;

    FILE* fp = fopen("fireteam-trace.log", "a");
    if(fp)
    {
        fprintf(fp, "%010lu [%s] %s\n",
            (unsigned long)GetTickCount(),
            module ? module : "?",
            message);
        fflush(fp);
        fclose(fp);
    }
}
#endif
'@
Write-Source (Join-Path $sdkRoot "fireteamtrace.h") $traceHeader

$clientH = Join-Path $sealRoot "cshell\src\ltclientshell.h"
$text = Read-Source $clientH
if (-not $text.Contains("m_bClientFXReady")) {
    $needle = "    CClientFXMgr"
    $idx = $text.IndexOf($needle)
    if ($idx -lt 0) { throw "Could not locate ClientFXMgr member." }
    $lineEnd = $text.IndexOf([Environment]::NewLine, $idx)
    $text = $text.Insert($lineEnd, [Environment]::NewLine + "    bool                    m_bClientFXReady;")
    Write-Source $clientH $text
}

$clientCpp = Join-Path $sealRoot "cshell\src\ltclientshell.cpp"
$text = Read-Source $clientCpp

if (-not $text.Contains('#include <fireteamtrace.h>')) {
    $needle = '#include "ltclientshell.h"'
    $text = $text.Replace(
        $needle,
        $needle + [Environment]::NewLine +
        '#include <fireteamtrace.h>' + [Environment]::NewLine +
        '#include <time.h>' + [Environment]::NewLine +
        '#include <ltcrashhandler.h>')
}

if (-not $text.Contains("m_bClientFXReady(false)")) {
    $needle = "m_bInWorld(false),"
    $text = $text.Replace($needle, $needle + [Environment]::NewLine + "m_bClientFXReady(false),")
}

if (-not $text.Contains("// Fireteam ClientFX bring-up.")) {
    $startMarker = "    // Initialize Our ClientFX Database"
    $endMarker = "    //Register console programs"
    $start = $text.IndexOf($startMarker)
    $end = $text.IndexOf($endMarker, $start)
    if ($start -lt 0 -or $end -lt 0) { throw "Could not locate ClientFX init block." }

    $block = @"
    // Fireteam ClientFX bring-up.
    FTTraceLog("CLIENT", "06 ClientFX section begin");

    HCONSOLEVAR hDisableClientFX = g_pLTClient->GetConsoleVar("disableclientfx");
    bool bDisableClientFX = hDisableClientFX &&
        (g_pLTClient->GetVarValueFloat(hDisableClientFX) != 0.0f);

    m_bClientFXReady = false;

    if(bDisableClientFX)
    {
        FTTraceLog("CLIENT", "06A ClientFX disabled by launch option");
    }
    else
    {
        FTTraceLog("CLIENT", "06A ClientFXDB Init begin");
        if(!CClientFXDB::GetSingleton().Init(g_pLTClient))
        {
            FTTraceLog("CLIENT", "06X ClientFXDB Init failed");
            g_pLTClient->ShutdownWithMessage("Could not init ClientFXDB!");
            return LT_ERROR;
        }
        FTTraceLog("CLIENT", "06B ClientFXDB Init success");

        FTTraceLog("CLIENT", "06C ClientFXMgr Init begin");
        if(!m_ClientFXMgr.Init(g_pLTClient))
        {
            FTTraceLog("CLIENT", "06X ClientFXMgr Init failed");
            g_pLTClient->ShutdownWithMessage("Could not init ClientFXMgr!");
            return LT_ERROR;
        }

        m_ClientFXMgr.SetCamera(m_hCamera);
        m_bClientFXReady = true;
        FTTraceLog("CLIENT", "06D ClientFX ready");
    }

"@
    $text = $text.Substring(0, $start) + $block + $text.Substring($end)
}

$gates = @(
    @("m_ClientFXMgr.ShutdownAllFX();", "if(m_bClientFXReady) m_ClientFXMgr.ShutdownAllFX();"),
    @("m_ClientFXMgr.UpdateAllActiveFX( m_bRender );", "if(m_bClientFXReady) m_ClientFXMgr.UpdateAllActiveFX( m_bRender );"),
    @("m_ClientFXMgr.RenderAllActiveFX( m_bRender );", "if(m_bClientFXReady) m_ClientFXMgr.RenderAllActiveFX( m_bRender );"),
    @("m_ClientFXMgr.OnRendererShutdown();", "if(m_bClientFXReady) m_ClientFXMgr.OnRendererShutdown();"),
    @("m_ClientFXMgr.OnSpecialEffectNotify( hObj, pMessage );", "if(m_bClientFXReady) m_ClientFXMgr.OnSpecialEffectNotify( hObj, pMessage );"),
    @("m_ClientFXMgr.OnObjectRemove( hObj );", "if(m_bClientFXReady) m_ClientFXMgr.OnObjectRemove( hObj );"),
    @("m_ClientFXMgr.SetCamera(NULL);", "if(m_bClientFXReady) m_ClientFXMgr.SetCamera(NULL);")
)
foreach($gate in $gates) {
    if (-not $text.Contains($gate[1]) -and $text.Contains($gate[0])) {
        $text = $text.Replace($gate[0], $gate[1])
    }
}

if (-not $text.Contains("FTDIAG OnEngineInitialized begin")) {
    $sig = "LTRESULT CLTClientShell::OnEngineInitialized(RMode *pMode, LTGUID *pAppGuid)"
    $func = $text.IndexOf($sig)
    $brace = $text.IndexOf("{", $func)
    $insert = [Environment]::NewLine +
        "    // FTDIAG OnEngineInitialized begin" + [Environment]::NewLine +
        "    FTTraceReset();" + [Environment]::NewLine +
        "    static LTCrashHandler s_FireteamCrashHandler(true);" + [Environment]::NewLine +
        '    FTTraceLog("CLIENT", "00 OnEngineInitialized begin");'
    $text = $text.Insert($brace + 1, $insert)
}

$checks = @(
    @("result = VerifyClientInterfaces();", 'FTTraceLog("CLIENT", "01 VerifyClientInterfaces=%d", (int)result);'),
    @("result = InitRenderer(pMode);", 'FTTraceLog("CLIENT", "02 InitRenderer=%d", (int)result);'),
    @("result = InitSound();", 'FTTraceLog("CLIENT", "03 InitSound=%d", (int)result);'),
    @("CreateWorldPropObject();", 'FTTraceLog("CLIENT", "04 WorldProps created");'),
    @("result = this->CreateCamera();", 'FTTraceLog("CLIENT", "05 CreateCamera=%d", (int)result);')
)
foreach($check in $checks) {
    if (-not $text.Contains($check[1])) {
        $idx = $text.IndexOf($check[0])
        if ($idx -lt 0) { throw "Missing diagnostics checkpoint: $($check[0])" }
        $lineEnd = $text.IndexOf([Environment]::NewLine, $idx)
        $text = $text.Insert($lineEnd, [Environment]::NewLine + "    " + $check[1])
    }
}

Write-Source $clientCpp $text

$fxDb = Join-Path $engineRoot "clientfx\Shared\ClientFXDB.cpp"
$text = Read-Source $fxDb

if (-not $text.Contains('#include <fireteamtrace.h>')) {
    $idx = $text.IndexOf("#include")
    $text = $text.Insert($idx, '#include <fireteamtrace.h>' + [Environment]::NewLine)
}

if (-not $text.Contains('FTTraceLog("CFXDB", "Init begin")')) {
    $sig = "bool CClientFXDB::Init(ILTClient* pLTClient)"
    $func = $text.IndexOf($sig)
    $brace = $text.IndexOf("{", $func)
    $text = $text.Insert($brace + 1, [Environment]::NewLine + '    FTTraceLog("CFXDB", "Init begin");')
}

$pairs = @(
    @("if( !LoadFxDll() )", 'FTTraceLog("CFXDB", "LoadFxDll begin");' + [Environment]::NewLine + "    if( !LoadFxDll() )", 'FTTraceLog("CFXDB", "LoadFxDll begin")'),
    @('FileEntry *pFiles = pLTClient->GetFileList("ClientFX");', 'FTTraceLog("CFXDB", "LoadFxDll success; enumerate FXF");' + [Environment]::NewLine + '    FileEntry *pFiles = pLTClient->GetFileList("ClientFX");', 'FTTraceLog("CFXDB", "LoadFxDll success; enumerate FXF")'),
    @("m_nNumEffectTypes = pfnNum();", 'FTTraceLog("CFXDB", "fxGetNum begin");' + [Environment]::NewLine + "    m_nNumEffectTypes = pfnNum();" + [Environment]::NewLine + '    FTTraceLog("CFXDB", "fxGetNum=%u", m_nNumEffectTypes);', 'FTTraceLog("CFXDB", "fxGetNum begin")'),
    @("m_pEffectTypes[nCurrEffect] = pfnRef(nCurrEffect);", 'FTTraceLog("CFXDB", "fxGetRef(%u) begin", nCurrEffect);' + [Environment]::NewLine + "            m_pEffectTypes[nCurrEffect] = pfnRef(nCurrEffect);" + [Environment]::NewLine + '            FTTraceLog("CFXDB", "fxGetRef(%u)=%s", nCurrEffect, m_pEffectTypes[nCurrEffect].m_sName);', 'FTTraceLog("CFXDB", "fxGetRef(%u) begin", nCurrEffect)'),
    @("pSetMasterFn(GetMasterDatabase());", 'FTTraceLog("CFXDB", "SetMasterDatabase begin");' + [Environment]::NewLine + "        pSetMasterFn(GetMasterDatabase());" + [Environment]::NewLine + '        FTTraceLog("CFXDB", "SetMasterDatabase complete");', 'FTTraceLog("CFXDB", "SetMasterDatabase begin")')
)
foreach($pair in $pairs) {
    if (-not $text.Contains($pair[2])) {
        if (-not $text.Contains($pair[0])) { throw "Missing ClientFXDB trace anchor: $($pair[0])" }
        $text = $text.Replace($pair[0], $pair[1])
    }
}

if (-not $text.Contains('FTTraceLog("CFXDB", "LoadFxDll entered")')) {
    $sig = "bool CClientFXDB::LoadFxDll()"
    $func = $text.IndexOf($sig)
    $brace = $text.IndexOf("{", $func)
    $text = $text.Insert($brace + 1, [Environment]::NewLine + '    FTTraceLog("CFXDB", "LoadFxDll entered");')
}

if (-not $text.Contains('FTTraceLog("CFXDB", "LoadLibrary temp"))') {
    $needle = "m_hDLLInst = ::LoadLibrary(sDLLTmpFile);"
    $replacement = 'FTTraceLog("CFXDB", "LoadLibrary temp: %s", sDLLTmpFile);' +
        [Environment]::NewLine + "        " + $needle +
        [Environment]::NewLine + '        FTTraceLog("CFXDB", "LoadLibrary temp result=%p", m_hDLLInst);'
    $text = $text.Replace($needle, $replacement)
}

Write-Source $fxDb $text

$fxPlugin = Join-Path $engineRoot "clientfx\clientfx.cpp"
$text = Read-Source $fxPlugin
if (-not $text.Contains('#include <fireteamtrace.h>')) {
    $text = $text.Replace('#include "stdafx.h"', '#include "stdafx.h"' + [Environment]::NewLine + '#include <fireteamtrace.h>')
}
if (-not $text.Contains('FTTraceLog("CFXPLUGIN", "fxGetNum")')) {
    $needle = "__declspec(dllexport) int fxGetNum()" + [Environment]::NewLine + "{"
    $text = $text.Replace($needle, $needle + [Environment]::NewLine + '    FTTraceLog("CFXPLUGIN", "fxGetNum");')
}
if (-not $text.Contains('FTTraceLog("CFXPLUGIN", "fxGetRef %d", nFx)')) {
    $needle = "__declspec(dllexport) FX_REF fxGetRef(int nFx)" + [Environment]::NewLine + "{"
    $text = $text.Replace($needle, $needle + [Environment]::NewLine + '    FTTraceLog("CFXPLUGIN", "fxGetRef %d", nFx);')
}
Write-Source $fxPlugin $text

Write-Host "[OK] Fireteam diagnostics v2 patch complete."
