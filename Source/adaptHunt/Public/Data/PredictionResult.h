#pragma once

#include "CoreMinimal.h"
#include "Data/CombatTypes.h"

#include "PredictionResult.generated.h"

/** A predictor's high-level answer, including evidence strength. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FPredictionResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    EPlayerCombatAction PredictedAction = EPlayerCombatAction::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Confidence = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning", meta = (ClampMin = "0"))
    int32 SupportingSampleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    bool bHasPrediction = false;

    /** True when the result used a state-specific frequency table. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    bool bUsedContext = false;

    /** Enemy action that selected the contextual response table. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    EEnemyCombatAction ConditioningEnemyAction = EEnemyCombatAction::None;

    /** True when distance refined the enemy-action context. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    bool bUsedDistanceContext = false;

    /** Distance band used by the most-specific contextual response table. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    ECombatDistanceCategory ConditioningDistanceCategory =
        ECombatDistanceCategory::Medium;

    /** True when relative position refined the distance context. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    bool bUsedPositionContext = false;

    /** Relative player position used by the most-specific response table. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    ERelativePlayerPosition ConditioningRelativePlayerPosition =
        ERelativePlayerPosition::Front;

    /** True when the player's previous action refined the position context. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    bool bUsedPreviousPlayerActionContext = false;

    /** Previous player action used by the most-specific response table. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    EPlayerCombatAction ConditioningPreviousPlayerAction =
        EPlayerCombatAction::None;
};
