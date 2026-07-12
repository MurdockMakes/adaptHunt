#pragma once

#include "CoreMinimal.h"

#include "Data/CombatTypes.h"

#include "CombatSnapshot.generated.h"


USTRUCT(BlueprintType)
struct ADAPTHUNT_API FCombatSnapshot
{
    GENERATED_BODY()


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Snapshot")
    float CaptureTimeSeconds = 0.0f;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Health"
    )
    float PlayerHealthNormalized = 1.0f;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Health"
    )
    float EnemyHealthNormalized = 1.0f;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Stamina"
    )
    float PlayerStaminaNormalized = 1.0f;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Stamina"
    )
    float EnemyStaminaNormalized = 1.0f;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Position"
    )
    ECombatDistanceCategory DistanceCategory =
        ECombatDistanceCategory::Medium;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|History"
    )
    EPlayerCombatAction PreviousPlayerAction =
        EPlayerCombatAction::None;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|History"
    )
    EEnemyCombatAction PreviousEnemyAction =
        EEnemyCombatAction::None;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Timing"
    )
    float TimeSincePlayerAttack = 0.0f;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Timing"
    )
    float TimeSinceEnemyAttack = 0.0f;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Availability"
    )
    bool bHealAvailable = false;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Availability"
    )
    bool bDodgeAvailable = false;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Enemy"
    )
    bool bEnemyAttacking = false;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Position"
    )
    ERelativePlayerPosition RelativePlayerPosition =
        ERelativePlayerPosition::Front;


    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Snapshot|Round"
    )
    int32 RoundNumber = 1;
};
