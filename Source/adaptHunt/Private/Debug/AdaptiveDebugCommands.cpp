#include "Debug/AdaptiveDebugCommands.h"

#include "adaptHunt.h"
#include "Characters/AdaptiveEnemyCharacter.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CombatComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Data/CombatDataset.h"
#include "Data/TrainingSample.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/AdaptiveGameMode.h"
#include "Game/RoundManager.h"
#include "HAL/IConsoleManager.h"

namespace
{
FString NormalizeToken(const FString& Value)
{
    FString Result = Value;
    Result.TrimStartAndEndInline();
    Result.ReplaceInline(TEXT("_"), TEXT(""));
    Result.ReplaceInline(TEXT("-"), TEXT(""));
    return Result.ToLower();
}

EPlayerCombatAction GetPresetPlayerAction(
    const EAdaptiveDebugProfilePreset Preset
)
{
    switch (Preset)
    {
    case EAdaptiveDebugProfilePreset::Blocking:
        return EPlayerCombatAction::Block;
    case EAdaptiveDebugProfilePreset::Healing:
        return EPlayerCombatAction::Heal;
    case EAdaptiveDebugProfilePreset::LightAttacks:
        return EPlayerCombatAction::LightAttack;
    case EAdaptiveDebugProfilePreset::HeavyAttacks:
        return EPlayerCombatAction::HeavyAttack;
    case EAdaptiveDebugProfilePreset::DodgeBackward:
        return EPlayerCombatAction::DodgeBackward;
    case EAdaptiveDebugProfilePreset::DodgeLeft:
        return EPlayerCombatAction::DodgeLeft;
    case EAdaptiveDebugProfilePreset::DodgeRight:
        return EPlayerCombatAction::DodgeRight;
    default:
        return EPlayerCombatAction::None;
    }
}

template <typename ActorType>
ActorType* FindFirstActor(UWorld* World)
{
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ActorType> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            return *It;
        }
    }
    return nullptr;
}

AAdaptiveGameMode* GetAdaptiveGameMode(UWorld* World)
{
    return World ? Cast<AAdaptiveGameMode>(World->GetAuthGameMode()) : nullptr;
}

AAdaptiveEnemyCharacter* GetAdaptiveEnemy(UWorld* World)
{
    if (AAdaptiveGameMode* GameMode = GetAdaptiveGameMode(World))
    {
        if (AAdaptiveEnemyCharacter* Enemy = GameMode->GetSpawnedEnemy())
        {
            return Enemy;
        }
    }
    return FindFirstActor<AAdaptiveEnemyCharacter>(World);
}

void ForceTacticalProfileCommand(
    const TArray<FString>& Args,
    UWorld* World
)
{
    EAdaptiveDebugProfilePreset Preset;
    if (Args.IsEmpty()
        || !FAdaptiveDebugCommandPolicy::TryParseProfilePreset(
            Args[0],
            Preset
        ))
    {
        UE_LOG(
            LogAdaptHunt,
            Warning,
            TEXT("Usage: adapthunt.ForceTacticalProfile <clear|block|heal|light|heavy|dodgeback|dodgeleft|dodgeright>")
        );
        return;
    }

    AAdaptiveEnemyCharacter* Enemy = GetAdaptiveEnemy(World);
    UEnemyDecisionComponent* Decision = Enemy
        ? Enemy->GetEnemyDecisionComponent()
        : nullptr;
    if (!Decision)
    {
        UE_LOG(LogAdaptHunt, Warning, TEXT("No adaptive enemy is available."));
        return;
    }

    const FAdaptiveTacticalProfile Profile =
        FAdaptiveDebugCommandPolicy::BuildForcedTacticalProfile(
            Preset,
            Decision->GetAdaptiveTacticalProfileTuning()
        );
    const bool bApplied =
        Decision->DebugForceAdaptiveTacticalProfile(Profile);
    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Debug tactical profile '%s': %s."),
        *Args[0],
        bApplied ? TEXT("applied") : TEXT("rejected")
    );
}

void ForceActionPhaseCommand(
    const TArray<FString>& Args,
    UWorld* World
)
{
    ECombatActionPhase Phase;
    if (Args.Num() < 2
        || !FAdaptiveDebugCommandPolicy::TryParseActionPhase(
            Args[1],
            Phase
        ))
    {
        UE_LOG(
            LogAdaptHunt,
            Warning,
            TEXT("Usage: adapthunt.ForceActionPhase <player|enemy> <idle|windup|active|recovery|blocking|dodging|staggered|dead> [duration]")
        );
        return;
    }

    float Duration = Args.Num() >= 3 ? FCString::Atof(*Args[2]) : 1.0f;
    Duration = FMath::IsFinite(Duration)
        ? FMath::Clamp(Duration, 0.0f, 60.0f)
        : 1.0f;
    const FString Combatant = NormalizeToken(Args[0]);
    bool bApplied = false;
    if (Combatant == TEXT("player"))
    {
        AAdaptivePlayerCharacter* Player =
            FindFirstActor<AAdaptivePlayerCharacter>(World);
        UCombatComponent* Combat = Player
            ? Player->GetCombatComponent()
            : nullptr;
        bApplied = Combat && Combat->DebugForceActionPhase(Phase, Duration);
    }
    else if (Combatant == TEXT("enemy"))
    {
        AAdaptiveEnemyCharacter* Enemy = GetAdaptiveEnemy(World);
        UEnemyCombatComponent* Combat = Enemy
            ? Enemy->GetEnemyCombatComponent()
            : nullptr;
        bApplied = Combat && Combat->DebugForceActionPhase(Phase, Duration);
    }

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Debug action phase %s/%s for %.2fs: %s."),
        Args.Num() > 0 ? *Args[0] : TEXT("unknown"),
        Args.Num() > 1 ? *Args[1] : TEXT("unknown"),
        Duration,
        bApplied ? TEXT("applied") : TEXT("rejected")
    );
}

void ForceRoundTransitionCommand(
    const TArray<FString>& Args,
    UWorld* World
)
{
    EAdaptiveDebugRoundTransition Transition;
    if (Args.IsEmpty()
        || !FAdaptiveDebugCommandPolicy::TryParseRoundTransition(
            Args[0],
            Transition
        ))
    {
        UE_LOG(
            LogAdaptHunt,
            Warning,
            TEXT("Usage: adapthunt.ForceRoundTransition <start|playerwins|enemywins|advance|restart>")
        );
        return;
    }

    AAdaptiveGameMode* GameMode = GetAdaptiveGameMode(World);
    URoundManager* RoundManager = GameMode
        ? GameMode->GetRoundManager()
        : nullptr;
    bool bApplied = false;
    if (RoundManager)
    {
        switch (Transition)
        {
        case EAdaptiveDebugRoundTransition::Start:
            bApplied = RoundManager->DebugStartCurrentRoundNow();
            break;
        case EAdaptiveDebugRoundTransition::PlayerWins:
            bApplied = RoundManager->ForceEndCurrentRound(
                EAdaptiveRoundWinner::Player
            );
            break;
        case EAdaptiveDebugRoundTransition::EnemyWins:
            bApplied = RoundManager->ForceEndCurrentRound(
                EAdaptiveRoundWinner::Enemy
            );
            break;
        case EAdaptiveDebugRoundTransition::Advance:
            bApplied = RoundManager->AdvanceToNextRoundNow();
            break;
        case EAdaptiveDebugRoundTransition::Restart:
            bApplied = RoundManager->RestartMatch();
            break;
        default:
            break;
        }
    }

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Debug round transition '%s': %s."),
        *Args[0],
        bApplied ? TEXT("applied") : TEXT("not valid in the current phase")
    );
}

void ClearPersistentLearningCommand(
    const TArray<FString>& Args,
    UWorld* World
)
{
    static_cast<void>(Args);
    AAdaptiveGameMode* GameMode = GetAdaptiveGameMode(World);
    URoundManager* RoundManager = GameMode
        ? GameMode->GetRoundManager()
        : nullptr;
    const bool bCleared = RoundManager
        && RoundManager->ClearPersistentPlayerPatterns();
    if (bCleared)
    {
        UE_LOG(
            LogAdaptHunt,
            Log,
            TEXT("Debug persistent learning clear: complete.")
        );
    }
    else
    {
        UE_LOG(
            LogAdaptHunt,
            Warning,
            TEXT("Debug persistent learning clear: failed or unavailable.")
        );
    }
}

FAutoConsoleCommandWithWorldAndArgs ForceTacticalProfileConsoleCommand(
    TEXT("adapthunt.ForceTacticalProfile"),
    TEXT("Force an evidence-valid tactical profile preset, or clear it."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &ForceTacticalProfileCommand
    )
);

FAutoConsoleCommandWithWorldAndArgs ForceActionPhaseConsoleCommand(
    TEXT("adapthunt.ForceActionPhase"),
    TEXT("Force a non-damaging player or enemy action phase for inspection."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &ForceActionPhaseCommand
    )
);

FAutoConsoleCommandWithWorldAndArgs ForceRoundTransitionConsoleCommand(
    TEXT("adapthunt.ForceRoundTransition"),
    TEXT("Force a valid match-flow transition for vertical-slice testing."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &ForceRoundTransitionCommand
    )
);

FAutoConsoleCommandWithWorldAndArgs ClearPersistentLearningConsoleCommand(
    TEXT("adapthunt.ClearPersistentLearning"),
    TEXT("Delete bounded prior-run player patterns; live match data remains."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(
        &ClearPersistentLearningCommand
    )
);
}


bool FAdaptiveDebugCommandPolicy::TryParseProfilePreset(
    const FString& Value,
    EAdaptiveDebugProfilePreset& OutPreset
)
{
    const FString Token = NormalizeToken(Value);
    if (Token == TEXT("clear") || Token == TEXT("none")
        || Token == TEXT("baseline"))
    {
        OutPreset = EAdaptiveDebugProfilePreset::Clear;
        return true;
    }
    if (Token == TEXT("block") || Token == TEXT("blocking"))
    {
        OutPreset = EAdaptiveDebugProfilePreset::Blocking;
        return true;
    }
    if (Token == TEXT("heal") || Token == TEXT("healing"))
    {
        OutPreset = EAdaptiveDebugProfilePreset::Healing;
        return true;
    }
    if (Token == TEXT("light") || Token == TEXT("lightattacks"))
    {
        OutPreset = EAdaptiveDebugProfilePreset::LightAttacks;
        return true;
    }
    if (Token == TEXT("heavy") || Token == TEXT("heavyattacks"))
    {
        OutPreset = EAdaptiveDebugProfilePreset::HeavyAttacks;
        return true;
    }
    if (Token == TEXT("dodgeback") || Token == TEXT("dodgebackward"))
    {
        OutPreset = EAdaptiveDebugProfilePreset::DodgeBackward;
        return true;
    }
    if (Token == TEXT("dodgeleft"))
    {
        OutPreset = EAdaptiveDebugProfilePreset::DodgeLeft;
        return true;
    }
    if (Token == TEXT("dodgeright"))
    {
        OutPreset = EAdaptiveDebugProfilePreset::DodgeRight;
        return true;
    }
    return false;
}

bool FAdaptiveDebugCommandPolicy::TryParseActionPhase(
    const FString& Value,
    ECombatActionPhase& OutPhase
)
{
    const FString Token = NormalizeToken(Value);
    if (Token == TEXT("idle"))
    {
        OutPhase = ECombatActionPhase::Idle;
    }
    else if (Token == TEXT("windup"))
    {
        OutPhase = ECombatActionPhase::Windup;
    }
    else if (Token == TEXT("active"))
    {
        OutPhase = ECombatActionPhase::Active;
    }
    else if (Token == TEXT("recovery"))
    {
        OutPhase = ECombatActionPhase::Recovery;
    }
    else if (Token == TEXT("blocking") || Token == TEXT("block"))
    {
        OutPhase = ECombatActionPhase::Blocking;
    }
    else if (Token == TEXT("dodging") || Token == TEXT("dodge"))
    {
        OutPhase = ECombatActionPhase::Dodging;
    }
    else if (Token == TEXT("staggered") || Token == TEXT("stagger"))
    {
        OutPhase = ECombatActionPhase::Staggered;
    }
    else if (Token == TEXT("dead") || Token == TEXT("death"))
    {
        OutPhase = ECombatActionPhase::Dead;
    }
    else
    {
        return false;
    }
    return true;
}

bool FAdaptiveDebugCommandPolicy::TryParseRoundTransition(
    const FString& Value,
    EAdaptiveDebugRoundTransition& OutTransition
)
{
    const FString Token = NormalizeToken(Value);
    if (Token == TEXT("start") || Token == TEXT("skipcountdown"))
    {
        OutTransition = EAdaptiveDebugRoundTransition::Start;
    }
    else if (Token == TEXT("playerwins") || Token == TEXT("player"))
    {
        OutTransition = EAdaptiveDebugRoundTransition::PlayerWins;
    }
    else if (Token == TEXT("enemywins") || Token == TEXT("enemy"))
    {
        OutTransition = EAdaptiveDebugRoundTransition::EnemyWins;
    }
    else if (Token == TEXT("advance") || Token == TEXT("next"))
    {
        OutTransition = EAdaptiveDebugRoundTransition::Advance;
    }
    else if (Token == TEXT("restart"))
    {
        OutTransition = EAdaptiveDebugRoundTransition::Restart;
    }
    else
    {
        return false;
    }
    return true;
}

FAdaptiveTacticalProfile
FAdaptiveDebugCommandPolicy::BuildForcedTacticalProfile(
    const EAdaptiveDebugProfilePreset Preset,
    const FAdaptiveTacticalProfileTuning& Tuning
)
{
    const EPlayerCombatAction Action = GetPresetPlayerAction(Preset);
    if (Action == EPlayerCombatAction::None)
    {
        return FAdaptiveTacticalProfile();
    }

    const FAdaptiveTacticalProfileTuning SafeTuning =
        Tuning.GetSanitized();
    const int32 SampleCount = FMath::Max3(
        SafeTuning.MinimumRoundSamples,
        SafeTuning.MinimumContextSamples,
        SafeTuning.MinimumSupportingSamples
    );
    FCombatDataset Dataset;
    for (int32 Index = 0; Index < SampleCount; ++Index)
    {
        FTrainingSample Sample;
        Sample.Snapshot.RoundNumber = 1;
        Sample.NextPlayerAction = Action;
        Dataset.AddSample(Sample);
    }

    FPredictionResult Prediction;
    Prediction.bHasPrediction = true;
    Prediction.PredictedAction = Action;
    Prediction.Confidence = 1.0f;
    Prediction.SupportingSampleCount = SampleCount;
    return FAdaptiveTacticalProfilePolicy::Derive(
        Dataset,
        1,
        Prediction,
        SafeTuning
    );
}
