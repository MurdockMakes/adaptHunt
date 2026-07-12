#pragma once

#include "CoreMinimal.h"
#include "Data/CombatDataset.h"
#include "Data/CombatSnapshot.h"
#include "Data/PredictionResult.h"

/**
 * Replaceable prediction-model contract.
 *
 * This is intentionally a plain C++ interface. Predictors do not need Actor
 * behavior, reflection, garbage collection, or Blueprint implementation.
 */
class ADAPTHUNT_API IPlayerActionPredictor
{
public:
    virtual ~IPlayerActionPredictor() = default;

    virtual void Train(const FCombatDataset& Dataset) = 0;
    virtual FPredictionResult Predict(const FCombatSnapshot& Snapshot) const = 0;
    virtual void Reset() = 0;
    virtual float GetPredictionConfidence() const = 0;
};
