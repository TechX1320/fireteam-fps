param(
    [Parameter(Mandatory = $true)]
    [string]$LocalRoot
)

$ErrorActionPreference = "Stop"
$encoding = [System.Text.Encoding]::Default

function Read-LegacyFile([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing source file: $Path"
    }
    return [System.IO.File]::ReadAllText($Path, $encoding)
}

function Write-LegacyFile([string]$Path, [string]$Text) {
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

function Replace-Required([string]$Path, [string]$Old, [string]$New, [string]$Label) {
    $text = Read-LegacyFile $Path

    if ($text.Contains($New)) {
        Write-Host "[OK] $Label already patched"
        return
    }

    if (-not $text.Contains($Old)) {
        throw "Could not locate expected legacy code for $Label in $Path"
    }

    $text = $text.Replace($Old, $New)
    Write-LegacyFile $Path $text
    Write-Host "[OK] $Label"
}

$shared = Join-Path $LocalRoot "imports\engine\clientfx\Shared"

Replace-Required (Join-Path $shared "BaseFx.h") "operator == (FX_COLOURKEY k)" "bool operator == (FX_COLOURKEY k)" "BaseFx return type"

$parsedMsg = Join-Path $shared "ParsedMsg.cpp"
$text = Read-LegacyFile $parsedMsg
if (-not $text.Contains("#include <cctype>")) {
    $needle = '#include "ParsedMsg.h"'
    if (-not $text.Contains($needle)) {
        throw "Could not locate ParsedMsg include insertion point."
    }
    $text = $text.Replace($needle, $needle + [Environment]::NewLine + "#include <cctype>")
    Write-LegacyFile $parsedMsg $text
}
Write-Host "[OK] ParsedMsg cctype include"

$oldDb = "for (uint32 k = 0; k < dwNumProps; k ++)"
$newDb = "uint32 k = 0;" + [Environment]::NewLine + [char]9 + "for (; k < dwNumProps; k ++)"
Replace-Required (Join-Path $shared "ClientFXDB.cpp") $oldDb $newDb "ClientFXDB loop scope"

$oldMgr = "for(float fCurrTime = 0.0f; fCurrTime + fTimeInc <= fOffsetAmount; fCurrTime += fTimeInc)"
$newMgr = "float fCurrTime = 0.0f;" + [Environment]::NewLine + [char]9 + "for(; fCurrTime + fTimeInc <= fOffsetAmount; fCurrTime += fTimeInc)"
Replace-Required (Join-Path $shared "ClientFXMgr.cpp") $oldMgr $newMgr "ClientFXMgr loop scope"


$clientFxRoot = Join-Path $LocalRoot "imports\engine\clientfx"
$clientFxStdafx = Join-Path $clientFxRoot "stdafx.h"
if (Test-Path -LiteralPath $clientFxStdafx) {
    $text = Read-LegacyFile $clientFxStdafx
    if ($text.Contains('#include "mfcstub.h"')) {
        $text = $text.Replace(
            '#include "mfcstub.h"',
            '#include <ltassert.h>' + [Environment]::NewLine + '#include <string.h>' + [Environment]::NewLine + '#include <stdlib.h>')
        Write-LegacyFile $clientFxStdafx $text
        Write-Host "[OK] ClientFX stdafx MFCStub removal"
    } elseif ($text.Contains("#include <ltassert.h>")) {
        Write-Host "[OK] ClientFX stdafx MFCStub removal already patched"
    } else {
        throw "Could not locate ClientFX MFCStub include."
    }
}

Write-Host "[OK] Legacy Jupiter compatibility patch set complete."
