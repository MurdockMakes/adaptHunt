#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/LearningTelemetry.h"
#include "TimerManager.h"

#include "RoundManager.generated.h"

class AAdaptiveEnemyCharacter;
class AAdaptivePlayerCharacter;
class UCombatComponent;
class UCombatSnapshotComponent;
class UEnemyCombatComponent;
class UEnemyDecisionComponent;
class UHealthComponent;
class UPlayerBehaviorTrackerComponent;
class UStaminaComponent;

UENUM(BlueprintType)
enum class EAdaptiveRoundPhase : uint8
{
    WaitingToStart,
    InProgress,
    Intermission,
    MatchComplete
};

UENUM(BlueprintType)
enum class EAdaptiveRoundWinner : uint8
{
    None,
    Player,
    Enemy
};

/**
 * Engine-independent three-round state machine used by URoundManager.
 *
 * Keeping transition rules separate from actors makes the progression fully
 * deterministic and testable without constructing an Unreal world.
 */
struct ADAPTHUNT_API FAdaptiveRoundProgression
{
public:
    static constexpr int32 TotalRounds = 3;

    FAdaptiveRoundProgression();

    void Reset();
    bool BeginMatch();
    bool CompleteCurrentRound(EAdaptiveRoundWinner Winner);
    bool AdvanceToNextRound();

    int32 GetCurrentRound() const;
    int32 GetLastCompletedRound() const;
    EAdaptiveRoundPhase GetPhase() const;
    EAdaptiveRoundWinner GetLastWinner() const;

private:
    int32 CurrentRound;
    int32 LastCompletedRound;
    EAdaptiveRoundPhase Phase;
    EAdaptiveRoundWinner LastWinner;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FAdaptiveRoundStartedEvent,
    int32,
    RoundNumber,
    bool,
    bPredictionsEnabled,
    int32,
    TrainedSampleCount
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FAdaptiveRoundEndedEvent,
    int32,
    RoundNumber,
    EAdaptiveRoundWinner,
    Winner,
    int32,
    CollectedSampleCount,
    int32,
    TrainedSampleCount
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FAdaptiveMatchCompletedEvent,
    EAdaptiveRoundWinner,
    FinalRoundWinner
);

/**
 * Owns three-round combat flow and the between-round learning lifecycle.
 *
 * The GameMode owns this component because round authority belongs to the
 * server-side rules layer. Combatants remain alive across rounds, allowing the
 * player behavior dataset to accumulate while health, stamina, cooldowns, and
 * transforms are reset for each rematch.
 */
UCLASS(ClassGroup = (Game), meta = (BlueprintSpawnableComponent))
class ADAPTHUNT_API URoundManager : public UActorComponent
{
    GENERATED_BODY()

public:
    URoundManager();

    /** Binds the combatants and immediately begins Round 1. */
    bool Initialize(
        AAdaptivePlayerCharacter* PlayerCharacter,
        AAdaptiveEnemyCharacter* EnemyCharacter
    );

    /** Removes bindings and returns the manager to WaitingToStart. */
    void Shutdown();

    UFUNCTION(BlueprintCallable, Category = "Adaptive Hunt|Rounds")
    bool AdvanceToNextRoundNow();

    /** Deterministic debug/test hook; normal play ends through health death. */
    UFUNCTION(BlueprintCallable, Category = "Adaptive Hunt|Rounds")
    bool ForceEndCurrentRound(EAdaptiveRoundWinner Winner);

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    bool IsInitialized() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    int32 GetCurrentRound() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    int32 GetTotalRounds() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    int32 GetLastCompletedRound() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    EAdaptiveRoundPhase GetRoundPhase() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    EAdaptiveRoundWinner GetLastWinner() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    bool ArePredictionsEnabledForCurrentRound() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    int32 GetCollectedSampleCount() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    int32 GetTrainedSampleCount() const;

    /** Immutable, round-filtered analysis captured when the last round ended. */
    const FAdaptiveConditionalPattern& GetLastRoundObservedPattern() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    float GetIntermissionDuration() const;

    /** Round 1 is baseline; Rounds 2 and 3 are prediction-eligible. */
    static bool IsPredictionRound(int32 RoundNumber);

    /** Models are updated only when another round remains. */
    static bool ShouldTrainAfterRound(int32 CompletedRoundNumber);

    UPROPERTY(BlueprintAssignable, Category = "Adaptive Hunt|Rounds")
    FAdaptiveRoundStartedEvent OnRoundStarted;

    UPROPERTY(BlueprintAssignable, Category = "Adaptive Hunt|Rounds")
    FAdaptiveRoundEndedEvent OnRoundEnded;

    UPROPERTY(BlueprintAssignable, Category = "Adaptive Hunt|Rounds")
    FAdaptiveMatchCompletedEvent OnMatchCompleted;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool CacheCombatantComponents();
    void BindDeathEvents();
    void UnbindDeathEvents();
    void BeginCurrentRound();
    bool EndCurrentRound(EAdaptiveRoundWinner Winner);
    bool TrainForNextRound();
    void ResetCombatantsForRound();
    void RemoveLingeringProjectiles();
    void SetIntermissionInputLocked(bool bLocked);
    void StopCombatantMovement() const;
    void ScheduleNextRound();
    void HandleNextRoundTimer();
    void HandlePlayerDeath(UHealthComponent*, AActor*);
    void HandleEnemyDeath(UHealthComponent*, AActor*);

    UPROPERTY(Transient)
    TObjectPtr<AAdaptivePlayerCharacter> PlayerCharacter;

    UPROPERTY(Transient)
    TObjectPtr<AAdaptiveEnemyCharacter> EnemyCharacter;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> PlayerHealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UStaminaComponent> PlayerStaminaComponent;

    UPROPERTY(Transient)
    TObjectPtr<UCombatComponent> PlayerCombatComponent;

    UPROPERTY(Transient)
    TObjectPtr<UCombatSnapshotComponent> SnapshotComponent;

    UPROPERTY(Transient)
    TObjectPtr<UPlayerBehaviorTrackerComponent> BehaviorTrackerComponent;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> EnemyHealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UStaminaComponent> EnemyStaminaComponent;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyDecisionComponent> EnemyDecisionComponent;

    UPROPERTY(EditDefaultsOnly, Category = "Rounds", meta = (ClampMin = "0.0"))
    float IntermissionDuration;

    UPROPERTY(VisibleInstanceOnly, Category = "Rounds")
    bool bPredictionsEnabledForCurrentRound;

    FAdaptiveConditionalPattern LastRoundObservedPattern;

    FAdaptiveRoundProgression Progression;
    FTransform PlayerStartTransform;
    FTransform EnemyStartTransform;
    FTimerHandle NextRoundTimer;
    bool bInitialized;
};
