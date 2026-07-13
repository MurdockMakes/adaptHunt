#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/LearningTelemetry.h"
#include "Data/PersistentPlayerPatterns.h"
#include "Game/AdaptationReveal.h"
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
    PreRoundCountdown,
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
    bool StartCurrentRound();
    bool CompleteCurrentRound(EAdaptiveRoundWinner Winner);
    bool AdvanceToNextRound();

    int32 GetCurrentRound() const;
    int32 GetLastCompletedRound() const;
    EAdaptiveRoundPhase GetPhase() const;
    EAdaptiveRoundWinner GetLastWinner() const;
    EAdaptiveRoundWinner GetMatchWinner() const;
    int32 GetPlayerRoundWins() const;
    int32 GetEnemyRoundWins() const;

private:
    int32 CurrentRound;
    int32 LastCompletedRound;
    EAdaptiveRoundPhase Phase;
    EAdaptiveRoundWinner LastWinner;
    int32 PlayerRoundWins;
    int32 EnemyRoundWins;
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

    /** Binds combatants, loads prior patterns, and starts Round 1 countdown. */
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

    /** Skips only the current locked countdown; intended for console testing. */
    bool DebugStartCurrentRoundNow();

    /** Clears match state and starts Round 1 with bounded history retained. */
    UFUNCTION(BlueprintCallable, Category = "Adaptive Hunt|Rounds")
    bool RestartMatch();

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
    EAdaptiveRoundWinner GetMatchWinner() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    int32 GetPlayerRoundWins() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    int32 GetEnemyRoundWins() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    bool ArePredictionsEnabledForCurrentRound() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    int32 GetCollectedSampleCount() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    int32 GetTrainedSampleCount() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Learning")
    int32 GetPersistentPlayerPatternCount() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Learning")
    bool IsPersistentPlayerLearningEnabled() const;

    /** Deletes prior-run patterns; current-match samples remain live. */
    UFUNCTION(BlueprintCallable, Category = "Adaptive Hunt|Learning")
    bool ClearPersistentPlayerPatterns();

    /** Immutable, round-filtered analysis captured when the last round ended. */
    const FAdaptiveConditionalPattern& GetLastRoundObservedPattern() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    float GetIntermissionDuration() const;

    /** Live countdown derived from the existing round timer; never authoritative. */
    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    float GetIntermissionRemainingTime() const;

    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    float GetPreRoundCountdownDuration() const;

    /** Live presentation countdown; combat remains locked until it expires. */
    UFUNCTION(BlueprintPure, Category = "Adaptive Hunt|Rounds")
    float GetPreRoundCountdownRemainingTime() const;

    const FAdaptiveRevealText& GetAdaptationReveal() const;

    /** Every playable round permits evidence-gated prediction use. */
    static bool IsPredictionRound(int32 RoundNumber);

    /** A boundary rebuild is useful only when another round remains. */
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
    bool StartNewMatch();
    void ResetMatchScopedState();
    void LoadPersistentPlayerPatterns();
    bool SaveNewPersistentPlayerPatterns();
    void PrepareCurrentRound();
    void BeginCurrentRound();
    bool EndCurrentRound(EAdaptiveRoundWinner Winner);
    bool TrainForNextRound();
    void ResetCombatantsForRound();
    void RemoveLingeringProjectiles();
    void SetIntermissionInputLocked(bool bLocked);
    void StopCombatantMovement() const;
    void ClearFlowTimers();
    void ScheduleNextRound();
    void SchedulePreRoundCountdown();
    void HandleNextRoundTimer();
    void HandlePreRoundCountdownTimer();
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

    UPROPERTY(EditDefaultsOnly, Category = "Rounds", meta = (ClampMin = "0.0"))
    float PreRoundCountdownDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Learning|Persistence")
    bool bPersistPlayerPatterns;

    UPROPERTY(EditDefaultsOnly, Category = "Learning|Persistence", meta = (ClampMin = "16", ClampMax = "512"))
    int32 MaximumPersistentSamples;

    UPROPERTY(VisibleInstanceOnly, Category = "Rounds")
    bool bPredictionsEnabledForCurrentRound;

    FAdaptiveConditionalPattern LastRoundObservedPattern;
    FAdaptiveRevealText AdaptationReveal;
    TArray<FPersistentPlayerPattern> PersistentPlayerPatterns;

    FAdaptiveRoundProgression Progression;
    FTransform PlayerStartTransform;
    FTransform EnemyStartTransform;
    FTimerHandle NextRoundTimer;
    FTimerHandle PreRoundCountdownTimer;
    bool bInputLockedByRoundManager;
    int32 SavedCurrentSessionSampleCount;
    bool bInitialized;
};
