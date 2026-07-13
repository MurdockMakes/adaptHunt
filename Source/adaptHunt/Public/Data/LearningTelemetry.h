#pragma once

#include "CoreMinimal.h"
#include "AI/AdaptiveCounterOutcome.h"
#include "AI/AdaptiveTacticalProfile.h"
#include "AI/EnemyDecisionPolicy.h"
#include "Data/CombatDataset.h"
#include "Data/PredictionResult.h"

/** One deterministic player-action percentage within a condition. */
struct ADAPTHUNT_API FAdaptiveActionPercentage
{
    EPlayerCombatAction Action = EPlayerCombatAction::None;
    int32 Count = 0;
    int32 Percentage = 0;
};

/**
 * Interpretable relationship between the enemy's previous action and the
 * player's next action. Percentages always sum to 100 for a valid pattern.
 */
struct ADAPTHUNT_API FAdaptiveConditionalPattern
{
    EEnemyCombatAction PreviousEnemyAction = EEnemyCombatAction::None;
    EPlayerCombatAction DominantPlayerAction = EPlayerCombatAction::None;
    int32 TotalSampleCount = 0;
    int32 DominantActionCount = 0;
    float Confidence = 0.0f;
    TArray<FAdaptiveActionPercentage> ActionPercentages;

    bool IsValid() const;
    int32 GetPercentage(EPlayerCombatAction Action) const;
};

/** Dataset summary consumed by both the live HUD and round-end reporting. */
struct ADAPTHUNT_API FAdaptiveLearningSummary
{
    int32 SampleCount = 0;
    EPlayerCombatAction MostCommonPlayerAction =
        EPlayerCombatAction::None;
    FAdaptiveConditionalPattern StrongestConditionalPattern;
};

/** Per-candidate debug values from the two short-term memory channels. */
struct ADAPTHUNT_API FEnemyDecisionModifierTelemetry
{
    EEnemyCombatAction Action = EEnemyCombatAction::None;
    float RepetitionModifier = 0.0f;
    float RecentOutcomeModifier = 0.0f;
};

/** Value-only state formatted for the greybox HUD. */
struct ADAPTHUNT_API FAdaptiveDebugTelemetrySnapshot
{
    int32 CurrentRound = 0;
    int32 TotalRounds = 3;
    bool bPredictionsEnabled = false;
    FString RoundPresentationStage;
    FString AdaptationObservation;
    FString AdaptationAdjustment;

    float PlayerHealth = 0.0f;
    float PlayerMaxHealth = 0.0f;
    float EnemyHealth = 0.0f;
    float EnemyMaxHealth = 0.0f;
    float PlayerStamina = 0.0f;
    float PlayerMaxStamina = 0.0f;
    float EnemyStamina = 0.0f;
    float EnemyMaxStamina = 0.0f;

    EPlayerCombatAction LastPlayerAction = EPlayerCombatAction::None;
    FPredictionResult Prediction;
    int32 CollectedSampleCount = 0;
    EPlayerCombatAction MostCommonPlayerAction =
        EPlayerCombatAction::None;
    FAdaptiveConditionalPattern StrongestLearnedPattern;
    EEnemyCombatAction CurrentEnemySelectedAction =
        EEnemyCombatAction::None;
    bool bLastDecisionUrgent = false;
    FEnemyActionSelectionResult LastSelection;
    TArray<FEnemyDecisionModifierTelemetry> LastDecisionModifiers;
    int32 RecentCommittedActionCount = 0;
    int32 RecentOutcomeMemoryCount = 0;
    FAdaptiveTacticalProfile AdaptiveTacticalProfile;
    int32 AdaptiveCounterUsesRemaining = 0;
    float AdaptiveCounterCooldownRemaining = 0.0f;
    int32 CounterOutcomeCount = 0;
    bool bHasLastCounterOutcome = false;
    FAdaptiveCounterOutcomeRecord LastCounterOutcome;
    TArray<FAdaptiveCounterEffectivenessSummary>
        LastRoundCounterEffectiveness;
    FAdaptiveCounterEffectivenessSummary ActiveCounterEffectiveness;

    int32 LastCompletedRound = 0;
    FAdaptiveConditionalPattern LastRoundObservedPattern;
};

/** Pure, repeatable aggregation over immutable combat samples. */
class ADAPTHUNT_API FAdaptiveLearningTelemetryAnalyzer
{
public:
    /** RoundNumber <= 0 analyzes the full accumulated dataset. */
    static FAdaptiveLearningSummary Analyze(
        const FCombatDataset& Dataset,
        int32 RoundNumber = 0
    );
};

/** Pure formatting kept separate from AHUD drawing and UObject state. */
class ADAPTHUNT_API FAdaptiveDebugTelemetryFormatter
{
public:
    static TArray<FString> FormatHudLines(
        const FAdaptiveDebugTelemetrySnapshot& Snapshot
    );
    static FString FormatConditionalPattern(
        const FAdaptiveConditionalPattern& Pattern
    );
    static FString GetPlayerActionName(EPlayerCombatAction Action);
    static FString GetEnemyActionName(EEnemyCombatAction Action);
};
