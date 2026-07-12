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
};
