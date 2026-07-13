#include "Presentation/CombatFeedbackTypes.h"


namespace
{
float SafeUnit(const float Value)
{
    return FMath::IsFinite(Value)
        ? FMath::Clamp(Value, 0.0f, 1.0f)
        : 0.0f;
}

float SafeNonNegative(const float Value)
{
    return FMath::IsFinite(Value) ? FMath::Max(0.0f, Value) : 0.0f;
}
}


int32 AdaptiveCombatFeedback::GetCuePriority(
    const ECombatFeedbackCue Cue
)
{
    switch (Cue)
    {
    case ECombatFeedbackCue::Death:
        return 5;
    case ECombatFeedbackCue::Hit:
    case ECombatFeedbackCue::Blocked:
    case ECombatFeedbackCue::Dodged:
        return 4;
    case ECombatFeedbackCue::Miss:
        return 3;
    case ECombatFeedbackCue::HeavyAttack:
        return 2;
    case ECombatFeedbackCue::LightAttack:
        return 1;
    default:
        return 0;
    }
}

FCombatFeedbackPulse AdaptiveCombatFeedback::EvaluatePulse(
    const ECombatFeedbackCue Cue,
    const float NormalizedElapsedTime
)
{
    FCombatFeedbackPulse Result;
    const float Time = SafeUnit(NormalizedElapsedTime);
    const float Pulse = FMath::Sin(Time * UE_PI);

    switch (Cue)
    {
    case ECombatFeedbackCue::Hit:
        Result.ScaleMultiplier = FVector(
            1.0f - 0.08f * Pulse,
            1.0f + 0.13f * Pulse,
            1.0f + 0.08f * Pulse
        );
        Result.TintColor = FLinearColor(1.0f, 0.12f, 0.04f);
        Result.TintStrength = 0.35f + 0.45f * Pulse;
        break;
    case ECombatFeedbackCue::Blocked:
        Result.ScaleMultiplier = FVector(
            1.0f - 0.10f * Pulse,
            1.0f + 0.18f * Pulse,
            1.0f + 0.12f * Pulse
        );
        Result.TintColor = FLinearColor(0.10f, 0.68f, 1.0f);
        Result.TintStrength = 0.45f + 0.40f * Pulse;
        break;
    case ECombatFeedbackCue::Dodged:
        Result.ScaleMultiplier = FVector(
            1.0f + 0.12f * Pulse,
            1.0f + 0.08f * Pulse,
            1.0f - 0.14f * Pulse
        );
        Result.TintColor = FLinearColor(0.16f, 1.0f, 0.82f);
        Result.TintStrength = 0.30f + 0.35f * Pulse;
        break;
    case ECombatFeedbackCue::Miss:
        Result.ScaleMultiplier = FVector(
            1.0f - 0.05f * Pulse,
            1.0f - 0.05f * Pulse,
            1.0f - 0.03f * Pulse
        );
        Result.TintColor = FLinearColor(1.0f, 0.78f, 0.12f);
        Result.TintStrength = 0.24f + 0.24f * Pulse;
        break;
    case ECombatFeedbackCue::Death:
        Result.TintColor = FLinearColor(0.08f, 0.08f, 0.08f);
        Result.TintStrength = 0.75f;
        break;
    default:
        break;
    }
    return Result;
}

FVector AdaptiveCombatFeedback::ResolveKnockbackVelocity(
    const FVector& TargetLocation,
    const FVector& InstigatorLocation,
    const FVector& FallbackForward,
    const float HorizontalStrength,
    const float VerticalVelocity,
    const float MaximumHorizontalStrength
)
{
    const float SafeMaximum = SafeNonNegative(MaximumHorizontalStrength);
    const float SafeStrength = FMath::Min(
        SafeNonNegative(HorizontalStrength),
        SafeMaximum
    );
    FVector Direction = TargetLocation - InstigatorLocation;
    Direction.Z = 0.0f;
    if (!Direction.Normalize())
    {
        Direction = FallbackForward.GetSafeNormal2D();
    }
    if (Direction.IsNearlyZero())
    {
        Direction = FVector::ForwardVector;
    }

    FVector Velocity = Direction * SafeStrength;
    Velocity.Z = SafeNonNegative(VerticalVelocity);
    return Velocity;
}

FVector AdaptiveCombatFeedback::ResolveCameraImpulse(
    const ECombatFeedbackCue Cue
)
{
    switch (Cue)
    {
    case ECombatFeedbackCue::LightAttack:
        return FVector(-3.5f, 0.0f, 1.5f);
    case ECombatFeedbackCue::HeavyAttack:
        return FVector(-8.0f, 0.0f, 3.5f);
    case ECombatFeedbackCue::Hit:
        return FVector(7.0f, 0.0f, 4.0f);
    case ECombatFeedbackCue::Blocked:
        return FVector(3.0f, 0.0f, 2.0f);
    case ECombatFeedbackCue::Dodged:
        return FVector(0.0f, 7.0f, -2.5f);
    default:
        return FVector::ZeroVector;
    }
}

FString AdaptiveCombatFeedback::GetCueLabel(
    const ECombatFeedbackCue Cue
)
{
    switch (Cue)
    {
    case ECombatFeedbackCue::Hit:
        return TEXT("HIT");
    case ECombatFeedbackCue::Blocked:
        return TEXT("BLOCKED");
    case ECombatFeedbackCue::Dodged:
        return TEXT("DODGED");
    case ECombatFeedbackCue::Miss:
        return TEXT("MISS");
    case ECombatFeedbackCue::Death:
        return TEXT("DEFEATED");
    default:
        return FString();
    }
}

FLinearColor AdaptiveCombatFeedback::GetCueColor(
    const ECombatFeedbackCue Cue
)
{
    switch (Cue)
    {
    case ECombatFeedbackCue::Hit:
        return FLinearColor(1.0f, 0.22f, 0.08f);
    case ECombatFeedbackCue::Blocked:
        return FLinearColor(0.16f, 0.72f, 1.0f);
    case ECombatFeedbackCue::Dodged:
        return FLinearColor(0.18f, 1.0f, 0.72f);
    case ECombatFeedbackCue::Miss:
        return FLinearColor(1.0f, 0.78f, 0.12f);
    case ECombatFeedbackCue::Death:
        return FLinearColor(1.0f, 0.22f, 0.22f);
    default:
        return FLinearColor::White;
    }
}
