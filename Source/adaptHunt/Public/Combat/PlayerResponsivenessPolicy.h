#pragma once

#include "CoreMinimal.h"
#include "Data/CombatTypes.h"

/**
 * Side-effect-free player-feel policies used by UCombatComponent.
 *
 * Keeping direction, buffer-window, and facing calculations independent from
 * world state makes the responsiveness rules deterministic and testable.
 */
namespace AdaptivePlayerResponsiveness
{
    ADAPTHUNT_API bool IsBufferableAction(EPlayerCombatAction Action);

    ADAPTHUNT_API float SanitizeInputBufferDuration(float Duration);

    ADAPTHUNT_API bool CanBufferDuringRecovery(
        ECombatActionPhase Phase,
        float RemainingRecoveryTime,
        float InputBufferDuration,
        bool bCombatEnabled,
        bool bOwnerAlive
    );

    ADAPTHUNT_API FVector ResolveDodgeDirection(
        EPlayerCombatAction DodgeAction,
        const FVector& ActorForward,
        const FVector& ActorRight,
        const FVector& CameraForward,
        const FVector& CameraRight,
        bool bHasCameraBasis
    );

    /**
     * Returns the bounded yaw correction toward a target inside the assist
     * cone. The cone is capped below 90 degrees so targets behind the player
     * can never influence facing, even with invalid tuning.
     */
    ADAPTHUNT_API bool ComputeMeleeFacingCorrection(
        const FVector& PlayerForward,
        const FVector& DirectionToTarget,
        float ConeHalfAngleDegrees,
        float MaxCorrectionDegrees,
        float& OutYawCorrectionDegrees
    );
}

/** Stores at most one action without committing gameplay side effects. */
class ADAPTHUNT_API FPlayerCombatInputBuffer
{
public:
    FPlayerCombatInputBuffer();

    /**
     * Stores an action until ExpirationTime. Repeated requests for the same
     * action are accepted without refreshing or duplicating it. A different
     * action cannot replace the existing entry.
     */
    bool TryStore(EPlayerCombatAction Action, double ExpirationTime);

    EPlayerCombatAction ConsumeIfValid(double CurrentTime);
    void Clear();

    bool HasAction() const;
    EPlayerCombatAction GetAction() const;
    double GetExpirationTime() const;

private:
    EPlayerCombatAction BufferedAction;
    double BufferedActionExpirationTime;
};
