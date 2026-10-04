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



# Combat Arms LTBSystemFX compatibility.
# CA's FXF database contains an effect type that is not present in the stock
# Jupiter 2006 plugin. Stock ClientFXDB dereferences the result of FindFX()
# without checking for NULL, causing an immediate startup crash while parsing
# CA CLIENTFX.FXF. For bring-up, map LTBSystemFX to Jupiter's LTBModel effect.
$fxDbPath = Join-Path $shared "ClientFXDB.cpp"
$text = Read-LegacyFile $fxDbPath

if (-not $text.Contains("Combat Arms LTBSystemFX compatibility")) {
    # Match the assignment structurally instead of depending on the exact
    # whitespace used by a particular Jupiter source snapshot.
    $findFxPattern = '(?m)^[ \t]*pKey->m_pFxRef\s*=\s*FindFX\s*\(\s*strtok\s*\(\s*sTmp\s*,\s*";"\s*\)\s*\)\s*;'
    $findFxMatch = [regex]::Match($text, $findFxPattern)

    if (-not $findFxMatch.Success) {
        throw "Could not locate ClientFX FindFX assignment inside ReadFXKey()."
    }

    $newFindFx = @'
	char *pFxTypeName = strtok(sTmp, ";");
	pKey->m_pFxRef = FindFX(pFxTypeName);

	// Combat Arms LTBSystemFX compatibility.
	// The later CA ClientFX database adds LTBSystemFX, which is not part of
	// stock Jupiter Build 68. Use LTBModel as the first compatibility layer:
	// it preserves the model-based visual instead of crashing on a NULL FX_REF.
	if(!pKey->m_pFxRef && pFxTypeName && !_stricmp(pFxTypeName, "LTBSystemFX"))
	{
		FTTraceLog("CFXDB", "Compatibility map: LTBSystemFX -> LTBModel");
		pKey->m_pFxRef = FindFX("LTBModel");
	}

	if(!pKey->m_pFxRef)
	{
		FTTraceLog("CFXDB", "Unsupported ClientFX type: %s", pFxTypeName ? pFxTypeName : "<null>");
		return false;
	}
'@

    $text = $text.Substring(0, $findFxMatch.Index) +
        $newFindFx +
        $text.Substring($findFxMatch.Index + $findFxMatch.Length)

    Write-LegacyFile $fxDbPath $text
    Write-Host "[OK] Combat Arms LTBSystemFX compatibility"
} else {
    Write-Host "[OK] Combat Arms LTBSystemFX compatibility already patched"
}

# CA LTBSystemFX uses singular Skin/RenderStyle property names while Jupiter's
# LTBModel effect expects Skin0/RenderStyle0. Accept both forms.
$ltbModelPath = Join-Path $LocalRoot "imports\engine\clientfx\ltbmodelfx.cpp"
$text = Read-LegacyFile $ltbModelPath

if (-not $text.Contains('Combat Arms singular Skin alias')) {
    $skinNeedle = 'if( !_stricmp( fxProp.m_sName, "Skin0" ))'
    if (-not $text.Contains($skinNeedle)) {
        throw "Could not locate LTBModel Skin0 property parser."
    }

    $skinAlias = @'
if( !_stricmp( fxProp.m_sName, "Skin" ))
		{
			// Combat Arms singular Skin alias.
			fxProp.GetPath( m_szSkinName[0] );
		}
		else 
'@
    $text = $text.Replace($skinNeedle, $skinAlias + $skinNeedle)
}

if (-not $text.Contains('Combat Arms singular RenderStyle alias')) {
    $rsNeedle = 'else if( !_stricmp( fxProp.m_sName, "RenderStyle0" ))'
    if (-not $text.Contains($rsNeedle)) {
        throw "Could not locate LTBModel RenderStyle0 property parser."
    }

    $rsAlias = @'
else if( !_stricmp( fxProp.m_sName, "RenderStyle" ))
		{
			// Combat Arms singular RenderStyle alias.
			fxProp.GetPath( m_szRenderStyle[0] );
		}
		
'@
    $text = $text.Replace($rsNeedle, $rsAlias + $rsNeedle)
}

Write-LegacyFile $ltbModelPath $text
Write-Host "[OK] Combat Arms LTBModel property aliases"

Write-Host "[OK] Legacy Jupiter compatibility patch set complete."
