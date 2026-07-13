#pragma once

#include "CoreMinimal.h"
#include "Data/CombatSnapshot.h"
#include "Data/PredictionResult.h"

#include "AdaptiveCounterOutcome.generated.h"


/** One terminal result for a committed enemy utility action. */
UENUM(BlueprintType)
enum class EAdaptiveCounterOutcomeResult : uint8
{
    None UMETA(DisplayName = "None"),
    Hit UMETA(DisplayName = "Hit"),
    Missed UMETA(DisplayName = "Missed"),
    Blocked UMETA(DisplayName = "Blocked"),
    Dodged UMETA(DisplayName = "Dodged"),
    InterruptedHeal UMETA(DisplayName = "Interrupted Heal"),
    InvalidatedBeforeActiveFrame
        UMETA(DisplayName = "Invalidated Before Active Frame")
};


/** Raw executor event. Prediction and round context are attached elsewhere. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FEnemyCombatActionOutcome
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Outcome")
    int32 ActionId = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Outcome")
    EEnemyCombatAction Action = EEnemyCombatAction::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Outcome")
    EAdaptiveCounterOutcomeResult Result =
        EAdaptiveCounterOutcomeResult::None;

    bool IsValid() const;
};


/**
 * Fully associated, immutable action result stored for one local match.
 * It deliberately contains predictor output rather than depending on a
 * predictor implementation, and it never owns or applies damage.
 */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveCounterOutcomeRecord
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    int32 ActionId = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    int32 RoundNumber = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    EEnemyCombatAction Action = EEnemyCombatAction::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    EEnemyCombatAction ProfilePreferredCounterAction =
        EEnemyCombatAction::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    EAdaptiveCounterOutcomeResult Result =
        EAdaptiveCounterOutcomeResult::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    FPredictionResult Prediction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    FCombatSnapshot ContextSnapshot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    bool bPredictionApplied = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    bool bAdaptiveCounterOpportunity = false;

    bool IsValid() const;
    bool IsAdaptiveCounterAttempt() const;
    bool WasSuccessfulCounter() const;
};


/** Safe evidence and utility bounds for outcome-aware adaptation. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveCounterEffectivenessTuning
{
    GENERATED_BODY()

    FAdaptiveCounterEffectivenessTuning();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Outcome", meta = (ClampMin = "2", ClampMax = "12"))
    int32 MinimumSamples;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Outcome", meta = (ClampMin = "0.0", ClampMax = "0.10"))
    float MaximumUtilityAdjustment;

    /** Symmetric pseudo-count per result, preventing tiny samples dominating. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Outcome", meta = (ClampMin = "0.0", ClampMax = "4.0"))
    float SmoothingSampleWeight;

    FAdaptiveCounterEffectivenessTuning GetSanitized() const;
};


/** Deterministic telemetry for one predicted-action/counter-action pair. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveCounterEffectivenessSummary
{
    GENERATED_BODY()

    /** Zero means cumulative through EvaluatedThroughRound. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    int32 RoundNumber = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    int32 EvaluatedThroughRound = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    EPlayerCombatAction PredictedPlayerAction =
        EPlayerCombatAction::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    EEnemyCombatAction CounterAction = EEnemyCombatAction::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    int32 SampleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    int32 SuccessfulCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    float SmoothedEffectiveness = 0.5f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    float UtilityModifier = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Outcome")
    bool bMeetsMinimumSamples = false;

    bool HasSamples() const;
};


/** Match-scoped storage with an explicit duplicate-event guard. */
struct ADAPTHUNT_API FAdaptiveCounterOutcomeHistory
{
public:
    bool Record(const FAdaptiveCounterOutcomeRecord& Record);
    void Reset();
    int32 Num() const;
    const TArray<FAdaptiveCounterOutcomeRecord>& GetRecords() const;
    const FAdaptiveCounterOutcomeRecord* GetLastRecord() const;

private:
    TArray<FAdaptiveCounterOutcomeRecord> Records;
    TSet<int32> RecordedActionIds;
};


/** Pure aggregation and independently bounded secondary utility math. */
struct ADAPTHUNT_API FAdaptiveCounterEffectivenessPolicy
{
    static TArray<FAdaptiveCounterEffectivenessSummary> AnalyzeRound(
        const TArray<FAdaptiveCounterOutcomeRecord>& Records,
        int32 RoundNumber,
        const FAdaptiveCounterEffectivenessTuning& Tuning
    );

    static FAdaptiveCounterEffectivenessSummary AnalyzeForCounter(
        const TArray<FAdaptiveCounterOutcomeRecord>& Records,
        EPlayerCombatAction PredictedPlayerAction,
        EEnemyCombatAction CounterAction,
        int32 EvaluatedThroughRound,
        const FAdaptiveCounterEffectivenessTuning& Tuning
    );

    static float GetUtilityModifier(
        const FAdaptiveCounterEffectivenessSummary& Summary,
        EEnemyCombatAction Action,
        bool bAdaptiveCounterOpportunity,
        const FAdaptiveCounterEffectivenessTuning& Tuning
    );
};


ADAPTHUNT_API FString GetAdaptiveCounterOutcomeName(
    EAdaptiveCounterOutcomeResult Result
);
