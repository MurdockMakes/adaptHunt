#pragma once

#include "CoreMinimal.h"
#include "AI/FrequencyActionPredictor.h"

/**
 * Interpretable predictor conditioned on discrete combat snapshot features.
 *
 * Predict first tries the exact previous-enemy-action, distance-band,
 * relative-position, and previous-player-action table. It then backs off
 * through position, distance, enemy action, and finally the global frequency
 * model. Each level requires the configured evidence threshold, and the fixed
 * player-action order provides deterministic tie breaking throughout the
 * hierarchy.
 */
class ADAPTHUNT_API FConditionalActionPredictor final
    : public IPlayerActionPredictor
{
public:
    explicit FConditionalActionPredictor(
        int32 InMinimumConditionalSampleCount = 2
    );

    virtual void Train(const FCombatDataset& Dataset) override;
    virtual FPredictionResult Predict(
        const FCombatSnapshot& Snapshot
    ) const override;
    virtual void Reset() override;
    virtual float GetPredictionConfidence() const override;

    int32 GetTrainedSampleCount() const;
    int32 GetMinimumConditionalSampleCount() const;
    int32 GetConditionalSampleCount(EEnemyCombatAction EnemyAction) const;
    int32 GetConditionalActionCount(
        EEnemyCombatAction EnemyAction,
        EPlayerCombatAction PlayerAction
    ) const;
    int32 GetDistanceConditionalSampleCount(
        EEnemyCombatAction EnemyAction,
        ECombatDistanceCategory DistanceCategory
    ) const;
    int32 GetDistanceConditionalActionCount(
        EEnemyCombatAction EnemyAction,
        ECombatDistanceCategory DistanceCategory,
        EPlayerCombatAction PlayerAction
    ) const;
    int32 GetPositionConditionalSampleCount(
        EEnemyCombatAction EnemyAction,
        ECombatDistanceCategory DistanceCategory,
        ERelativePlayerPosition RelativePlayerPosition
    ) const;
    int32 GetPositionConditionalActionCount(
        EEnemyCombatAction EnemyAction,
        ECombatDistanceCategory DistanceCategory,
        ERelativePlayerPosition RelativePlayerPosition,
        EPlayerCombatAction PlayerAction
    ) const;
    int32 GetPreviousPlayerActionConditionalSampleCount(
        EEnemyCombatAction EnemyAction,
        ECombatDistanceCategory DistanceCategory,
        ERelativePlayerPosition RelativePlayerPosition,
        EPlayerCombatAction PreviousPlayerAction
    ) const;
    int32 GetPreviousPlayerActionConditionalActionCount(
        EEnemyCombatAction EnemyAction,
        ECombatDistanceCategory DistanceCategory,
        ERelativePlayerPosition RelativePlayerPosition,
        EPlayerCombatAction PreviousPlayerAction,
        EPlayerCombatAction NextPlayerAction
    ) const;

private:
    using FActionCounts = TMap<EPlayerCombatAction, int32>;
    using FDistanceActionCounts =
        TMap<ECombatDistanceCategory, FActionCounts>;
    using FPositionActionCounts =
        TMap<ERelativePlayerPosition, FActionCounts>;
    using FDistancePositionActionCounts =
        TMap<ECombatDistanceCategory, FPositionActionCounts>;
    using FPreviousPlayerActionCounts =
        TMap<EPlayerCombatAction, FActionCounts>;
    using FPositionPreviousPlayerActionCounts =
        TMap<ERelativePlayerPosition, FPreviousPlayerActionCounts>;
    using FDistancePositionPreviousPlayerActionCounts =
        TMap<ECombatDistanceCategory, FPositionPreviousPlayerActionCounts>;

    FFrequencyActionPredictor GlobalPredictor;
    TMap<EEnemyCombatAction, FActionCounts> ConditionalActionCounts;
    TMap<EEnemyCombatAction, FDistanceActionCounts>
        DistanceConditionalActionCounts;
    TMap<EEnemyCombatAction, FDistancePositionActionCounts>
        PositionConditionalActionCounts;
    TMap<EEnemyCombatAction, FDistancePositionPreviousPlayerActionCounts>
        PreviousPlayerActionConditionalActionCounts;
    int32 TrainedSampleCount;
    int32 MinimumConditionalSampleCount;
    mutable float LastPredictionConfidence;
};
