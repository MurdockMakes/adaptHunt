#include "Components/PlayerBehaviorTrackerComponent.h"

#include "adaptHunt.h"
#include "Components/CombatSnapshotComponent.h"
#include "GameFramework/Actor.h"

namespace
{
const EPlayerCombatAction TrackableActions[] = {
    EPlayerCombatAction::LightAttack,
    EPlayerCombatAction::HeavyAttack,
    EPlayerCombatAction::DodgeLeft,
    EPlayerCombatAction::DodgeRight,
    EPlayerCombatAction::DodgeBackward,
    EPlayerCombatAction::Block,
    EPlayerCombatAction::Heal
};

FString GetPlayerActionName(const EPlayerCombatAction Action)
{
    const UEnum* ActionEnum = StaticEnum<EPlayerCombatAction>();
    return ActionEnum
        ? ActionEnum->GetNameStringByValue(static_cast<int64>(Action))
        : TEXT("Unknown");
}

FString GetEnemyActionName(const EEnemyCombatAction Action)
{
    const UEnum* ActionEnum = StaticEnum<EEnemyCombatAction>();
    return ActionEnum
        ? ActionEnum->GetNameStringByValue(static_cast<int64>(Action))
        : TEXT("Unknown");
}
}

UPlayerBehaviorTrackerComponent::UPlayerBehaviorTrackerComponent()
    : SnapshotComponent(nullptr)
    , bTrackingEnabled(true)
    , bLogEverySample(true)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerBehaviorTrackerComponent::BeginPlay()
{
    Super::BeginPlay();

    CacheSnapshotComponent();
    if (SnapshotComponent)
    {
        SnapshotComponent->OnPlayerActionObserved.RemoveAll(this);
        SnapshotComponent->OnPlayerActionObserved.AddUObject(
            this,
            &UPlayerBehaviorTrackerComponent::HandlePlayerActionObserved
        );
    }
    else
    {
        UE_LOG(
            LogAdaptHunt,
            Error,
            TEXT("%s requires a CombatSnapshotComponent."),
            GetOwner()
                ? *GetOwner()->GetName()
                : TEXT("PlayerBehaviorTrackerComponent")
        );
    }
}

void UPlayerBehaviorTrackerComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (SnapshotComponent)
    {
        SnapshotComponent->OnPlayerActionObserved.RemoveAll(this);
    }

    Super::EndPlay(EndPlayReason);
}

void UPlayerBehaviorTrackerComponent::StartTracking()
{
    bTrackingEnabled = true;
}

void UPlayerBehaviorTrackerComponent::StopTracking()
{
    bTrackingEnabled = false;
}

bool UPlayerBehaviorTrackerComponent::IsTrackingEnabled() const
{
    return bTrackingEnabled;
}

void UPlayerBehaviorTrackerComponent::ResetDataset()
{
    Dataset.Reset();

    UE_LOG(LogAdaptHunt, Log, TEXT("Player behavior dataset reset."));
}

bool UPlayerBehaviorTrackerComponent::RecordPlayerAction(
    const FCombatSnapshot& Snapshot,
    const EPlayerCombatAction Action
)
{
    if (!bTrackingEnabled || Action == EPlayerCombatAction::None)
    {
        return false;
    }

    FTrainingSample Sample;
    Sample.Snapshot = Snapshot;
    Sample.NextPlayerAction = Action;
    Sample.ActionTimeSeconds = Snapshot.CaptureTimeSeconds;

    if (!Dataset.AddSample(Sample))
    {
        return false;
    }

    if (bLogEverySample)
    {
        UE_LOG(
            LogAdaptHunt,
            Log,
            TEXT("Behavior sample %d: R%d after player=%s enemy=%s -> %s"),
            Dataset.Num(),
            Snapshot.RoundNumber,
            *GetPlayerActionName(Snapshot.PreviousPlayerAction),
            *GetEnemyActionName(Snapshot.PreviousEnemyAction),
            *GetPlayerActionName(Action)
        );
    }

    OnTrainingSampleRecorded.Broadcast(this, Sample);
    return true;
}

int32 UPlayerBehaviorTrackerComponent::GetSampleCount() const
{
    return Dataset.Num();
}

int32 UPlayerBehaviorTrackerComponent::GetActionCount(
    const EPlayerCombatAction Action
) const
{
    return Dataset.GetActionCount(Action);
}

EPlayerCombatAction
UPlayerBehaviorTrackerComponent::GetMostCommonAction() const
{
    EPlayerCombatAction MostCommonAction = EPlayerCombatAction::None;
    int32 HighestCount = 0;

    for (const EPlayerCombatAction Action : TrackableActions)
    {
        const int32 Count = Dataset.GetActionCount(Action);
        if (Count > HighestCount)
        {
            HighestCount = Count;
            MostCommonAction = Action;
        }
    }

    return MostCommonAction;
}

void UPlayerBehaviorTrackerComponent::LogDataset() const
{
    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Behavior dataset: %d samples; most common action=%s."),
        Dataset.Num(),
        *GetPlayerActionName(GetMostCommonAction())
    );

    const TArray<FTrainingSample>& Samples = Dataset.GetSamples();
    for (int32 Index = 0; Index < Samples.Num(); ++Index)
    {
        const FTrainingSample& Sample = Samples[Index];
        UE_LOG(
            LogAdaptHunt,
            Log,
            TEXT("  [%d] R%d t=%.2f previous player=%s enemy=%s -> %s"),
            Index,
            Sample.Snapshot.RoundNumber,
            Sample.ActionTimeSeconds,
            *GetPlayerActionName(Sample.Snapshot.PreviousPlayerAction),
            *GetEnemyActionName(Sample.Snapshot.PreviousEnemyAction),
            *GetPlayerActionName(Sample.NextPlayerAction)
        );
    }
}

const FCombatDataset& UPlayerBehaviorTrackerComponent::GetDataset() const
{
    return Dataset;
}

void UPlayerBehaviorTrackerComponent::CacheSnapshotComponent()
{
    if (!SnapshotComponent && GetOwner())
    {
        SnapshotComponent =
            GetOwner()->FindComponentByClass<UCombatSnapshotComponent>();
    }
}

void UPlayerBehaviorTrackerComponent::HandlePlayerActionObserved(
    const FCombatSnapshot& Snapshot,
    const EPlayerCombatAction Action
)
{
    RecordPlayerAction(Snapshot, Action);
}
