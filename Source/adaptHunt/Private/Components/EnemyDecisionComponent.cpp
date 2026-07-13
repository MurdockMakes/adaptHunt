#include "Components/EnemyDecisionComponent.h"

#include "adaptHunt.h"
#include "AI/ConditionalActionPredictor.h"
#include "Components/CombatComponent.h"
#include "Components/CombatSnapshotComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/EnemyLocomotionComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PlayerBehaviorTrackerComponent.h"
#include "Components/StaminaComponent.h"
#include "Data/PersistentPlayerPatterns.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Game/AdaptiveHuntTuningSettings.h"
#include "Kismet/GameplayStatics.h"

UEnemyDecisionComponent::UEnemyDecisionComponent()
    : TargetActor(nullptr)
    , EnemyCombatComponent(nullptr)
    , EnemyLocomotionComponent(nullptr)
    , OwnerHealthComponent(nullptr)
    , BehaviorTrackerComponent(nullptr)
    , TargetCombatComponent(nullptr)
    , LastSelectedUtilityAction(EEnemyCombatAction::None)
    , LastSelectedAction(EEnemyCombatAction::None)
    , TrainedSampleCount(0)
    , LiveSampleCountAtLastTraining(0)
    , bLastPredictionApplied(false)
    , TacticalState(EEnemyTacticalState::Observe)
    , bLastLineOfSightClear(false)
    , bLastDecisionUrgent(false)
    , bDecisionMakingEnabled(true)
    , bAutomaticRetrainingEnabled(true)
    , bPredictionUsageEnabled(true)
    , bStrafeRight(false)
    , NextCombatDecisionTime(0.0)
    , TacticalStateEnteredTime(0.0)
    , NextOrbitDirectionChangeTime(0.0)
    , Predictor(MakeUnique<FConditionalActionPredictor>())
{
    const FAdaptiveTacticalRuntimeTuning TacticalDefaults =
        UAdaptiveHuntTuningSettings::Get().Tactical.GetSanitized();
    EvaluationInterval = TacticalDefaults.EvaluationInterval;
    CombatDecisionInterval = TacticalDefaults.CombatDecisionInterval;
    RangedCombatDistance = TacticalDefaults.RangedCombatDistance;
    DashCombatDistance = TacticalDefaults.DashCombatDistance;
    InterruptDistance = TacticalDefaults.InterruptDistance;
    TacticalRandomSeed = TacticalDefaults.RandomSeed;
    TacticalTuning = TacticalDefaults.Policy;
    ActionSelectionTuning = TacticalDefaults.Selection;
    RepetitionTuning = TacticalDefaults.Repetition;
    OffenseDefenseBalanceTuning =
        TacticalDefaults.OffenseDefenseBalance;
    RecentOutcomeTuning = TacticalDefaults.RecentOutcome;
    UrgentDecisionTuning = TacticalDefaults.UrgentReactions;

    const FAdaptiveLearningTuning AdaptationDefaults =
        UAdaptiveHuntTuningSettings::Get().Adaptation.GetSanitized();
    PredictionInfluence = AdaptationDefaults.PredictionInfluence;
    MinimumPredictionConfidence =
        AdaptationDefaults.MinimumPredictionConfidence;
    PredictionApplicationChance =
        AdaptationDefaults.PredictionApplicationChance;
    MinimumPredictionSupportingSamples =
        AdaptationDefaults.MinimumPredictionSupportingSamples;
    MinimumTrainingSamples = AdaptationDefaults.MinimumTrainingSamples;
    RetrainSampleInterval = AdaptationDefaults.RetrainSampleInterval;
    RecentPlayerPatternWindow = AdaptationDefaults.RecentActionWindow;
    MinimumRepeatedLightAttacks =
        AdaptationDefaults.MinimumRepeatedLightAttacks;
    MaximumLightAttackPatternAdjustment =
        AdaptationDefaults.MaximumLightAttackPatternAdjustment;
    AdaptationRandomSeed = AdaptationDefaults.RandomSeed;
    AdaptiveProfileTuning = AdaptationDefaults.Profile;
    CounterEffectivenessTuning = AdaptationDefaults.Outcome;

    AdaptationRandomStream.Initialize(AdaptationRandomSeed);
    TacticalRandomStream.Initialize(TacticalRandomSeed);
    SelectionRandomStream.Initialize(TacticalRandomSeed ^ 0x5E1EC7);
    PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyDecisionComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    EnemyCombatComponent = Owner
        ? Owner->FindComponentByClass<UEnemyCombatComponent>()
        : nullptr;
    if (EnemyCombatComponent)
    {
        EnemyCombatComponent->OnActionResolved.RemoveAll(this);
        EnemyCombatComponent->OnActionResolved.AddUObject(
            this,
            &UEnemyDecisionComponent::HandleEnemyActionResolved
        );
    }
    EnemyLocomotionComponent = Owner
        ? Owner->FindComponentByClass<UEnemyLocomotionComponent>()
        : nullptr;
    OwnerHealthComponent = Owner
        ? Owner->FindComponentByClass<UHealthComponent>()
        : nullptr;
    if (OwnerHealthComponent)
    {
        OwnerHealthComponent->OnDeath.AddUObject(
            this,
            &UEnemyDecisionComponent::HandleOwnerDeath
        );
    }

    AdaptationRandomStream.Initialize(AdaptationRandomSeed);
    TacticalRandomStream.Initialize(TacticalRandomSeed);
    SelectionRandomStream.Initialize(TacticalRandomSeed ^ 0x5E1EC7);
    AdaptiveCounterBudget.Reset(0);
    ResetShortTermDecisionMemory();
    ResetTacticalState(GetDecisionTimeSeconds());
    AcquirePlayerTarget();
    BindBehaviorTracker();
    BindTargetCombat();
    RefreshEvaluationTimer();

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Adaptive enemy decision component initialized for %s."),
        Owner ? *Owner->GetName() : TEXT("unowned component")
    );
}

void UEnemyDecisionComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    UnbindBehaviorTracker();
    UnbindTargetCombat();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(EvaluationTimer);
        World->GetTimerManager().ClearTimer(UrgentDecisionTimer);
    }
    if (OwnerHealthComponent)
    {
        OwnerHealthComponent->OnDeath.RemoveAll(this);
    }
    if (EnemyCombatComponent)
    {
        EnemyCombatComponent->OnActionResolved.RemoveAll(this);
    }

    Super::EndPlay(EndPlayReason);
}

void UEnemyDecisionComponent::RefreshEvaluationTimer()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    World->GetTimerManager().ClearTimer(EvaluationTimer);
    if (!bDecisionMakingEnabled || !HasBegunPlay())
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        EvaluationTimer,
        this,
        &UEnemyDecisionComponent::EvaluateDecision,
        FMath::Max(0.02f, EvaluationInterval),
        true,
        0.1f
    );
}

void UEnemyDecisionComponent::SetTargetActor(AActor* NewTargetActor)
{
    AActor* ValidTarget = IsValid(NewTargetActor)
        && NewTargetActor != GetOwner()
        ? NewTargetActor
        : nullptr;
    if (TargetActor == ValidTarget)
    {
        return;
    }

    UnbindBehaviorTracker();
    UnbindTargetCombat();
    if (EnemyCombatComponent)
    {
        EnemyCombatComponent->HandleTargetLost();
    }
    if (EnemyLocomotionComponent)
    {
        EnemyLocomotionComponent->ClearMovementIntent();
    }
    TargetActor = ValidTarget;
    ResetPredictor();
    LastActionScores.Reset();
    LastSelectedUtilityAction = EEnemyCombatAction::None;
    LastSelectedAction = EEnemyCombatAction::None;
    ResetShortTermDecisionMemory();
    ResetTacticalState(GetDecisionTimeSeconds());
    if (HasBegunPlay())
    {
        BindBehaviorTracker();
        BindTargetCombat();
    }
}

AActor* UEnemyDecisionComponent::GetTargetActor() const
{
    return TargetActor;
}

void UEnemyDecisionComponent::SetDecisionMakingEnabled(const bool bEnabled)
{
    const bool bChanged = bDecisionMakingEnabled != bEnabled;
    bDecisionMakingEnabled = bEnabled;
    if (!bEnabled)
    {
        LastSelectedAction = EEnemyCombatAction::None;
        UrgentDecisionState.Reset();
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(UrgentDecisionTimer);
        }
    }
    if (bChanged)
    {
        ResetTacticalState(GetDecisionTimeSeconds());
        if (bEnabled)
        {
            NextCombatDecisionTime = GetDecisionTimeSeconds();
        }
        RefreshEvaluationTimer();
    }
    if (EnemyLocomotionComponent)
    {
        EnemyLocomotionComponent->SetLocomotionEnabled(bEnabled);
    }
}

bool UEnemyDecisionComponent::IsDecisionMakingEnabled() const
{
    return bDecisionMakingEnabled;
}

bool UEnemyDecisionComponent::HasActiveEvaluationTimer() const
{
    const UWorld* World = GetWorld();
    return World
        && World->GetTimerManager().IsTimerActive(EvaluationTimer);
}

void UEnemyDecisionComponent::EvaluateNow()
{
    EvaluateDecision();
}

void UEnemyDecisionComponent::TrainPredictor(
    const FCombatDataset& Dataset
)
{
    if (!Predictor)
    {
        Predictor = MakeUnique<FConditionalActionPredictor>();
    }

    FCombatDataset CombinedDataset;
    for (const FTrainingSample& Sample
        : PersistentTrainingDataset.GetSamples())
    {
        CombinedDataset.AddSample(Sample);
    }
    for (const FTrainingSample& Sample : Dataset.GetSamples())
    {
        CombinedDataset.AddSample(Sample);
    }

    Predictor->Train(CombinedDataset);
    TrainedSampleCount = CombinedDataset.Num();
    LiveSampleCountAtLastTraining = Dataset.Num();
    LastPrediction = FPredictionResult();
    bLastPredictionApplied = false;
}

void UEnemyDecisionComponent::SetPersistentTrainingDataset(
    const FCombatDataset& Dataset
)
{
    PersistentTrainingDataset.Reset();
    for (const FTrainingSample& Sample : Dataset.GetSamples())
    {
        PersistentTrainingDataset.AddSample(Sample);
    }

    ResetPredictor();
    if (PersistentTrainingDataset.Num()
        >= FMath::Max(1, MinimumTrainingSamples))
    {
        const FCombatDataset EmptyLiveDataset;
        TrainPredictor(EmptyLiveDataset);
    }
}

void UEnemyDecisionComponent::ClearPersistentTrainingDataset()
{
    PersistentTrainingDataset.Reset();
    ResetPredictor();
}

void UEnemyDecisionComponent::RebuildAdaptiveTacticalProfile(
    const FCombatDataset& Dataset,
    const int32 CompletedRound
)
{
    FAdaptiveTacticalProfile BestProfile =
        FAdaptiveTacticalProfilePolicy::Derive(
            Dataset,
            CompletedRound,
            FPredictionResult(),
            AdaptiveProfileTuning
        );

    if (Predictor)
    {
        for (const FTrainingSample& Sample : Dataset.GetSamples())
        {
            if (!Sample.IsValid()
                || Sample.Snapshot.RoundNumber != CompletedRound)
            {
                continue;
            }

            const FAdaptiveTacticalProfile Candidate =
                FAdaptiveTacticalProfilePolicy::Derive(
                    Dataset,
                    CompletedRound,
                    Predictor->Predict(Sample.Snapshot),
                    AdaptiveProfileTuning
                );
            if (FAdaptiveTacticalProfilePolicy::IsStrongerCandidate(
                Candidate,
                BestProfile
            ))
            {
                BestProfile = Candidate;
            }
        }
    }

    AdaptiveTacticalProfile = BestProfile;
    LastRoundCounterEffectiveness =
        FAdaptiveCounterEffectivenessPolicy::AnalyzeRound(
            CounterOutcomeHistory.GetRecords(),
            CompletedRound,
            CounterEffectivenessTuning
        );
    ActiveCounterEffectiveness = AdaptiveTacticalProfile.IsActive()
        ? FAdaptiveCounterEffectivenessPolicy::AnalyzeForCounter(
            CounterOutcomeHistory.GetRecords(),
            AdaptiveTacticalProfile.MostLikelyPlayerAction,
            AdaptiveTacticalProfile.PreferredCounterAction,
            CompletedRound,
            CounterEffectivenessTuning
        )
        : FAdaptiveCounterEffectivenessSummary();
    AdaptiveCounterBudget.Reset(
        AdaptiveTacticalProfile.IsActive()
            ? AdaptiveProfileTuning.GetSanitized().CounterBudgetPerRound
            : 0
    );

    const UEnum* PlayerActionEnum = StaticEnum<EPlayerCombatAction>();
    const UEnum* EnemyActionEnum = StaticEnum<EEnemyCombatAction>();
    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT(
            "Round %d adaptive tactical profile: active=%s, habit=%s, "
            "confidence=%.1f%%, support=%d/%d, counter=%s, spacing=%+.0f, "
            "aggression=%+.2f, defense=%+.2f, heal priority=%.2f."
        ),
        CompletedRound,
        AdaptiveTacticalProfile.IsActive() ? TEXT("true") : TEXT("false"),
        PlayerActionEnum
            ? *PlayerActionEnum->GetNameStringByValue(static_cast<int64>(
                AdaptiveTacticalProfile.MostLikelyPlayerAction
            ))
            : TEXT("Unknown"),
        AdaptiveTacticalProfile.Confidence * 100.0f,
        AdaptiveTacticalProfile.SupportingSampleCount,
        AdaptiveTacticalProfile.ContextSampleCount,
        EnemyActionEnum
            ? *EnemyActionEnum->GetNameStringByValue(static_cast<int64>(
                AdaptiveTacticalProfile.PreferredCounterAction
            ))
            : TEXT("Unknown"),
        AdaptiveTacticalProfile.PreferredSpacingAdjustment,
        AdaptiveTacticalProfile.AggressionAdjustment,
        AdaptiveTacticalProfile.DefensiveAdjustment,
        AdaptiveTacticalProfile.HealInterruptionPriority
    );

    for (const FAdaptiveCounterEffectivenessSummary& Summary :
        LastRoundCounterEffectiveness)
    {
        UE_LOG(
            LogAdaptHunt,
            Log,
            TEXT(
                "Round %d counter effectiveness: predicted=%s, "
                "counter=%s, worked=%d/%d, smoothed=%.1f%%, "
                "eligible=%s, modifier=%+.3f."
            ),
            CompletedRound,
            PlayerActionEnum
                ? *PlayerActionEnum->GetNameStringByValue(
                    static_cast<int64>(Summary.PredictedPlayerAction)
                )
                : TEXT("Unknown"),
            EnemyActionEnum
                ? *EnemyActionEnum->GetNameStringByValue(
                    static_cast<int64>(Summary.CounterAction)
                )
                : TEXT("Unknown"),
            Summary.SuccessfulCount,
            Summary.SampleCount,
            Summary.SmoothedEffectiveness * 100.0f,
            Summary.bMeetsMinimumSamples ? TEXT("true") : TEXT("false"),
            Summary.UtilityModifier
        );
    }
}

void UEnemyDecisionComponent::ResetAdaptiveTacticalProfile()
{
    AdaptiveTacticalProfile = FAdaptiveTacticalProfile();
    AdaptiveCounterBudget.Reset(0);
    ActiveCounterEffectiveness =
        FAdaptiveCounterEffectivenessSummary();
}

bool UEnemyDecisionComponent::DebugForceAdaptiveTacticalProfile(
    const FAdaptiveTacticalProfile& Profile
)
{
    const bool bExplicitEmptyProfile = !Profile.bEvidenceStrongEnough
        && Profile.EvidenceStatus == EAdaptiveProfileEvidenceStatus::NoData
        && Profile.SourceRound == 0
        && Profile.PreferredCounterAction == EEnemyCombatAction::None;
    if (!Profile.IsActive() && !bExplicitEmptyProfile)
    {
        return false;
    }

    AdaptiveTacticalProfile = Profile.IsActive()
        ? Profile
        : FAdaptiveTacticalProfile();
    AdaptiveCounterBudget.Reset(
        AdaptiveTacticalProfile.IsActive()
        ? AdaptiveProfileTuning.GetSanitized().CounterBudgetPerRound
        : 0
    );
    ActiveCounterEffectiveness = FAdaptiveCounterEffectivenessSummary();
    return true;
}

void UEnemyDecisionComponent::ResetCounterOutcomeTracking()
{
    CounterOutcomeHistory.Reset();
    PendingOutcomeRecords.Reset();
    LastRoundCounterEffectiveness.Reset();
    ActiveCounterEffectiveness =
        FAdaptiveCounterEffectivenessSummary();
    ResetShortTermDecisionMemory();
    SelectionRandomStream.Initialize(TacticalRandomSeed ^ 0x5E1EC7);
}

void UEnemyDecisionComponent::ResetShortTermDecisionMemory()
{
    CommittedActionHistory.Reset();
    RecentOutcomeMemory.Reset();
    UrgentDecisionState.Reset();
    LastSelectionResult = FEnemyActionSelectionResult();
    LastRepetitionModifiers.Reset();
    LastOffenseDefenseBalanceModifiers.Reset();
    LastRecentOutcomeModifiers.Reset();
    LastActionScores.Reset();
    LastSelectedUtilityAction = EEnemyCombatAction::None;
    LastSelectedAction = EEnemyCombatAction::None;
    bLastDecisionUrgent = false;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(UrgentDecisionTimer);
    }
}

bool UEnemyDecisionComponent::TrainPredictorFromTargetHistory()
{
    BindBehaviorTracker();
    if (!BehaviorTrackerComponent
        || BehaviorTrackerComponent->GetSampleCount()
                + PersistentTrainingDataset.Num()
            < FMath::Max(1, MinimumTrainingSamples))
    {
        return false;
    }

    TrainPredictor(BehaviorTrackerComponent->GetDataset());
    return true;
}

void UEnemyDecisionComponent::ResetPredictor()
{
    if (Predictor)
    {
        Predictor->Reset();
    }
    TrainedSampleCount = 0;
    LiveSampleCountAtLastTraining = 0;
    LastPrediction = FPredictionResult();
    bLastPredictionApplied = false;
    ResetAdaptiveTacticalProfile();
}

void UEnemyDecisionComponent::SetPredictor(
    TUniquePtr<IPlayerActionPredictor> NewPredictor
)
{
    Predictor = MoveTemp(NewPredictor);
    if (!Predictor)
    {
        Predictor = MakeUnique<FConditionalActionPredictor>();
    }
    ResetPredictor();
}

void UEnemyDecisionComponent::SetAutomaticRetrainingEnabled(
    const bool bEnabled
)
{
    bAutomaticRetrainingEnabled = bEnabled;
}

void UEnemyDecisionComponent::SetPredictionUsageEnabled(const bool bEnabled)
{
    const bool bWasEnabled = bPredictionUsageEnabled;
    bPredictionUsageEnabled = bEnabled;
    if (!bEnabled)
    {
        bLastPredictionApplied = false;
    }
    else if (!bWasEnabled)
    {
        AdaptiveCounterBudget.Reset(
            AdaptiveTacticalProfile.IsActive()
                ? AdaptiveProfileTuning.GetSanitized()
                    .CounterBudgetPerRound
                : 0
        );
    }
}

bool UEnemyDecisionComponent::IsAutomaticRetrainingEnabled() const
{
    return bAutomaticRetrainingEnabled;
}

bool UEnemyDecisionComponent::IsPredictionUsageEnabled() const
{
    return bPredictionUsageEnabled;
}

EEnemyCombatAction UEnemyDecisionComponent::SelectMovementAction(
    const float DistanceToTarget
) const
{
    switch (SelectMovementIntent(DistanceToTarget))
    {
    case EEnemyLocomotionIntent::Approach:
        return EEnemyCombatAction::MoveTowardPlayer;
    case EEnemyLocomotionIntent::Retreat:
        return EEnemyCombatAction::MoveAwayFromPlayer;
    case EEnemyLocomotionIntent::OrbitLeft:
        return EEnemyCombatAction::StrafeLeft;
    case EEnemyLocomotionIntent::OrbitRight:
        return EEnemyCombatAction::StrafeRight;
    default:
        return EEnemyCombatAction::None;
    }
}

EEnemyLocomotionIntent UEnemyDecisionComponent::SelectMovementIntent(
    const float DistanceToTarget
) const
{
    return FEnemyMovementPolicy::SelectIntent(
        DistanceToTarget,
        GetRetreatDistance(),
        GetStrafeDistance(),
        bStrafeRight
    );
}

EEnemyCombatAction UEnemyDecisionComponent::SelectCombatAction(
    const float DistanceToTarget,
    const bool bTargetRecentlyHealed,
    const float RandomValue
) const
{
    const float SafeDistance = FMath::IsFinite(DistanceToTarget)
        ? FMath::Max(0.0f, DistanceToTarget)
        : MAX_flt;
    const float Choice = FMath::IsFinite(RandomValue)
        ? FMath::Clamp(RandomValue, 0.0f, 1.0f)
        : 0.5f;

    if (bTargetRecentlyHealed && SafeDistance <= InterruptDistance)
    {
        return EEnemyCombatAction::InterruptHeal;
    }
    if (SafeDistance >= DashCombatDistance)
    {
        return Choice < 0.55f
            ? EEnemyCombatAction::DashAttack
            : EEnemyCombatAction::ProjectileAttack;
    }
    if (SafeDistance >= RangedCombatDistance)
    {
        return Choice < 0.65f
            ? EEnemyCombatAction::ProjectileAttack
            : EEnemyCombatAction::DashAttack;
    }
    if (Choice < 0.42f)
    {
        return EEnemyCombatAction::LightAttack;
    }
    if (Choice < 0.66f)
    {
        return EEnemyCombatAction::HeavyAttack;
    }
    if (Choice < 0.79f)
    {
        return EEnemyCombatAction::Block;
    }
    if (Choice < 0.92f)
    {
        return EEnemyCombatAction::Dodge;
    }
    return EEnemyCombatAction::LightAttack;
}

bool UEnemyDecisionComponent::ShouldApplyPredictionDeterministically(
    const FPredictionResult& Prediction,
    const bool bUsageEnabled,
    const float MinimumConfidence,
    const int32 MinimumSupportingSamples,
    const float ApplicationChance,
    const float RandomValue
)
{
    if (!bUsageEnabled || !Prediction.bHasPrediction
        || !AdaptiveCombat::IsTrackablePlayerAction(
            Prediction.PredictedAction
        )
        || !FMath::IsFinite(Prediction.Confidence)
        || !FMath::IsFinite(RandomValue)
        || RandomValue < 0.0f || RandomValue >= 1.0f
        || Prediction.SupportingSampleCount
            < FMath::Max(1, MinimumSupportingSamples))
    {
        return false;
    }

    const float SafeMinimumConfidence = FMath::IsFinite(MinimumConfidence)
        ? FMath::Clamp(MinimumConfidence, 0.0f, 1.0f)
        : 1.0f;
    const float SafeConfidence = FMath::Clamp(
        Prediction.Confidence,
        0.0f,
        1.0f
    );
    if (SafeConfidence < SafeMinimumConfidence)
    {
        return false;
    }

    const float SafeApplicationChance = FMath::IsFinite(ApplicationChance)
        ? FMath::Clamp(ApplicationChance, 0.0f, 1.0f)
        : 0.0f;
    const float EffectiveChance = SafeApplicationChance * SafeConfidence;
    return EffectiveChance > 0.0f && RandomValue < EffectiveChance;
}

float UEnemyDecisionComponent::GetEvaluationInterval() const
{
    return EvaluationInterval;
}

float UEnemyDecisionComponent::GetCombatDecisionInterval() const
{
    return CombatDecisionInterval;
}

float UEnemyDecisionComponent::GetRetreatDistance() const
{
    const UEnemyLocomotionComponent* Locomotion =
        EnemyLocomotionComponent
            ? EnemyLocomotionComponent.Get()
            : (GetOwner()
                ? GetOwner()->FindComponentByClass<
                    UEnemyLocomotionComponent>()
                : nullptr);
    return Locomotion ? Locomotion->GetRetreatRange() : 180.0f;
}

float UEnemyDecisionComponent::GetStrafeDistance() const
{
    const UEnemyLocomotionComponent* Locomotion =
        EnemyLocomotionComponent
            ? EnemyLocomotionComponent.Get()
            : (GetOwner()
                ? GetOwner()->FindComponentByClass<
                    UEnemyLocomotionComponent>()
                : nullptr);
    return Locomotion ? Locomotion->GetOrbitRange() : 625.0f;
}

float UEnemyDecisionComponent::GetPredictionInfluence() const
{
    return PredictionInfluence;
}

float UEnemyDecisionComponent::GetMinimumPredictionConfidence() const
{
    return MinimumPredictionConfidence;
}

float UEnemyDecisionComponent::GetPredictionApplicationChance() const
{
    return PredictionApplicationChance;
}

int32 UEnemyDecisionComponent::GetMinimumPredictionSupportingSamples() const
{
    return MinimumPredictionSupportingSamples;
}

const TArray<FEnemyActionScore>&
UEnemyDecisionComponent::GetLastActionScores() const
{
    return LastActionScores;
}

EEnemyCombatAction
UEnemyDecisionComponent::GetLastSelectedUtilityAction() const
{
    return LastSelectedUtilityAction;
}

EEnemyCombatAction UEnemyDecisionComponent::GetLastSelectedAction() const
{
    return LastSelectedAction;
}

const FPredictionResult& UEnemyDecisionComponent::GetLastPrediction() const
{
    return LastPrediction;
}

bool UEnemyDecisionComponent::WasLastPredictionApplied() const
{
    return bLastPredictionApplied;
}

int32 UEnemyDecisionComponent::GetTrainedSampleCount() const
{
    return TrainedSampleCount;
}

int32 UEnemyDecisionComponent::GetPersistentTrainingSampleCount() const
{
    return PersistentTrainingDataset.Num();
}

float UEnemyDecisionComponent::GetRecentLightAttackPressure() const
{
    const FCombatDataset EmptyCurrentSession;
    const FCombatDataset& CurrentSession = BehaviorTrackerComponent
        ? BehaviorTrackerComponent->GetDataset()
        : EmptyCurrentSession;
    return FAdaptivePlayerPatternPolicy::
        CalculateRecentLightAttackPressure(
            PersistentTrainingDataset,
            CurrentSession,
            RecentPlayerPatternWindow,
            MinimumRepeatedLightAttacks
        );
}

const FAdaptiveTacticalProfile&
UEnemyDecisionComponent::GetAdaptiveTacticalProfile() const
{
    return AdaptiveTacticalProfile;
}

const FAdaptiveTacticalProfileTuning&
UEnemyDecisionComponent::GetAdaptiveTacticalProfileTuning() const
{
    return AdaptiveProfileTuning;
}

int32 UEnemyDecisionComponent::GetAdaptiveCounterUsesRemaining() const
{
    return AdaptiveCounterBudget.RemainingUses;
}

float UEnemyDecisionComponent::GetAdaptiveCounterCooldownRemaining() const
{
    return AdaptiveCounterBudget.GetRemainingCooldown(
        GetDecisionTimeSeconds()
    );
}

int32 UEnemyDecisionComponent::GetCounterOutcomeCount() const
{
    return CounterOutcomeHistory.Num();
}

const FAdaptiveCounterOutcomeRecord*
UEnemyDecisionComponent::GetLastCounterOutcome() const
{
    return CounterOutcomeHistory.GetLastRecord();
}

const TArray<FAdaptiveCounterEffectivenessSummary>&
UEnemyDecisionComponent::GetLastRoundCounterEffectiveness() const
{
    return LastRoundCounterEffectiveness;
}

const FAdaptiveCounterEffectivenessSummary&
UEnemyDecisionComponent::GetActiveCounterEffectiveness() const
{
    return ActiveCounterEffectiveness;
}

const FAdaptiveCounterEffectivenessTuning&
UEnemyDecisionComponent::GetCounterEffectivenessTuning() const
{
    return CounterEffectivenessTuning;
}

EEnemyTacticalState UEnemyDecisionComponent::GetTacticalState() const
{
    return TacticalState;
}

bool UEnemyDecisionComponent::HasActiveAttackSequence() const
{
    return AttackSequence.IsActive();
}

EEnemyCombatAction
UEnemyDecisionComponent::GetAttackSequenceFollowUp() const
{
    return AttackSequence.FollowUpAction;
}

bool UEnemyDecisionComponent::WasLastLineOfSightClear() const
{
    return bLastLineOfSightClear;
}

const FEnemyTacticalTuning&
UEnemyDecisionComponent::GetTacticalTuning() const
{
    return TacticalTuning;
}

const FEnemyActionSelectionTuning&
UEnemyDecisionComponent::GetActionSelectionTuning() const
{
    return ActionSelectionTuning;
}

const FEnemyActionRepetitionTuning&
UEnemyDecisionComponent::GetRepetitionTuning() const
{
    return RepetitionTuning;
}

const FEnemyOffenseDefenseBalanceTuning&
UEnemyDecisionComponent::GetOffenseDefenseBalanceTuning() const
{
    return OffenseDefenseBalanceTuning;
}

const FEnemyRecentOutcomeTuning&
UEnemyDecisionComponent::GetRecentOutcomeTuning() const
{
    return RecentOutcomeTuning;
}

const FEnemyUrgentDecisionTuning&
UEnemyDecisionComponent::GetUrgentDecisionTuning() const
{
    return UrgentDecisionTuning;
}

const FEnemyActionSelectionResult&
UEnemyDecisionComponent::GetLastSelectionResult() const
{
    return LastSelectionResult;
}

const TMap<EEnemyCombatAction, float>&
UEnemyDecisionComponent::GetLastRepetitionModifiers() const
{
    return LastRepetitionModifiers;
}

const TMap<EEnemyCombatAction, float>&
UEnemyDecisionComponent::GetLastOffenseDefenseBalanceModifiers() const
{
    return LastOffenseDefenseBalanceModifiers;
}

const TMap<EEnemyCombatAction, float>&
UEnemyDecisionComponent::GetLastRecentOutcomeModifiers() const
{
    return LastRecentOutcomeModifiers;
}

int32 UEnemyDecisionComponent::GetRecentCommittedActionCount() const
{
    return CommittedActionHistory.Num();
}

int32 UEnemyDecisionComponent::GetRecentOutcomeMemoryCount() const
{
    return RecentOutcomeMemory.Num();
}

bool UEnemyDecisionComponent::WasLastDecisionUrgent() const
{
    return bLastDecisionUrgent;
}

void UEnemyDecisionComponent::EvaluateDecision()
{
    if (!bDecisionMakingEnabled || !EnemyCombatComponent
        || (OwnerHealthComponent && OwnerHealthComponent->IsDead()))
    {
        return;
    }

    if (!IsValid(TargetActor))
    {
        AcquirePlayerTarget();
    }

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !TargetActor)
    {
        return;
    }

    const double CurrentTime = GetDecisionTimeSeconds();
    const bool bEnemyCommitted =
        EnemyCombatComponent->IsCommittedActionActive();
    const UCombatComponent* PlayerCombat = TargetCombatComponent
        ? TargetCombatComponent.Get()
        : TargetActor->FindComponentByClass<UCombatComponent>();
    const bool bUrgentDecisionDue = PlayerCombat
        && UrgentDecisionState.ConsumeIfReady(
            PlayerCombat->GetLastAction(),
            PlayerCombat->GetCurrentPhase(),
            !bEnemyCommitted,
            CurrentTime,
            UrgentDecisionTuning
        );
    if (!UrgentDecisionState.IsPending())
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(UrgentDecisionTimer);
        }
    }

    // A committed phase owns the enemy until it reaches Idle. This prevents
    // urgent or normal selection from canceling a telegraph or recovery.
    if (bEnemyCommitted)
    {
        return;
    }

    const FVector ToTarget = TargetActor->GetActorLocation()
        - Character->GetActorLocation();
    const float Distance = ToTarget.Size2D();
    if (Distance <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const bool bNormalDecisionDue = CurrentTime >= NextCombatDecisionTime;
    const bool bCombatDecisionDue = bNormalDecisionDue
        || bUrgentDecisionDue;
    const FCombatSnapshot Snapshot = CaptureDecisionSnapshot(Distance);
    if (bNormalDecisionDue)
    {
        NextCombatDecisionTime = CurrentTime
            + FMath::Max(0.1f, CombatDecisionInterval);
        LastPrediction = PredictPlayerAction(Snapshot);
        bLastPredictionApplied = ShouldApplyPrediction(LastPrediction);
    }

    const bool bProfileAffectsPlay = bPredictionUsageEnabled
        && AdaptiveTacticalProfile.IsActive();
    const bool bAdaptiveCounterOpportunity = bNormalDecisionDue
        && bProfileAffectsPlay
        && FAdaptiveTacticalProfilePolicy::
            IsPredictionMatchedCounterOpportunity(
                AdaptiveTacticalProfile,
                LastPrediction,
                bLastPredictionApplied,
                AdaptiveCounterBudget,
                CurrentTime
            );

    bLastLineOfSightClear = HasLineOfSightToTarget();
    FEnemyTacticalContext TacticalContext = BuildTacticalContext(
        Snapshot,
        Distance,
        bLastLineOfSightClear
    );
    if (bProfileAffectsPlay && !bAdaptiveCounterOpportunity)
    {
        TacticalContext.PredictedPlayerAction =
            EPlayerCombatAction::None;
        TacticalContext.PredictionConfidence = 0.0f;
    }
    UpdateTacticalState(TacticalContext, CurrentTime);
    UpdateAttackSequence(
        TacticalContext,
        CurrentTime,
        bCombatDecisionDue
    );

    if (!EnemyCombatComponent->IsBlocking())
    {
        const EEnemyLocomotionIntent BaseMovementIntent =
            SelectMovementIntent(Distance);
        const EEnemyLocomotionIntent MovementIntent =
            FEnemyTacticalPolicy::SelectMovementIntent(
                TacticalContext,
                TacticalState,
                BaseMovementIntent,
                bProfileAffectsPlay
                    ? FAdaptiveTacticalProfilePolicy::ResolveOrbitRight(
                        AdaptiveTacticalProfile,
                        bStrafeRight
                    )
                    : bStrafeRight
            );
        ApplyMovement(MovementIntent);
        EnemyCombatComponent->CommitMovementAction(
            ToMovementAction(MovementIntent)
        );
    }

    if (!bCombatDecisionDue)
    {
        return;
    }

    bLastDecisionUrgent = bUrgentDecisionDue;

    FEnemyActionScoringContext ScoringContext =
        BuildScoringContext(
            Snapshot,
            TacticalContext,
            bAdaptiveCounterOpportunity
        );
    if (bLastPredictionApplied
        && (!bProfileAffectsPlay || bAdaptiveCounterOpportunity))
    {
        ScoringContext.Prediction = LastPrediction;
    }
    const bool bExecutedAction = TryExecuteUtilityAction(
        ScoringContext,
        TacticalContext,
        CurrentTime,
        bAdaptiveCounterOpportunity
    );
    if (bExecutedAction && bAdaptiveCounterOpportunity)
    {
        AdaptiveCounterBudget.Consume(
            CurrentTime,
            AdaptiveProfileTuning.GetSanitized().CounterCooldown
        );
    }
    if (!bExecutedAction && AttackSequence.IsActive())
    {
        // A failed or unavailable follow-up ends the sequence instead of
        // retrying it every decision interval.
        AttackSequence.Reset();
    }
}

void UEnemyDecisionComponent::AcquirePlayerTarget()
{
    if (UWorld* World = GetWorld())
    {
        SetTargetActor(UGameplayStatics::GetPlayerPawn(World, 0));
    }
}

void UEnemyDecisionComponent::BindBehaviorTracker()
{
    UPlayerBehaviorTrackerComponent* NewTracker = TargetActor
        ? TargetActor->FindComponentByClass<
            UPlayerBehaviorTrackerComponent>()
        : nullptr;
    if (BehaviorTrackerComponent == NewTracker)
    {
        return;
    }

    UnbindBehaviorTracker();
    BehaviorTrackerComponent = NewTracker;
    if (!BehaviorTrackerComponent)
    {
        return;
    }

    BehaviorTrackerComponent->OnTrainingSampleRecorded.RemoveAll(this);
    BehaviorTrackerComponent->OnTrainingSampleRecorded.AddUObject(
        this,
        &UEnemyDecisionComponent::HandleTrainingSampleRecorded
    );

    if (bAutomaticRetrainingEnabled
        && BehaviorTrackerComponent->GetSampleCount()
                + PersistentTrainingDataset.Num()
            >= FMath::Max(1, MinimumTrainingSamples))
    {
        TrainPredictor(BehaviorTrackerComponent->GetDataset());
    }
}

void UEnemyDecisionComponent::UnbindBehaviorTracker()
{
    if (BehaviorTrackerComponent)
    {
        BehaviorTrackerComponent->OnTrainingSampleRecorded.RemoveAll(this);
        BehaviorTrackerComponent = nullptr;
    }
}

void UEnemyDecisionComponent::BindTargetCombat()
{
    UCombatComponent* NewCombat = TargetActor
        ? TargetActor->FindComponentByClass<UCombatComponent>()
        : nullptr;
    if (TargetCombatComponent == NewCombat)
    {
        return;
    }

    UnbindTargetCombat();
    TargetCombatComponent = NewCombat;
    if (TargetCombatComponent)
    {
        TargetCombatComponent->OnActionPhaseChanged.RemoveDynamic(
            this,
            &UEnemyDecisionComponent::HandleTargetActionPhaseChanged
        );
        TargetCombatComponent->OnActionPhaseChanged.AddDynamic(
            this,
            &UEnemyDecisionComponent::HandleTargetActionPhaseChanged
        );
    }
}

void UEnemyDecisionComponent::UnbindTargetCombat()
{
    if (TargetCombatComponent)
    {
        TargetCombatComponent->OnActionPhaseChanged.RemoveDynamic(
            this,
            &UEnemyDecisionComponent::HandleTargetActionPhaseChanged
        );
        TargetCombatComponent = nullptr;
    }
}

void UEnemyDecisionComponent::HandleTargetActionPhaseChanged(
    const ECombatActionPhase PreviousPhase,
    const ECombatActionPhase NewPhase
)
{
    static_cast<void>(PreviousPhase);
    if (!TargetCombatComponent)
    {
        return;
    }

    const double CurrentTime = GetDecisionTimeSeconds();
    const bool bEnemyCanDecide = bDecisionMakingEnabled
        && EnemyCombatComponent
        && !EnemyCombatComponent->IsCommittedActionActive()
        && (!OwnerHealthComponent || !OwnerHealthComponent->IsDead());
    const bool bScheduled = UrgentDecisionState.ObservePhaseEdge(
        TargetCombatComponent->GetLastAction(),
        NewPhase,
        bEnemyCanDecide,
        CurrentTime,
        UrgentDecisionTuning
    );

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    if (!UrgentDecisionState.IsPending())
    {
        World->GetTimerManager().ClearTimer(UrgentDecisionTimer);
        return;
    }
    if (bScheduled)
    {
        const float Delay = static_cast<float>(FMath::Max(
            0.001,
            UrgentDecisionState.GetReadyTime() - CurrentTime
        ));
        World->GetTimerManager().SetTimer(
            UrgentDecisionTimer,
            this,
            &UEnemyDecisionComponent::EvaluateDecision,
            Delay,
            false
        );
    }
}

void UEnemyDecisionComponent::HandleTrainingSampleRecorded(
    UPlayerBehaviorTrackerComponent* Tracker,
    const FTrainingSample& Sample
)
{
    static_cast<void>(Sample);
    if (!bAutomaticRetrainingEnabled || Tracker != BehaviorTrackerComponent)
    {
        return;
    }

    const int32 SampleCount = Tracker->GetSampleCount();
    const int32 TotalAvailableSampleCount = SampleCount
        + PersistentTrainingDataset.Num();
    const int32 RequiredSamples = FMath::Max(1, MinimumTrainingSamples);
    const int32 RefreshInterval = FMath::Max(1, RetrainSampleInterval);
    if (TotalAvailableSampleCount >= RequiredSamples
        && (TrainedSampleCount < RequiredSamples
            || SampleCount - LiveSampleCountAtLastTraining
                >= RefreshInterval))
    {
        TrainPredictor(Tracker->GetDataset());
    }
}

void UEnemyDecisionComponent::ApplyMovement(
    const EEnemyLocomotionIntent MovementIntent
)
{
    if (!EnemyLocomotionComponent && GetOwner())
    {
        EnemyLocomotionComponent =
            GetOwner()->FindComponentByClass<UEnemyLocomotionComponent>();
    }
    if (EnemyLocomotionComponent)
    {
        EnemyLocomotionComponent->RequestMovementIntent(
            MovementIntent,
            TargetActor
        );
    }
}

FCombatSnapshot UEnemyDecisionComponent::CaptureDecisionSnapshot(
    const float DistanceToTarget
) const
{
    if (UCombatSnapshotComponent* SnapshotComponent = TargetActor
        ? TargetActor->FindComponentByClass<UCombatSnapshotComponent>()
        : nullptr)
    {
        return SnapshotComponent->CaptureSnapshot();
    }

    FCombatSnapshot Snapshot;
    Snapshot.DistanceCategory = UCombatSnapshotComponent::ClassifyDistance(
        DistanceToTarget,
        300.0f,
        700.0f
    );

    if (const UHealthComponent* TargetHealth = TargetActor
        ? TargetActor->FindComponentByClass<UHealthComponent>()
        : nullptr)
    {
        Snapshot.PlayerHealthNormalized =
            TargetHealth->GetNormalizedHealth();
    }
    if (OwnerHealthComponent)
    {
        Snapshot.EnemyHealthNormalized =
            OwnerHealthComponent->GetNormalizedHealth();
    }
    if (const UStaminaComponent* TargetStamina = TargetActor
        ? TargetActor->FindComponentByClass<UStaminaComponent>()
        : nullptr)
    {
        Snapshot.PlayerStaminaNormalized =
            TargetStamina->GetNormalizedStamina();
    }
    if (const UStaminaComponent* OwnerStamina = GetOwner()
        ? GetOwner()->FindComponentByClass<UStaminaComponent>()
        : nullptr)
    {
        Snapshot.EnemyStaminaNormalized =
            OwnerStamina->GetNormalizedStamina();
    }
    if (const UCombatComponent* TargetCombat = TargetActor
        ? TargetActor->FindComponentByClass<UCombatComponent>()
        : nullptr)
    {
        Snapshot.PreviousPlayerAction = TargetCombat->GetLastAction();
    }
    if (EnemyCombatComponent)
    {
        Snapshot.PreviousEnemyAction = EnemyCombatComponent->GetLastAction();
    }
    if (GetOwner() && TargetActor)
    {
        Snapshot.RelativePlayerPosition =
            UCombatSnapshotComponent::ClassifyRelativePosition(
                TargetActor->GetActorLocation(),
                GetOwner()->GetActorLocation(),
                GetOwner()->GetActorForwardVector()
            );
    }
    return Snapshot;
}

FEnemyTacticalContext UEnemyDecisionComponent::BuildTacticalContext(
    const FCombatSnapshot& Snapshot,
    const float DistanceToTarget,
    const bool bHasLineOfSight
) const
{
    FEnemyTacticalContext Context;
    Context.DistanceToTarget = DistanceToTarget;
    const bool bProfileAffectsPlay = bPredictionUsageEnabled
        && AdaptiveTacticalProfile.IsActive();
    Context.RetreatDistance = bProfileAffectsPlay
        ? FAdaptiveTacticalProfilePolicy::AdjustSpacingDistance(
            GetRetreatDistance(),
            AdaptiveTacticalProfile,
            0.25f,
            AdaptiveProfileTuning
        )
        : GetRetreatDistance();
    Context.PreferredDistance = bProfileAffectsPlay
        ? FAdaptiveTacticalProfilePolicy::AdjustSpacingDistance(
            GetPreferredDistance(),
            AdaptiveTacticalProfile,
            1.0f,
            AdaptiveProfileTuning
        )
        : GetPreferredDistance();
    Context.OrbitDistance = bProfileAffectsPlay
        ? FAdaptiveTacticalProfilePolicy::AdjustSpacingDistance(
            GetStrafeDistance(),
            AdaptiveTacticalProfile,
            0.75f,
            AdaptiveProfileTuning
        )
        : GetStrafeDistance();
    Context.MeleeDistance = EnemyCombatComponent
        ? EnemyCombatComponent->GetMeleeRange()
        : 190.0f;
    Context.EnemyHealthNormalized = Snapshot.EnemyHealthNormalized;
    Context.EnemyStaminaNormalized = Snapshot.EnemyStaminaNormalized;
    Context.TimeSincePlayerAttack = Snapshot.TimeSincePlayerAttack;
    Context.RecentPlayerAction = Snapshot.PreviousPlayerAction;
    Context.RecentEnemyAction = LastSelectedAction;
    Context.bHasLineOfSight = bHasLineOfSight;
    Context.bTargetRecentlyHealing = IsTargetRecentlyHealing();
    Context.bAttackSequenceActive = AttackSequence.IsActive();

    const UCombatComponent* PlayerCombat = TargetActor
        ? TargetActor->FindComponentByClass<UCombatComponent>()
        : nullptr;
    if (PlayerCombat)
    {
        // Both inputs come from the executor after commit. Buffered input is
        // private to UCombatComponent and cannot enter the tactical context.
        Context.RecentPlayerAction = PlayerCombat->GetLastAction();
        Context.bPlayerAttacking =
            FEnemyTacticalPolicy::IsCommittedPlayerAttack(
                PlayerCombat->GetLastAction(),
                PlayerCombat->GetCurrentPhase()
            );
    }

    if (bLastPredictionApplied)
    {
        Context.PredictedPlayerAction = LastPrediction.PredictedAction;
        Context.PredictionConfidence = LastPrediction.Confidence;
    }

    if (!EnemyCombatComponent)
    {
        return Context;
    }

    const bool bInMeleeRange = FMath::IsFinite(DistanceToTarget)
        && DistanceToTarget <= EnemyCombatComponent->GetMeleeRange();
    Context.bLightAttackAvailable = bInMeleeRange
        && EnemyCombatComponent->CanExecuteAction(
            EEnemyCombatAction::LightAttack
        );
    Context.bHeavyAttackAvailable = bInMeleeRange
        && EnemyCombatComponent->CanExecuteAction(
            EEnemyCombatAction::HeavyAttack
        );
    Context.bProjectileAvailable =
        FEnemyTacticalPolicy::IsProjectilePermitted(
            bHasLineOfSight,
            EnemyCombatComponent->CanExecuteAction(
                EEnemyCombatAction::ProjectileAttack
            )
        );
    Context.bDashAvailable = EnemyCombatComponent->CanExecuteAction(
        EEnemyCombatAction::DashAttack
    );
    Context.bBlockAvailable = EnemyCombatComponent->CanExecuteAction(
        EEnemyCombatAction::Block
    );
    Context.bDodgeAvailable = EnemyCombatComponent->CanExecuteAction(
        EEnemyCombatAction::Dodge
    );
    Context.bInterruptAvailable = Context.bTargetRecentlyHealing
        && FMath::IsFinite(DistanceToTarget)
        && DistanceToTarget <= FMath::Max(0.0f, InterruptDistance)
        && EnemyCombatComponent->CanExecuteAction(
            EEnemyCombatAction::InterruptHeal
        );
    return Context;
}

FEnemyActionScoringContext UEnemyDecisionComponent::BuildScoringContext(
    const FCombatSnapshot& Snapshot,
    const FEnemyTacticalContext& TacticalContext,
    const bool bAdaptiveCounterOpportunity
) const
{
    FEnemyActionScoringContext Context;
    Context.Snapshot = Snapshot;
    Context.bTargetRecentlyHealing =
        TacticalContext.bTargetRecentlyHealing;
    Context.PredictionInfluence = PredictionInfluence;
    Context.RecentLightAttackPressure = GetRecentLightAttackPressure();
    Context.MaximumLightAttackPatternAdjustment =
        MaximumLightAttackPatternAdjustment;
    const FEnemyTacticalTuning SafeTuning =
        TacticalTuning.GetSanitized();
    Context.MaximumTacticalUtilityAdjustment =
        SafeTuning.MaxUtilityAdjustment;
    const FAdaptiveTacticalProfileTuning SafeProfileTuning =
        AdaptiveProfileTuning.GetSanitized();
    Context.MaximumAdaptiveUtilityAdjustment =
        SafeProfileTuning.MaximumUtilityAdjustment;
    const FAdaptiveCounterEffectivenessTuning SafeOutcomeTuning =
        CounterEffectivenessTuning.GetSanitized();
    Context.MaximumOutcomeEffectivenessUtilityAdjustment =
        SafeOutcomeTuning.MaximumUtilityAdjustment;
    const FEnemyActionRepetitionTuning SafeRepetitionTuning =
        RepetitionTuning.GetSanitized();
    const FEnemyOffenseDefenseBalanceTuning SafeBalanceTuning =
        OffenseDefenseBalanceTuning.GetSanitized();
    const FEnemyRecentOutcomeTuning SafeRecentOutcomeTuning =
        RecentOutcomeTuning.GetSanitized();
    Context.MaximumRepetitionUtilityAdjustment =
        SafeRepetitionTuning.MaximumRepetitionAdjustment;
    Context.MaximumOffenseDefenseBalanceUtilityAdjustment =
        SafeBalanceTuning.MaximumBalanceAdjustment;
    Context.MaximumRecentOutcomeUtilityAdjustment =
        SafeRecentOutcomeTuning.MaximumOutcomeAdjustment;
    const bool bProfileAffectsPlay = bPredictionUsageEnabled
        && AdaptiveTacticalProfile.IsActive();

    const EEnemyCombatAction ScoredActions[] = {
        EEnemyCombatAction::LightAttack,
        EEnemyCombatAction::HeavyAttack,
        EEnemyCombatAction::ProjectileAttack,
        EEnemyCombatAction::DashAttack,
        EEnemyCombatAction::Block,
        EEnemyCombatAction::Dodge,
        EEnemyCombatAction::InterruptHeal
    };
    for (const EEnemyCombatAction Action : ScoredActions)
    {
        if (TacticalState == EEnemyTacticalState::Recover
            || !TacticalContext.IsActionAvailable(Action))
        {
            Context.UnavailableActions.Add(Action);
        }
        Context.TacticalUtilityModifiers.Add(
            Action,
            FEnemyTacticalPolicy::GetUtilityModifier(
                TacticalState,
                Action,
                SafeTuning.MaxUtilityAdjustment
            )
        );
        Context.AdaptiveUtilityModifiers.Add(
            Action,
            bProfileAffectsPlay
                ? FAdaptiveTacticalProfilePolicy::GetUtilityModifier(
                    AdaptiveTacticalProfile,
                    Action,
                    bAdaptiveCounterOpportunity,
                    SafeProfileTuning
                )
                : 0.0f
        );
        Context.OutcomeEffectivenessUtilityModifiers.Add(
            Action,
            bProfileAffectsPlay
                ? FAdaptiveCounterEffectivenessPolicy::GetUtilityModifier(
                    ActiveCounterEffectiveness,
                    Action,
                    bAdaptiveCounterOpportunity,
                    SafeOutcomeTuning
                )
                : 0.0f
        );
        Context.RepetitionUtilityModifiers.Add(
            Action,
            FEnemyActionRepetitionPolicy::Evaluate(
                Action,
                CommittedActionHistory,
                SafeRepetitionTuning
            ).Total
        );
        Context.OffenseDefenseBalanceUtilityModifiers.Add(
            Action,
            FEnemyOffenseDefenseBalancePolicy::Evaluate(
                Action,
                CommittedActionHistory,
                SafeBalanceTuning
            ).Total
        );
        Context.RecentOutcomeUtilityModifiers.Add(
            Action,
            FEnemyRecentOutcomePolicy::Evaluate(
                Action,
                RecentOutcomeMemory,
                SafeRecentOutcomeTuning
            ).Total
        );
    }

    if (AttackSequence.IsActive())
    {
        // The scorer still decides. The sequence merely uses the same bounded
        // state-weight budget to prefer one follow-up and de-emphasize other
        // attacks; it can never bypass availability, range, or cooldowns.
        for (const EEnemyCombatAction Action : ScoredActions)
        {
            if (!FEnemyTacticalPolicy::IsOffensiveAction(Action))
            {
                continue;
            }
            Context.TacticalUtilityModifiers.Add(
                Action,
                Action == AttackSequence.FollowUpAction
                    ? SafeTuning.MaxUtilityAdjustment
                    : -SafeTuning.MaxUtilityAdjustment
            );
        }
    }
    return Context;
}

bool UEnemyDecisionComponent::TryExecuteUtilityAction(
    const FEnemyActionScoringContext& Context,
    const FEnemyTacticalContext& TacticalContext,
    const double CurrentTime,
    const bool bAdaptiveCounterOpportunity
)
{
    LastActionScores = ActionScorer.ScoreActions(Context);
    LastSelectedUtilityAction = EEnemyCombatAction::None;
    LastSelectionResult = FEnemyActionSelectionResult();
    LastRepetitionModifiers = Context.RepetitionUtilityModifiers;
    LastOffenseDefenseBalanceModifiers =
        Context.OffenseDefenseBalanceUtilityModifiers;
    LastRecentOutcomeModifiers = Context.RecentOutcomeUtilityModifiers;

    TSet<EEnemyCombatAction> RejectedActions;
    for (int32 Attempt = 0; Attempt < LastActionScores.Num(); ++Attempt)
    {
        const FEnemyActionSelectionResult Selection =
            FEnemyActionSelectionPolicy::Select(
                LastActionScores,
                RejectedActions,
                SelectionRandomStream.FRand(),
                ActionSelectionTuning
            );
        LastSelectionResult = Selection;
        if (!Selection.IsValid())
        {
            break;
        }

        if (EnemyCombatComponent->TryExecuteAction(
            Selection.Action,
            TargetActor
        ))
        {
            RegisterOutcomeContext(
                EnemyCombatComponent->GetLastCommittedOutcomeId(),
                Selection.Action,
                Context.Snapshot,
                bAdaptiveCounterOpportunity
            );
            CommittedActionHistory.Record(
                Selection.Action,
                FMath::Max(
                    RepetitionTuning.GetSanitized().HistorySize,
                    OffenseDefenseBalanceTuning.GetSanitized().HistorySize
                )
            );
            LastSelectedUtilityAction = Selection.Action;
            LastSelectedAction = Selection.Action;
            HandleExecutedCombatAction(
                Selection.Action,
                TacticalContext,
                CurrentTime
            );
            const UEnum* ActionEnum = StaticEnum<EEnemyCombatAction>();
            UE_LOG(
                LogAdaptHunt,
                Verbose,
                TEXT(
                    "Enemy %s decision selected %s at raw %.3f "
                    "(best %.3f, candidates %d, repetition %+.3f, "
                    "balance %+.3f, recent outcome %+.3f)."
                ),
                bLastDecisionUrgent ? TEXT("urgent") : TEXT("normal"),
                ActionEnum
                    ? *ActionEnum->GetNameStringByValue(
                        static_cast<int64>(Selection.Action)
                    )
                    : TEXT("Unknown"),
                Selection.SelectedScore,
                Selection.BestScore,
                Selection.CandidateActions.Num(),
                LastRepetitionModifiers.FindRef(Selection.Action),
                LastOffenseDefenseBalanceModifiers.FindRef(
                    Selection.Action
                ),
                LastRecentOutcomeModifiers.FindRef(Selection.Action)
            );
            return true;
        }
        RejectedActions.Add(Selection.Action);
    }

    return false;
}

void UEnemyDecisionComponent::RegisterOutcomeContext(
    const int32 ActionId,
    const EEnemyCombatAction Action,
    const FCombatSnapshot& Snapshot,
    const bool bAdaptiveCounterOpportunity
)
{
    if (ActionId <= 0 || !AdaptiveCombat::IsUtilityEnemyAction(Action))
    {
        return;
    }

    FAdaptiveCounterOutcomeRecord Record;
    Record.ActionId = ActionId;
    Record.RoundNumber = FMath::Max(1, Snapshot.RoundNumber);
    Record.Action = Action;
    Record.ProfilePreferredCounterAction =
        AdaptiveTacticalProfile.IsActive()
        ? AdaptiveTacticalProfile.PreferredCounterAction
        : EEnemyCombatAction::None;
    Record.Prediction = LastPrediction;
    Record.ContextSnapshot = Snapshot;
    Record.ContextSnapshot.RoundNumber = Record.RoundNumber;
    Record.bPredictionApplied = bLastPredictionApplied;
    Record.bAdaptiveCounterOpportunity =
        bAdaptiveCounterOpportunity;
    PendingOutcomeRecords.Add(ActionId, Record);
}

void UEnemyDecisionComponent::HandleEnemyActionResolved(
    const FEnemyCombatActionOutcome& Outcome
)
{
    FAdaptiveCounterOutcomeRecord* Pending =
        PendingOutcomeRecords.Find(Outcome.ActionId);
    if (!Outcome.IsValid() || !Pending || Pending->Action != Outcome.Action)
    {
        return;
    }

    FAdaptiveCounterOutcomeRecord Record = *Pending;
    PendingOutcomeRecords.Remove(Outcome.ActionId);
    Record.Result = Outcome.Result;
    if (!CounterOutcomeHistory.Record(Record))
    {
        return;
    }
    RecentOutcomeMemory.Record(
        Outcome,
        RecentOutcomeTuning.GetSanitized().MemorySize
    );

    const UEnum* PlayerActionEnum = StaticEnum<EPlayerCombatAction>();
    const UEnum* EnemyActionEnum = StaticEnum<EEnemyCombatAction>();
    UE_LOG(
        LogAdaptHunt,
        Verbose,
        TEXT(
            "Enemy outcome #%d round %d: predicted=%s (%.1f%%, "
            "applied=%s), counter=%s, result=%s, worked=%s."
        ),
        Record.ActionId,
        Record.RoundNumber,
        PlayerActionEnum && Record.Prediction.bHasPrediction
            ? *PlayerActionEnum->GetNameStringByValue(static_cast<int64>(
                Record.Prediction.PredictedAction
            ))
            : TEXT("None"),
        FMath::Clamp(Record.Prediction.Confidence, 0.0f, 1.0f)
            * 100.0f,
        Record.bPredictionApplied ? TEXT("true") : TEXT("false"),
        EnemyActionEnum
            ? *EnemyActionEnum->GetNameStringByValue(
                static_cast<int64>(Record.Action)
            )
            : TEXT("Unknown"),
        *GetAdaptiveCounterOutcomeName(Record.Result),
        Record.IsAdaptiveCounterAttempt()
            ? (Record.WasSuccessfulCounter()
                ? TEXT("true")
                : TEXT("false"))
            : TEXT("not evaluated")
    );
}

void UEnemyDecisionComponent::UpdateTacticalState(
    const FEnemyTacticalContext& Context,
    const double CurrentTime
)
{
    const float TimeInState = FMath::IsFinite(CurrentTime)
        ? static_cast<float>(FMath::Max(
            0.0,
            CurrentTime - TacticalStateEnteredTime
        ))
        : TacticalTuning.GetSanitized().StateCommitDuration;
    const EEnemyTacticalState NewState =
        FEnemyTacticalPolicy::SelectState(
            Context,
            TacticalState,
            TimeInState,
            TacticalTuning
        );
    if (NewState == TacticalState)
    {
        return;
    }

    const EEnemyTacticalState PreviousState = TacticalState;
    TacticalState = NewState;
    TacticalStateEnteredTime = CurrentTime;

    const bool bUsesOrbitDirection =
        NewState == EEnemyTacticalState::Observe
        || NewState == EEnemyTacticalState::Orbit
        || NewState == EEnemyTacticalState::Retreat
        || NewState == EEnemyTacticalState::Defend
        || NewState == EEnemyTacticalState::Recover;
    if (bUsesOrbitDirection && CurrentTime >= NextOrbitDirectionChangeTime)
    {
        bStrafeRight = TacticalRandomStream.FRand() >= 0.5f;
        NextOrbitDirectionChangeTime = CurrentTime
            + TacticalTuning.GetSanitized().OrbitDirectionCommitDuration;
    }

    const UEnum* StateEnum = StaticEnum<EEnemyTacticalState>();
    UE_LOG(
        LogAdaptHunt,
        Verbose,
        TEXT("Enemy tactic changed from %s to %s."),
        StateEnum
            ? *StateEnum->GetNameStringByValue(
                static_cast<int64>(PreviousState)
            )
            : TEXT("Unknown"),
        StateEnum
            ? *StateEnum->GetNameStringByValue(
                static_cast<int64>(NewState)
            )
            : TEXT("Unknown")
    );
}

void UEnemyDecisionComponent::UpdateAttackSequence(
    const FEnemyTacticalContext& Context,
    const double CurrentTime,
    const bool bCombatDecisionDue
)
{
    if (!AttackSequence.IsActive())
    {
        return;
    }

    if (!FEnemyTacticalPolicy::ShouldContinueSequence(
        AttackSequence,
        Context,
        TacticalState,
        CurrentTime,
        TacticalTuning
    ) || (bCombatDecisionDue
        && !Context.IsActionAvailable(AttackSequence.FollowUpAction)))
    {
        AttackSequence.Reset();
    }
}

void UEnemyDecisionComponent::HandleExecutedCombatAction(
    const EEnemyCombatAction Action,
    const FEnemyTacticalContext& Context,
    const double CurrentTime
)
{
    if (AttackSequence.IsActive())
    {
        // A follow-up or any incompatible selected action is a hard exit. This
        // caps the sequence at exactly two committed attacks.
        AttackSequence.Reset();
        return;
    }

    const FEnemyTacticalTuning SafeTuning =
        TacticalTuning.GetSanitized();
    const bool bSequenceState = TacticalState == EEnemyTacticalState::Pressure
        || TacticalState == EEnemyTacticalState::Punish;
    if (Action != EEnemyCombatAction::LightAttack || !bSequenceState
        || !Context.bHasLineOfSight || Context.bPlayerAttacking
        || !FMath::IsFinite(Context.DistanceToTarget)
        || Context.DistanceToTarget > SafeTuning.SequenceExitDistance
        || Context.EnemyStaminaNormalized
            <= SafeTuning.LowStaminaThreshold
        || TacticalRandomStream.FRand() >= SafeTuning.SequenceStartChance)
    {
        return;
    }

    AttackSequence.Start(
        FEnemyTacticalPolicy::SelectSequenceFollowUp(
            TacticalRandomStream.FRand()
        ),
        CurrentTime + SafeTuning.SequenceWindow
    );
}

void UEnemyDecisionComponent::ResetTacticalState(const double CurrentTime)
{
    const FEnemyTacticalTuning SafeTuning =
        TacticalTuning.GetSanitized();
    TacticalState = EEnemyTacticalState::Observe;
    TacticalStateEnteredTime = FMath::IsFinite(CurrentTime)
        ? CurrentTime - SafeTuning.StateCommitDuration
        : -static_cast<double>(SafeTuning.StateCommitDuration);
    NextOrbitDirectionChangeTime = FMath::IsFinite(CurrentTime)
        ? CurrentTime
        : 0.0;
    AttackSequence.Reset();
    bLastLineOfSightClear = false;
}

bool UEnemyDecisionComponent::HasLineOfSightToTarget() const
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World || !IsValid(TargetActor))
    {
        return false;
    }

    FVector Start;
    FRotator ViewRotation;
    Owner->GetActorEyesViewPoint(Start, ViewRotation);
    FVector End;
    TargetActor->GetActorEyesViewPoint(End, ViewRotation);

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(EnemyProjectileLineOfSight),
        true,
        Owner
    );
    FHitResult Hit;
    const bool bBlocked = World->LineTraceSingleByChannel(
        Hit,
        Start,
        End,
        ECC_Visibility,
        QueryParams
    );
    return !bBlocked || Hit.GetActor() == TargetActor;
}

float UEnemyDecisionComponent::GetPreferredDistance() const
{
    const UEnemyLocomotionComponent* Locomotion =
        EnemyLocomotionComponent
            ? EnemyLocomotionComponent.Get()
            : (GetOwner()
                ? GetOwner()->FindComponentByClass<
                    UEnemyLocomotionComponent>()
                : nullptr);
    return Locomotion
        ? Locomotion->GetPreferredRange()
        : (GetRetreatDistance() + GetStrafeDistance()) * 0.5f;
}

EEnemyCombatAction UEnemyDecisionComponent::ToMovementAction(
    const EEnemyLocomotionIntent MovementIntent
)
{
    switch (MovementIntent)
    {
    case EEnemyLocomotionIntent::Approach:
        return EEnemyCombatAction::MoveTowardPlayer;
    case EEnemyLocomotionIntent::Retreat:
        return EEnemyCombatAction::MoveAwayFromPlayer;
    case EEnemyLocomotionIntent::OrbitLeft:
        return EEnemyCombatAction::StrafeLeft;
    case EEnemyLocomotionIntent::OrbitRight:
        return EEnemyCombatAction::StrafeRight;
    default:
        return EEnemyCombatAction::None;
    }
}

FPredictionResult UEnemyDecisionComponent::PredictPlayerAction(
    const FCombatSnapshot& Snapshot
) const
{
    return Predictor
        ? Predictor->Predict(Snapshot)
        : FPredictionResult();
}

bool UEnemyDecisionComponent::ShouldApplyPrediction(
    const FPredictionResult& Prediction
)
{
    return ShouldApplyPredictionDeterministically(
        Prediction,
        bPredictionUsageEnabled,
        MinimumPredictionConfidence,
        MinimumPredictionSupportingSamples,
        PredictionApplicationChance,
        AdaptationRandomStream.FRand()
    );
}

bool UEnemyDecisionComponent::IsTargetRecentlyHealing() const
{
    const UCombatComponent* PlayerCombat = TargetActor
        ? TargetActor->FindComponentByClass<UCombatComponent>()
        : nullptr;
    if (!PlayerCombat
        || PlayerCombat->GetLastAction() != EPlayerCombatAction::Heal)
    {
        return false;
    }

    const ECombatActionPhase Phase = PlayerCombat->GetCurrentPhase();
    return Phase == ECombatActionPhase::Windup
        || Phase == ECombatActionPhase::Active
        || Phase == ECombatActionPhase::Recovery;
}

void UEnemyDecisionComponent::HandleOwnerDeath(UHealthComponent*, AActor*)
{
    SetDecisionMakingEnabled(false);
}

double UEnemyDecisionComponent::GetDecisionTimeSeconds() const
{
    const UWorld* World = GetWorld();
    return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}
