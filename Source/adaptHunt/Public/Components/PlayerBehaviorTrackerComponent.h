#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatDataset.h"

#include "PlayerBehaviorTrackerComponent.generated.h"

class UCombatSnapshotComponent;
class UPlayerBehaviorTrackerComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(
    FTrainingSampleRecordedEvent,
    UPlayerBehaviorTrackerComponent*,
    const FTrainingSample&
);

/**
 * Converts observed player decisions into supervised-learning samples.
 *
 * This component owns the dataset but does not interpret or predict from it.
 * Milestone 8 can therefore replace predictor implementations without
 * changing combat input, snapshot capture, or sample collection.
 */
UCLASS(ClassGroup = (MachineLearning), meta = (BlueprintSpawnableComponent))
class ADAPTHUNT_API UPlayerBehaviorTrackerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlayerBehaviorTrackerComponent();

    UFUNCTION(BlueprintCallable, Category = "Machine Learning|Tracking")
    void StartTracking();

    UFUNCTION(BlueprintCallable, Category = "Machine Learning|Tracking")
    void StopTracking();

    UFUNCTION(BlueprintPure, Category = "Machine Learning|Tracking")
    bool IsTrackingEnabled() const;

    UFUNCTION(BlueprintCallable, Category = "Machine Learning|Tracking")
    void ResetDataset();

    /** Records one labeled decision. Exposed directly for deterministic tests. */
    UFUNCTION(BlueprintCallable, Category = "Machine Learning|Tracking")
    bool RecordPlayerAction(
        const FCombatSnapshot& Snapshot,
        EPlayerCombatAction Action
    );

    UFUNCTION(BlueprintPure, Category = "Machine Learning|Tracking")
    int32 GetSampleCount() const;

    UFUNCTION(BlueprintPure, Category = "Machine Learning|Tracking")
    int32 GetActionCount(EPlayerCombatAction Action) const;

    UFUNCTION(BlueprintPure, Category = "Machine Learning|Tracking")
    EPlayerCombatAction GetMostCommonAction() const;

    /** Writes a summary and every collected relationship to LogAdaptHunt. */
    UFUNCTION(BlueprintCallable, Category = "Machine Learning|Tracking")
    void LogDataset() const;

    const FCombatDataset& GetDataset() const;

    FTrainingSampleRecordedEvent OnTrainingSampleRecorded;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void CacheSnapshotComponent();
    void HandlePlayerActionObserved(
        const FCombatSnapshot& Snapshot,
        EPlayerCombatAction Action
    );

    UPROPERTY(Transient)
    TObjectPtr<UCombatSnapshotComponent> SnapshotComponent;

    UPROPERTY(
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "Machine Learning|Tracking",
        meta = (AllowPrivateAccess = "true")
    )
    FCombatDataset Dataset;

    UPROPERTY(VisibleInstanceOnly, Category = "Machine Learning|Tracking")
    bool bTrackingEnabled;

    UPROPERTY(EditDefaultsOnly, Category = "Machine Learning|Debug")
    bool bLogEverySample;
};
