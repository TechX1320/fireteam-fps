param(
    [Parameter(Mandatory = $true)]
    [string]$LocalRoot
)

$ErrorActionPreference = "Stop"
$encoding = [System.Text.Encoding]::Default
$sealRoot = Join-Path $LocalRoot "imports\sealhunter"

function Read-Source([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing source file: $Path"
    }
    return [System.IO.File]::ReadAllText($Path, $encoding)
}

function Write-Source([string]$Path, [string]$Text) {
    [System.IO.File]::WriteAllText($Path, $Text, $encoding)
}

function Replace-Required([string]$Path, [string]$Old, [string]$New, [string]$Label) {
    $text = Read-Source $Path
    if ($text.Contains($New)) {
        Write-Host "[OK] $Label already patched"
        return
    }
    if (-not $text.Contains($Old)) {
        throw "Could not locate expected code for $Label in $Path"
    }
    $text = $text.Replace($Old, $New)
    Write-Source $Path $text
    Write-Host "[OK] $Label"
}

function Insert-AfterLineContaining([string]$Path, [string]$Needle, [string[]]$NewLines, [string]$Guard, [string]$Label) {
    $text = Read-Source $Path
    if ($text.Contains($Guard)) {
        Write-Host "[OK] $Label already patched"
        return
    }

    $lines = $text -split "\r\n|\n|\r"
    $index = -1
    for ($i = 0; $i -lt $lines.Length; $i++) {
        if ($lines[$i].Contains($Needle)) {
            $index = $i
            break
        }
    }

    if ($index -lt 0) {
        throw "Could not locate $Needle for $Label in $Path"
    }

    $before = @()
    if ($index -ge 0) {
        $before = $lines[0..$index]
    }

    $after = @()
    if (($index + 1) -lt $lines.Length) {
        $after = $lines[($index + 1)..($lines.Length - 1)]
    }

    $out = @($before + $NewLines + $after)
    Write-Source $Path ([string]::Join([Environment]::NewLine, $out))
    Write-Host "[OK] $Label"
}

$playerH = Join-Path $sealRoot "cshell\src\playerclnt.h"

Insert-AfterLineContaining $playerH "UpdateRotation(float yaw, float pitch, float roll);" @(
    "    void                UpdateWeaponView(bool bFirstPerson);",
    "    bool                AltAttack();"
) "UpdateWeaponView(bool bFirstPerson)" "Bowie public update declaration"

Insert-AfterLineContaining $playerH "UpdateWeaponView(bool bFirstPerson);" @(
    "    bool                AltAttack();"
) "AltAttack();" "Bowie secondary declaration"

Insert-AfterLineContaining $playerH "PlayAttackAnimation(const char* sAnimName, uint8 nTracker);" @(
    "    void                CreateViewWeapon();",
    "    void                PlayViewWeaponAnimation(const char* sAnimName, bool bLooping);",
    "    void                UpdateViewWeaponAnimation();",
    "    void                PlayViewWeaponSound(const char* sFilename);"
) "CreateViewWeapon();" "Bowie private method declarations"

Insert-AfterLineContaining $playerH "UpdateViewWeaponAnimation();" @(
    "    void                PlayViewWeaponSound(const char* sFilename);"
) "PlayViewWeaponSound(const char* sFilename);" "Bowie sound declaration"

Insert-AfterLineContaining $playerH "m_hClubObject;" @(
    "    HLOCALOBJ           m_hViewWeaponObject;",
    "    bool                m_bViewWeaponAction;",
    "    uint32              m_nViewAttackVariant;"
) "m_hViewWeaponObject;" "Bowie member declarations"

$playerCpp = Join-Path $sealRoot "cshell\src\playerclnt.cpp"
$text = Read-Source $playerCpp

if (-not $text.Contains("#include <iltsoundmgr.h>")) {
    $needle = "#include <iltmodel.h>"
    if (-not $text.Contains($needle)) { throw "Could not locate player client includes for sound support." }
    $text = $text.Replace($needle, $needle + [Environment]::NewLine + "#include <iltsoundmgr.h>")
}

# Migrate previously staged Bowie texture paths to the bare names embedded in the LTBs.
$text = $text.Replace("ModelTextures\\Weapons\\Bowie\\PV_ML_DF_BOWIEKNIFE_BC.DTX", "PV_ML_DF_BowieKnife_BC.dtx")

if (-not $text.Contains("m_hViewWeaponObject(NULL)")) {
    $needle = "m_hClubObject(NULL),"
    if (-not $text.Contains($needle)) {
        throw "Could not locate CPlayerClnt constructor club initializer."
    }
    $text = $text.Replace($needle,
        $needle + [Environment]::NewLine +
        "m_hViewWeaponObject(NULL)," + [Environment]::NewLine +
        "m_bViewWeaponAction(false)," + [Environment]::NewLine +
        "m_nViewAttackVariant(0),")
}

if (-not $text.Contains("RemoveObject(m_hViewWeaponObject)")) {
    $start = $text.IndexOf("CPlayerClnt::~CPlayerClnt()")
    if ($start -lt 0) {
        throw "Could not locate CPlayerClnt destructor."
    }
    $open = $text.IndexOf("{", $start)
    $close = $text.IndexOf("}", $open)
    if ($open -lt 0 -or $close -lt 0) {
        throw "Could not locate CPlayerClnt destructor body."
    }

    $body = "{" + [Environment]::NewLine +
        "    if(m_hViewWeaponObject)" + [Environment]::NewLine +
        "    {" + [Environment]::NewLine +
        "        g_pLTClient->RemoveObject(m_hViewWeaponObject);" + [Environment]::NewLine +
        "        m_hViewWeaponObject = NULL;" + [Environment]::NewLine +
        "    }" + [Environment]::NewLine +
        "}"
    $text = $text.Substring(0, $open) + $body + $text.Substring($close + 1)
}

if (-not $text.Contains("CreateViewWeapon();")) {
    $needle = "    m_hObject = g_pLTClient->CreateObject(&objCreateStruct);"
    if (-not $text.Contains($needle)) {
        throw "Could not locate local player object creation."
    }
    $text = $text.Replace($needle, $needle + [Environment]::NewLine + [Environment]::NewLine + "    CreateViewWeapon();")
}

if (-not $text.Contains("m_nViewAttackVariant++")) {
    $needle = "    m_bAttacking = true;"
    if (-not $text.Contains($needle)) {
        throw "Could not locate player attack state."
    }
    $replacement = $needle + [Environment]::NewLine +
        '    PlayViewWeaponAnimation((m_nViewAttackVariant++ & 1) ? "fire_1" : "fire_0", false);' + [Environment]::NewLine +
        "    m_bViewWeaponAction = true;"
    $text = $text.Replace($needle, $replacement)
}


# Left mouse is primary fire_0. Right mouse gets a dedicated fire_1 path.
$text = $text.Replace(
    '    PlayViewWeaponAnimation((m_nViewAttackVariant++ & 1) ? "fire_1" : "fire_0", false);',
    '    PlayViewWeaponAnimation("fire_0", false);' + [Environment]::NewLine + '    PlayViewWeaponSound("FIRE.WAV");'
)


# Existing local workspaces may already have the Bowie viewmodel implementation.
# Add the new sound helper/select sound without forcing a clean setup.
if ($text.Contains('PlayViewWeaponAnimation("select", false);') -and
    -not $text.Contains('PlayViewWeaponSound("SELECT.WAV");')) {
    $text = $text.Replace(
        '    PlayViewWeaponAnimation("select", false);',
        '    PlayViewWeaponAnimation("select", false);' + [Environment]::NewLine +
        '    PlayViewWeaponSound("SELECT.WAV");')
}

if (-not $text.Contains("void CPlayerClnt::PlayViewWeaponSound(const char* sFilename)")) {
    $needle = "void CPlayerClnt::UpdateViewWeaponAnimation()"
    if ($text.Contains($needle)) {
        $soundHelper = @"
void CPlayerClnt::PlayViewWeaponSound(const char* sFilename)
{
    if(!sFilename || !sFilename[0])
    {
        return;
    }

    PlaySoundInfo psi;
    PLAYSOUNDINFO_INIT(psi);
    psi.m_dwFlags = PLAYSOUND_LOCAL;
    sprintf(psi.m_szSoundName, "Sounds\\Weapons\\Bowie\\%s", sFilename);

    HLTSOUND hSound = NULL;
    g_pLTCSoundMgr->PlaySound(&psi, hSound);
}

"@
        $text = $text.Replace($needle, $soundHelper + $needle)
    }
}

if (-not $text.Contains("bool CPlayerClnt::AltAttack()")) {
    $attackMarker = "//----------------------------------------------------------------------------" + [Environment]::NewLine +
        "// void CPlayerClnt::UpdateAttacking()"
    $idx = $text.IndexOf($attackMarker)
    if ($idx -lt 0) { throw "Could not locate UpdateAttacking marker for AltAttack." }

    $altAttack = @"

//----------------------------------------------------------------------------
// bool CPlayerClnt::AltAttack()
//
//----------------------------------------------------------------------------
bool CPlayerClnt::AltAttack()
{
    if(m_bAttacking)
    {
        return false;
    }

    PlayAttackAnimation("UMFi", m_idUpperBodyTracker);
    m_bAttacking = true;

    PlayViewWeaponAnimation("fire_1", false);
    PlayViewWeaponSound("FIRE.WAV");
    m_bViewWeaponAction = true;

    return true;
}


"@
    $text = $text.Insert($idx, $altAttack)
}

if (-not $text.Contains("void CPlayerClnt::CreateViewWeapon()")) {
    $append = @"

//----------------------------------------------------------------------------
// Fireteam FPS Bowie knife player-view model.
//----------------------------------------------------------------------------
void CPlayerClnt::CreateViewWeapon()
{
    if(m_hViewWeaponObject)
    {
        g_pLTClient->RemoveObject(m_hViewWeaponObject);
        m_hViewWeaponObject = NULL;
    }

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;
    ocs.m_Flags = FLAG_VISIBLE | FLAG_REALLYCLOSE;
    ocs.m_Flags2 = FLAG2_DYNAMICDIRLIGHT;
    ocs.m_Pos.Init(0.0f, 0.0f, 0.0f);

    strcpy(ocs.m_Filenames[0], "Models\\Weapons\\Bowie\\CM_HND_NM_DF_BOWIEKNIFE_CH.LTB");
    strcpy(ocs.m_Filenames[1], "Models\\Weapons\\Bowie\\ANI_G_BOWIEKNIFE_CH.LTB");
    strcpy(ocs.m_SkinNames[0], "PV_ML_DF_BowieKnife_BC.dtx");

    m_hViewWeaponObject = g_pLTClient->CreateObject(&ocs);
    if(!m_hViewWeaponObject)
    {
        g_pLTClient->CPrint("Fireteam FPS: Bowie PV model failed to load.");
        return;
    }

    PlayViewWeaponAnimation("select", false);
    PlayViewWeaponSound("SELECT.WAV");
    m_bViewWeaponAction = true;
}

void CPlayerClnt::PlayViewWeaponAnimation(const char* sAnimName, bool bLooping)
{
    if(!m_hViewWeaponObject || !sAnimName)
    {
        return;
    }

    HMODELANIM hAnim = g_pLTClient->GetAnimIndex(m_hViewWeaponObject, (char*)sAnimName);
    if(hAnim == INVALID_MODEL_ANIM)
    {
        g_pLTClient->CPrint("Fireteam FPS: Bowie animation missing: %s", sAnimName);
        return;
    }

    g_pLTCModel->SetCurAnim(m_hViewWeaponObject, MAIN_TRACKER, hAnim);
    g_pLTCModel->SetLooping(m_hViewWeaponObject, MAIN_TRACKER, bLooping ? LTTRUE : LTFALSE);
}

void CPlayerClnt::PlayViewWeaponSound(const char* sFilename)
{
    if(!sFilename || !sFilename[0])
    {
        return;
    }

    PlaySoundInfo psi;
    PLAYSOUNDINFO_INIT(psi);
    psi.m_dwFlags = PLAYSOUND_LOCAL;
    sprintf(psi.m_szSoundName, "Sounds\\Weapons\\Bowie\\%s", sFilename);

    HLTSOUND hSound = NULL;
    g_pLTCSoundMgr->PlaySound(&psi, hSound);
}

void CPlayerClnt::UpdateViewWeaponAnimation()
{
    if(!m_hViewWeaponObject || !m_bViewWeaponAction)
    {
        return;
    }

    uint32 nAnimLen = 0;
    uint32 nAnimTime = 0;
    g_pLTCModel->GetCurAnimLength(m_hViewWeaponObject, MAIN_TRACKER, nAnimLen);
    g_pLTCModel->GetCurAnimTime(m_hViewWeaponObject, MAIN_TRACKER, nAnimTime);

    if(nAnimLen > 0 && nAnimTime >= nAnimLen)
    {
        PlayViewWeaponAnimation("idle_0", true);
        m_bViewWeaponAction = false;
    }
}

void CPlayerClnt::UpdateWeaponView(bool bFirstPerson)
{
    if(m_hViewWeaponObject)
    {
        g_pLTCCommon->SetObjectFlags(
            m_hViewWeaponObject,
            OFT_Flags,
            bFirstPerson ? FLAG_VISIBLE : 0,
            FLAG_VISIBLE);

        if(bFirstPerson)
        {
            UpdateViewWeaponAnimation();
        }
    }

    if(m_hClubObject)
    {
        g_pLTCCommon->SetObjectFlags(
            m_hClubObject,
            OFT_Flags,
            bFirstPerson ? 0 : FLAG_VISIBLE,
            FLAG_VISIBLE);
    }
}
"@
    $text += $append
}

Write-Source $playerCpp $text
Write-Host "[OK] Bowie player-view implementation"

$clientShell = Join-Path $sealRoot "cshell\src\ltclientshell.cpp"
$text = Read-Source $clientShell
if (-not $text.Contains("UpdateWeaponView(m_pCamera->IsFirstPerson())")) {
    $needle = "            m_pCamera->UpdatePosition(m_pPlayer->GetPlayerObject());"
    if (-not $text.Contains($needle)) {
        throw "Could not locate camera update for Bowie visibility hook."
    }
    $text = $text.Replace($needle, $needle + [Environment]::NewLine + "            m_pPlayer->UpdateWeaponView(m_pCamera->IsFirstPerson());")
    Write-Source $clientShell $text
}
Write-Host "[OK] Bowie first/third-person visibility hook"

$serverPlayer = Join-Path $sealRoot "sshell\src\playersrvr.cpp"
$serverText = Read-Source $serverPlayer

# All three random SealHunter melee choices become the same Bowie world model.
# Use direct replacements because Replace-Required cannot distinguish three old
# strings that intentionally share one new destination.
$serverText = $serverText.Replace('"Models/Mallet.ltb"', '"Models/Weapons/Bowie/HH_ML_DF_BOWIEKNIFE_CH.LTB"')
$serverText = $serverText.Replace('"Models/TelePole.ltb"', '"Models/Weapons/Bowie/HH_ML_DF_BOWIEKNIFE_CH.LTB"')
$serverText = $serverText.Replace('"Models/billyclub.ltb"', '"Models/Weapons/Bowie/HH_ML_DF_BOWIEKNIFE_CH.LTB"')

$serverText = $serverText.Replace('"ModelTextures/Mallet.dtx"', '"HH_ML_DF_BowieKnife_BC.dtx"')
$serverText = $serverText.Replace('"ModelTextures/TelePole.dtx"', '"HH_ML_DF_BowieKnife_BC.dtx"')
$serverText = $serverText.Replace('"ModelTextures/club.dtx"', '"HH_ML_DF_BowieKnife_BC.dtx"')
$serverText = $serverText.Replace('"ModelTextures/Weapons/Bowie/HH_ML_DF_BOWIEKNIFE_BC.DTX"', '"HH_ML_DF_BowieKnife_BC.dtx"')

Write-Source $serverPlayer $serverText
Write-Host "[OK] all SealHunter melee world models migrated to Bowie"

$commandIds = Join-Path $sealRoot "shared\src\commandids.h"
$text = Read-Source $commandIds
if (-not $text.Contains("COMMAND_ALT_ATTACK")) {
    $needle = "    COMMAND_CHAT                    = 19,"
    if (-not $text.Contains($needle)) { throw "Could not locate command id insertion point." }
    $text = $text.Replace($needle, $needle + [Environment]::NewLine + "    COMMAND_ALT_ATTACK              = 20,")
    Write-Source $commandIds $text
}
Write-Host "[OK] right-click command id"

$clientShell = Join-Path $sealRoot "cshell\src\ltclientshell.cpp"
$text = Read-Source $clientShell
if (-not $text.Contains("m_pPlayer->AltAttack();")) {
    $needle = "// jump"
    $idx = $text.IndexOf($needle)
    if ($idx -lt 0) { throw "Could not locate jump marker for secondary attack input." }

    $insert = "    // Combat Arms knife secondary attack." + [Environment]::NewLine +
        "    if (g_pLTClient->IsCommandOn(COMMAND_ALT_ATTACK))" + [Environment]::NewLine +
        "    {" + [Environment]::NewLine +
        "        m_pPlayer->AltAttack();" + [Environment]::NewLine +
        "    }" + [Environment]::NewLine + [Environment]::NewLine + "    "

    $text = $text.Insert($idx, $insert)
    Write-Source $clientShell $text
}
Write-Host "[OK] right-click fire_1 input"

Write-Host "[OK] Combat Arms Bowie knife patch set complete."
