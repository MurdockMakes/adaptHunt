#include "Game/RoundManager.h"

#include "adaptHunt.h"
#include "Characters/AdaptiveEnemyCharacter.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Combat/EnemyProjectile.h"
#include "Components/CombatComponent.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/CombatSnapshotComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Components/EnemyLocomotionComponent.h"
#include "Components/GreyboxPresentationComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PlayerBehaviorTrackerComponent.h"
#include "Components/StaminaComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Game/AdaptiveHuntTuningSettings.h"
#include "Kismet/GameplayStatics.h"
#include "CoreGlobals.h"

namespace
{
const FString PersistentPlayerPatternSlot =
    TEXT("AdaptivePlayerPatterns_v1");
constexpr int32 PersistentPlayerPatternUserIndex = 0;

FString GetRoundWinnerName(const EAdaptiveRoundWinner Winner)
{
    const UEnum* WinnerEnum = StaticEnum<EAdaptiveRoundWinner>();
    return WinnerEnum
        ? WinnerEnum->GetNameStringByValue(static_cast<int64>(Winner))
        : TEXT("Unknown");
}

FString GetRoundPlayerActionName(const EPlayerCombatAction Action)
{
    const UEnum* ActionEnum = StaticEnum<EPlayerCombatAction>();
    return ActionEnum
        ? ActionEnum->GetNameStringByValue(static_cast<int64>(Action))
        : TEXT("Unknown");
}
}

FAdaptiveRoundProgression::FAdaptiveRoundProgression()
{
    Reset();
}

void FAdaptiveRoundProgression::Reset()
{
    CurrentRound = 0;
    LastCompletedRound = 0;
    Phase = EAdaptiveRoundPhase::WaitingToStart;
    LastWinner = EAdaptiveRoundWinner::None;
    PlayerRoundWins = 0;
    EnemyRoundWins = 0;
}

bool FAdaptiveRoundProgression::BeginMatch()
{
    if (Phase == EAdaptiveRoundPhase::PreRoundCountdown
        || Phase == EAdaptiveRoundPhase::InProgress
        || Phase == EAdaptiveRoundPhase::Intermission)
    {
        return false;
    }

    CurrentRound = 1;
    LastCompletedRound = 0;
    LastWinner = EAdaptiveRoundWinner::None;
    PlayerRoundWins = 0;
    EnemyRoundWins = 0;
    Phase = EAdaptiveRoundPhase::PreRoundCountdown;
    return true;
}

bool FAdaptiveRoundProgression::StartCurrentRound()
{
    if (Phase != EAdaptiveRoundPhase::PreRoundCountdown
        || CurrentRound <= 0 || CurrentRound > TotalRounds)
    {
        return false;
    }

    Phase = EAdaptiveRoundPhase::InProgress;
    return true;
}

bool FAdaptiveRoundProgression::CompleteCurrentRound(
    const EAdaptiveRoundWinner Winner
)
{
    if (Phase != EAdaptiveRoundPhase::InProgress
        || (Winner != EAdaptiveRoundWinner::Player
            && Winner != EAdaptiveRoundWinner::Enemy))
    {
        return false;
    }

    LastCompletedRound = CurrentRound;
    LastWinner = Winner;
    if (Winner == EAdaptiveRoundWinner::Player)
    {
        ++PlayerRoundWins;
    }
    else
    {
        ++EnemyRoundWins;
    }
    Phase = CurrentRound >= TotalRounds
        ? EAdaptiveRoundPhase::MatchComplete
        : EAdaptiveRoundPhase::Intermission;
    return true;
}

bool FAdaptiveRoundProgression::AdvanceToNextRound()
{
    if (Phase != EAdaptiveRoundPhase::Intermission
        || CurrentRound >= TotalRounds)
    {
        return false;
    }

    ++CurrentRound;
    Phase = EAdaptiveRoundPhase::PreRoundCountdown;
    return true;
}

int32 FAdaptiveRoundProgression::GetCurrentRound() const
{
    return CurrentRound;
}

int32 FAdaptiveRoundProgression::GetLastCompletedRound() const
{
    return LastCompletedRound;
}

EAdaptiveRoundPhase FAdaptiveRoundProgression::GetPhase() const
{
    return Phase;
}

EAdaptiveRoundWinner FAdaptiveRoundProgression::GetLastWinner() const
{
    return LastWinner;
}

EAdaptiveRoundWinner FAdaptiveRoundProgression::GetMatchWinner() const
{
    if (Phase != EAdaptiveRoundPhase::MatchComplete)
    {
        return EAdaptiveRoundWinner::None;
    }
    if (PlayerRoundWins == EnemyRoundWins)
    {
        return EAdaptiveRoundWinner::None;
    }
    return PlayerRoundWins > EnemyRoundWins
        ? EAdaptiveRoundWinner::Player
        : EAdaptiveRoundWinner::Enemy;
}

int32 FAdaptiveRoundProgression::GetPlayerRoundWins() const
{
    return PlayerRoundWins;
}

int32 FAdaptiveRoundProgression::GetEnemyRoundWins() const
{
    return EnemyRoundWins;
}

URoundManager::URoundManager()
    : PlayerCharacter(nullptr)
    , EnemyCharacter(nullptr)
    , PlayerHealthComponent(nullptr)
    , PlayerStaminaComponent(nullptr)
    , PlayerCombatComponent(nullptr)
    , SnapshotComponent(nullptr)
    , BehaviorTrackerComponent(nullptr)
    , EnemyHealthComponent(nullptr)
    , EnemyStaminaComponent(nullptr)
    , EnemyCombatComponent(nullptr)
    , EnemyDecisionComponent(nullptr)
    , bPredictionsEnabledForCurrentRound(false)
    , bInputLockedByRoundManager(false)
    , SavedCurrentSessionSampleCount(0)
    , bInitialized(false)
{
    const FAdaptiveMatchFlowTuning FlowTuning =
        UAdaptiveHuntTuningSettings::Get().MatchFlow.GetSanitized();
    IntermissionDuration = FlowTuning.IntermissionDuration;
    PreRoundCountdownDuration = FlowTuning.PreRoundCountdownDuration;
    const FAdaptiveLearningTuning LearningTuning =
        UAdaptiveHuntTuningSettings::Get().Adaptation.GetSanitized();
    bPersistPlayerPatterns = LearningTuning.bPersistPlayerPatterns;
    MaximumPersistentSamples = LearningTuning.MaximumPersistentSamples;

    PrimaryComponentTick.bCanEverTick = false;
}

bool URoundManager::Initialize(
    AAdaptivePlayerCharacter* NewPlayerCharacter,
    AAdaptiveEnemyCharacter* NewEnemyCharacter
)
{
    Shutdown();

    PlayerCharacter = NewPlayerCharacter;
    EnemyCharacter = NewEnemyCharacter;
    if (!IsValid(PlayerCharacter) || !IsValid(EnemyCharacter)
        || !CacheCombatantComponents())
    {
        UE_LOG(
            LogAdaptHunt,
            Error,
            TEXT("Round manager requires complete adaptive player and enemy combatants.")
        );
        Shutdown();
        return false;
    }

    PlayerStartTransform = PlayerCharacter->GetActorTransform();
    EnemyStartTransform = EnemyCharacter->GetActorTransform();
    BindDeathEvents();

    SnapshotComponent->SetOpponentActor(EnemyCharacter);
    EnemyDecisionComponent->SetTargetActor(PlayerCharacter);
    bInitialized = true;
    if (!StartNewMatch())
    {
        Shutdown();
        return false;
    }
    return true;
}

void URoundManager::Shutdown()
{
    if (bInitialized)
    {
        SaveNewPersistentPlayerPatterns();
    }
    ClearFlowTimers();

    SetIntermissionInputLocked(false);
    UnbindDeathEvents();
    if (IsValid(BehaviorTrackerComponent))
    {
        BehaviorTrackerComponent->StopTracking();
    }
    if (IsValid(EnemyDecisionComponent))
    {
        EnemyDecisionComponent->SetDecisionMakingEnabled(false);
        EnemyDecisionComponent->SetPredictionUsageEnabled(false);
    }
    if (IsValid(PlayerCombatComponent))
    {
        PlayerCombatComponent->SetCombatEnabled(false);
    }
    if (IsValid(EnemyCombatComponent))
    {
        EnemyCombatComponent->SetCombatEnabled(false);
    }

    PlayerCharacter = nullptr;
    EnemyCharacter = nullptr;
    PlayerHealthComponent = nullptr;
    PlayerStaminaComponent = nullptr;
    PlayerCombatComponent = nullptr;
    SnapshotComponent = nullptr;
    BehaviorTrackerComponent = nullptr;
    EnemyHealthComponent = nullptr;
    EnemyStaminaComponent = nullptr;
    EnemyCombatComponent = nullptr;
    EnemyDecisionComponent = nullptr;
    bPredictionsEnabledForCurrentRound = false;
    bInputLockedByRoundManager = false;
    LastRoundObservedPattern = FAdaptiveConditionalPattern();
    AdaptationReveal = FAdaptiveRevealText();
    PersistentPlayerPatterns.Reset();
    SavedCurrentSessionSampleCount = 0;
    bInitialized = false;
    Progression.Reset();
}

bool URoundManager::AdvanceToNextRoundNow()
{
    if (!bInitialized || !Progression.AdvanceToNextRound())
    {
        return false;
    }

    ClearFlowTimers();
    PrepareCurrentRound();
    SchedulePreRoundCountdown();
    return true;
}

bool URoundManager::ForceEndCurrentRound(
    const EAdaptiveRoundWinner Winner
)
{
    return EndCurrentRound(Winner);
}

bool URoundManager::DebugStartCurrentRoundNow()
{
    if (!bInitialized
        || Progression.GetPhase()
            != EAdaptiveRoundPhase::PreRoundCountdown)
    {
        return false;
    }

    ClearFlowTimers();
    BeginCurrentRound();
    return Progression.GetPhase() == EAdaptiveRoundPhase::InProgress;
}

bool URoundManager::RestartMatch()
{
    if (!bInitialized
        || Progression.GetPhase() != EAdaptiveRoundPhase::MatchComplete)
    {
        return false;
    }
    return StartNewMatch();
}

bool URoundManager::IsInitialized() const
{
    return bInitialized;
}

int32 URoundManager::GetCurrentRound() const
{
    return Progression.GetCurrentRound();
}

int32 URoundManager::GetTotalRounds() const
{
    return FAdaptiveRoundProgression::TotalRounds;
}

int32 URoundManager::GetLastCompletedRound() const
{
    return Progression.GetLastCompletedRound();
}

EAdaptiveRoundPhase URoundManager::GetRoundPhase() const
{
    return Progression.GetPhase();
}

EAdaptiveRoundWinner URoundManager::GetLastWinner() const
{
    return Progression.GetLastWinner();
}

EAdaptiveRoundWinner URoundManager::GetMatchWinner() const
{
    return Progression.GetMatchWinner();
}

int32 URoundManager::GetPlayerRoundWins() const
{
    return Progression.GetPlayerRoundWins();
}

int32 URoundManager::GetEnemyRoundWins() const
{
    return Progression.GetEnemyRoundWins();
}

bool URoundManager::ArePredictionsEnabledForCurrentRound() const
{
    return bPredictionsEnabledForCurrentRound;
}

int32 URoundManager::GetCollectedSampleCount() const
{
    return BehaviorTrackerComponent
        ? BehaviorTrackerComponent->GetSampleCount()
        : 0;
}

int32 URoundManager::GetTrainedSampleCount() const
{
    return EnemyDecisionComponent
        ? EnemyDecisionComponent->GetTrainedSampleCount()
        : 0;
}

int32 URoundManager::GetPersistentPlayerPatternCount() const
{
    return PersistentPlayerPatterns.Num();
}

bool URoundManager::IsPersistentPlayerLearningEnabled() const
{
    return bPersistPlayerPatterns;
}

bool URoundManager::ClearPersistentPlayerPatterns()
{
    bool bDeleted = true;
    if (!GIsAutomationTesting
        && UGameplayStatics::DoesSaveGameExist(
            PersistentPlayerPatternSlot,
            PersistentPlayerPatternUserIndex
        ))
    {
        bDeleted = UGameplayStatics::DeleteGameInSlot(
            PersistentPlayerPatternSlot,
            PersistentPlayerPatternUserIndex
        );
    }

    PersistentPlayerPatterns.Reset();
    SavedCurrentSessionSampleCount = 0;
    if (IsValid(EnemyDecisionComponent))
    {
        EnemyDecisionComponent->ClearPersistentTrainingDataset();
        if (IsValid(BehaviorTrackerComponent)
            && BehaviorTrackerComponent->GetSampleCount() > 0)
        {
            EnemyDecisionComponent->TrainPredictor(
                BehaviorTrackerComponent->GetDataset()
            );
        }
    }

    if (bDeleted)
    {
        UE_LOG(
            LogAdaptHunt,
            Log,
            TEXT("Persistent player-pattern learning cleared; current-session samples remain available.")
        );
    }
    else
    {
        UE_LOG(
            LogAdaptHunt,
            Warning,
            TEXT("Persistent player-pattern learning could not be deleted; current-session samples remain available.")
        );
    }
    return bDeleted;
}

const FAdaptiveConditionalPattern&
URoundManager::GetLastRoundObservedPattern() const
{
    return LastRoundObservedPattern;
}

float URoundManager::GetIntermissionDuration() const
{
    return IntermissionDuration;
}

float URoundManager::GetIntermissionRemainingTime() const
{
    if (Progression.GetPhase() != EAdaptiveRoundPhase::Intermission)
    {
        return 0.0f;
    }
    const UWorld* World = GetWorld();
    if (!World)
    {
        return 0.0f;
    }
    const float Remaining =
        World->GetTimerManager().GetTimerRemaining(NextRoundTimer);
    return FMath::IsFinite(Remaining) ? FMath::Max(0.0f, Remaining) : 0.0f;
}

float URoundManager::GetPreRoundCountdownDuration() const
{
    return PreRoundCountdownDuration;
}

float URoundManager::GetPreRoundCountdownRemainingTime() const
{
    if (Progression.GetPhase() != EAdaptiveRoundPhase::PreRoundCountdown)
    {
        return 0.0f;
    }
    const UWorld* World = GetWorld();
    if (!World)
    {
        return 0.0f;
    }
    const float Remaining = World->GetTimerManager().GetTimerRemaining(
        PreRoundCountdownTimer
    );
    return FMath::IsFinite(Remaining) ? FMath::Max(0.0f, Remaining) : 0.0f;
}

const FAdaptiveRevealText& URoundManager::GetAdaptationReveal() const
{
    return AdaptationReveal;
}

bool URoundManager::IsPredictionRound(const int32 RoundNumber)
{
    return RoundNumber >= 1
        && RoundNumber <= FAdaptiveRoundProgression::TotalRounds;
}

bool URoundManager::ShouldTrainAfterRound(const int32 CompletedRoundNumber)
{
    return CompletedRoundNumber >= 1
        && CompletedRoundNumber < FAdaptiveRoundProgression::TotalRounds;
}

void URoundManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Shutdown();
    Super::EndPlay(EndPlayReason);
}

bool URoundManager::CacheCombatantComponents()
{
    PlayerHealthComponent = PlayerCharacter->GetHealthComponent();
    PlayerStaminaComponent = PlayerCharacter->GetStaminaComponent();
    PlayerCombatComponent = PlayerCharacter->GetCombatComponent();
    SnapshotComponent = PlayerCharacter->GetCombatSnapshotComponent();
    BehaviorTrackerComponent = PlayerCharacter->GetBehaviorTrackerComponent();

    EnemyHealthComponent = EnemyCharacter->GetHealthComponent();
    EnemyStaminaComponent = EnemyCharacter->GetStaminaComponent();
    EnemyCombatComponent = EnemyCharacter->GetEnemyCombatComponent();
    EnemyDecisionComponent = EnemyCharacter->GetEnemyDecisionComponent();

    return PlayerHealthComponent && PlayerStaminaComponent
        && PlayerCombatComponent && SnapshotComponent
        && BehaviorTrackerComponent && EnemyHealthComponent
        && EnemyStaminaComponent && EnemyCombatComponent
        && EnemyDecisionComponent;
}

void URoundManager::BindDeathEvents()
{
    UnbindDeathEvents();
    PlayerHealthComponent->OnDeath.AddUObject(
        this,
        &URoundManager::HandlePlayerDeath
    );
    EnemyHealthComponent->OnDeath.AddUObject(
        this,
        &URoundManager::HandleEnemyDeath
    );
}

void URoundManager::UnbindDeathEvents()
{
    if (IsValid(PlayerHealthComponent))
    {
        PlayerHealthComponent->OnDeath.RemoveAll(this);
    }
    if (IsValid(EnemyHealthComponent))
    {
        EnemyHealthComponent->OnDeath.RemoveAll(this);
    }
}

bool URoundManager::StartNewMatch()
{
    ResetMatchScopedState();
    LoadPersistentPlayerPatterns();
    if (!Progression.BeginMatch())
    {
        return false;
    }

    PrepareCurrentRound();
    SchedulePreRoundCountdown();
    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT(
            "New match initialized: Round 1 live adaptation, "
            "persistent patterns=%d, session learning=clear, "
            "outcomes=clear, projectiles=clear, timers=clear, "
            "buffered_input=clear, presentation=clear, reveal=clear."
        ),
        PersistentPlayerPatterns.Num()
    );
    return true;
}

void URoundManager::ResetMatchScopedState()
{
    ClearFlowTimers();
    if (IsValid(BehaviorTrackerComponent))
    {
        BehaviorTrackerComponent->StopTracking();
    }
    if (IsValid(EnemyDecisionComponent))
    {
        EnemyDecisionComponent->SetDecisionMakingEnabled(false);
        EnemyDecisionComponent->SetPredictionUsageEnabled(false);
        EnemyDecisionComponent->SetAutomaticRetrainingEnabled(false);
    }
    if (IsValid(PlayerCombatComponent))
    {
        PlayerCombatComponent->SetCombatEnabled(false);
        PlayerCombatComponent->ResetCombatState();
    }
    if (IsValid(EnemyCombatComponent))
    {
        EnemyCombatComponent->SetCombatEnabled(false);
        EnemyCombatComponent->ResetCombatState();
    }

    // Projectiles report their terminal result before the match-scoped outcome
    // history is cleared, so no in-flight actor can leak into the new match.
    RemoveLingeringProjectiles();
    if (IsValid(EnemyDecisionComponent))
    {
        EnemyDecisionComponent->ResetPredictor();
        EnemyDecisionComponent->ClearPersistentTrainingDataset();
        EnemyDecisionComponent->ResetCounterOutcomeTracking();
    }
    if (IsValid(BehaviorTrackerComponent))
    {
        BehaviorTrackerComponent->ResetDataset();
    }
    if (IsValid(SnapshotComponent))
    {
        SnapshotComponent->ResetRoundState();
    }

    LastRoundObservedPattern = FAdaptiveConditionalPattern();
    AdaptationReveal = FAdaptiveRevealText();
    PersistentPlayerPatterns.Reset();
    SavedCurrentSessionSampleCount = 0;
    bPredictionsEnabledForCurrentRound = false;
    Progression.Reset();
}

void URoundManager::PrepareCurrentRound()
{
    ResetCombatantsForRound();
    const int32 RoundNumber = Progression.GetCurrentRound();
    EnemyDecisionComponent->ResetShortTermDecisionMemory();
    SnapshotComponent->ResetRoundState();
    SnapshotComponent->SetRoundNumber(RoundNumber);
    BehaviorTrackerComponent->StopTracking();
    EnemyDecisionComponent->SetDecisionMakingEnabled(false);
    EnemyDecisionComponent->SetPredictionUsageEnabled(false);
    EnemyDecisionComponent->SetAutomaticRetrainingEnabled(false);
    PlayerCombatComponent->SetCombatEnabled(false);
    EnemyCombatComponent->SetCombatEnabled(false);
    bPredictionsEnabledForCurrentRound = false;
    SetIntermissionInputLocked(true);
    StopCombatantMovement();

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Round %d/%d preparing: stage=%s, countdown=%.1fs."),
        RoundNumber,
        FAdaptiveRoundProgression::TotalRounds,
        *FAdaptiveAdaptationRevealPolicy::FormatRoundStage(RoundNumber),
        FMath::Max(0.0f, PreRoundCountdownDuration)
    );
}

void URoundManager::BeginCurrentRound()
{
    if (!bInitialized || !Progression.StartCurrentRound())
    {
        return;
    }

    const int32 RoundNumber = Progression.GetCurrentRound();
    BehaviorTrackerComponent->StartTracking();

    // The channel is live in every round. An empty model has no effect, then
    // committed samples begin influencing play as soon as evidence is valid.
    bPredictionsEnabledForCurrentRound = true;
    EnemyDecisionComponent->SetAutomaticRetrainingEnabled(true);
    EnemyDecisionComponent->SetPredictionUsageEnabled(true);
    EnemyDecisionComponent->SetDecisionMakingEnabled(true);
    PlayerCombatComponent->SetCombatEnabled(true);
    EnemyCombatComponent->SetCombatEnabled(true);
    SetIntermissionInputLocked(false);

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Round %d/%d started: stage=%s, predictions=%s, profile=%s, trained samples=%d, collected samples=%d."),
        RoundNumber,
        FAdaptiveRoundProgression::TotalRounds,
        *FAdaptiveAdaptationRevealPolicy::FormatRoundStage(RoundNumber),
        bPredictionsEnabledForCurrentRound ? TEXT("enabled") : TEXT("disabled"),
        EnemyDecisionComponent->GetAdaptiveTacticalProfile().IsActive()
            ? TEXT("active")
            : TEXT("baseline"),
        EnemyDecisionComponent->GetTrainedSampleCount(),
        BehaviorTrackerComponent->GetSampleCount()
    );

    OnRoundStarted.Broadcast(
        RoundNumber,
        bPredictionsEnabledForCurrentRound,
        EnemyDecisionComponent->GetTrainedSampleCount()
    );
}

bool URoundManager::EndCurrentRound(const EAdaptiveRoundWinner Winner)
{
    if (!bInitialized || !Progression.CompleteCurrentRound(Winner))
    {
        return false;
    }

    BehaviorTrackerComponent->StopTracking();
    EnemyDecisionComponent->SetDecisionMakingEnabled(false);
    EnemyDecisionComponent->SetPredictionUsageEnabled(false);
    EnemyDecisionComponent->SetAutomaticRetrainingEnabled(false);
    PlayerCombatComponent->SetCombatEnabled(false);
    EnemyCombatComponent->SetCombatEnabled(false);
    RemoveLingeringProjectiles();
    bPredictionsEnabledForCurrentRound = false;
    StopCombatantMovement();
    SetIntermissionInputLocked(true);

    const int32 CompletedRound = Progression.GetLastCompletedRound();
    const int32 CollectedSamples = BehaviorTrackerComponent->GetSampleCount();
    SaveNewPersistentPlayerPatterns();
    LastRoundObservedPattern =
        FAdaptiveLearningTelemetryAnalyzer::Analyze(
            BehaviorTrackerComponent->GetDataset(),
            CompletedRound
        ).StrongestConditionalPattern;
    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Round %d observed pattern: %s"),
        CompletedRound,
        *FAdaptiveDebugTelemetryFormatter::FormatConditionalPattern(
            LastRoundObservedPattern
        )
    );
    if (ShouldTrainAfterRound(CompletedRound))
    {
        TrainForNextRound();
    }
    EnemyDecisionComponent->RebuildAdaptiveTacticalProfile(
        BehaviorTrackerComponent->GetDataset(),
        CompletedRound
    );
    AdaptationReveal = FAdaptiveAdaptationRevealPolicy::Build(
        LastRoundObservedPattern,
        EnemyDecisionComponent->GetAdaptiveTacticalProfile()
    );
    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT(
            "Round %d adaptation reveal: %s | %s "
            "[supported_observation=%s active_adjustment=%s]."
        ),
        CompletedRound,
        *AdaptationReveal.Observation,
        *AdaptationReveal.Adjustment,
        AdaptationReveal.bHasSupportedObservation
            ? TEXT("true")
            : TEXT("false"),
        AdaptationReveal.bHasActiveAdjustment
            ? TEXT("true")
            : TEXT("false")
    );

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Round %d ended: winner=%s, collected samples=%d, trained samples=%d, most common action=%s."),
        CompletedRound,
        *GetRoundWinnerName(Winner),
        CollectedSamples,
        EnemyDecisionComponent->GetTrainedSampleCount(),
        *GetRoundPlayerActionName(
            BehaviorTrackerComponent->GetMostCommonAction()
        )
    );
    BehaviorTrackerComponent->LogDataset();

    OnRoundEnded.Broadcast(
        CompletedRound,
        Winner,
        CollectedSamples,
        EnemyDecisionComponent->GetTrainedSampleCount()
    );

    if (Progression.GetPhase() == EAdaptiveRoundPhase::MatchComplete)
    {
        const EAdaptiveRoundWinner MatchWinner =
            Progression.GetMatchWinner();
        UE_LOG(
            LogAdaptHunt,
            Log,
            TEXT(
                "Three-round match complete: Player %d - %d Enemy, "
                "match winner=%s. Restart retains %d bounded player patterns."
            ),
            Progression.GetPlayerRoundWins(),
            Progression.GetEnemyRoundWins(),
            *GetRoundWinnerName(MatchWinner),
            PersistentPlayerPatterns.Num()
        );
        OnMatchCompleted.Broadcast(MatchWinner);
        return true;
    }

    ScheduleNextRound();
    return true;
}

void URoundManager::LoadPersistentPlayerPatterns()
{
    PersistentPlayerPatterns.Reset();
    SavedCurrentSessionSampleCount = 0;
    if (!IsValid(EnemyDecisionComponent))
    {
        return;
    }

    EnemyDecisionComponent->ClearPersistentTrainingDataset();
    if (!bPersistPlayerPatterns || GIsAutomationTesting)
    {
        return;
    }

    const UAdaptivePlayerPatternSaveGame* SaveGame =
        Cast<UAdaptivePlayerPatternSaveGame>(
            UGameplayStatics::LoadGameFromSlot(
                PersistentPlayerPatternSlot,
                PersistentPlayerPatternUserIndex
            )
        );
    if (!SaveGame)
    {
        UE_LOG(
            LogAdaptHunt,
            Log,
            TEXT("No persistent player-pattern history was found; learning begins live.")
        );
        return;
    }
    if (SaveGame->SchemaVersion
        != UAdaptivePlayerPatternSaveGame::CurrentSchemaVersion)
    {
        UE_LOG(
            LogAdaptHunt,
            Warning,
            TEXT("Persistent player-pattern history uses unsupported schema %d and was ignored."),
            SaveGame->SchemaVersion
        );
        return;
    }

    PersistentPlayerPatterns = SaveGame->Patterns;
    FAdaptivePlayerPatternPolicy::Normalize(
        PersistentPlayerPatterns,
        MaximumPersistentSamples
    );
    const FCombatDataset PersistentDataset =
        FAdaptivePlayerPatternPolicy::BuildDataset(
            PersistentPlayerPatterns
        );
    EnemyDecisionComponent->SetPersistentTrainingDataset(
        PersistentDataset
    );
    if (EnemyDecisionComponent->GetTrainedSampleCount() > 0)
    {
        EnemyDecisionComponent->RebuildAdaptiveTacticalProfile(
            PersistentDataset,
            1
        );
    }

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Loaded %d bounded persistent player patterns; predictor samples=%d, profile=%s."),
        PersistentPlayerPatterns.Num(),
        EnemyDecisionComponent->GetTrainedSampleCount(),
        EnemyDecisionComponent->GetAdaptiveTacticalProfile().IsActive()
            ? TEXT("active")
            : TEXT("insufficient evidence")
    );
}

bool URoundManager::SaveNewPersistentPlayerPatterns()
{
    if (!bPersistPlayerPatterns || GIsAutomationTesting
        || !IsValid(BehaviorTrackerComponent))
    {
        return true;
    }

    const FCombatDataset& SessionDataset =
        BehaviorTrackerComponent->GetDataset();
    if (SavedCurrentSessionSampleCount >= SessionDataset.Num())
    {
        return true;
    }

    TArray<FPersistentPlayerPattern> CandidatePatterns =
        PersistentPlayerPatterns;
    const int32 AddedCount = FAdaptivePlayerPatternPolicy::AppendDataset(
        CandidatePatterns,
        SessionDataset,
        SavedCurrentSessionSampleCount,
        MaximumPersistentSamples
    );
    if (AddedCount <= 0)
    {
        SavedCurrentSessionSampleCount = SessionDataset.Num();
        return true;
    }

    UAdaptivePlayerPatternSaveGame* SaveGame = Cast<
        UAdaptivePlayerPatternSaveGame>(
            UGameplayStatics::CreateSaveGameObject(
                UAdaptivePlayerPatternSaveGame::StaticClass()
            )
        );
    if (!SaveGame)
    {
        return false;
    }
    SaveGame->Patterns = CandidatePatterns;
    if (!UGameplayStatics::SaveGameToSlot(
        SaveGame,
        PersistentPlayerPatternSlot,
        PersistentPlayerPatternUserIndex
    ))
    {
        UE_LOG(
            LogAdaptHunt,
            Warning,
            TEXT("Failed to persist %d new player-pattern samples."),
            AddedCount
        );
        return false;
    }

    PersistentPlayerPatterns = MoveTemp(CandidatePatterns);
    SavedCurrentSessionSampleCount = SessionDataset.Num();
    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Persisted %d new player patterns; bounded history=%d/%d."),
        AddedCount,
        PersistentPlayerPatterns.Num(),
        MaximumPersistentSamples
    );
    return true;
}

bool URoundManager::TrainForNextRound()
{
    const bool bTrained =
        EnemyDecisionComponent->TrainPredictorFromTargetHistory();
    if (!bTrained)
    {
        UE_LOG(
            LogAdaptHunt,
            Warning,
            TEXT("Round %d ended with %d samples; the predictor needs more data before adaptation can begin."),
            Progression.GetLastCompletedRound(),
            BehaviorTrackerComponent->GetSampleCount()
        );
        return false;
    }

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Predictor trained between rounds from %d accumulated samples."),
        EnemyDecisionComponent->GetTrainedSampleCount()
    );
    return true;
}

void URoundManager::ResetCombatantsForRound()
{
    RemoveLingeringProjectiles();
    StopCombatantMovement();

    PlayerCombatComponent->ResetCombatState();
    EnemyCombatComponent->ResetCombatState();
    if (UEnemyLocomotionComponent* Locomotion =
        EnemyCharacter->GetEnemyLocomotionComponent())
    {
        Locomotion->ResetLocomotionState();
    }
    PlayerHealthComponent->ResetHealth();
    EnemyHealthComponent->ResetHealth();
    PlayerStaminaComponent->ResetStamina();
    EnemyStaminaComponent->ResetStamina();
    if (UGreyboxPresentationComponent* Presentation =
        PlayerCharacter->GetGreyboxPresentationComponent())
    {
        Presentation->ResetPresentation();
    }
    if (UGreyboxPresentationComponent* Presentation =
        EnemyCharacter->GetGreyboxPresentationComponent())
    {
        Presentation->ResetPresentation();
    }
    if (UCombatFeedbackComponent* Feedback =
        PlayerCharacter->GetCombatFeedbackComponent())
    {
        Feedback->ResetFeedback();
    }
    if (UCombatFeedbackComponent* Feedback =
        EnemyCharacter->GetCombatFeedbackComponent())
    {
        Feedback->ResetFeedback();
    }

    PlayerCharacter->TeleportTo(
        PlayerStartTransform.GetLocation(),
        PlayerStartTransform.Rotator(),
        false,
        true
    );
    EnemyCharacter->TeleportTo(
        EnemyStartTransform.GetLocation(),
        EnemyStartTransform.Rotator(),
        false,
        true
    );
}

void URoundManager::RemoveLingeringProjectiles()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (TActorIterator<AEnemyProjectile> It(World); It; ++It)
    {
        It->Destroy();
    }
}

void URoundManager::SetIntermissionInputLocked(const bool bLocked)
{
    if (!IsValid(PlayerCharacter)
        || bInputLockedByRoundManager == bLocked)
    {
        return;
    }

    if (APlayerController* PlayerController = Cast<APlayerController>(
        PlayerCharacter->GetController()
    ))
    {
        PlayerController->SetIgnoreMoveInput(bLocked);
        bInputLockedByRoundManager = bLocked;
    }
}

void URoundManager::StopCombatantMovement() const
{
    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->GetCharacterMovement()->StopMovementImmediately();
    }
    if (IsValid(EnemyCharacter))
    {
        EnemyCharacter->GetCharacterMovement()->StopMovementImmediately();
    }
}

void URoundManager::ClearFlowTimers()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();
    TimerManager.ClearTimer(NextRoundTimer);
    TimerManager.ClearTimer(PreRoundCountdownTimer);
    TimerManager.ClearAllTimersForObject(this);
    if (IsValid(PlayerCombatComponent))
    {
        TimerManager.ClearAllTimersForObject(PlayerCombatComponent.Get());
    }
    if (IsValid(EnemyCombatComponent))
    {
        TimerManager.ClearAllTimersForObject(EnemyCombatComponent.Get());
    }
    if (IsValid(PlayerCharacter))
    {
        TimerManager.ClearAllTimersForObject(PlayerCharacter.Get());
    }
    if (IsValid(EnemyCharacter))
    {
        TimerManager.ClearAllTimersForObject(EnemyCharacter.Get());
    }
}

void URoundManager::ScheduleNextRound()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();
    if (IntermissionDuration <= 0.0f)
    {
        TimerManager.SetTimerForNextTick(
            this,
            &URoundManager::HandleNextRoundTimer
        );
        return;
    }

    TimerManager.SetTimer(
        NextRoundTimer,
        this,
        &URoundManager::HandleNextRoundTimer,
        IntermissionDuration,
        false
    );
}

void URoundManager::SchedulePreRoundCountdown()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();
    if (PreRoundCountdownDuration <= 0.0f)
    {
        TimerManager.SetTimerForNextTick(
            this,
            &URoundManager::HandlePreRoundCountdownTimer
        );
        return;
    }

    TimerManager.SetTimer(
        PreRoundCountdownTimer,
        this,
        &URoundManager::HandlePreRoundCountdownTimer,
        PreRoundCountdownDuration,
        false
    );
}

void URoundManager::HandleNextRoundTimer()
{
    AdvanceToNextRoundNow();
}

void URoundManager::HandlePreRoundCountdownTimer()
{
    BeginCurrentRound();
}

void URoundManager::HandlePlayerDeath(UHealthComponent*, AActor*)
{
    EndCurrentRound(EAdaptiveRoundWinner::Enemy);
}

void URoundManager::HandleEnemyDeath(UHealthComponent*, AActor*)
{
    EndCurrentRound(EAdaptiveRoundWinner::Player);
}
