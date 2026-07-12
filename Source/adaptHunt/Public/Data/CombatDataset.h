#pragma once

#include "CoreMinimal.h"
#include "Data/TrainingSample.h"

#include "CombatDataset.generated.h"

/**
 * Owned collection of labeled player decisions.
 *
 * This is a value type rather than a UObject because it has no independent
 * lifetime. A behavior-tracking component will own it in a later milestone.
 */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FCombatDataset
{
    GENERATED_BODY()

public:
    bool AddSample(const FTrainingSample& Sample);
    void Reset();

    int32 Num() const;
    bool IsEmpty() const;
    int32 GetActionCount(EPlayerCombatAction Action) const;

    const TArray<FTrainingSample>& GetSamples() const;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning", meta = (AllowPrivateAccess = "true"))
    TArray<FTrainingSample> Samples;
};
