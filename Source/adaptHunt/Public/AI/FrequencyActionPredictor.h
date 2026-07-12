#pragma once

#include "CoreMinimal.h"
#include "AI/PlayerActionPredictor.h"

/**
 * Interpretable Stage 1 predictor based on global action frequencies.
 *
 * Training rebuilds the model from the supplied dataset. Predict therefore
 * returns the most frequently observed player action regardless of snapshot
 * features. A later conditional predictor can replace this implementation
 * through IPlayerActionPredictor without changing its consumers.
 *
 * This is a plain C++ value type: it has no UObject lifetime, reflection, Tick,
 * or garbage-collection requirements.
 */
class ADAPTHUNT_API FFrequencyActionPredictor final
    : public IPlayerActionPredictor
{
public:
    FFrequencyActionPredictor();

    virtual void Train(const FCombatDataset& Dataset) override;
    virtual FPredictionResult Predict(
        const FCombatSnapshot& Snapshot
    ) const override;
    virtual void Reset() override;
    virtual float GetPredictionConfidence() const override;

    int32 GetTrainedSampleCount() const;
    int32 GetActionCount(EPlayerCombatAction Action) const;

private:
    TMap<EPlayerCombatAction, int32> ActionCounts;
    EPlayerCombatAction MostFrequentAction;
    int32 MostFrequentActionCount;
    int32 TrainedSampleCount;
    mutable float LastPredictionConfidence;
};
