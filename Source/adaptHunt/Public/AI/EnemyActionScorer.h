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
