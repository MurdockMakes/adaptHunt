#pragma once

#include "CoreMinimal.h"
#include "Data/CombatSnapshot.h"
#include "Data/EnemyActionScore.h"
#include "Data/PredictionResult.h"

/** Complete, immutable input for one utility-scoring pass. */
struct ADAPTHUNT_API FEnemyActionScoringContext
{
    FCombatSnapshot Snapshot;
    TSet<EEnemyCombatAction> UnavailableActions;
    bool bTargetRecentlyHealing = false;

    /** Optional learned player-action prediction for adaptive scoring. */
    FPredictionResult Prediction;

    /** Global tuning multiplier applied after model confidence. */
    float PredictionInfluence = 0.65f;

    /** Bounded state-level adjustments supplied by the tactical policy. */
    TMap<EEnemyCombatAction, float> TacticalUtilityModifiers;

    /** Hard limit applied independently of caller-provided values. */
    float MaximumTacticalUtilityAdjustment = 0.25f;

    /** Persistent round-profile adjustments, separate from tactical state. */
    TMap<EEnemyCombatAction, float> AdaptiveUtilityModifiers;

    /** Independent hard limit for the adaptive profile contribution. */
    float MaximumAdaptiveUtilityAdjustment = 0.24f;

    /** Evidence-gated counter results, separate from profile preference. */
    TMap<EEnemyCombatAction, float> OutcomeEffectivenessUtilityModifiers;

    /** Smaller independent hard bound for outcome-aware secondary input. */
    float MaximumOutcomeEffectivenessUtilityAdjustment = 0.08f;

    /** Evidence strength for a recent, committed light-attack habit. */
    float RecentLightAttackPressure = 0.0f;

    /** Independent cap for the immediate anti-spam response channel. */
    float MaximumLightAttackPatternAdjustment = 0.55f;

    /** Recent committed-action fatigue, independent of availability. */
    TMap<EEnemyCombatAction, float> RepetitionUtilityModifiers;

    /** Hard bound for the complete repetition-fatigue channel. */
    float MaximumRepetitionUtilityAdjustment = 0.42f;

    /** Recent offense/defense cadence correction, separate from repetition. */
    TMap<EEnemyCombatAction, float> OffenseDefenseBalanceUtilityModifiers;

    /** Hard bound for the cadence-balance channel. */
    float MaximumOffenseDefenseBalanceUtilityAdjustment = 0.35f;

    /** Bounded moment-to-moment terminal-result memory. */
    TMap<EEnemyCombatAction, float> RecentOutcomeUtilityModifiers;

    /** Independent hard bound for short-term result memory. */
    float MaximumRecentOutcomeUtilityAdjustment = 0.24f;
};

/** Deterministic, side-effect-free enemy combat utility model. */
class ADAPTHUNT_API FEnemyActionScorer
{
public:
    TArray<FEnemyActionScore> ScoreActions(
        const FEnemyActionScoringContext& Context
    ) const;

    FEnemyActionScore ScoreAction(
        EEnemyCombatAction Action,
        const FEnemyActionScoringContext& Context
    ) const;

    EEnemyCombatAction SelectBestAction(
        const FEnemyActionScoringContext& Context
    ) const;

    EEnemyCombatAction SelectBestAction(
        const TArray<FEnemyActionScore>& Scores
    ) const;
};
