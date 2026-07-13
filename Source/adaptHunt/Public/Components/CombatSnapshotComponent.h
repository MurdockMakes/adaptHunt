#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatSnapshot.h"

#include "CombatSnapshotComponent.generated.h"

class UCombatComponent;
class UEnemyCombatComponent;
class UHealthComponent;
class UStaminaComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(
    FPlayerActionObservedEvent,
    const FCombatSnapshot&,
    EPlayerCombatAction
);

/**
 * Captures the current high-level combat state for behavior learning.
 *
 * The component belongs to the player and observes committed combat actions
 * through delegates. It does not Tick; consumers request a snapshot when they
 * need one. Behavior tracking can therefore label snapshots without depending on
 * character input or enemy decision implementation details.
 */
UCLASS(ClassGroup = (MachineLearning), meta = (BlueprintSpawnableComponent))
class ADAPTHUNT_API UCombatSnapshotComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatSnapshotComponent();

    /** Selects the opponent whose state is included in future snapshots. */
    UFUNCTION(BlueprintCallable, Category = "Machine Learning|Snapshot")
    void SetOpponentActor(AActor* NewOpponentActor);

    UFUNCTION(BlueprintPure, Category = "Machine Learning|Snapshot")
    AActor* GetOpponentActor() const;

    UFUNCTION(BlueprintCallable, Category = "Machine Learning|Snapshot")
    void SetRoundNumber(int32 NewRoundNumber);

    UFUNCTION(BlueprintPure, Category = "Machine Learning|Snapshot")
    int32 GetRoundNumber() const;

    /** Clears action/timing carryover without touching collected samples. */
    UFUNCTION(BlueprintCallable, Category = "Machine Learning|Snapshot")
    void ResetRoundState();

    /** Builds a value snapshot from the current actors and action history. */
    UFUNCTION(BlueprintCallable, Category = "Machine Learning|Snapshot")
    FCombatSnapshot CaptureSnapshot();

    /** Writes one capture to LogAdaptHunt for greybox verification. */
    UFUNCTION(BlueprintCallable, Category = "Machine Learning|Snapshot")
    void LogSnapshot();

    float GetCloseDistanceThreshold() const;
    float GetFarDistanceThreshold() const;
    float GetEnemyAttackWindow() const;

    static ECombatDistanceCategory ClassifyDistance(
        float Distance,
        float CloseThreshold,
        float FarThreshold
    );

    static ERelativePlayerPosition ClassifyRelativePosition(
        const FVector& PlayerLocation,
        const FVector& EnemyLocation,
        const FVector& EnemyForward
    );

    /**
     * Publishes the state captured immediately before a committed action.
     * Behavior tracking subscribes here instead of depending on input code.
     */
    FPlayerActionObservedEvent OnPlayerActionObserved;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void CachePlayerComponents();
    void BindOpponentCombat();
    void HandlePlayerActionCommitStarted(
        UCombatComponent* Source,
        EPlayerCombatAction Action
    );
    void HandlePlayerAction(
        UCombatComponent* Source,
        EPlayerCombatAction Action
    );
    void HandleEnemyAction(
        UEnemyCombatComponent* Source,
        EEnemyCombatAction Action
    );
    double GetSnapshotTimeSeconds() const;

    UPROPERTY(Transient)
    TObjectPtr<AActor> OpponentActor;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> PlayerHealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UStaminaComponent> PlayerStaminaComponent;

    UPROPERTY(Transient)
    TObjectPtr<UCombatComponent> PlayerCombatComponent;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> EnemyHealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UStaminaComponent> EnemyStaminaComponent;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

    UPROPERTY(EditDefaultsOnly, Category = "Snapshot|Distance", meta = (ClampMin = "0.0"))
    float CloseDistanceThreshold;

    UPROPERTY(EditDefaultsOnly, Category = "Snapshot|Distance", meta = (ClampMin = "0.0"))
    float FarDistanceThreshold;

    /** Duration after an enemy attack commit that counts as attacking. */
    UPROPERTY(EditDefaultsOnly, Category = "Snapshot|Timing", meta = (ClampMin = "0.0"))
    float EnemyAttackWindow;

    UPROPERTY(VisibleInstanceOnly, Category = "Snapshot|Round")
    int32 RoundNumber;

    EPlayerCombatAction PreviousPlayerAction;
    EEnemyCombatAction PreviousEnemyAction;
    double TrackingStartTime;
    double LastPlayerAttackTime;
    double LastEnemyAttackTime;
    bool bHasPlayerAttack;
    bool bHasEnemyAttack;
    FCombatSnapshot PendingPlayerActionSnapshot;
    bool bHasPendingPlayerActionSnapshot;
};
