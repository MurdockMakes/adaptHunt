#include "Combat/PlayerResponsivenessPolicy.h"

namespace
{
FVector FlattenAndNormalize(const FVector& Direction)
{
    return FVector(Direction.X, Direction.Y, 0.0f).GetSafeNormal();
}
}

bool AdaptivePlayerResponsiveness::IsBufferableAction(
    const EPlayerCombatAction Action
)
{
    switch (Action)
    {
    case EPlayerCombatAction::LightAttack:
    case EPlayerCombatAction::HeavyAttack:
    case EPlayerCombatAction::DodgeLeft:
    case EPlayerCombatAction::DodgeRight:
    case EPlayerCombatAction::DodgeBackward:
    case EPlayerCombatAction::Block:
    case EPlayerCombatAction::Heal:
        return true;
    default:
        return false;
    }
}

float AdaptivePlayerResponsiveness::SanitizeInputBufferDuration(
    const float Duration
)
{
    return FMath::IsFinite(Duration)
        ? FMath::Clamp(Duration, 0.0f, 0.5f)
        : 0.0f;
}

bool AdaptivePlayerResponsiveness::CanBufferDuringRecovery(
    const ECombatActionPhase Phase,
    const float RemainingRecoveryTime,
    const float InputBufferDuration,
    const bool bCombatEnabled,
    const bool bOwnerAlive
)
{
    const float SafeWindow = SanitizeInputBufferDuration(
        InputBufferDuration
    );
    return bCombatEnabled && bOwnerAlive
        && Phase == ECombatActionPhase::Recovery
        && FMath::IsFinite(RemainingRecoveryTime)
        && RemainingRecoveryTime >= 0.0f
        && RemainingRecoveryTime <= SafeWindow;
}

FVector AdaptivePlayerResponsiveness::ResolveDodgeDirection(
    const EPlayerCombatAction DodgeAction,
    const FVector& ActorForward,
    const FVector& ActorRight,
    const FVector& CameraForward,
    const FVector& CameraRight,
    const bool bHasCameraBasis
)
{
    FVector Forward = FlattenAndNormalize(
        bHasCameraBasis ? CameraForward : ActorForward
    );
    FVector Right = FlattenAndNormalize(
        bHasCameraBasis ? CameraRight : ActorRight
    );
    if (Forward.IsNearlyZero())
    {
        Forward = FlattenAndNormalize(ActorForward);
    }
    if (Right.IsNearlyZero())
    {
        Right = FlattenAndNormalize(ActorRight);
    }

    switch (DodgeAction)
    {
    case EPlayerCombatAction::DodgeLeft:
        return -Right;
    case EPlayerCombatAction::DodgeRight:
        return Right;
    case EPlayerCombatAction::DodgeBackward:
        return -Forward;
    default:
        return FVector::ZeroVector;
    }
}

bool AdaptivePlayerResponsiveness::ComputeMeleeFacingCorrection(
    const FVector& PlayerForward,
    const FVector& DirectionToTarget,
    const float ConeHalfAngleDegrees,
    const float MaxCorrectionDegrees,
    float& OutYawCorrectionDegrees
)
{
    OutYawCorrectionDegrees = 0.0f;
    const FVector Forward = FlattenAndNormalize(PlayerForward);
    const FVector TargetDirection = FlattenAndNormalize(DirectionToTarget);
    if (Forward.IsNearlyZero() || TargetDirection.IsNearlyZero())
    {
        return false;
    }

    const float SafeConeHalfAngle = FMath::Clamp(
        FMath::IsFinite(ConeHalfAngleDegrees)
            ? ConeHalfAngleDegrees
            : 0.0f,
        0.0f,
        89.0f
    );
    const float SafeMaxCorrection = FMath::Clamp(
        FMath::IsFinite(MaxCorrectionDegrees)
            ? MaxCorrectionDegrees
            : 0.0f,
        0.0f,
        SafeConeHalfAngle
    );
    const float ForwardDot = FVector::DotProduct(Forward, TargetDirection);
    const float MinimumDot = FMath::Cos(FMath::DegreesToRadians(
        SafeConeHalfAngle
    ));
    if (ForwardDot <= 0.0f || ForwardDot < MinimumDot)
    {
        return false;
    }

    const float CrossZ = FVector::CrossProduct(
        Forward,
        TargetDirection
    ).Z;
    const float SignedAngle = FMath::RadiansToDegrees(FMath::Atan2(
        CrossZ,
        ForwardDot
    ));
    OutYawCorrectionDegrees = FMath::Clamp(
        SignedAngle,
        -SafeMaxCorrection,
        SafeMaxCorrection
    );
    return !FMath::IsNearlyZero(OutYawCorrectionDegrees);
}

FPlayerCombatInputBuffer::FPlayerCombatInputBuffer()
    : BufferedAction(EPlayerCombatAction::None)
    , BufferedActionExpirationTime(0.0)
{
}

bool FPlayerCombatInputBuffer::TryStore(
    const EPlayerCombatAction Action,
    const double ExpirationTime
)
{
    if (!AdaptivePlayerResponsiveness::IsBufferableAction(Action)
        || !FMath::IsFinite(ExpirationTime))
    {
        return false;
    }
    if (HasAction())
    {
        return BufferedAction == Action;
    }

    BufferedAction = Action;
    BufferedActionExpirationTime = ExpirationTime;
    return true;
}

EPlayerCombatAction FPlayerCombatInputBuffer::ConsumeIfValid(
    const double CurrentTime
)
{
    const EPlayerCombatAction Result = HasAction()
        && FMath::IsFinite(CurrentTime)
        && CurrentTime <= BufferedActionExpirationTime
        ? BufferedAction
        : EPlayerCombatAction::None;
    Clear();
    return Result;
}

void FPlayerCombatInputBuffer::Clear()
{
    BufferedAction = EPlayerCombatAction::None;
    BufferedActionExpirationTime = 0.0;
}

bool FPlayerCombatInputBuffer::HasAction() const
{
    return BufferedAction != EPlayerCombatAction::None;
}

EPlayerCombatAction FPlayerCombatInputBuffer::GetAction() const
{
    return BufferedAction;
}

double FPlayerCombatInputBuffer::GetExpirationTime() const
{
    return BufferedActionExpirationTime;
}
