#include "Components/EnemyDecisionComponent.h"

#include "adaptHunt.h"
#include "AI/FrequencyActionPredictor.h"
#include "Components/CombatComponent.h"
#include "Components/CombatSnapshotComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/PlayerBehaviorTrackerComponent.h"
#include "Components/StaminaComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UEnemyDecisionComponent::UEnemyDecisionComponent()
    : TargetActor(nullptr)
    , EnemyCombatComponent(nullptr)
    , OwnerHealthComponent(nullptr)
    , BehaviorTrackerComponent(nullptr)
    , EvaluationInterval(0.1f)
    , CombatDecisionInterval(0.85f)
    , RetreatDistance(180.0f)
    , StrafeDistance(625.0f)
    , StrafeSwitchInterval(1.4f)
    , RangedCombatDistance(425.0f)
    , DashCombatDistance(900.0f)
    , InterruptDistance(260.0f)
    , PredictionInfluence(0.40f)
    , MinimumPredictionConfidence(0.40f)
    , PredictionApplicationChance(0.80f)
    , MinimumPredictionSupportingSamples(2)
    , MinimumTrainingSamples(3)
    , RetrainSampleInterval(3)
    , AdaptationRandomSeed(1337)
    , LastSelectedUtilityAction(EEnemyCombatAction::None)
    , LastSelectedAction(EEnemyCombatAction::None)
    , TrainedSampleCount(0)
    , bLastPredictionApplied(false)
    , bDecisionMakingEnabled(true)
    , bAutomaticRetrainingEnabled(true)
    , bPredictionUsageEnabled(true)
    , bStrafeRight(false)
    , NextCombatDecisionTime(0.0)
    , NextStrafeSwitchTime(0.0)
    , Predictor(MakeUnique<FFrequencyActionPredictor>())
    , AdaptationRandomStream(1337)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyDecisionComponent::BeginPlay()
{
    Super::BeginPlay();

    AActor* Owner = GetOwner();
    EnemyCombatComponent = Owner
        ? Owner->FindComponentByClass<UEnemyCombatComponent>()
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
    AcquirePlayerTarget();
    BindBehaviorTracker();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            EvaluationTimer,
            this,
            &UEnemyDecisionComponent::EvaluateDecision,
            FMath::Max(0.02f, EvaluationInterval),
            true,
            0.1f
        );
    }

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
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(EvaluationTimer);
    }
    if (OwnerHealthComponent)
    {
        OwnerHealthComponent->OnDeath.RemoveAll(this);
    }

    Super::EndPlay(EndPlayReason);
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
    TargetActor = ValidTarget;
    ResetPredictor();
    LastActionScores.Reset();
    LastSelectedUtilityAction = EEnemyCombatAction::None;
    LastSelectedAction = EEnemyCombatAction::None;
    if (HasBegunPlay())
    {
        BindBehaviorTracker();
    }
}

AActor* UEnemyDecisionComponent::GetTargetActor() const
{
    return TargetActor;
}

void UEnemyDecisionComponent::SetDecisionMakingEnabled(const bool bEnabled)
{
    bDecisionMakingEnabled = bEnabled;
    if (!bEnabled)
    {
        LastSelectedAction = EEnemyCombatAction::None;
        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
            Character->GetMovementComponent()->StopMovementImmediately();
        }
    }
}

bool UEnemyDecisionComponent::IsDecisionMakingEnabled() const
{
    return bDecisionMakingEnabled;
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
        Predictor = MakeUnique<FFrequencyActionPredictor>();
    }

    Predictor->Train(Dataset);
    TrainedSampleCount = Dataset.Num();
    LastPrediction = FPredictionResult();
    bLastPredictionApplied = false;
}

bool UEnemyDecisionComponent::TrainPredictorFromTargetHistory()
{
    BindBehaviorTracker();
    if (!BehaviorTrackerComponent
        || BehaviorTrackerComponent->GetSampleCount()
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
    LastPrediction = FPredictionResult();
    bLastPredictionApplied = false;
}

void UEnemyDecisionComponent::SetPredictor(
    TUniquePtr<IPlayerActionPredictor> NewPredictor
)
{
    Predictor = MoveTemp(NewPredictor);
    if (!Predictor)
    {
        Predictor = MakeUnique<FFrequencyActionPredictor>();
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
    bPredictionUsageEnabled = bEnabled;
    if (!bEnabled)
    {
        bLastPredictionApplied = false;
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
    if (!FMath::IsFinite(DistanceToTarget))
    {
        return EEnemyCombatAction::MoveTowardPlayer;
    }

    const float SafeDistance = FMath::Max(0.0f, DistanceToTarget);
    if (SafeDistance < RetreatDistance)
    {
        return EEnemyCombatAction::MoveAwayFromPlayer;
    }
    if (SafeDistance > StrafeDistance)
    {
        return EEnemyCombatAction::MoveTowardPlayer;
    }
    return bStrafeRight
        ? EEnemyCombatAction::StrafeRight
        : EEnemyCombatAction::StrafeLeft;
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
    return RetreatDistance;
}

float UEnemyDecisionComponent::GetStrafeDistance() const
{
    return StrafeDistance;
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

    const FVector ToTarget = TargetActor->GetActorLocation()
        - Character->GetActorLocation();
    const float Distance = ToTarget.Size2D();
    if (Distance <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const FRotator DesiredRotation = ToTarget.Rotation();
    const FRotator FlatDesiredRotation(
        0.0f,
        DesiredRotation.Yaw,
        0.0f
    );
    Character->SetActorRotation(FMath::RInterpTo(
        Character->GetActorRotation(),
        FlatDesiredRotation,
        EvaluationInterval,
        8.0f
    ));

    const double CurrentTime = GetDecisionTimeSeconds();
    if (CurrentTime >= NextStrafeSwitchTime)
    {
        bStrafeRight = !bStrafeRight;
        NextStrafeSwitchTime = CurrentTime
            + FMath::Max(0.1f, StrafeSwitchInterval);
    }

    if (!EnemyCombatComponent->IsBlocking())
    {
        const EEnemyCombatAction MovementAction =
            SelectMovementAction(Distance);
        ApplyMovement(MovementAction);
        EnemyCombatComponent->CommitMovementAction(MovementAction);
    }

    if (CurrentTime < NextCombatDecisionTime)
    {
        return;
    }

    NextCombatDecisionTime = CurrentTime
        + FMath::Max(0.1f, CombatDecisionInterval);
    const FCombatSnapshot Snapshot = CaptureDecisionSnapshot(Distance);
    LastPrediction = PredictPlayerAction(Snapshot);
    bLastPredictionApplied = ShouldApplyPrediction(LastPrediction);

    FEnemyActionScoringContext ScoringContext =
        BuildScoringContext(Snapshot);
    if (bLastPredictionApplied)
    {
        ScoringContext.Prediction = LastPrediction;
    }
    if (!TryExecuteHighestUtilityAction(ScoringContext))
    {
        const EEnemyCombatAction Fallback =
            Distance <= EnemyCombatComponent->GetMeleeRange()
                ? EEnemyCombatAction::LightAttack
                : EEnemyCombatAction::ProjectileAttack;
        if (EnemyCombatComponent->TryExecuteAction(Fallback, TargetActor))
        {
            LastSelectedAction = Fallback;
        }
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
    const int32 RequiredSamples = FMath::Max(1, MinimumTrainingSamples);
    const int32 RefreshInterval = FMath::Max(1, RetrainSampleInterval);
    if (SampleCount >= RequiredSamples
        && (TrainedSampleCount < RequiredSamples
            || SampleCount - TrainedSampleCount >= RefreshInterval))
    {
        TrainPredictor(Tracker->GetDataset());
    }
}

void UEnemyDecisionComponent::ApplyMovement(
    const EEnemyCombatAction MovementAction
)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !TargetActor)
    {
        return;
    }

    const FVector ToTarget = (
        TargetActor->GetActorLocation() - Character->GetActorLocation()
    ).GetSafeNormal2D();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget);

    FVector Direction = FVector::ZeroVector;
    switch (MovementAction)
    {
    case EEnemyCombatAction::MoveTowardPlayer:
        Direction = ToTarget;
        break;
    case EEnemyCombatAction::MoveAwayFromPlayer:
        Direction = -ToTarget;
        break;
    case EEnemyCombatAction::StrafeLeft:
        Direction = -Right;
        break;
    case EEnemyCombatAction::StrafeRight:
        Direction = Right;
        break;
    default:
        return;
    }

    Character->AddMovementInput(Direction, 1.0f, true);
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

FEnemyActionScoringContext UEnemyDecisionComponent::BuildScoringContext(
    const FCombatSnapshot& Snapshot
) const
{
    FEnemyActionScoringContext Context;
    Context.Snapshot = Snapshot;
    Context.bTargetRecentlyHealing = IsTargetRecentlyHealing();
    Context.PredictionInfluence = PredictionInfluence;

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
        if (!EnemyCombatComponent
            || !EnemyCombatComponent->SupportsAction(Action)
            || EnemyCombatComponent->IsActionOnCooldown(Action))
        {
            Context.UnavailableActions.Add(Action);
        }
    }
    return Context;
}

bool UEnemyDecisionComponent::TryExecuteHighestUtilityAction(
    const FEnemyActionScoringContext& Context
)
{
    LastActionScores = ActionScorer.ScoreActions(Context);
    LastSelectedUtilityAction = EEnemyCombatAction::None;

    TSet<EEnemyCombatAction> RejectedActions;
    for (int32 Attempt = 0; Attempt < LastActionScores.Num(); ++Attempt)
    {
        EEnemyCombatAction BestAction = EEnemyCombatAction::None;
        float BestScore = -1.0f;
        for (const FEnemyActionScore& Candidate : LastActionScores)
        {
            if (Candidate.bAvailable
                && !RejectedActions.Contains(Candidate.Action)
                && Candidate.Score > BestScore)
            {
                BestAction = Candidate.Action;
                BestScore = Candidate.Score;
            }
        }

        if (BestAction == EEnemyCombatAction::None)
        {
            break;
        }

        if (EnemyCombatComponent->TryExecuteAction(BestAction, TargetActor))
        {
            LastSelectedUtilityAction = BestAction;
            LastSelectedAction = BestAction;
            const UEnum* ActionEnum = StaticEnum<EEnemyCombatAction>();
            UE_LOG(
                LogAdaptHunt,
                Verbose,
                TEXT("Enemy utility selected %s at %.2f."),
                ActionEnum
                    ? *ActionEnum->GetNameStringByValue(
                        static_cast<int64>(BestAction)
                    )
                    : TEXT("Unknown"),
                BestScore
            );
            return true;
        }
        RejectedActions.Add(BestAction);
    }

    return false;
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
    return PlayerCombat
        && PlayerCombat->GetLastAction() == EPlayerCombatAction::Heal;
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
