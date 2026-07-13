#include "AI/EnemyMovementPolicy.h"

EEnemyLocomotionIntent FEnemyMovementPolicy::SelectIntent(
    const float DistanceToTarget,
    const float RetreatRange,
    const float OrbitRange,
    const bool bOrbitRight
)
{
    if (!FMath::IsFinite(DistanceToTarget))
    {
        return EEnemyLocomotionIntent::Approach;
    }

    const float SafeRetreatRange = FMath::IsFinite(RetreatRange)
        ? FMath::Max(0.0f, RetreatRange)
        : 0.0f;
    const float SafeOrbitRange = FMath::IsFinite(OrbitRange)
        ? FMath::Max(SafeRetreatRange, OrbitRange)
        : SafeRetreatRange;
    const float SafeDistance = FMath::Max(0.0f, DistanceToTarget);

    if (SafeDistance < SafeRetreatRange)
    {
        return EEnemyLocomotionIntent::Retreat;
    }
    if (SafeDistance > SafeOrbitRange)
    {
        return EEnemyLocomotionIntent::Approach;
    }
    return bOrbitRight
        ? EEnemyLocomotionIntent::OrbitRight
        : EEnemyLocomotionIntent::OrbitLeft;
}

bool FEnemyMovementPolicy::IsMovementIntent(
    const EEnemyLocomotionIntent Intent
)
{
    return Intent == EEnemyLocomotionIntent::Approach
        || Intent == EEnemyLocomotionIntent::Retreat
        || Intent == EEnemyLocomotionIntent::OrbitLeft
        || Intent == EEnemyLocomotionIntent::OrbitRight;
}
