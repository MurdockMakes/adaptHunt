#include "AI/FrequencyActionPredictor.h"

#include "adaptHunt.h"

namespace
{
const EPlayerCombatAction PredictableActions[] = {
    EPlayerCombatAction::LightAttack,
    EPlayerCombatAction::HeavyAttack,
    EPlayerCombatAction::DodgeLeft,
    EPlayerCombatAction::DodgeRight,
    EPlayerCombatAction::DodgeBackward,
    EPlayerCombatAction::Block,
    EPlayerCombatAction::Heal
};

FString GetPredictedPlayerActionName(const EPlayerCombatAction Action)
{
    const UEnum* ActionEnum = StaticEnum<EPlayerCombatAction>();
    return ActionEnum
        ? ActionEnum->GetNameStringByValue(static_cast<int64>(Action))
        : TEXT("Unknown");
}
}

FFrequencyActionPredictor::FFrequencyActionPredictor()
    : MostFrequentAction(EPlayerCombatAction::None)
    , MostFrequentActionCount(0)
    , TrainedSampleCount(0)
    , LastPredictionConfidence(0.0f)
{
}

void FFrequencyActionPredictor::Train(const FCombatDataset& Dataset)
{
    Reset();

    for (const FTrainingSample& Sample : Dataset.GetSamples())
    {
        if (!Sample.IsValid())
        {
            continue;
        }

        ++ActionCounts.FindOrAdd(Sample.NextPlayerAction);
        ++TrainedSampleCount;
    }

    // The fixed action order is an explicit, repeatable tie-break policy.
    for (const EPlayerCombatAction Action : PredictableActions)
    {
        const int32 Count = ActionCounts.FindRef(Action);
        if (Count > MostFrequentActionCount)
        {
            MostFrequentAction = Action;
            MostFrequentActionCount = Count;
        }
    }

    const float LearnedConfidence = TrainedSampleCount > 0
        ? static_cast<float>(MostFrequentActionCount)
            / static_cast<float>(TrainedSampleCount)
        : 0.0f;

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Frequency predictor trained on %d samples: %s (%d, %.1f%%)."),
        TrainedSampleCount,
        *GetPredictedPlayerActionName(MostFrequentAction),
        MostFrequentActionCount,
        LearnedConfidence * 100.0f
    );
}

FPredictionResult FFrequencyActionPredictor::Predict(
    const FCombatSnapshot& Snapshot
) const
{
    // Stage 1 intentionally uses the global prior. Conditional use of snapshot
    // features belongs to the next predictor implementation.
    static_cast<void>(Snapshot);

    FPredictionResult Result;
    if (TrainedSampleCount <= 0 || MostFrequentActionCount <= 0
        || MostFrequentAction == EPlayerCombatAction::None)
    {
        LastPredictionConfidence = 0.0f;
        return Result;
    }

    Result.PredictedAction = MostFrequentAction;
    Result.Confidence = FMath::Clamp(
        static_cast<float>(MostFrequentActionCount)
            / static_cast<float>(TrainedSampleCount),
        0.0f,
        1.0f
    );
    Result.SupportingSampleCount = MostFrequentActionCount;
    Result.bHasPrediction = true;
    LastPredictionConfidence = Result.Confidence;
    return Result;
}

void FFrequencyActionPredictor::Reset()
{
    ActionCounts.Reset();
    MostFrequentAction = EPlayerCombatAction::None;
    MostFrequentActionCount = 0;
    TrainedSampleCount = 0;
    LastPredictionConfidence = 0.0f;
}

float FFrequencyActionPredictor::GetPredictionConfidence() const
{
    return LastPredictionConfidence;
}

int32 FFrequencyActionPredictor::GetTrainedSampleCount() const
{
    return TrainedSampleCount;
}

int32 FFrequencyActionPredictor::GetActionCount(
    const EPlayerCombatAction Action
) const
{
    return ActionCounts.FindRef(Action);
}
