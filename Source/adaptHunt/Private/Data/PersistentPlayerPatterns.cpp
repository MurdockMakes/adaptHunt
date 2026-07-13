#include "Data/PersistentPlayerPatterns.h"


bool FPersistentPlayerPattern::IsValid() const
{
    return AdaptiveCombat::IsKnownDistanceCategory(DistanceCategory)
        && AdaptiveCombat::IsKnownRelativePosition(RelativePlayerPosition)
        && AdaptiveCombat::IsKnownPlayerAction(PreviousPlayerAction)
        && AdaptiveCombat::IsKnownEnemyAction(PreviousEnemyAction)
        && AdaptiveCombat::IsTrackablePlayerAction(NextPlayerAction);
}

FTrainingSample FPersistentPlayerPattern::ToTrainingSample() const
{
    FTrainingSample Sample;
    if (!IsValid())
    {
        return Sample;
    }

    Sample.Snapshot.RoundNumber = 1;
    Sample.Snapshot.DistanceCategory = DistanceCategory;
    Sample.Snapshot.RelativePlayerPosition = RelativePlayerPosition;
    Sample.Snapshot.PreviousPlayerAction = PreviousPlayerAction;
    Sample.Snapshot.PreviousEnemyAction = PreviousEnemyAction;
    Sample.NextPlayerAction = NextPlayerAction;
    return Sample;
}

bool FPersistentPlayerPattern::TryFromTrainingSample(
    const FTrainingSample& Sample,
    FPersistentPlayerPattern& OutPattern
)
{
    if (!Sample.IsValid())
    {
        return false;
    }

    FPersistentPlayerPattern Candidate;
    Candidate.DistanceCategory = Sample.Snapshot.DistanceCategory;
    Candidate.RelativePlayerPosition =
        Sample.Snapshot.RelativePlayerPosition;
    Candidate.PreviousPlayerAction =
        Sample.Snapshot.PreviousPlayerAction;
    Candidate.PreviousEnemyAction = Sample.Snapshot.PreviousEnemyAction;
    Candidate.NextPlayerAction = Sample.NextPlayerAction;
    if (!Candidate.IsValid())
    {
        return false;
    }

    OutPattern = Candidate;
    return true;
}

void FAdaptivePlayerPatternPolicy::Normalize(
    TArray<FPersistentPlayerPattern>& Patterns,
    const int32 MaximumSampleCount
)
{
    const int32 SafeMaximum = FMath::Clamp(MaximumSampleCount, 1, 512);
    TArray<FPersistentPlayerPattern> ValidPatterns;
    ValidPatterns.Reserve(FMath::Min(Patterns.Num(), SafeMaximum));

    for (int32 Index = Patterns.Num() - 1;
        Index >= 0 && ValidPatterns.Num() < SafeMaximum;
        --Index)
    {
        if (Patterns[Index].IsValid())
        {
            ValidPatterns.Insert(Patterns[Index], 0);
        }
    }

    Patterns = MoveTemp(ValidPatterns);
}

int32 FAdaptivePlayerPatternPolicy::AppendDataset(
    TArray<FPersistentPlayerPattern>& Patterns,
    const FCombatDataset& Dataset,
    const int32 FirstSampleIndex,
    const int32 MaximumSampleCount
)
{
    const TArray<FTrainingSample>& Samples = Dataset.GetSamples();
    const int32 SafeFirstIndex = FMath::Clamp(
        FirstSampleIndex,
        0,
        Samples.Num()
    );
    int32 AddedCount = 0;
    for (int32 Index = SafeFirstIndex; Index < Samples.Num(); ++Index)
    {
        FPersistentPlayerPattern Pattern;
        if (FPersistentPlayerPattern::TryFromTrainingSample(
            Samples[Index],
            Pattern
        ))
        {
            Patterns.Add(Pattern);
            ++AddedCount;
        }
    }

    Normalize(Patterns, MaximumSampleCount);
    return AddedCount;
}

FCombatDataset FAdaptivePlayerPatternPolicy::BuildDataset(
    const TArray<FPersistentPlayerPattern>& Patterns
)
{
    FCombatDataset Dataset;
    for (const FPersistentPlayerPattern& Pattern : Patterns)
    {
        Dataset.AddSample(Pattern.ToTrainingSample());
    }
    return Dataset;
}

float FAdaptivePlayerPatternPolicy::CalculateRecentLightAttackPressure(
    const FCombatDataset& PersistentHistory,
    const FCombatDataset& CurrentSession,
    const int32 RecentActionWindow,
    const int32 MinimumLightAttackSamples
)
{
    const int32 SafeWindow = FMath::Clamp(RecentActionWindow, 1, 32);
    const int32 SafeMinimum = FMath::Clamp(
        MinimumLightAttackSamples,
        1,
        SafeWindow
    );
    int32 ObservedCount = 0;
    int32 LightAttackCount = 0;

    const auto ConsumeNewest = [
        &ObservedCount,
        &LightAttackCount,
        SafeWindow
    ](const FCombatDataset& Dataset)
    {
        const TArray<FTrainingSample>& Samples = Dataset.GetSamples();
        for (int32 Index = Samples.Num() - 1;
            Index >= 0 && ObservedCount < SafeWindow;
            --Index)
        {
            const FTrainingSample& Sample = Samples[Index];
            if (!Sample.IsValid())
            {
                continue;
            }
            ++ObservedCount;
            if (Sample.NextPlayerAction
                == EPlayerCombatAction::LightAttack)
            {
                ++LightAttackCount;
            }
        }
    };

    // Live actions are newer than anything loaded from disk.
    ConsumeNewest(CurrentSession);
    if (ObservedCount < SafeWindow)
    {
        ConsumeNewest(PersistentHistory);
    }

    if (ObservedCount < SafeMinimum || LightAttackCount < SafeMinimum)
    {
        return 0.0f;
    }

    const float LightRatio = static_cast<float>(LightAttackCount)
        / static_cast<float>(ObservedCount);
    // A bare majority begins to matter; 75% or more is full pressure.
    return FMath::Clamp((LightRatio - 0.50f) / 0.25f, 0.0f, 1.0f);
}
