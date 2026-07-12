#pragma once

#include "CoreMinimal.h"
#include "Data/CombatSnapshot.h"

#include "TrainingSample.generated.h"

/**
 * A supervised-learning row: the state captured immediately before the
 * player committed to the labeled action.
 */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FTrainingSample
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    FCombatSnapshot Snapshot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    EPlayerCombatAction NextPlayerAction = EPlayerCombatAction::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Machine Learning")
    float ActionTimeSeconds = 0.0f;

    bool IsValid() const
    {
        const auto IsNormalized = [](const float Value)
        {
            return FMath::IsFinite(Value)
                && Value >= 0.0f
                && Value <= 1.0f;
        };

        return AdaptiveCombat::IsTrackablePlayerAction(NextPlayerAction)
            && Snapshot.RoundNumber > 0
            && FMath::IsFinite(Snapshot.CaptureTimeSeconds)
            && Snapshot.CaptureTimeSeconds >= 0.0f
            && FMath::IsFinite(ActionTimeSeconds)
            && ActionTimeSeconds >= 0.0f
            && IsNormalized(Snapshot.PlayerHealthNormalized)
            && IsNormalized(Snapshot.EnemyHealthNormalized)
            && IsNormalized(Snapshot.PlayerStaminaNormalized)
            && IsNormalized(Snapshot.EnemyStaminaNormalized)
            && AdaptiveCombat::IsKnownDistanceCategory(
                Snapshot.DistanceCategory
            )
            && AdaptiveCombat::IsKnownPlayerAction(
                Snapshot.PreviousPlayerAction
            )
            && AdaptiveCombat::IsKnownEnemyAction(
                Snapshot.PreviousEnemyAction
            )
            && FMath::IsFinite(Snapshot.TimeSincePlayerAttack)
            && Snapshot.TimeSincePlayerAttack >= 0.0f
            && FMath::IsFinite(Snapshot.TimeSinceEnemyAttack)
            && Snapshot.TimeSinceEnemyAttack >= 0.0f
            && AdaptiveCombat::IsKnownRelativePosition(
                Snapshot.RelativePlayerPosition
            );
    }
};
