#include "FireteamZombie.h"
#include "FireteamNavigation.h"
#include "FireteamSpawner.h"
#include "FireteamMutationBox.h"
#include "playersrvr.h"
#include "serverinterfaces.h"
#include "msgids.h"
#include "FireteamDifficultyDefs.h"

#include <iltcommon.h>
#include <iltmodel.h>
#include <iltphysics.h>
#include <iltsoundmgr.h>
#include <ltobjectcreate.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

BEGIN_CLASS(FireteamZombie)
END_CLASS_DEFAULT_FLAGS(FireteamZombie, BaseClass, LTNULL, LTNULL, CF_ALWAYSLOAD)

static uint32 s_nZombieSerial = 0;
static bool s_bDifficultyLoaded = false;
static bool s_bAnimationAuditComplete = false;
static FTDifficultyDef s_ZombieDifficulty;

// Snapshot neighbor positions once per short interval; do not enumerate
// every world object separately for every zombie's steering decision.
struct FTZombieNeighborSample
{
    HOBJECT hObject;
    LTVector vPos;
};
static std::vector<FTZombieNeighborSample> s_aZombieNeighbors;
static float s_fLastNeighborSnapshot = -1.0f;

void FT_ResetZombieNeighborCache()
{
    s_aZombieNeighbors.clear();
    s_fLastNeighborSnapshot = -1.0f;
}

static const std::vector<FTZombieNeighborSample>& FT_GetZombieNeighborSnapshot()
{
    const float fNow = g_pLTServer->GetTime();
    if(s_fLastNeighborSnapshot >= 0.0f &&
       fNow >= s_fLastNeighborSnapshot &&
       fNow - s_fLastNeighborSnapshot < 0.075f)
        return s_aZombieNeighbors;

    s_aZombieNeighbors.clear();
    s_fLastNeighborSnapshot = fNow;
    HCLASS hZombieClass = g_pLTServer->GetClass("FireteamZombie");
    if(!hZombieClass)
        return s_aZombieNeighbors;

    for(HOBJECT hObj = g_pLTServer->GetNextObject(LTNULL);
        hObj; hObj = g_pLTServer->GetNextObject(hObj))
    {
        HCLASS hType = g_pLTServer->GetObjectClass(hObj);
        if(!hType || !g_pLTServer->IsKindOf(hType, hZombieClass))
            continue;

        FireteamZombie *pZombie =
            (FireteamZombie*)g_pLTServer->HandleToObject(hObj);
        if(!pZombie || !pZombie->IsAliveForRound())
            continue;

        FTZombieNeighborSample sample;
        sample.hObject = hObj;
        g_pLTServer->GetObjectPos(hObj, &sample.vPos);
        s_aZombieNeighbors.push_back(sample);
    }
    return s_aZombieNeighbors;
}

static float s_fZombieWallhackUntil = 0.0f;
static uint8 s_nZombieWallhackStacks = 0;
static bool s_bZombieWallhackApplied = false;

FireteamZombie::FireteamZombie() :
    m_nHealth(0),
    m_fAttackCooldown(0.0f),
    m_fAttackAnimationTime(0.0f),
    m_fJumpAnimationTime(0.0f),
    m_fRepathCooldown(0.0f),
    m_fStuckTime(0.0f),
    m_fForcePathTime(0.0f),
    m_fNoProgressTime(0.0f),
    m_fBestProgressDistance(FLT_MAX),
    m_fStragglerIdleSeconds(0.0f),
    m_fTargetMemory(0.0f),
    m_fVoiceCooldown(0.0f),
    m_bHasLastKnownTarget(false),
    m_eBehaviorState(kBehaviorSearch),
    m_bDying(false),
    m_fDeathTimeRemaining(0.0f),
    m_fLastServerTick(0.0f),
    m_fDecisionElapsed(0.0f),
    m_nTicksUntilDecision(0),
    m_nMotionStepsRemaining(0),
    m_nPathLane((s_nZombieSerial++) % 5),
    m_nWaypoint(0),
    m_hFace(LTNULL),
    m_hFaceAttachment(LTNULL),
    m_bDefLoaded(false)
{
    m_vLastPos.Init(0.0f, 0.0f, 0.0f);
    m_vLastKnownTargetPos.Init(0.0f, 0.0f, 0.0f);
    m_vProgressTarget.Init(0.0f, 0.0f, 0.0f);
    m_vStragglerProgressPos.Init(0.0f, 0.0f, 0.0f);
    m_vMotionGoal.Init(0.0f, 0.0f, 0.0f);
    m_vCollisionDims.Init(24.0f, 53.0f, 24.0f);
    m_sCurrentAnimation[0] = '\0';

    m_bDefLoaded =
        FT_LoadDefaultInfectedDef(
            "config/infected.cfg",
            m_Def);

    if(m_bDefLoaded)
    {
        if(!s_bDifficultyLoaded)
        {
            if(!FT_LoadActiveDifficulty(
                "config/difficulties.cfg",
                "config/session.cfg",
                s_ZombieDifficulty))
            {
                FT_InitDifficultyDefaults(
                    s_ZombieDifficulty);
            }
            s_bDifficultyLoaded = true;
        }

        float fHealth =
            (float)m_Def.nHealth *
            s_ZombieDifficulty.fHealthMultiplier;

        if(fHealth < 1.0f) fHealth = 1.0f;
        if(fHealth > 65535.0f) fHealth = 65535.0f;

        m_nHealth =
            (uint16)(fHealth + 0.5f);

        m_Def.fRunSpeed *=
            s_ZombieDifficulty.fSpeedMultiplier;

        if(m_Def.fWalkSpeed <= 0.0f)
        {
            m_Def.fWalkSpeed =
                m_Def.fRunSpeed * 0.52f;
        }
        else
        {
            m_Def.fWalkSpeed *=
                s_ZombieDifficulty.fSpeedMultiplier;
        }

        if(m_Def.fAlertDistance <= 0.0f)
        {
            m_Def.fAlertDistance = 300.0f;
        }

        if(m_Def.fTargetMemorySeconds <= 0.0f)
        {
            m_Def.fTargetMemorySeconds = 3.5f;
        }

        float fDamage =
            (float)m_Def.nAttackDamage *
            s_ZombieDifficulty.fDamageMultiplier;

        if(fDamage < 1.0f) fDamage = 1.0f;
        if(fDamage > 255.0f) fDamage = 255.0f;

        m_Def.nAttackDamage =
            (uint8)(fDamage + 0.5f);
    }
}

FireteamZombie::~FireteamZombie()
{
    if(m_hFace)
    {
        g_pLTServer->RemoveObject(m_hFace);
        m_hFace = LTNULL;
        m_hFaceAttachment = LTNULL;
    }
}

static bool FT_ZombieAssetExists(const char *pPath)
{
    FILE *pFile = fopen(pPath, "rb");
    if(!pFile)
    {
        return false;
    }

    fclose(pFile);
    return true;
}

static void FT_AuditZombieAnimationNames(
    HOBJECT hZombie)
{
    if(s_bAnimationAuditComplete ||
       !hZombie)
    {
        return;
    }

    s_bAnimationAuditComplete =
        true;

    FILE *pCandidates =
        fopen(
            "rez/Fireteam/ST_M_CHILD-strings.txt",
            "rt");

    if(!pCandidates)
    {
        g_pLTServer->CPrint(
            "Fireteam infected animation audit: no local ST_M_CHILD candidate report.");
        return;
    }

    FILE *pValid =
        fopen(
            "config/infected-animation-valid.txt",
            "wt");

    if(pValid)
    {
        fprintf(
            pValid,
            "# Valid animation names exposed by the composed normal infected model.\n");
        fprintf(
            pValid,
            "# Generated locally from ST_M_CHILD.LTB; safe to delete/regenerate.\n\n");
    }

    char sLine[128];
    uint32 nValid = 0;

    while(fgets(
        sLine,
        sizeof(sLine),
        pCandidates))
    {
        char *pName =
            FT_TrimInfectedLine(
                sLine);

        if(!pName[0])
        {
            continue;
        }

        HMODELANIM hAnim =
            g_pLTServer->GetAnimIndex(
                hZombie,
                pName);

        if(hAnim ==
           INVALID_MODEL_ANIM)
        {
            continue;
        }

        ++nValid;

        if(pValid)
        {
            fprintf(
                pValid,
                "%s\n",
                pName);
        }

        // Keep the exhaustive list in the local report; printing 1,400+
        // names to the game console at spawn can flood input/diagnostic logs.
    }

    fclose(
        pCandidates);

    if(pValid)
    {
        fclose(
            pValid);
    }

    g_pLTServer->CPrint(
        "Fireteam infected animation audit: %u valid animation name(s); report=config/infected-animation-valid.txt.",
        nValid);
}


static float FT_ZombieWallhackStackSeconds(
    float fBaseSeconds,
    uint8 nStack)
{
    if(fBaseSeconds <= 0.0f)
    {
        return 0.0f;
    }

    const uint32 nDifficulty =
        FT_GetActiveDifficultyLevel(
            "config/session.cfg");

    if(nDifficulty <= 4 ||
       nStack <= 1)
    {
        return fBaseSeconds;
    }

    float fFactor = 1.0f;

    if(nDifficulty <= 6)
    {
        if(nStack == 2) fFactor = 0.833333f;
        else if(nStack == 3) fFactor = 0.50f;
        else if(nStack == 4) fFactor = 0.333333f;
        else fFactor = 0.20f;
    }
    else if(nDifficulty <= 8)
    {
        if(nStack == 2) fFactor = 0.75f;
        else if(nStack == 3) fFactor = 0.40f;
        else if(nStack == 4) fFactor = 0.25f;
        else fFactor = 0.15f;
    }
    else
    {
        if(nStack == 2) fFactor = 0.50f;
        else if(nStack == 3) fFactor = 0.25f;
        else if(nStack == 4) fFactor = 0.15f;
        else fFactor = 0.10f;
    }

    return fBaseSeconds *
        fFactor;
}

void FireteamZombie::ApplyWallhackRenderStyle(
    bool bEnabled)
{
    const char *pWallhackStyle =
        "RenderStyles/ZombieThroughWall.ltb";

    // NOLF2's SetObjectRenderStyle helper does exactly this: submit a
    // temporary ObjectCreateStruct containing only the render-style slot(s).
    // Do not re-submit model filenames here; that can disturb child models.
    ObjectCreateStruct bodyOCS;
    bodyOCS.Clear();

    FT_CopyInfectedString(
        bodyOCS.m_RenderStyleNames[0],
        MAX_CS_FILENAME_LEN,
        bEnabled
            ? pWallhackStyle
            : (m_Def.sBodyRenderStyle0[0]
                ? m_Def.sBodyRenderStyle0
                : "RenderStyles/default.ltb"));

    FT_CopyInfectedString(
        bodyOCS.m_RenderStyleNames[1],
        MAX_CS_FILENAME_LEN,
        bEnabled
            ? pWallhackStyle
            : m_Def.sBodyRenderStyle1);

    const LTRESULT nBodyResult =
        g_pLTSCommon->SetObjectFilenames(
            m_hObject,
            &bodyOCS);

    if(m_hFace)
    {
        ObjectCreateStruct faceOCS;
        faceOCS.Clear();

        FT_CopyInfectedString(
            faceOCS.m_RenderStyleNames[0],
            MAX_CS_FILENAME_LEN,
            bEnabled
                ? pWallhackStyle
                : (m_Def.sFaceRenderStyle0[0]
                    ? m_Def.sFaceRenderStyle0
                    : "RenderStyles/default.ltb"));

        FT_CopyInfectedString(
            faceOCS.m_RenderStyleNames[1],
            MAX_CS_FILENAME_LEN,
            bEnabled
                ? pWallhackStyle
                : m_Def.sFaceRenderStyle1);

        g_pLTSCommon->SetObjectFilenames(
            m_hFace,
            &faceOCS);
    }

    g_pLTServer->CPrint(
        "Fireteam powerup: zombie wallhack style %s (%s).",
        bEnabled
            ? "ON"
            : "OFF",
        nBodyResult == LT_OK
            ? "OK"
            : "FAILED");
}

static void FT_ApplyZombieWallhackToAll(
    bool bEnabled)
{
    HCLASS hZombieClass =
        g_pLTServer->GetClass(
            "FireteamZombie");

    if(!hZombieClass)
    {
        return;
    }

    for(HOBJECT hObject =
            g_pLTServer->GetNextObject(
                LTNULL);
        hObject;
        hObject =
            g_pLTServer->GetNextObject(
                hObject))
    {
        HCLASS hClass =
            g_pLTServer->GetObjectClass(
                hObject);

        if(!hClass ||
           !g_pLTServer->IsKindOf(
                hClass,
                hZombieClass))
        {
            continue;
        }

        FireteamZombie *pZombie =
            (FireteamZombie*)
            g_pLTServer->HandleToObject(
                hObject);

        if(pZombie)
        {
            pZombie->ApplyWallhackRenderStyle(
                bEnabled);
        }
    }
}

static void FT_UpdateZombieWallhackState()
{
    if(!s_bZombieWallhackApplied)
    {
        return;
    }

    const float fNow =
        FT_GetRoundCombatTime();

    if(fNow <
       s_fZombieWallhackUntil)
    {
        return;
    }

    s_fZombieWallhackUntil =
        0.0f;
    s_nZombieWallhackStacks =
        0;
    s_bZombieWallhackApplied =
        false;

    FT_ApplyZombieWallhackToAll(
        false);

    g_pLTServer->CPrint(
        "Fireteam powerup: ZOMBIE WALLHACK expired.");
}

float FT_ExtendZombieWallhack(
    float fBaseSeconds)
{
    if(fBaseSeconds <= 0.0f)
    {
        return 0.0f;
    }

    if(!FT_ZombieAssetExists(
           "rez/RenderStyles/ZombieThroughWall.ltb"))
    {
        g_pLTServer->CPrint(
            "Fireteam powerup: ZombieThroughWall.ltb is not staged; wallhack reward skipped.");

        return 0.0f;
    }

    FT_UpdateZombieWallhackState();

    const float fNow =
        FT_GetRoundCombatTime();

    if(s_fZombieWallhackUntil <=
       fNow)
    {
        s_fZombieWallhackUntil =
            fNow;
        s_nZombieWallhackStacks =
            0;
    }

    if(s_nZombieWallhackStacks <
       255)
    {
        ++s_nZombieWallhackStacks;
    }

    const float fAdded =
        FT_ZombieWallhackStackSeconds(
            fBaseSeconds,
            s_nZombieWallhackStacks);

    s_fZombieWallhackUntil +=
        fAdded;

    if(!s_bZombieWallhackApplied)
    {
        s_bZombieWallhackApplied =
            true;

        FT_ApplyZombieWallhackToAll(
            true);
    }

    const float fRemaining =
        s_fZombieWallhackUntil -
        fNow;

    g_pLTServer->CPrint(
        "Fireteam powerup: ZOMBIE WALLHACK stack %u +%.1fs, %.1fs total.",
        (uint32)s_nZombieWallhackStacks,
        fAdded,
        fRemaining);

    return fRemaining;
}

float FT_GetZombieWallhackRemaining()
{
    FT_UpdateZombieWallhackState();

    if(!s_bZombieWallhackApplied)
    {
        return 0.0f;
    }

    const float fRemaining =
        s_fZombieWallhackUntil -
        FT_GetRoundCombatTime();

    return fRemaining > 0.0f
        ? fRemaining
        : 0.0f;
}

bool FT_IsZombieWallhackActive()
{
    return
        FT_GetZombieWallhackRemaining() >
        0.0f;
}

void FireteamZombie::PlayVoiceSound(
    const char *pFilename)
{
    if(!pFilename ||
       !pFilename[0] ||
       !m_Def.sVoiceDir[0])
    {
        return;
    }

    char sSound[256];
    sprintf(
        sSound,
        "%s/%s",
        m_Def.sVoiceDir,
        pFilename);

    PlaySoundInfo soundInfo;
    PLAYSOUNDINFO_INIT(
        soundInfo);

    soundInfo.m_dwFlags =
        PLAYSOUND_3D |
        PLAYSOUND_ATTACHED |
        PLAYSOUND_REVERB;
    soundInfo.m_hObject =
        m_hObject;
    soundInfo.m_fOuterRadius =
        m_Def.fVoiceRadius > 0.0f
        ? m_Def.fVoiceRadius
        : 850.0f;
    soundInfo.m_fInnerRadius =
        110.0f;

    strncpy(
        soundInfo.m_szSoundName,
        sSound,
        sizeof(soundInfo.m_szSoundName) - 1);
    soundInfo.m_szSoundName[
        sizeof(soundInfo.m_szSoundName) - 1] =
        '\0';

    HLTSOUND hSound =
        LTNULL;

    const LTRESULT nSound = g_pLTServer->SoundMgr()->PlaySound(
        &soundInfo,
        hSound);
    if(nSound != LT_OK)
    {
        static bool s_bWarnedMissingVoice = false;
        if(!s_bWarnedMissingVoice)
        {
            s_bWarnedMissingVoice = true;
            g_pLTServer->CPrint(
                "Fireteam infected audio: PlaySound failed for %s (error %d). Verify SND.zip voice paths in staged rez.",
                sSound, (int)nSound);
        }
    }
}

void FireteamZombie::PlayAttackVoice()
{
    const char *pVoice =
        LTNULL;

    const uint32 nChoice =
        (uint32)(rand() % 3);

    if(nChoice == 0)
        pVoice = m_Def.sVoiceAttack1;
    else if(nChoice == 1)
        pVoice = m_Def.sVoiceAttack2;
    else
        pVoice = m_Def.sVoiceAttack3;

    if(!pVoice ||
       !pVoice[0])
    {
        if(m_Def.sVoiceAttack1[0])
            pVoice = m_Def.sVoiceAttack1;
        else if(m_Def.sVoiceAttack2[0])
            pVoice = m_Def.sVoiceAttack2;
        else
            pVoice = m_Def.sVoiceAttack3;
    }

    PlayVoiceSound(
        pVoice);
}

void FireteamZombie::CreateInfectedFace()
{
    if(m_bDefLoaded &&
       _stricmp(m_Def.sFaceMode, "child_model") == 0)
    {
        return;
    }

    if(m_hFace ||
       !m_bDefLoaded ||
       !m_Def.sFaceModel[0] ||
       !m_Def.sFaceSocket[0])
    {
        return;
    }

    char sLocalFacePath[256];
    sprintf(
        sLocalFacePath,
        "rez/%s",
        m_Def.sFaceModel);

    if(!FT_ZombieAssetExists(sLocalFacePath))
    {
        g_pLTServer->CPrint(
            "Fireteam infected: face asset is not staged: %s",
            m_Def.sFaceModel);
        return;
    }

    HCLASS hBaseClass = g_pLTServer->GetClass("BaseClass");
    if(!hBaseClass)
    {
        return;
    }

    ObjectCreateStruct ocs;
    ocs.Clear();
    ocs.m_ObjectType = OT_MODEL;
    ocs.m_Flags = FLAG_VISIBLE |
                  FLAG_FORCECLIENTUPDATE |
                  FLAG_SHADOW;

    FT_CopyInfectedString(
        ocs.m_Filename,
        MAX_CS_FILENAME_LEN,
        m_Def.sFaceModel);
    FT_CopyInfectedString(
        ocs.m_SkinName,
        MAX_CS_FILENAME_LEN,
        m_Def.sFaceTexture);

    if(m_Def.sFaceRenderStyle0[0])
    {
        FT_CopyInfectedString(
            ocs.m_RenderStyleNames[0],
            MAX_CS_FILENAME_LEN,
            m_Def.sFaceRenderStyle0);
    }

    if(m_Def.sFaceRenderStyle1[0])
    {
        FT_CopyInfectedString(
            ocs.m_RenderStyleNames[1],
            MAX_CS_FILENAME_LEN,
            m_Def.sFaceRenderStyle1);
    }

    BaseClass *pFace =
        (BaseClass*)g_pLTServer->CreateObject(
            hBaseClass,
            &ocs);

    if(!pFace)
    {
        g_pLTServer->CPrint(
            "Fireteam infected: failed to create face %s.",
            m_Def.sFaceModel);
        return;
    }

    m_hFace = pFace->m_hObject;

    LTVector vOffset(
        m_Def.fFacePosX,
        m_Def.fFacePosY,
        m_Def.fFacePosZ);

    LTRotation rOffset;
    rOffset.Init();

    if(m_Def.bFaceAutoAlign &&
       m_Def.sFaceAlignNode[0])
    {
        HMODELNODE hAlignNode;
        if(g_pLTSModel->GetNode(
            m_hFace,
            m_Def.sFaceAlignNode,
            hAlignNode) == LT_OK)
        {
            LTransform tAlign;
            if(g_pLTSModel->GetNodeTransform(
                m_hFace,
                hAlignNode,
                tAlign,
                LTFALSE) == LT_OK)
            {
                // The face LTB carries a full skeleton. Attach its root with
                // the inverse Head-node transform so its Head lands on the
                // body's Head socket instead of offsetting the full skeleton
                // above/beside the zombie.
                rOffset = tAlign.m_Rot.Conjugate();
                LTVector vInverseAlignPos =
                    tAlign.m_Pos * -1.0f;

                // Jupiter's LTRotation only overloads rotation*rotation.
                // Rotate the local offset explicitly through the inverse
                // rotation basis instead.
                LTVector vRotatedInverse =
                    (rOffset.Right() * vInverseAlignPos.x) +
                    (rOffset.Up() * vInverseAlignPos.y) +
                    (rOffset.Forward() * vInverseAlignPos.z);

                vOffset += vRotatedInverse;

                g_pLTServer->CPrint(
                    "Fireteam infected: face auto-align node %s local %.1f %.1f %.1f.",
                    m_Def.sFaceAlignNode,
                    tAlign.m_Pos.x,
                    tAlign.m_Pos.y,
                    tAlign.m_Pos.z);
            }
        }
    }

    rOffset.Rotate(
        rOffset.Right(),
        MATH_DEGREES_TO_RADIANS(m_Def.fFaceRotX));
    rOffset.Rotate(
        rOffset.Up(),
        MATH_DEGREES_TO_RADIANS(m_Def.fFaceRotY));
    rOffset.Rotate(
        rOffset.Forward(),
        MATH_DEGREES_TO_RADIANS(m_Def.fFaceRotZ));

    LTRESULT result =
        g_pLTServer->CreateAttachment(
            m_hObject,
            m_hFace,
            m_Def.sFaceSocket,
            &vOffset,
            &rOffset,
            &m_hFaceAttachment);

    if(result != LT_OK)
    {
        g_pLTServer->CPrint(
            "Fireteam infected: face attachment failed on socket %s.",
            m_Def.sFaceSocket);
        g_pLTServer->RemoveObject(m_hFace);
        m_hFace = LTNULL;
        m_hFaceAttachment = LTNULL;
        return;
    }

    g_pLTServer->CPrint(
        "Fireteam infected: attached %s on %s rot=%.1f %.1f %.1f.",
        m_Def.sFaceModel,
        m_Def.sFaceSocket,
        m_Def.fFaceRotX,
        m_Def.fFaceRotY,
        m_Def.fFaceRotZ);
}

void FireteamZombie::SetZombieAnimation(
    const char *pAnimation,
    bool bLooping)
{
    if(!pAnimation ||
       !pAnimation[0] ||
       strcmp(
           m_sCurrentAnimation,
           pAnimation) == 0)
    {
        return;
    }

    HMODELANIM hAnim =
        g_pLTServer->GetAnimIndex(
            m_hObject,
            (char*)pAnimation);

    if(hAnim == INVALID_MODEL_ANIM)
    {
        g_pLTServer->CPrint(
            "Fireteam infected: animation missing: %s",
            pAnimation);
        return;
    }

    // Preserve stride phase when switching between validated WALK and RUN
    // banks. This avoids an obvious foot-pop at chase transitions without
    // altering death, jump, attack or one-shot animation timing.
    const bool bOldLocomotion =
        m_sCurrentAnimation[0] &&
        (strcmp(m_sCurrentAnimation, m_Def.sWalkAnim) == 0 ||
         strcmp(m_sCurrentAnimation, m_Def.sRunAnim) == 0);
    const bool bNewLocomotion =
        bLooping &&
        (strcmp(pAnimation, m_Def.sWalkAnim) == 0 ||
         strcmp(pAnimation, m_Def.sRunAnim) == 0);

    uint32 nOldTime = 0;
    uint32 nOldLength = 0;
    const bool bKeepStridePhase =
        bOldLocomotion && bNewLocomotion &&
        g_pLTSModel->GetCurAnimTime(
            m_hObject, MAIN_TRACKER, nOldTime) == LT_OK &&
        g_pLTSModel->GetCurAnimLength(
            m_hObject, MAIN_TRACKER, nOldLength) == LT_OK &&
        nOldLength > 0;

    g_pLTSModel->SetCurAnim(
        m_hObject,
        MAIN_TRACKER,
        hAnim);
    g_pLTSModel->SetLooping(
        m_hObject,
        MAIN_TRACKER,
        bLooping ? LTTRUE : LTFALSE);

    if(bKeepStridePhase)
    {
        uint32 nNewLength = 0;
        if(g_pLTSModel->GetCurAnimLength(
               m_hObject, MAIN_TRACKER, nNewLength) == LT_OK &&
           nNewLength > 1)
        {
            const uint32 nPhaseTime =
                (uint32)(((double)(nOldTime % nOldLength) *
                    (double)nNewLength) / (double)nOldLength);
            g_pLTSModel->SetCurAnimTime(
                m_hObject, MAIN_TRACKER, nPhaseTime);
        }
    }

    FT_CopyInfectedString(
        m_sCurrentAnimation,
        sizeof(m_sCurrentAnimation),
        pAnimation);
}

struct FTZombieMovementFilterData
{
    HOBJECT hZombie;
};

static bool FTZombieMovementFilter(
    HOBJECT hObject,
    void *pUserData)
{
    FTZombieMovementFilterData *pData =
        (FTZombieMovementFilterData*)pUserData;

    if(!pData)
    {
        return true;
    }

    if(hObject == pData->hZombie)
    {
        return false;
    }

    HCLASS hClass =
        g_pLTServer->GetObjectClass(
            hObject);

    HCLASS hZombieClass =
        g_pLTServer->GetClass(
            "FireteamZombie");
    HCLASS hPlayerClass =
        g_pLTServer->GetClass(
            "CPlayerSrvr");

    if(hClass &&
       ((hZombieClass &&
         g_pLTServer->IsKindOf(
             hClass,
             hZombieClass)) ||
        (hPlayerClass &&
         g_pLTServer->IsKindOf(
             hClass,
             hPlayerClass))))
    {
        return false;
    }

    return true;
}

bool FireteamZombie::IsMovementStepClear(
    const LTVector &vPos,
    const LTVector &vDirection,
    float fDistance)
{
    LTVector vDir = vDirection;
    vDir.y = 0.0f;

    if(vDir.MagSqr() < 0.001f)
    {
        return true;
    }

    vDir.Normalize();

    LTVector vRight(
        -vDir.z,
        0.0f,
        vDir.x);

    float fHalfWidth =
        m_vCollisionDims.x;

    if(m_vCollisionDims.z >
       fHalfWidth)
    {
        fHalfWidth =
            m_vCollisionDims.z;
    }

    if(fHalfWidth < 6.0f)
    {
        fHalfWidth = 6.0f;
    }

    LTVector vProbeFrom = vPos;
    vProbeFrom.y +=
        m_vCollisionDims.y * 0.45f;

    const float aOffsets[3] =
    {
        0.0f,
        -(fHalfWidth + 2.0f),
        (fHalfWidth + 2.0f)
    };

    FTZombieMovementFilterData filter;
    filter.hZombie = m_hObject;

    for(uint32 i = 0; i < 3; ++i)
    {
        const LTVector vOffset =
            vRight * aOffsets[i];

        IntersectQuery query;
        IntersectInfo info;

        query.m_From =
            vProbeFrom + vOffset;
        query.m_To =
            query.m_From +
            (vDir *
             (fDistance +
              fHalfWidth));
        query.m_Flags =
            INTERSECT_OBJECTS |
            IGNORE_NONSOLID |
            INTERSECT_HPOLY;
        query.m_FilterFn =
            FTZombieMovementFilter;
        query.m_pUserData =
            &filter;

        if(g_pLTServer->IntersectSegment(
            &query,
            &info))
        {
            return false;
        }
    }

    return true;
}
bool FireteamZombie::BuildLocalEscapeWaypoint(
    const LTVector &vPos,
    const LTVector &vGoal)
{
    LTVector vForward =
        vGoal - vPos;
    vForward.y = 0.0f;

    if(vForward.MagSqr() < 0.001f)
    {
        return false;
    }

    vForward.Normalize();

    LTVector vRight(
        -vForward.z,
        0.0f,
        vForward.x);

    LTVector aDirections[4];
    aDirections[0] =
        vForward +
        (vRight * 0.80f);
    aDirections[1] =
        vForward -
        (vRight * 0.80f);
    aDirections[2] = vRight;
    aDirections[3] = vRight * -1.0f;

    const uint32 nStart =
        (m_nPathLane & 1) ? 1 : 0;

    const bool bConstrainToVolume =
        FT_IsPositionInNavigationVolume(
            vPos);

    bool bFound = false;
    LTVector vBest;
    float fBestScore = FLT_MAX;

    for(uint32 nPass = 0;
        nPass < 4;
        ++nPass)
    {
        uint32 nIndex = nPass;

        if(nPass < 2)
        {
            nIndex =
                (nStart + nPass) % 2;
        }

        LTVector vDir =
            aDirections[nIndex];

        if(vDir.MagSqr() < 0.001f)
        {
            continue;
        }

        vDir.Normalize();

        if(!IsMovementStepClear(
            vPos,
            vDir,
            72.0f))
        {
            continue;
        }

        const LTVector vEscape =
            vPos +
            (vDir * 72.0f);

        if(bConstrainToVolume &&
           !FT_ArePositionsInSameNavigationVolume(
               vPos,
               vEscape))
        {
            continue;
        }

        LTVector vRemaining =
            vGoal - vEscape;
        vRemaining.y = 0.0f;

        const float fScore =
            vRemaining.MagSqr();

        if(!bFound ||
           fScore < fBestScore)
        {
            bFound = true;
            vBest = vEscape;
            fBestScore = fScore;
        }
    }

    if(!bFound)
    {
        return false;
    }

    if(m_nWaypoint >
       m_aPath.size())
    {
        m_nWaypoint =
            (uint32)m_aPath.size();
    }

    m_aPath.insert(
        m_aPath.begin() +
            m_nWaypoint,
        vBest);

    return true;
}
HOBJECT FireteamZombie::FindNearestPlayer()
{
    HCLASS hPlayerClass = g_pLTServer->GetClass("CPlayerSrvr");
    if(!hPlayerClass)
    {
        return LTNULL;
    }

    LTVector vMyPos;
    g_pLTServer->GetObjectPos(m_hObject, &vMyPos);

    HOBJECT hBest = LTNULL;
    float fBestDist = FLT_MAX;

    for(HOBJECT hObj = g_pLTServer->GetNextObject(LTNULL);
        hObj;
        hObj = g_pLTServer->GetNextObject(hObj))
    {
        HCLASS hClass = g_pLTServer->GetObjectClass(hObj);
        if(!hClass || !g_pLTServer->IsKindOf(hClass, hPlayerClass))
        {
            continue;
        }

        CPlayerSrvr *pPlayer = (CPlayerSrvr*)g_pLTServer->HandleToObject(hObj);
        if(!pPlayer ||
           !pPlayer->IsTargetable())
        {
            continue;
        }

        LTVector vPlayerPos;
        g_pLTServer->GetObjectPos(hObj, &vPlayerPos);
        float fDist = vMyPos.DistSqr(vPlayerPos);

        if(fDist < fBestDist)
        {
            fBestDist = fDist;
            hBest = hObj;
        }
    }

    return hBest;
}

struct FTZombieSightFilterData
{
    HOBJECT hZombie;
    HOBJECT hTarget;
};

static bool FTZombieSightFilter(
    HOBJECT hObject,
    void *pUserData)
{
    FTZombieSightFilterData *pData =
        (FTZombieSightFilterData*)pUserData;

    if(!pData)
    {
        return true;
    }

    return hObject != pData->hZombie &&
           hObject != pData->hTarget;
}

bool FireteamZombie::CanSeeTarget(
    HOBJECT hTarget,
    const LTVector &vFrom,
    const LTVector &vTarget)
{
    IntersectQuery query;
    IntersectInfo info;

    query.m_From = vFrom;
    query.m_To = vTarget;

    const float fProbeHeight =
        m_vCollisionDims.y > 20.0f
        ? m_vCollisionDims.y * 0.45f
        : 12.0f;

    query.m_From.y += fProbeHeight;
    query.m_To.y += fProbeHeight;
    query.m_Flags =
        INTERSECT_OBJECTS |
        IGNORE_NONSOLID |
        INTERSECT_HPOLY;

    FTZombieSightFilterData filterData;
    filterData.hZombie = m_hObject;
    filterData.hTarget = hTarget;

    query.m_FilterFn =
        FTZombieSightFilter;
    query.m_pUserData =
        &filterData;

    return !g_pLTServer->IntersectSegment(
        &query,
        &info);
}

bool FireteamZombie::HasDirectPathToTarget(
    HOBJECT hTarget,
    const LTVector &vFrom,
    const LTVector &vTarget)
{
    LTVector vForward = vTarget - vFrom;
    vForward.y = 0.0f;

    if(vForward.Mag() < 1.0f)
    {
        return true;
    }

    vForward.Normalize();

    // A center ray can see through a doorway even when the infected's collision
    // box cannot fit along that exact steering line. Probe a corridor as wide
    // as the actual model so door jambs keep us on the authored nav gate.
    LTVector vRight(
        -vForward.z,
        0.0f,
        vForward.x);

    float fHalfWidth = m_vCollisionDims.x;
    if(m_vCollisionDims.z > fHalfWidth)
    {
        fHalfWidth = m_vCollisionDims.z;
    }

    if(fHalfWidth < 6.0f) fHalfWidth = 6.0f;

    const float fProbeHeight =
        m_vCollisionDims.y > 20.0f
        ? m_vCollisionDims.y * 0.45f
        : 12.0f;

    const float aOffsets[3] =
    {
        0.0f,
        -(fHalfWidth + 2.0f),
        (fHalfWidth + 2.0f)
    };

    FTZombieSightFilterData filterData;
    filterData.hZombie = m_hObject;
    filterData.hTarget = hTarget;

    for(uint32 i = 0; i < 3; ++i)
    {
        LTVector vOffset = vRight * aOffsets[i];

        IntersectQuery query;
        IntersectInfo info;

        query.m_From = vFrom + vOffset;
        query.m_To = vTarget + vOffset;
        query.m_From.y += fProbeHeight;
        query.m_To.y += fProbeHeight;
        query.m_Flags =
            INTERSECT_OBJECTS |
            IGNORE_NONSOLID |
            INTERSECT_HPOLY;
        query.m_FilterFn = FTZombieSightFilter;
        query.m_pUserData = &filterData;

        if(g_pLTServer->IntersectSegment(
            &query,
            &info))
        {
            return false;
        }
    }

    return true;
}

void FireteamZombie::RebuildPath(const LTVector &vTarget)
{
    LTVector vPos;
    g_pLTServer->GetObjectPos(
        m_hObject,
        &vPos);

    m_aPath.clear();

    float fAgentHalfWidth =
        m_vCollisionDims.x;

    if(m_vCollisionDims.z >
       fAgentHalfWidth)
    {
        fAgentHalfWidth =
            m_vCollisionDims.z;
    }

    if(fAgentHalfWidth < 6.0f)
    {
        fAgentHalfWidth = 6.0f;
    }

    const bool bBuilt =
        FT_BuildNavigationPath(
            vPos,
            vTarget,
            fAgentHalfWidth,
            m_nPathLane,
            m_aPath);

    // Maps without CA/NOLF AIVolumes still need a useful baseline. Let the
    // existing collision probes/local steering work toward the target directly;
    // authored navigation remains authoritative whenever volumes exist.
    if(!bBuilt &&
       (FT_GetNavigationVolumeCount() == 0 ||
        FT_ArePositionsInSameNavigationVolume(
            vPos,
            vTarget)))
    {
        m_aPath.push_back(
            vTarget);
    }

    m_nWaypoint = 0;
    m_fRepathCooldown = 1.50f;
}

void FireteamZombie::AdvanceSmoothMotion()
{
    if(m_nMotionStepsRemaining == 0)
        return;

    // Step toward the NEXT AI destination, not an extrapolated player
    // position. Every substep uses Jupiter collision resolution; geometry
    // cannot be bypassed by a client-only cosmetic interpolation.
    LTVector vCurrent;
    g_pLTServer->GetObjectPos(m_hObject, &vCurrent);
    const float fPart =
        1.0f / (float)m_nMotionStepsRemaining;
    LTVector vNext =
        vCurrent + (m_vMotionGoal - vCurrent) * fPart;

    // Critical world-collision guard: MoveObject can resolve a small
    // displacement into an unintended neighboring brush/room when a
    // crowd keeps pushing an actor against a thin wall. Never advance
    // an interpolated substep through a solid corridor probe.
    LTVector vHorizontal = vNext - vCurrent;
    vHorizontal.y = 0.0f;
    const float fDistance = vHorizontal.Mag();
    if(fDistance > 0.001f &&
       !IsMovementStepClear(vCurrent, vHorizontal, fDistance))
    {
        m_nMotionStepsRemaining = 0;
        m_fRepathCooldown = 0.0f;
        return;
    }

    g_pLTServer->MoveObject(m_hObject, &vNext);
    --m_nMotionStepsRemaining;
}

void FireteamZombie::UpdateZombie(float fDeltaSeconds)
{
    FT_UpdateZombieWallhackState();

    // The real measured time between AI decisions, capped to avoid moving
    // through a wall after a hitch. Repath/attack timers share this clock.
    const float kUpdate =
        fDeltaSeconds > 0.0f ? fDeltaSeconds : 0.10f;

    const float kAttackRange =
        m_Def.fAttackRange;

    const float kWaypointRadius = 24.0f;
    const float kSeparationRadius = 96.0f;
    const float kSeparationRadiusSqr =
        kSeparationRadius *
        kSeparationRadius;

    if(m_fAttackCooldown > 0.0f)
        m_fAttackCooldown -= kUpdate;

    if(m_fAttackAnimationTime > 0.0f)
        m_fAttackAnimationTime -= kUpdate;

    if(m_fJumpAnimationTime > 0.0f)
        m_fJumpAnimationTime -= kUpdate;

    if(m_fRepathCooldown > 0.0f)
        m_fRepathCooldown -= kUpdate;

    if(m_fForcePathTime > 0.0f)
        m_fForcePathTime -= kUpdate;

    if(m_fVoiceCooldown > 0.0f)
        m_fVoiceCooldown -= kUpdate;

    const BehaviorState ePreviousBehavior =
        m_eBehaviorState;

    HOBJECT hTarget =
        FindNearestPlayer();

    if(!hTarget)
    {
        LTVector vStop(
            0.0f,
            0.0f,
            0.0f);

        g_pLTSPhysics->SetVelocity(
            m_hObject,
            &vStop);

        if(m_fAttackAnimationTime <= 0.0f &&
           m_fJumpAnimationTime <= 0.0f)
        {
            SetZombieAnimation(
                m_Def.sIdleAnim,
                true);
        }

        m_fTargetMemory = 0.0f;
        m_bHasLastKnownTarget = false;
        m_eBehaviorState =
            kBehaviorSearch;
        m_fNoProgressTime = 0.0f;
        m_fBestProgressDistance =
            FLT_MAX;
        m_fStragglerIdleSeconds = 0.0f;
        m_nMotionStepsRemaining = 0;

        return;
    }

    LTVector vPos;
    LTVector vTarget;

    g_pLTServer->GetObjectPos(
        m_hObject,
        &vPos);

    g_pLTServer->GetObjectPos(
        hTarget,
        &vTarget);

    LTVector vToPlayer =
        vTarget - vPos;

    vToPlayer.y = 0.0f;

    const float fPlayerDistance =
        vToPlayer.Mag();

    const bool bCanSeePlayer =
        fPlayerDistance <=
            m_Def.fAlertDistance &&
        CanSeeTarget(
            hTarget,
            vPos,
            vTarget);

    if(bCanSeePlayer)
    {
        m_vLastKnownTargetPos =
            vTarget;
        m_fTargetMemory =
            m_Def.fTargetMemorySeconds;
        m_bHasLastKnownTarget =
            true;
        m_eBehaviorState =
            kBehaviorChase;

        if(m_fVoiceCooldown <= 0.0f &&
           (ePreviousBehavior ==
                kBehaviorSearch ||
            ePreviousBehavior ==
                kBehaviorLostTarget) &&
           m_Def.sVoiceAlert[0] &&
           (rand() % 100) <
                (int)m_Def.nVoiceAlertChance)
        {
            PlayVoiceSound(
                m_Def.sVoiceAlert);
            m_fVoiceCooldown =
                2.5f;
        }
    }
    else if(m_bHasLastKnownTarget &&
            m_fTargetMemory > 0.0f)
    {
        m_fTargetMemory -=
            kUpdate;

        if(m_fTargetMemory > 0.0f)
        {
            m_eBehaviorState =
                kBehaviorLostTarget;
        }
        else
        {
            m_fTargetMemory = 0.0f;
            m_bHasLastKnownTarget =
                false;
            m_eBehaviorState =
                kBehaviorSearch;
        }
    }
    else
    {
        m_fTargetMemory = 0.0f;
        m_bHasLastKnownTarget =
            false;
        m_eBehaviorState =
            kBehaviorSearch;
    }

    if(fPlayerDistance <= kAttackRange &&
       bCanSeePlayer)
    {
        m_eBehaviorState =
            kBehaviorAttack;

        LTVector vStop(
            0.0f,
            0.0f,
            0.0f);

        g_pLTSPhysics->SetVelocity(
            m_hObject,
            &vStop);

        if(m_fAttackAnimationTime <= 0.0f &&
           m_fAttackCooldown > 0.0f)
        {
            SetZombieAnimation(
                m_Def.sIdleAnim,
                true);
        }

        if(fPlayerDistance > 1.0f)
        {
            vToPlayer.Normalize();

            LTRotation rLook(
                vToPlayer,
                LTVector(
                    0.0f,
                    1.0f,
                    0.0f));

            g_pLTServer->SetObjectRotation(
                m_hObject,
                &rLook);
        }

        if(m_fAttackCooldown <= 0.0f)
        {
            const char *aAttackAnimations[3] =
            {
                m_Def.sAttackAnim1,
                m_Def.sAttackAnim2,
                m_Def.sAttackAnim3
            };

            const uint32 nAttackStart =
                (uint32)(rand() % 3);

            const char *pAttackAnimation =
                LTNULL;

            for(uint32 nAttackTry = 0;
                nAttackTry < 3;
                ++nAttackTry)
            {
                const char *pCandidate =
                    aAttackAnimations[
                        (nAttackStart +
                         nAttackTry) %
                        3];

                if(pCandidate &&
                   pCandidate[0])
                {
                    pAttackAnimation =
                        pCandidate;
                    break;
                }
            }

            if(pAttackAnimation)
            {
                SetZombieAnimation(
                    pAttackAnimation,
                    false);

                m_fAttackAnimationTime =
                    m_Def.fAttackAnimSeconds > 0.0f
                    ? m_Def.fAttackAnimSeconds
                    : 0.65f;
            }
            else
            {
                SetZombieAnimation(
                    m_Def.sIdleAnim,
                    true);
                m_fAttackAnimationTime =
                    0.0f;
            }

            CPlayerSrvr *pPlayer =
                (CPlayerSrvr*)
                g_pLTServer->HandleToObject(
                    hTarget);

            if(pPlayer)
            {
                pPlayer->ApplyDamage(
                    m_Def.nAttackDamage);
            }

            if(m_fVoiceCooldown <= 0.0f &&
               (rand() % 100) <
                    (int)m_Def.nVoiceAttackChance)
            {
                PlayAttackVoice();
                m_fVoiceCooldown =
                    1.15f;
            }

            m_fAttackCooldown =
                m_Def.fAttackCooldown;
        }

        m_fStuckTime = 0.0f;
        m_fNoProgressTime = 0.0f;
        m_fBestProgressDistance =
            FLT_MAX;
        m_vLastPos = vPos;
        m_vStragglerProgressPos = vPos;
        m_fStragglerIdleSeconds = 0.0f;
        m_nMotionStepsRemaining = 0;
        return;
    }

    LTVector vPursuitTarget =
        vTarget;

    if(m_eBehaviorState ==
           kBehaviorLostTarget &&
       m_bHasLastKnownTarget)
    {
        vPursuitTarget =
            m_vLastKnownTargetPos;

        LTVector vToLastKnown =
            vPursuitTarget -
            vPos;

        vToLastKnown.y = 0.0f;

        if(vToLastKnown.Mag() <=
           (kWaypointRadius * 1.5f))
        {
            m_fTargetMemory = 0.0f;
            m_bHasLastKnownTarget =
                false;
            m_eBehaviorState =
                kBehaviorSearch;
            vPursuitTarget =
                vTarget;
        }
    }

    // Direct pursuit is only allowed inside the same authored volume.
    // Path construction may still recover from a slightly out-of-volume
    // source position using its nearest-volume fallback.
    const bool bSameVolume =
        FT_ArePositionsInSameNavigationVolume(
            vPos,
            vPursuitTarget);

    const bool bClearRouteToTarget =
        HasDirectPathToTarget(
            hTarget,
            vPos,
            vPursuitTarget);

    const bool bRunning =
        m_eBehaviorState ==
            kBehaviorChase ||
        m_eBehaviorState ==
            kBehaviorLostTarget;

    const float fMoveSpeed =
        bRunning
        ? m_Def.fRunSpeed
        : m_Def.fWalkSpeed;

    const bool bHasAuthoredNavigation =
        FT_GetNavigationVolumeCount() > 0;

    const bool bDirectPursuit =
        m_fForcePathTime <= 0.0f &&
        bClearRouteToTarget &&
        (bSameVolume ||
         !bHasAuthoredNavigation);

    if(bDirectPursuit)
    {
        m_aPath.clear();
        m_nWaypoint = 0;
    }
    else
    {
        bool bTargetMoved =
            false;

        if(!m_aPath.empty())
        {
            LTVector vTargetDelta =
                m_aPath.back() -
                vPursuitTarget;

            vTargetDelta.y = 0.0f;

            bTargetMoved =
                vTargetDelta.MagSqr() >
                (160.0f * 160.0f);
        }

        if(m_aPath.empty() ||
           m_nWaypoint >=
               m_aPath.size() ||
           (bTargetMoved &&
            m_fRepathCooldown <= 0.0f))
        {
            RebuildPath(vPursuitTarget);
        }

        while(m_nWaypoint <
              m_aPath.size())
        {
            LTVector vCheck =
                m_aPath[m_nWaypoint] -
                vPos;

            vCheck.y = 0.0f;

            if(vCheck.Mag() >
               kWaypointRadius)
            {
                break;
            }

            ++m_nWaypoint;
        }

        if(m_nWaypoint >=
           m_aPath.size() &&
           m_fRepathCooldown <= 0.0f)
        {
            RebuildPath(vPursuitTarget);
        }
    }

    const LTVector vMoveTarget =
        bDirectPursuit
        ? vPursuitTarget
        : ((m_nWaypoint <
            m_aPath.size())
            ? m_aPath[m_nWaypoint]
            : vPos);

    LTVector vMove =
        vMoveTarget -
        vPos;

    vMove.y = 0.0f;

    if(vMove.MagSqr() > 1.0f)
    {
        vMove.Normalize();

        const float fStepDistance =
            fMoveSpeed *
            kUpdate;

        // NOLF2-style dynamic character avoidance.  Other infected close to
        // our current waypoint are intentionally ignored so they cannot form
        // a permanent wall across a doorway.  Repulsion is also constrained
        // so it can never reverse the desired direction of travel.
        LTVector vDesiredStep =
            vMove *
            fStepDistance;

        LTVector vTotalForce(
            0.0f,
            0.0f,
            0.0f);

        const std::vector<FTZombieNeighborSample> &aNeighbors =
            FT_GetZombieNeighborSnapshot();
        for(uint32 i = 0; i < (uint32)aNeighbors.size(); ++i)
        {
            const FTZombieNeighborSample &other = aNeighbors[i];
            if(other.hObject == m_hObject)
                continue;

            const LTVector &vOther = other.vPos;

                LTVector vOtherToDest =
                    vOther -
                    vMoveTarget;

                vOtherToDest.y = 0.0f;

                if(vOtherToDest.MagSqr() <=
                   kSeparationRadiusSqr)
                {
                    continue;
                }

                LTVector vAway =
                    vPos -
                    vOther;

                vAway.y = 0.0f;

                const float fDistanceSqr =
                    vAway.MagSqr();

                if(fDistanceSqr <= 0.25f ||
                   fDistanceSqr >=
                       kSeparationRadiusSqr)
                {
                    continue;
                }

                const float fDistance =
                    (float)sqrt(
                        fDistanceSqr);

                float fStrength =
                    (kSeparationRadius -
                     fDistance) /
                    kSeparationRadius;

                fStrength *=
                    fStrength;

                vAway.Normalize();

                vTotalForce +=
                    vAway *
                    (fStrength *
                     2.0f *
                     fStepDistance);
        }

        LTVector vSteerStep =
            vDesiredStep +
            vTotalForce;

        if(vSteerStep.Dot(
            vDesiredStep) < 0.0f)
        {
            LTVector vRight(
                -vMove.z,
                0.0f,
                vMove.x);

            if(vRight.Dot(
                vSteerStep) < 0.0f)
            {
                vSteerStep =
                    vRight * -1.0f;
            }
            else
            {
                vSteerStep =
                    vRight;
            }
        }

        if(vSteerStep.MagSqr() >
           0.001f)
        {
            vSteerStep.Normalize();
            vSteerStep *=
                fStepDistance;
        }
        else
        {
            vSteerStep =
                vDesiredStep;
        }

        LTVector vSteer =
            vSteerStep;

        vSteer.y = 0.0f;

        if(vSteer.MagSqr() > 0.001f)
        {
            vSteer.Normalize();

            LTRotation rLook(
                vSteer,
                LTVector(
                    0.0f,
                    1.0f,
                    0.0f));

            g_pLTServer->SetObjectRotation(
                m_hObject,
                &rLook);

            if(m_fJumpAnimationTime <= 0.0f)
            {
                SetZombieAnimation(
                    bRunning
                        ? (m_Def.sRunAnim[0]
                            ? m_Def.sRunAnim
                            : m_Def.sWalkAnim)
                        : (m_Def.sWalkAnim[0]
                            ? m_Def.sWalkAnim
                            : m_Def.sRunAnim),
                    true);
            }

            LTVector vDesired =
                vPos +
                (vSteer *
                 fStepDistance);

            // Match NOLF2's cheap-ground movement: trace below the desired
            // horizontal position, then let MoveObject + STAIRSTEP perform
            // collision resolution.  Do not reject solid WorldModels merely
            // because they are not the root BSP.
            IntersectQuery floorQuery;
            IntersectInfo floorInfo;

            floorQuery.m_From =
                LTVector(
                    vDesired.x,
                    vPos.y +
                        m_vCollisionDims.y,
                    vDesired.z);

            floorQuery.m_To =
                LTVector(
                    vDesired.x,
                    vPos.y -
                        (m_vCollisionDims.y *
                         10.0f),
                    vDesired.z);

            floorQuery.m_Flags =
                INTERSECT_OBJECTS |
                IGNORE_NONSOLID |
                INTERSECT_HPOLY;

            FTZombieMovementFilterData
                floorFilter;

            floorFilter.hZombie =
                m_hObject;

            floorQuery.m_FilterFn =
                FTZombieMovementFilter;

            floorQuery.m_pUserData =
                &floorFilter;

            if(g_pLTServer->IntersectSegment(
                &floorQuery,
                &floorInfo))
            {
                const float fFloorY =
                    floorInfo.m_Point.y +
                    m_vCollisionDims.y;

                const float fHeightDelta =
                    fFloorY -
                    vPos.y;

                const float fNormalStepHeight =
                    42.0f;

                const float fJumpHeight =
                    m_Def.fJumpHeight >
                        fNormalStepHeight
                    ? m_Def.fJumpHeight
                    : fNormalStepHeight;

                if(fHeightDelta <=
                       fNormalStepHeight &&
                   fHeightDelta >= -80.0f)
                {
                    vDesired.y =
                        fFloorY;
                }
                else if(fHeightDelta >
                            fNormalStepHeight &&
                        fHeightDelta <=
                            fJumpHeight)
                {
                    vDesired.y =
                        fFloorY;

                    if(m_Def.sJumpAnim[0] &&
                       m_fJumpAnimationTime <=
                           0.0f)
                    {
                        SetZombieAnimation(
                            m_Def.sJumpAnim,
                            false);

                        m_fJumpAnimationTime =
                            m_Def.fJumpAnimSeconds >
                                0.0f
                            ? m_Def.fJumpAnimSeconds
                            : 0.45f;
                    }
                }
                else
                {
                    vDesired.y =
                        vPos.y;
                }
            }
            else
            {
                vDesired.y =
                    vPos.y;
            }

            LTVector vZero(
                0.0f,
                0.0f,
                0.0f);

            g_pLTSPhysics->SetVelocity(
                m_hObject,
                &vZero);

            if(m_Def.nMotionSubsteps > 1)
            {
                // AI decisions stay at 10 Hz; only cheap collision movement
                // runs at 30 Hz (or configurable 2-4 substeps per decision).
                m_vMotionGoal = vDesired;
                m_nMotionStepsRemaining = m_Def.nMotionSubsteps;
            }
            else
            {
                g_pLTServer->MoveObject(
                    m_hObject,
                    &vDesired);
            }
        }
    }
    else
    {
        if(m_fAttackAnimationTime <= 0.0f &&
           m_fJumpAnimationTime <= 0.0f)
        {
            SetZombieAnimation(
                m_Def.sIdleAnim,
                true);
        }
    }

    LTVector vNewPos;

    g_pLTServer->GetObjectPos(
        m_hObject,
        &vNewPos);

    LTVector vMoved =
        vNewPos -
        m_vLastPos;

    vMoved.y = 0.0f;

    if(!bDirectPursuit &&
       m_nWaypoint <
           m_aPath.size())
    {
        LTVector vTargetChange =
            vMoveTarget -
            m_vProgressTarget;
        vTargetChange.y = 0.0f;

        LTVector vToWaypoint =
            vMoveTarget -
            vNewPos;
        vToWaypoint.y = 0.0f;

        const float fDistanceToWaypoint =
            vToWaypoint.Mag();

        if(m_fBestProgressDistance ==
               FLT_MAX ||
           vTargetChange.MagSqr() >
               (12.0f * 12.0f))
        {
            m_vProgressTarget =
                vMoveTarget;
            m_fBestProgressDistance =
                fDistanceToWaypoint;
            m_fNoProgressTime =
                0.0f;
        }
        else if(fDistanceToWaypoint +
                    2.0f <
                m_fBestProgressDistance)
        {
            m_fBestProgressDistance =
                fDistanceToWaypoint;
            m_fNoProgressTime =
                0.0f;
        }
        else
        {
            m_fNoProgressTime +=
                kUpdate;
        }
    }
    else
    {
        m_fNoProgressTime = 0.0f;
        m_fBestProgressDistance =
            FLT_MAX;
    }

    if(vMoved.Mag() < 0.75f &&
       fPlayerDistance >
           kAttackRange)
    {
        m_fStuckTime +=
            kUpdate;
    }
    else
    {
        m_fStuckTime = 0.0f;
    }

    const bool bNoRouteProgress =
        m_fNoProgressTime >= 1.25f;

    if((m_fStuckTime >= 1.50f ||
        bNoRouteProgress) &&
       fPlayerDistance >
           kAttackRange)
    {
        bool bEscaped = false;

        // Rotate before rebuilding so this recovery really tries a different
        // character-width volume gate.
        ++m_nPathLane;

        if(m_fForcePathTime <= 0.0f)
        {
            bEscaped =
                BuildLocalEscapeWaypoint(
                    vNewPos,
                    vMoveTarget);
        }

        if(!bEscaped)
        {
            m_aPath.clear();
            m_nWaypoint = 0;
            m_fRepathCooldown = 0.0f;
            RebuildPath(
                vPursuitTarget);
        }

        m_fForcePathTime = 3.0f;

        g_pLTServer->CPrint(
            "Fireteam infected: recovery %s reason=%s waypoint=%u/%u lane=%u zombie=%.1f %.1f %.1f target=%.1f %.1f %.1f dist=%.1f.",
            bEscaped
                ? "local-steer"
                : "repath",
            bNoRouteProgress
                ? "no-progress"
                : "stationary",
            m_nWaypoint,
            (uint32)m_aPath.size(),
            m_nPathLane,
            vNewPos.x,
            vNewPos.y,
            vNewPos.z,
            vPursuitTarget.x,
            vPursuitTarget.y,
            vPursuitTarget.z,
            fPlayerDistance);

        m_fStuckTime = 0.0f;
        m_fNoProgressTime = 0.0f;
        m_fBestProgressDistance =
            FLT_MAX;
    }

    // Genuine last-straggler rescue. Check NET movement, not lifetime:
    // active infected that advance 96 units, or engage a nearby survivor,
    // never get repositioned merely because the clock has elapsed.
    // A zombie directly above or below the player can have near-zero
    // horizontal distance yet still be trapped on a different floor.
    if(FT_IsFinalLivingInfected(m_hObject) &&
       (fPlayerDistance > 250.0f ||
        (float)fabs(vTarget.y - vNewPos.y) > 160.0f))
    {
        LTVector vSinceProgress =
            vNewPos - m_vStragglerProgressPos;
        vSinceProgress.y = 0.0f;

        if(vSinceProgress.MagSqr() >= 96.0f * 96.0f)
        {
            m_vStragglerProgressPos = vNewPos;
            m_fStragglerIdleSeconds = 0.0f;
        }
        else
        {
            m_fStragglerIdleSeconds += kUpdate;
            if(m_fStragglerIdleSeconds >= 18.0f)
            {
                LTVector vRecovered;
                if(FT_TryRecoverFinalInfected(
                    m_hObject, hTarget, vRecovered))
                {
                    m_vLastPos = vRecovered;
                    m_vStragglerProgressPos = vRecovered;
                    m_aPath.clear();
                    m_nWaypoint = 0;
                    ++m_nPathLane;
                    m_fRepathCooldown = 0.0f;
                    m_fStuckTime = 0.0f;
                    m_fNoProgressTime = 0.0f;
                    m_fForcePathTime = 0.0f;
                    m_fBestProgressDistance = FLT_MAX;
                    m_fStragglerIdleSeconds = 0.0f;
                    m_nMotionStepsRemaining = 0;
                    m_vMotionGoal = vRecovered;
                    return;
                }
                // No valid authored position: keep enemy alive, retry only
                // after a further 6 seconds, and emit useful coordinates.
                m_fStragglerIdleSeconds = 12.0f;
            }
        }
    }
    else
    {
        m_vStragglerProgressPos = vNewPos;
        m_fStragglerIdleSeconds = 0.0f;
    }

    m_vLastPos =
        vNewPos;
}

uint32 FireteamZombie::EngineMessageFn(uint32 messageID, void *pData, LTFLOAT fData)
{
    switch(messageID)
    {
        case MID_PRECREATE:
        {
            ObjectCreateStruct *pOCS = (ObjectCreateStruct*)pData;
            if(pOCS)
            {
                pOCS->m_ObjectType = OT_MODEL;
                pOCS->m_Flags |= FLAG_SOLID | FLAG_VISIBLE | FLAG_GRAVITY |
                                 FLAG_STAIRSTEP | FLAG_YROTATION |
                                 FLAG_FORCECLIENTUPDATE | FLAG_SHADOW;
                pOCS->m_Flags2 |= FLAG2_PLAYERCOLLIDE;

                if(!m_bDefLoaded)
                {
                    g_pLTServer->CPrint(
                        "Fireteam infected: could not load config/infected.cfg.");
                }
                else
                {
                    FT_CopyInfectedString(
                        pOCS->m_Filenames[0],
                        MAX_CS_FILENAME_LEN,
                        m_Def.sBodyModel);

                    if(m_Def.sAnimationModel[0])
                    {
                        HMODELDB hAnimationModel = LTNULL;

                        if(g_pLTSModel->CacheModelDB(
                            m_Def.sAnimationModel,
                            hAnimationModel) == LT_OK)
                        {
                            FT_CopyInfectedString(
                                pOCS->m_Filenames[1],
                                MAX_CS_FILENAME_LEN,
                                m_Def.sAnimationModel);
                        }
                        else
                        {
                            g_pLTServer->CPrint(
                                "Fireteam infected: could not cache animation child %s.",
                                m_Def.sAnimationModel);
                        }
                    }

                    FT_CopyInfectedString(
                        pOCS->m_SkinNames[0],
                        MAX_CS_FILENAME_LEN,
                        m_Def.sBodyTexture0);
                    FT_CopyInfectedString(
                        pOCS->m_SkinNames[1],
                        MAX_CS_FILENAME_LEN,
                        m_Def.sBodyTexture1);

                    if(m_Def.sBodyRenderStyle0[0])
                    {
                        FT_CopyInfectedString(
                            pOCS->m_RenderStyleNames[0],
                            MAX_CS_FILENAME_LEN,
                            m_Def.sBodyRenderStyle0);
                    }

                    if(m_Def.sBodyRenderStyle1[0])
                    {
                        FT_CopyInfectedString(
                            pOCS->m_RenderStyleNames[1],
                            MAX_CS_FILENAME_LEN,
                            m_Def.sBodyRenderStyle1);
                    }

                    if(_stricmp(
                        m_Def.sFaceMode,
                        "child_model") == 0 &&
                       m_Def.sFaceModel[0])
                    {
                        HMODELDB hFaceModel = LTNULL;

                        if(g_pLTSModel->CacheModelDB(
                            m_Def.sFaceModel,
                            hFaceModel) == LT_OK)
                        {
                            FT_CopyInfectedString(
                                pOCS->m_Filenames[2],
                                MAX_CS_FILENAME_LEN,
                                m_Def.sFaceModel);
                            FT_CopyInfectedString(
                                pOCS->m_SkinNames[2],
                                MAX_CS_FILENAME_LEN,
                                m_Def.sFaceTexture);

                            g_pLTServer->CPrint(
                                "Fireteam infected: cached face child %s.",
                                m_Def.sFaceModel);
                        }
                        else
                        {
                            g_pLTServer->CPrint(
                                "Fireteam infected: could not cache face child %s.",
                                m_Def.sFaceModel);
                        }
                    }

                    if(m_Def.sHeadModel[0])
                    {
                        HMODELDB hHeadModel = LTNULL;

                        if(g_pLTSModel->CacheModelDB(
                            m_Def.sHeadModel,
                            hHeadModel) == LT_OK)
                        {
                            FT_CopyInfectedString(
                                pOCS->m_Filenames[3],
                                MAX_CS_FILENAME_LEN,
                                m_Def.sHeadModel);
                            FT_CopyInfectedString(
                                pOCS->m_SkinNames[3],
                                MAX_CS_FILENAME_LEN,
                                m_Def.sHeadTexture);

                            g_pLTServer->CPrint(
                                "Fireteam infected: cached head child %s.",
                                m_Def.sHeadModel);
                        }
                        else
                        {
                            g_pLTServer->CPrint(
                                "Fireteam infected: could not cache head child %s.",
                                m_Def.sHeadModel);
                        }
                    }
                }
            }
        }
        break;

        case MID_INITIALUPDATE:
        {
            g_pLTServer->SetObjectColor(
                m_hObject,
                1.0f,
                1.0f,
                1.0f,
                1.0f);

            if(m_bDefLoaded)
            {
                // NOLF2's CAI applies extra child models again after object
                // creation with ILTCommon::SetObjectFilenames. Do the same
                // here so the animation DB and CA face child are guaranteed
                // to be composed onto the live server object before animation
                // lookup and client replication.
                ObjectCreateStruct modelOCS;
                modelOCS.Clear();

                FT_CopyInfectedString(
                    modelOCS.m_Filenames[0],
                    MAX_CS_FILENAME_LEN,
                    m_Def.sBodyModel);
                FT_CopyInfectedString(
                    modelOCS.m_SkinNames[0],
                    MAX_CS_FILENAME_LEN,
                    m_Def.sBodyTexture0);
                FT_CopyInfectedString(
                    modelOCS.m_SkinNames[1],
                    MAX_CS_FILENAME_LEN,
                    m_Def.sBodyTexture1);

                if(m_Def.sBodyRenderStyle0[0])
                {
                    FT_CopyInfectedString(
                        modelOCS.m_RenderStyleNames[0],
                        MAX_CS_FILENAME_LEN,
                        m_Def.sBodyRenderStyle0);
                }

                if(m_Def.sBodyRenderStyle1[0])
                {
                    FT_CopyInfectedString(
                        modelOCS.m_RenderStyleNames[1],
                        MAX_CS_FILENAME_LEN,
                        m_Def.sBodyRenderStyle1);
                }

                if(m_Def.sAnimationModel[0])
                {
                    HMODELDB hAnimationModel = LTNULL;
                    if(g_pLTSModel->CacheModelDB(
                        m_Def.sAnimationModel,
                        hAnimationModel) == LT_OK)
                    {
                        FT_CopyInfectedString(
                            modelOCS.m_Filenames[1],
                            MAX_CS_FILENAME_LEN,
                            m_Def.sAnimationModel);
                    }
                }

                if(_stricmp(
                    m_Def.sFaceMode,
                    "child_model") == 0 &&
                   m_Def.sFaceModel[0])
                {
                    HMODELDB hFaceModel = LTNULL;
                    if(g_pLTSModel->CacheModelDB(
                        m_Def.sFaceModel,
                        hFaceModel) == LT_OK)
                    {
                        FT_CopyInfectedString(
                            modelOCS.m_Filenames[2],
                            MAX_CS_FILENAME_LEN,
                            m_Def.sFaceModel);
                        FT_CopyInfectedString(
                            modelOCS.m_SkinNames[2],
                            MAX_CS_FILENAME_LEN,
                            m_Def.sFaceTexture);
                    }
                }

                if(m_Def.sHeadModel[0])
                {
                    HMODELDB hHeadModel = LTNULL;
                    if(g_pLTSModel->CacheModelDB(
                        m_Def.sHeadModel,
                        hHeadModel) == LT_OK)
                    {
                        FT_CopyInfectedString(
                            modelOCS.m_Filenames[3],
                            MAX_CS_FILENAME_LEN,
                            m_Def.sHeadModel);
                        FT_CopyInfectedString(
                            modelOCS.m_SkinNames[3],
                            MAX_CS_FILENAME_LEN,
                            m_Def.sHeadTexture);
                    }
                }

                LTRESULT nModelResult =
                    g_pLTSCommon->SetObjectFilenames(
                        m_hObject,
                        &modelOCS);

                g_pLTServer->CPrint(
                    "Fireteam infected: post-create model composition %s.",
                    nModelResult == LT_OK ? "OK" : "FAILED");

                g_pLTServer->CPrint(
                    "Fireteam infected: content %s body=%s health=%u speed=%.1f",
                    m_Def.sId,
                    m_Def.sBodyModel,
                    (uint32)m_Def.nHealth,
                    m_Def.fRunSpeed);

                FT_AuditZombieAnimationNames(
                    m_hObject);

                SetZombieAnimation(
                    m_Def.sIdleAnim,
                    true);

                CreateInfectedFace();

                if(FT_IsZombieWallhackActive())
                {
                    ApplyWallhackRenderStyle(
                        true);
                }

                LTVector vHumanDims(
                    m_Def.fCollisionX,
                    m_Def.fCollisionY,
                    m_Def.fCollisionZ);

                if(_stricmp(
                    m_Def.sCollisionMode,
                    "model") == 0)
                {
                    LTVector vModelDims;
                    if(g_pLTSCommon->GetModelAnimUserDims(
                        m_hObject,
                        &vModelDims,
                        g_pLTServer->GetModelAnimation(m_hObject)) == LT_OK &&
                       vModelDims.x > 1.0f &&
                       vModelDims.y > 1.0f &&
                       vModelDims.z > 1.0f &&
                       vModelDims.x < 200.0f &&
                       vModelDims.y < 300.0f &&
                       vModelDims.z < 200.0f)
                    {
                        vHumanDims = vModelDims;
                    }
                }

                m_vCollisionDims = vHumanDims;

                g_pLTSPhysics->SetObjectDims(
                    m_hObject,
                    &m_vCollisionDims,
                    0);

                g_pLTServer->CPrint(
                    "Fireteam infected: collision half-dims %.1f %.1f %.1f mode=%s face=%s",
                    m_vCollisionDims.x,
                    m_vCollisionDims.y,
                    m_vCollisionDims.z,
                    m_Def.sCollisionMode[0]
                        ? m_Def.sCollisionMode
                        : "config",
                    m_Def.sFaceMode[0]
                        ? m_Def.sFaceMode
                        : "attachment");
            }

            g_pLTServer->GetObjectPos(m_hObject, &m_vLastPos);
            m_vStragglerProgressPos = m_vLastPos;
            m_vMotionGoal = m_vLastPos;

            const float fAIStep =
                m_Def.fUpdateSeconds > 0.0f
                ? m_Def.fUpdateSeconds : 0.10f;
            const uint8 nSubsteps =
                m_Def.nMotionSubsteps > 1
                ? (m_Def.nMotionSubsteps > 4 ? 4 : m_Def.nMotionSubsteps)
                : 1;
            m_fLastServerTick = g_pLTServer->GetTime();
            m_fDecisionElapsed =
                fAIStep - (fAIStep / (float)nSubsteps);
            m_nTicksUntilDecision = 0;
            m_nMotionStepsRemaining = 0;
            g_pLTServer->SetNextUpdate(
                m_hObject, fAIStep / (float)nSubsteps);
        }
        break;

        case MID_UPDATE:
        {
            const float fAIStep =
                m_Def.fUpdateSeconds > 0.0f
                ? m_Def.fUpdateSeconds : 0.10f;
            const uint8 nSubsteps =
                m_Def.nMotionSubsteps > 1
                ? (m_Def.nMotionSubsteps > 4 ? 4 : m_Def.nMotionSubsteps)
                : 1;
            const float fMotionStep =
                fAIStep / (float)nSubsteps;

            const float fNow = g_pLTServer->GetTime();
            float fElapsed = fNow - m_fLastServerTick;
            m_fLastServerTick = fNow;
            if(fElapsed < 0.0f) fElapsed = 0.0f;
            // Clamp hitches: collision moves must never leap far ahead.
            if(fElapsed > 0.15f) fElapsed = 0.15f;

            if(m_bDying)
            {
                m_nMotionStepsRemaining = 0;
                m_fDeathTimeRemaining -= fElapsed;
                if(m_fDeathTimeRemaining <= 0.0f)
                {
                    g_pLTServer->RemoveObject(m_hObject);
                    return 1;
                }
            }
            else
            {
                m_fDecisionElapsed += fElapsed;
                if(m_nTicksUntilDecision == 0)
                {
                    float fDecisionDelta = m_fDecisionElapsed;
                    if(fDecisionDelta > fAIStep * 2.0f)
                        fDecisionDelta = fAIStep * 2.0f;
                    UpdateZombie(fDecisionDelta);
                    m_fDecisionElapsed = 0.0f;
                    m_nTicksUntilDecision = nSubsteps;
                }

                if(nSubsteps > 1)
                    AdvanceSmoothMotion();
                if(m_nTicksUntilDecision > 0)
                    --m_nTicksUntilDecision;
            }

            g_pLTServer->SetNextUpdate(m_hObject, fMotionStep);
        }
        break;

        default:
            break;
    }

    return BaseClass::EngineMessageFn(messageID, pData, fData);
}

uint32 FireteamZombie::ObjectMessageFn(HOBJECT hSender, ILTMessage_Read *pMsg)
{
    pMsg->SeekTo(0);
    uint32 messageID = pMsg->Readuint32();

    if((messageID == OBJ_MID_DAMAGE ||
        messageID == OBJ_MID_DAMAGE_REGIONAL) &&
       !m_bDying)
    {
        const uint8 nDamage =
            pMsg->Readuint8();

        uint8 nKillingHitRegion = 0;
        if(messageID ==
           OBJ_MID_DAMAGE_REGIONAL)
        {
            nKillingHitRegion =
                pMsg->Readuint8();
        }

        m_nHealth =
            (nDamage >= m_nHealth)
            ? 0
            : (uint16)(
                m_nHealth - nDamage);

        if(m_nHealth == 0)
        {
            m_bDying = true;
            m_fDeathTimeRemaining =
                m_Def.fDeathSeconds > 0.0f
                ? m_Def.fDeathSeconds
                : 1.8f;

            LTVector vStop(
                0.0f,
                0.0f,
                0.0f);
            g_pLTSPhysics->SetVelocity(
                m_hObject,
                &vStop);

            // A dying infected becomes a visual corpse immediately so it does
            // not block teammates/players while its death animation finishes.
            g_pLTSCommon->SetObjectFlags(
                m_hObject,
                OFT_Flags,
                0,
                FLAG_SOLID);
            g_pLTSCommon->SetObjectFlags(
                m_hObject,
                OFT_Flags2,
                0,
                FLAG2_PLAYERCOLLIDE);

            SetZombieAnimation(
                m_Def.sDeathAnim,
                false);

            PlayVoiceSound(
                m_Def.sVoiceDeath);

            LTVector vDeathPos;
            g_pLTServer->GetObjectPos(
                m_hObject,
                &vDeathPos);

            FT_MaybeSpawnMutationBoxOnKill(
                vDeathPos);

            FT_OnFireteamEnemyKilled();

            if(hSender)
            {
                HCLASS hPlayerClass =
                    g_pLTServer->GetClass(
                        "CPlayerSrvr");
                HCLASS hSenderClass =
                    g_pLTServer->GetObjectClass(
                        hSender);

                if(hPlayerClass &&
                   hSenderClass &&
                   g_pLTServer->IsKindOf(
                       hSenderClass,
                       hPlayerClass))
                {
                    ILTMessage_Write *pKill =
                        LTNULL;

                    if(g_pLTSCommon->CreateMessage(
                        pKill) == LT_OK &&
                       pKill)
                    {
                        pKill->IncRef();
                        pKill->Writeuint32(
                            OBJ_MID_KILLSCORE_INFECTED);
                        pKill->Writefloat(
                            0.0f);
                        pKill->Writeuint8(
                            nKillingHitRegion);
                        // Authoritative zombie subtype for local match
                        // receipts and the future per-type kill leaderboard.
                        pKill->WriteString(
                            m_Def.sId[0] ? m_Def.sId : "unknown");
                        g_pLTServer->SendToObject(
                            pKill->Read(),
                            m_hObject,
                            hSender,
                            0);
                        pKill->DecRef();
                    }
                }
            }

            g_pLTServer->CPrint(
                "Fireteam: infected %s killed; death anim=%s %.2fs.",
                m_bDefLoaded
                    ? m_Def.sId
                    : "unknown",
                m_Def.sDeathAnim[0]
                    ? m_Def.sDeathAnim
                    : "<none>",
                m_fDeathTimeRemaining);

            return 1;
        }
    }

    return BaseClass::ObjectMessageFn(hSender, pMsg);
}