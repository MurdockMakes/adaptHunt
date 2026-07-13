#pragma once

#include "CoreMinimal.h"

#include "EnemyMovementPolicy.generated.h"

/** High-level movement request selected by the enemy brain. */
UENUM(BlueprintType)
enum class EEnemyLocomotionIntent : uint8
{
    None,
    Approach,
    Retreat,
    OrbitLeft,
    OrbitRight
};

/**
 * Side-effect-free spacing policy used by UEnemyDecisionComponent.
 *
 * Keeping this policy out of the movement driver prevents navigation and
 * collision handling from becoming another combat-decision system.
 */
struct ADAPTHUNT_API FEnemyMovementPolicy
{
    static EEnemyLocomotionIntent SelectIntent(
        float DistanceToTarget,
        float RetreatRange,
        float OrbitRange,
        bool bOrbitRight
    );

    static bool IsMovementIntent(EEnemyLocomotionIntent Intent);
};
