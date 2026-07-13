#pragma once

#include "CoreMinimal.h"
#include "Data/CombatDataset.h"
#include "GameFramework/SaveGame.h"

#include "PersistentPlayerPatterns.generated.h"


/**
 * Compact, privacy-minimal representation of one committed player decision.
 * Runtime-only values such as health, timestamps, and round progression are
 * deliberately not persisted because the predictor does not need them.
 */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FPersistentPlayerPattern
{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    ECombatDistanceCategory DistanceCategory =
        ECombatDistanceCategory::Medium;

    UPROPERTY(SaveGame)
    ERelativePlayerPosition RelativePlayerPosition =
        ERelativePlayerPosition::Front;

    UPROPERTY(SaveGame)
    EPlayerCombatAction PreviousPlayerAction =
        EPlayerCombatAction::None;

    UPROPERTY(SaveGame)
    EEnemyCombatAction PreviousEnemyAction =
        EEnemyCombatAction::None;

    UPROPERTY(SaveGame)
    EPlayerCombatAction NextPlayerAction =
        EPlayerCombatAction::None;

    bool IsValid() const;
    FTrainingSample ToTrainingSample() const;

    static bool TryFromTrainingSample(
        const FTrainingSample& Sample,
        FPersistentPlayerPattern& OutPattern
    );
};


/** Versioned local save payload for bounded cross-session player patterns. */
UCLASS()
class ADAPTHUNT_API UAdaptivePlayerPatternSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentSchemaVersion = 1;

    UPROPERTY(SaveGame)
    int32 SchemaVersion = CurrentSchemaVersion;

    UPROPERTY(SaveGame)
    TArray<FPersistentPlayerPattern> Patterns;
};


/** Pure validation, FIFO retention, reconstruction, and habit-pressure math. */
struct ADAPTHUNT_API FAdaptivePlayerPatternPolicy
{
    static void Normalize(
        TArray<FPersistentPlayerPattern>& Patterns,
        int32 MaximumSampleCount
    );

    /** Appends valid samples beginning at FirstSampleIndex and returns a count. */
    static int32 AppendDataset(
        TArray<FPersistentPlayerPattern>& Patterns,
        const FCombatDataset& Dataset,
        int32 FirstSampleIndex,
        int32 MaximumSampleCount
    );

    static FCombatDataset BuildDataset(
        const TArray<FPersistentPlayerPattern>& Patterns
    );

    /**
     * Returns [0, 1] once light attacks dominate a recent committed-action
     * window with enough evidence. Current-session samples are always newest.
     */
    static float CalculateRecentLightAttackPressure(
        const FCombatDataset& PersistentHistory,
        const FCombatDataset& CurrentSession,
        int32 RecentActionWindow,
        int32 MinimumLightAttackSamples
    );
};
