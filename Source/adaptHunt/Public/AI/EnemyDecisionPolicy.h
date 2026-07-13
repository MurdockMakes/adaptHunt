#pragma once

#include "CoreMinimal.h"
#include "AI/AdaptiveCounterOutcome.h"
#include "Data/EnemyActionScore.h"

#include "EnemyDecisionPolicy.generated.h"


/** Controls deterministic weighted choice among credible near-best actions. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FEnemyActionSelectionTuning
{
    GENERATED_BODY()

    FEnemyActionSelectionTuning();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Selection", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float NearBestScoreWindow;

    /** Lower values more strongly favor the best action. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Selection", meta = (ClampMin = "0.01", ClampMax = "0.5"))
    float SelectionTemperature;

    FEnemyActionSelectionTuning GetSanitized() const;
};


/** Bounded recent-commit fatigue. It never changes action availability. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FEnemyActionRepetitionTuning
{
    GENERATED_BODY()

    FEnemyActionRepetitionTuning();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Repetition", meta = (ClampMin = "1", ClampMax = "8"))
    int32 HistorySize;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Repetition", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float ImmediateRepeatPenalty;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Repetition", meta = (ClampMin = "0.0", ClampMax = "0.25"))
    float AccumulatedRepeatPenalty;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Repetition", meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float OffensiveFamilyPenalty;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Repetition", meta = (ClampMin = "0.0", ClampMax = "0.6"))
    float MaximumRepetitionAdjustment;

    FEnemyActionRepetitionTuning GetSanitized() const;
};


/** Keeps recent committed choices near a configurable offense/defense mix. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FEnemyOffenseDefenseBalanceTuning
{
    GENERATED_BODY()

    FEnemyOffenseDefenseBalanceTuning();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Balance", meta = (ClampMin = "1", ClampMax = "8"))
    int32 HistorySize;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Balance", meta = (ClampMin = "1", ClampMax = "8"))
    int32 MinimumHistoryEntries;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Balance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TargetDefensiveRatio;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Balance", meta = (ClampMin = "0.0", ClampMax = "0.25"))
    float RatioDeadZone;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Balance", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float MaximumBalanceAdjustment;

    FEnemyOffenseDefenseBalanceTuning GetSanitized() const;
};


/** Short, independently bounded memory of ordinary resolved action results. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FEnemyRecentOutcomeTuning
{
    GENERATED_BODY()

    FEnemyRecentOutcomeTuning();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Recent Outcome", meta = (ClampMin = "1", ClampMax = "12"))
    int32 MemorySize;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Recent Outcome", meta = (ClampMin = "0.0", ClampMax = "0.2"))
    float FailurePenalty;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Recent Outcome", meta = (ClampMin = "0.0", ClampMax = "0.4"))
    float MaximumOutcomeAdjustment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Recent Outcome", meta = (ClampMin = "0.0", ClampMax = "0.05"))
    float SuccessBonus;

    FEnemyRecentOutcomeTuning GetSanitized() const;
};


/** Human-scale delay and spam guard for committed observable threats. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FEnemyUrgentDecisionTuning
{
    GENERATED_BODY()

    FEnemyUrgentDecisionTuning();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Urgent", meta = (ClampMin = "0.15", ClampMax = "0.25"))
    float ThreatReactionDelay;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Urgent", meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float UrgentDecisionCooldown;

    FEnemyUrgentDecisionTuning GetSanitized() const;
};


struct ADAPTHUNT_API FEnemyActionSelectionResult
{
    EEnemyCombatAction Action = EEnemyCombatAction::None;
    float BestScore = 0.0f;
    float SelectedScore = 0.0f;
    TArray<EEnemyCombatAction> CandidateActions;

    bool IsValid() const;
};


/** Pure score-window construction and deterministic weighted selection. */
struct ADAPTHUNT_API FEnemyActionSelectionPolicy
{
    static float GetSelectionScore(const FEnemyActionScore& Score);

    static FEnemyActionSelectionResult Select(
        const TArray<FEnemyActionScore>& Scores,
        float RandomRoll,
        const FEnemyActionSelectionTuning& Tuning
    );

    static FEnemyActionSelectionResult Select(
        const TArray<FEnemyActionScore>& Scores,
        const TSet<EEnemyCombatAction>& ExcludedActions,
        float RandomRoll,
        const FEnemyActionSelectionTuning& Tuning
    );

    /** Deterministic diagnostic API; ties retain stable input ordering. */
    static EEnemyCombatAction SelectBest(
        const TArray<FEnemyActionScore>& Scores
    );
};


struct ADAPTHUNT_API FEnemyCommittedActionHistory
{
    void Record(EEnemyCombatAction Action, int32 MaximumEntries);
    void Reset();
    int32 Num() const;
    const TArray<EEnemyCombatAction>& GetActions() const;

private:
    TArray<EEnemyCombatAction> Actions;
};


struct ADAPTHUNT_API FEnemyRepetitionModifier
{
    float Immediate = 0.0f;
    float Accumulated = 0.0f;
    float OffensiveFamily = 0.0f;
    float Total = 0.0f;
};


/** Pure fatigue math over successfully committed utility actions. */
struct ADAPTHUNT_API FEnemyActionRepetitionPolicy
{
    static FEnemyRepetitionModifier Evaluate(
        EEnemyCombatAction Action,
        const FEnemyCommittedActionHistory& History,
        const FEnemyActionRepetitionTuning& Tuning
    );
};


struct ADAPTHUNT_API FEnemyOffenseDefenseBalanceModifier
{
    int32 EvidenceCount = 0;
    float DefensiveRatio = 0.0f;
    float Total = 0.0f;
};


/** Pure cadence correction; it affects utility but never availability. */
struct ADAPTHUNT_API FEnemyOffenseDefenseBalancePolicy
{
    static FEnemyOffenseDefenseBalanceModifier Evaluate(
        EEnemyCombatAction Action,
        const FEnemyCommittedActionHistory& History,
        const FEnemyOffenseDefenseBalanceTuning& Tuning
    );
};


struct ADAPTHUNT_API FEnemyRecentOutcomeRecord
{
    int32 ActionId = 0;
    EEnemyCombatAction Action = EEnemyCombatAction::None;
    EAdaptiveCounterOutcomeResult Result =
        EAdaptiveCounterOutcomeResult::None;
};


struct ADAPTHUNT_API FEnemyRecentOutcomeModifier
{
    int32 FailureCount = 0;
    int32 SuccessCount = 0;
    float Total = 0.0f;
};


/** Bounded, duplicate-safe storage separate from predictor/counter history. */
struct ADAPTHUNT_API FEnemyRecentOutcomeMemory
{
    bool Record(
        const FEnemyCombatActionOutcome& Outcome,
        int32 MaximumEntries
    );
    void Reset();
    int32 Num() const;
    const TArray<FEnemyRecentOutcomeRecord>& GetRecords() const;

private:
    TArray<FEnemyRecentOutcomeRecord> Records;
    int32 HighestRecordedActionId = 0;
};


/** Pure short-term result evaluation with a weak single-failure response. */
struct ADAPTHUNT_API FEnemyRecentOutcomePolicy
{
    static FEnemyRecentOutcomeModifier Evaluate(
        EEnemyCombatAction Action,
        const FEnemyRecentOutcomeMemory& Memory,
        const FEnemyRecentOutcomeTuning& Tuning
    );
};


UENUM(BlueprintType)
enum class EEnemyUrgentThreat : uint8
{
    None,
    LightAttack,
    HeavyAttack,
    Heal
};


/** Runtime timestamps updated only from committed target action phases. */
struct ADAPTHUNT_API FEnemyUrgentDecisionState
{
    bool ObservePhaseEdge(
        EPlayerCombatAction Action,
        ECombatActionPhase Phase,
        bool bEnemyCanDecide,
        double CurrentTime,
        const FEnemyUrgentDecisionTuning& Tuning
    );

    bool ConsumeIfReady(
        EPlayerCombatAction Action,
        ECombatActionPhase Phase,
        bool bEnemyCanDecide,
        double CurrentTime,
        const FEnemyUrgentDecisionTuning& Tuning
    );

    void Reset();
    bool IsPending() const;
    double GetReadyTime() const;

    static EEnemyUrgentThreat ClassifyWindup(
        EPlayerCombatAction Action,
        ECombatActionPhase Phase
    );

private:
    static bool IsThreatStillCommitted(
        EEnemyUrgentThreat Threat,
        EPlayerCombatAction Action,
        ECombatActionPhase Phase
    );

    EEnemyUrgentThreat LastObservedWindup = EEnemyUrgentThreat::None;
    EEnemyUrgentThreat PendingThreat = EEnemyUrgentThreat::None;
    double ReadyTime = 0.0;
    double NextAllowedTime = 0.0;
};
