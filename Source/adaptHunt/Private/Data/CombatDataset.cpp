#include "Data/CombatDataset.h"

bool FCombatDataset::AddSample(const FTrainingSample& Sample)
{
    if (!Sample.IsValid())
    {
        return false;
    }

    Samples.Add(Sample);
    return true;
}

void FCombatDataset::Reset()
{
    Samples.Reset();
}

int32 FCombatDataset::Num() const
{
    return Samples.Num();
}

bool FCombatDataset::IsEmpty() const
{
    return Samples.IsEmpty();
}

int32 FCombatDataset::GetActionCount(const EPlayerCombatAction Action) const
{
    int32 Count = 0;

    for (const FTrainingSample& Sample : Samples)
    {
        if (Sample.NextPlayerAction == Action)
        {
            ++Count;
        }
    }

    return Count;
}

const TArray<FTrainingSample>& FCombatDataset::GetSamples() const
{
    return Samples;
}
