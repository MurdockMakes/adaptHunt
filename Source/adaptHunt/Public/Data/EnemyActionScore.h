#pragma once

#include "CoreMinimal.h"
#include "Data/CombatTypes.h"

#include "EnemyActionScore.generated.h"

/** Observable result of evaluating one enemy combat action. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FEnemyActionScore
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Utility")
    EEnemyCombatAction Action = EEnemyCombatAction::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Utility")
    float Score = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Utility")
    bool bAvailable = false;
};
