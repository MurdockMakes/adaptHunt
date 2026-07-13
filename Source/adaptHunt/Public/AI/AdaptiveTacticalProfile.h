#pragma once

#include "CoreMinimal.h"
#include "Data/CombatDataset.h"
#include "Data/PredictionResult.h"

#include "AdaptiveTacticalProfile.generated.h"


/** Why the latest round did or did not produce a playable profile. */
UENUM(BlueprintType)
enum class EAdaptiveProfileEvidenceStatus : uint8
{
    NoData,
    NoPrediction,
    InsufficientRoundSamples,
    InsufficientContextSamples,
    LowConfidence,
    PredictorDisagreement,
    Active
};


/** Optional deterministic orbit bias learned from a lateral dodge habit. */
UENUM(BlueprintType)
enum class EAdaptiveOrbitPreference : uint8
{
    None,
    Left,
    Right
};


/** Safe defaults and hard bounds for between-round tactical adaptation. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveTacticalProfileTuning
{
    GENERATED_BODY()

    FAdaptiveTacticalProfileTuning();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Evidence", meta = (ClampMin = "1"))
    int32 MinimumRoundSamples;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Evidence", meta = (ClampMin = "1"))
    int32 MinimumContextSamples;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Evidence", meta = (ClampMin = "1"))
    int32 MinimumSupportingSamples;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Evidence", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumConfidence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Bounds", meta = (ClampMin = "0.0", ClampMax = "160.0"))
    float MaximumSpacingAdjustment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Bounds", meta = (ClampMin = "0.0", ClampMax = "0.25"))
    float MaximumAggressionAdjustment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Bounds", meta = (ClampMin = "0.0", ClampMax = "0.25"))
    float MaximumDefensiveAdjustment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Bounds", meta = (ClampMin = "0.0", ClampMax = "0.30"))
    float MaximumHealInterruptionPriority;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Bounds", meta = (ClampMin = "0.0", ClampMax = "0.25"))
    float MaximumUtilityAdjustment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Counter", meta = (ClampMin = "0.0", ClampMax = "0.25"))
    float PreferredCounterUtilityBoost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Counter", meta = (ClampMin = "0.0"))
    float CounterCooldown;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Adaptation|Counter", meta = (ClampMin = "0", ClampMax = "8"))
    int32 CounterBudgetPerRound;

    FAdaptiveTacticalProfileTuning GetSanitized() const;
};


/**
 * Explainable value derived at a round boundary. It contains no timers,
 * prediction calls, executor state, or world references.
 */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveTacticalProfile
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Evidence")
    int32 SourceRound = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Evidence")
    EAdaptiveProfileEvidenceStatus EvidenceStatus =
        EAdaptiveProfileEvidenceStatus::NoData;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Evidence")
    EPlayerCombatAction MostLikelyPlayerAction =
        EPlayerCombatAction::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Evidence", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Confidence = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Evidence", meta = (ClampMin = "0"))
    int32 SupportingSampleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Evidence", meta = (ClampMin = "0"))
    int32 ContextSampleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Evidence", meta = (ClampMin = "0"))
    int32 RoundSampleCount = 0;

    /** Predictor provenance for the strongest supported context. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Evidence")
    FPredictionResult EvidencePrediction;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Tactics")
    EEnemyCombatAction PreferredCounterAction =
        EEnemyCombatAction::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Tactics")
    float PreferredSpacingAdjustment = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Tactics")
    float AggressionAdjustment = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Tactics")
    float DefensiveAdjustment = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Tactics")
    float HealInterruptionPriority = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Tactics")
    EAdaptiveOrbitPreference OrbitPreference =
        EAdaptiveOrbitPreference::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Adaptation|Evidence")
    bool bEvidenceStrongEnough = false;

    bool IsActive() const;
};


/** Runtime limiter for prediction-informed counter decisions within a round. */
struct ADAPTHUNT_API FAdaptiveCounterBudgetState
{
    int32 RemainingUses = 0;
    double NextAllowedTime = 0.0;

    void Reset(int32 MaximumUses);
    bool CanConsume(double CurrentTime) const;
    bool Consume(double CurrentTime, float Cooldown);
    float GetRemainingCooldown(double CurrentTime) const;
};


/** Deterministic, side-effect-free derivation and profile application math. */
struct ADAPTHUNT_API FAdaptiveTacticalProfilePolicy
{
    static FAdaptiveTacticalProfile Derive(
        const FCombatDataset& Dataset,
        int32 CompletedRound,
        const FPredictionResult& Prediction,
        const FAdaptiveTacticalProfileTuning& Tuning
    );

    static bool IsStrongerCandidate(
        const FAdaptiveTacticalProfile& Candidate,
        const FAdaptiveTacticalProfile& Current
    );

    static float GetUtilityModifier(
        const FAdaptiveTacticalProfile& Profile,
        EEnemyCombatAction Action,
        bool bPreferredCounterOpportunity,
        const FAdaptiveTacticalProfileTuning& Tuning
    );

    static float AdjustSpacingDistance(
        float BaseDistance,
        const FAdaptiveTacticalProfile& Profile,
        float AdjustmentScale,
        const FAdaptiveTacticalProfileTuning& Tuning
    );

    static bool ResolveOrbitRight(
        const FAdaptiveTacticalProfile& Profile,
        bool bSeededOrbitRight
    );

    static bool IsPredictionMatchedCounterOpportunity(
        const FAdaptiveTacticalProfile& Profile,
        const FPredictionResult& Prediction,
        bool bPredictionApplied,
        const FAdaptiveCounterBudgetState& CounterBudget,
        double CurrentTime
    );
};
