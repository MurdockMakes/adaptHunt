#include "Game/RoundManager.h"

#include "adaptHunt.h"
#include "Characters/AdaptiveEnemyCharacter.h"
#include "Characters/AdaptivePlayerCharacter.h"
#include "Combat/EnemyProjectile.h"
#include "Components/CombatComponent.h"
#include "Components/CombatSnapshotComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/EnemyDecisionComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PlayerBehaviorTrackerComponent.h"
#include "Components/StaminaComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
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
}

bool FAdaptiveRoundProgression::BeginMatch()
{
    if (Phase == EAdaptiveRoundPhase::InProgress
        || Phase == EAdaptiveRoundPhase::Intermission)
    {
        return false;
    }

    CurrentRound = 1;
    LastCompletedRound = 0;
    LastWinner = EAdaptiveRoundWinner::None;
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
    Phase = EAdaptiveRoundPhase::InProgress;
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
    , IntermissionDuration(3.0f)
    , bPredictionsEnabledForCurrentRound(false)
    , bInitialized(false)
{
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
    EnemyDecisionComponent->SetAutomaticRetrainingEnabled(false);
    EnemyDecisionComponent->SetPredictionUsageEnabled(false);
    EnemyDecisionComponent->ResetPredictor();
    BehaviorTrackerComponent->ResetDataset();
    LastRoundObservedPattern = FAdaptiveConditionalPattern();

    bInitialized = Progression.BeginMatch();
    if (!bInitialized)
    {
        Shutdown();
        return false;
    }

    BeginCurrentRound();
    return true;
}

void URoundManager::Shutdown()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(NextRoundTimer);
    }

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
    LastRoundObservedPattern = FAdaptiveConditionalPattern();
    bInitialized = false;
    Progression.Reset();
}

bool URoundManager::AdvanceToNextRoundNow()
{
    if (!bInitialized || !Progression.AdvanceToNextRound())
    {
        return false;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(NextRoundTimer);
    }
    BeginCurrentRound();
    return true;
}

bool URoundManager::ForceEndCurrentRound(
    const EAdaptiveRoundWinner Winner
)
{
    return EndCurrentRound(Winner);
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

const FAdaptiveConditionalPattern&
URoundManager::GetLastRoundObservedPattern() const
{
    return LastRoundObservedPattern;
}

float URoundManager::GetIntermissionDuration() const
{
    return IntermissionDuration;
}

bool URoundManager::IsPredictionRound(const int32 RoundNumber)
{
    return RoundNumber >= 2
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

void URoundManager::BeginCurrentRound()
{
    ResetCombatantsForRound();

    const int32 RoundNumber = Progression.GetCurrentRound();
    SnapshotComponent->ResetRoundState();
    SnapshotComponent->SetRoundNumber(RoundNumber);
    BehaviorTrackerComponent->StartTracking();

    bPredictionsEnabledForCurrentRound = IsPredictionRound(RoundNumber)
        && EnemyDecisionComponent->GetTrainedSampleCount() > 0;
    EnemyDecisionComponent->SetAutomaticRetrainingEnabled(false);
    EnemyDecisionComponent->SetPredictionUsageEnabled(
        bPredictionsEnabledForCurrentRound
    );
    EnemyDecisionComponent->SetDecisionMakingEnabled(true);
    PlayerCombatComponent->SetCombatEnabled(true);
    EnemyCombatComponent->SetCombatEnabled(true);
    SetIntermissionInputLocked(false);

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Round %d/%d started: predictions=%s, trained samples=%d, collected samples=%d."),
        RoundNumber,
        FAdaptiveRoundProgression::TotalRounds,
        bPredictionsEnabledForCurrentRound ? TEXT("enabled") : TEXT("disabled"),
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
    PlayerCombatComponent->SetCombatEnabled(false);
    EnemyCombatComponent->SetCombatEnabled(false);
    bPredictionsEnabledForCurrentRound = false;
    StopCombatantMovement();
    SetIntermissionInputLocked(true);

    const int32 CompletedRound = Progression.GetLastCompletedRound();
    const int32 CollectedSamples = BehaviorTrackerComponent->GetSampleCount();
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
        UE_LOG(
            LogAdaptHunt,
            Log,
            TEXT("Three-round match complete. Final-round winner=%s."),
            *GetRoundWinnerName(Winner)
        );
        OnMatchCompleted.Broadcast(Winner);
        return true;
    }

    ScheduleNextRound();
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
    PlayerHealthComponent->ResetHealth();
    EnemyHealthComponent->ResetHealth();
    PlayerStaminaComponent->ResetStamina();
    EnemyStaminaComponent->ResetStamina();

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
    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    if (APlayerController* PlayerController = Cast<APlayerController>(
        PlayerCharacter->GetController()
    ))
    {
        PlayerController->SetIgnoreMoveInput(bLocked);
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

void URoundManager::HandleNextRoundTimer()
{
    AdvanceToNextRoundNow();
}

void URoundManager::HandlePlayerDeath(UHealthComponent*, AActor*)
{
    EndCurrentRound(EAdaptiveRoundWinner::Enemy);
}

void URoundManager::HandleEnemyDeath(UHealthComponent*, AActor*)
{
    EndCurrentRound(EAdaptiveRoundWinner::Player);
}
