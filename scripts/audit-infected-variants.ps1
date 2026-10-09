param([string]$RepoRoot)

$ErrorActionPreference = "Stop"
if([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Join-Path $PSScriptRoot ".."
}
$repo = (Resolve-Path -LiteralPath $RepoRoot).Path
$rez = Join-Path $repo "BUILT\rez"
if(-not (Test-Path -LiteralPath $rez -PathType Container)) {
    throw "Missing BUILT\rez. Run build.cmd to stage local CA assets first."
}

$models = @(
    [pscustomobject]@{
        id = "normal_common"
        body = "MT_AR_BODY.LTB"
        animation = "ST_M_CHILD.LTB"
        textures = @("MT_AR_BODY.DTX", "MT_MG_LEG.DTX")
    },
    [pscustomobject]@{
        id = "infected_assassin"
        body = "VIW_F_NM_DF_ASSASSIN_CH.LTB"
        animation = "ANI_VI_ASSASSIN_CH.LTB"
        textures = @(
            "CW_VST_ASSAVIRUS_HM.DTX",
            "CW_LG_ASSAVIRUS_HM.DTX",
            "CW_FC_NM_VIRUS_HM.DTX")
    },
    [pscustomobject]@{
        id = "infected_tanker"
        body = "VIM_F_NM_DF_TANKER_SH.LTB"
        animation = "ANI_VI_TANKER_SH.LTB"
        textures = @(
            "CM_FC_TANKERBLUE_YK.DTX",
            "CM_LG_TANKERBLUE_YK.DTX",
            "CM_VST_TANKERBLUE_YK.DTX")
    }
)

$reportModels = foreach($model in $models) {
    $body = Join-Path $rez ("Characters\infected\body\" + $model.body)
    $animation = Join-Path $rez ("Characters\infected\body\" + $model.animation)
    $report = Join-Path $rez (
        "Fireteam\" + [IO.Path]::GetFileNameWithoutExtension($model.animation) +
        "-strings.txt")
    $hasBody = Test-Path -LiteralPath $body -PathType Leaf
    $hasAnimation = Test-Path -LiteralPath $animation -PathType Leaf
    $hasCandidateStrings = Test-Path -LiteralPath $report -PathType Leaf
    $missingTextures = @(
        foreach($tex in $model.textures) {
            if(-not (Test-Path -LiteralPath (
                Join-Path $rez ("Characters\infected\body\" + $tex)) -PathType Leaf)) {
                $tex
            }
        }
    )
    $hasTextures = $missingTextures.Count -eq 0
    Write-Host ("[{0}] {1}: body={2} animation={3} skins={4} candidate-report={5}" -f
        $(if($hasBody -and $hasAnimation -and $hasTextures) {"READY"} else {"MISSING"}),
        $model.id, $hasBody, $hasAnimation, $hasTextures, $hasCandidateStrings)
    [pscustomobject]@{
        id = $model.id
        bodyPath = $model.body
        bodyPresent = $hasBody
        animationBank = $model.animation
        animationPresent = $hasAnimation
        animationCandidateReportPresent = $hasCandidateStrings
        missingTextures = $missingTextures
        modelAndAnimationStaged = ($hasBody -and $hasAnimation)
        readyForRuntimeTest = ($hasBody -and $hasAnimation -and $hasTextures)
        # Do not claim a playable zombie before runtime GetAnimIndex, skin,
        # collision dimensions, and hitbox verification.
        runtimeVerified = $false
    }
}

$voiceBase = Join-Path $rez "Snd\COOPMODE\NPC_VOICE\CABINFEVER\NORMAL"
$voice = foreach($name in @("SEEENEMY.WAV", "ATTACK1.WAV",
                            "ATTACK2.WAV", "ATTACK3.WAV", "DEATH.WAV")) {
    $path = Join-Path $voiceBase $name
    $present = Test-Path -LiteralPath $path -PathType Leaf
    if(-not $present) {
        Write-Warning "Configured infected voice missing: $path"
    }
    [pscustomobject]@{
        sound = $name
        expectedPath = "Snd/COOPMODE/NPC_VOICE/CABINFEVER/NORMAL/$name"
        present = $present
    }
}

$summary = [ordered]@{
    schemaVersion = 1
    inspectedAtUtc = [DateTime]::UtcNow.ToString("o")
    rezPath = $rez
    variants = @($reportModels)
    normalInfectedVoice = @($voice)
    limitations = @(
        "File existence and LTB printable tokens do NOT prove animations are playable.",
        "Each candidate needs GetAnimIndex on the composed runtime character.",
        "Textures, collision dimensions, hitboxes and attack timing remain unverified for Assassin/Tanker.",
        "No commercial model/sound bytes are committed or exported by this script."
    )
}

$outputDirectory = Join-Path $repo "assets-local\Reports"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$output = Join-Path $outputDirectory "infected-variants-readiness.json"
$summary | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath $output -Encoding UTF8
Write-Host "[OK] $output"
