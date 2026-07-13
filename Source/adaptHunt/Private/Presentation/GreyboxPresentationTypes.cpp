#include "Presentation/GreyboxPresentationTypes.h"


namespace
{
float SanitizeUnit(const float Value)
{
    return FMath::IsFinite(Value)
        ? FMath::Clamp(Value, -1.0f, 1.0f)
        : 0.0f;
}

FLinearColor SanitizeColor(const FLinearColor& Color)
{
    return FLinearColor(
        FMath::IsFinite(Color.R) ? FMath::Clamp(Color.R, 0.0f, 1.0f) : 1.0f,
        FMath::IsFinite(Color.G) ? FMath::Clamp(Color.G, 0.0f, 1.0f) : 1.0f,
        FMath::IsFinite(Color.B) ? FMath::Clamp(Color.B, 0.0f, 1.0f) : 1.0f,
        1.0f
    );
}

FLinearColor BlendColor(
    const FLinearColor& A,
    const FLinearColor& B,
    const float Alpha
)
{
    const float SafeAlpha = FMath::IsFinite(Alpha)
        ? FMath::Clamp(Alpha, 0.0f, 1.0f)
        : 0.0f;
    return SanitizeColor(A * (1.0f - SafeAlpha) + B * SafeAlpha);
}
}


EGreyboxPresentationAction AdaptiveGreyboxPresentation::FromPlayerAction(
    const EPlayerCombatAction Action
)
{
    switch (Action)
    {
    case EPlayerCombatAction::LightAttack:
        return EGreyboxPresentationAction::LightAttack;
    case EPlayerCombatAction::HeavyAttack:
        return EGreyboxPresentationAction::HeavyAttack;
    case EPlayerCombatAction::Block:
        return EGreyboxPresentationAction::Block;
    case EPlayerCombatAction::DodgeLeft:
        return EGreyboxPresentationAction::DodgeLeft;
    case EPlayerCombatAction::DodgeRight:
        return EGreyboxPresentationAction::DodgeRight;
    case EPlayerCombatAction::DodgeBackward:
        return EGreyboxPresentationAction::DodgeBackward;
    case EPlayerCombatAction::Heal:
        return EGreyboxPresentationAction::Heal;
    default:
        return EGreyboxPresentationAction::None;
    }
}

EGreyboxPresentationAction AdaptiveGreyboxPresentation::FromEnemyAction(
    const EEnemyCombatAction Action
)
{
    switch (Action)
    {
    case EEnemyCombatAction::LightAttack:
        return EGreyboxPresentationAction::LightAttack;
    case EEnemyCombatAction::HeavyAttack:
        return EGreyboxPresentationAction::HeavyAttack;
    case EEnemyCombatAction::ProjectileAttack:
        return EGreyboxPresentationAction::ProjectileAttack;
    case EEnemyCombatAction::DashAttack:
        return EGreyboxPresentationAction::DashAttack;
    case EEnemyCombatAction::InterruptHeal:
        return EGreyboxPresentationAction::InterruptAttack;
    case EEnemyCombatAction::Block:
        return EGreyboxPresentationAction::Block;
    case EEnemyCombatAction::Dodge:
        return EGreyboxPresentationAction::Dodge;
    default:
        return EGreyboxPresentationAction::None;
    }
}

EGreyboxAnimationCue AdaptiveGreyboxPresentation::ResolveCue(
    const ECombatActionPhase Phase,
    const EGreyboxPresentationAction Action,
    const bool bIsMoving
)
{
    switch (Phase)
    {
    case ECombatActionPhase::Dead:
        return EGreyboxAnimationCue::Death;
    case ECombatActionPhase::Staggered:
        return EGreyboxAnimationCue::HitReaction;
    case ECombatActionPhase::Blocking:
        return EGreyboxAnimationCue::Block;
    case ECombatActionPhase::Dodging:
        return EGreyboxAnimationCue::Dodge;
    case ECombatActionPhase::Windup:
        switch (Action)
        {
        case EGreyboxPresentationAction::LightAttack:
            return EGreyboxAnimationCue::LightWindup;
        case EGreyboxPresentationAction::HeavyAttack:
            return EGreyboxAnimationCue::HeavyWindup;
        case EGreyboxPresentationAction::ProjectileAttack:
            return EGreyboxAnimationCue::ProjectileWindup;
        case EGreyboxPresentationAction::DashAttack:
            return EGreyboxAnimationCue::DashWindup;
        case EGreyboxPresentationAction::InterruptAttack:
            return EGreyboxAnimationCue::InterruptWindup;
        case EGreyboxPresentationAction::Heal:
            return EGreyboxAnimationCue::Heal;
        default:
            return EGreyboxAnimationCue::Neutral;
        }
    case ECombatActionPhase::Active:
        switch (Action)
        {
        case EGreyboxPresentationAction::LightAttack:
            return EGreyboxAnimationCue::LightStrike;
        case EGreyboxPresentationAction::HeavyAttack:
            return EGreyboxAnimationCue::HeavyStrike;
        case EGreyboxPresentationAction::ProjectileAttack:
            return EGreyboxAnimationCue::ProjectileRelease;
        case EGreyboxPresentationAction::DashAttack:
            return EGreyboxAnimationCue::DashBurst;
        case EGreyboxPresentationAction::InterruptAttack:
            return EGreyboxAnimationCue::InterruptStrike;
        case EGreyboxPresentationAction::Heal:
            return EGreyboxAnimationCue::Heal;
        default:
            return EGreyboxAnimationCue::Neutral;
        }
    case ECombatActionPhase::Idle:
        return bIsMoving
            ? EGreyboxAnimationCue::Locomotion
            : EGreyboxAnimationCue::Neutral;
    case ECombatActionPhase::Recovery:
    default:
        return EGreyboxAnimationCue::Neutral;
    }
}

bool AdaptiveGreyboxPresentation::IsAttackAction(
    const EGreyboxPresentationAction Action
)
{
    return Action == EGreyboxPresentationAction::LightAttack
        || Action == EGreyboxPresentationAction::HeavyAttack
        || Action == EGreyboxPresentationAction::ProjectileAttack
        || Action == EGreyboxPresentationAction::DashAttack
        || Action == EGreyboxPresentationAction::InterruptAttack;
}

FGreyboxPresentationPose AdaptiveGreyboxPresentation::EvaluatePose(
    const EGreyboxAnimationCue Cue,
    const EGreyboxPresentationAction Action,
    const FLinearColor& NeutralColor,
    const float NormalizedForwardSpeed,
    const float NormalizedRightSpeed,
    const float MotionWave,
    const float StateAlpha,
    const bool bPredictionCue
)
{
    FGreyboxPresentationPose Pose;
    Pose.Color = SanitizeColor(NeutralColor);

    const float Forward = SanitizeUnit(NormalizedForwardSpeed);
    const float Right = SanitizeUnit(NormalizedRightSpeed);
    const float Wave = SanitizeUnit(MotionWave);
    const float Alpha = FMath::IsFinite(StateAlpha)
        ? FMath::Clamp(StateAlpha, 0.0f, 1.0f)
        : 0.0f;

    switch (Cue)
    {
    case EGreyboxAnimationCue::Neutral:
        Pose.LocationOffset.Z = 1.75f * Wave;
        Pose.ScaleMultiplier = FVector(
            1.0f + 0.012f * Wave,
            1.0f + 0.012f * Wave,
            1.0f + 0.020f * Wave
        );
        break;
    case EGreyboxAnimationCue::Locomotion:
        Pose.LocationOffset.Z = 4.0f * FMath::Abs(Wave);
        Pose.RotationOffset.Pitch = -8.0f * Forward;
        Pose.RotationOffset.Roll = -12.0f * Right;
        Pose.ScaleMultiplier = FVector(
            1.0f + 0.025f * FMath::Abs(Wave),
            1.0f - 0.015f * FMath::Abs(Wave),
            1.0f - 0.025f * FMath::Abs(Wave)
        );
        break;
    case EGreyboxAnimationCue::LightWindup:
        Pose.LocationOffset = FVector(-10.0f, -3.0f, 0.0f);
        Pose.RotationOffset = FRotator(10.0f, -5.0f, -5.0f);
        Pose.ScaleMultiplier = FVector(0.90f, 0.96f, 1.04f);
        Pose.Color = FLinearColor(1.0f, 0.72f, 0.08f);
        break;
    case EGreyboxAnimationCue::LightStrike:
        Pose.LocationOffset = FVector(24.0f, 2.0f, 2.0f);
        Pose.RotationOffset = FRotator(-18.0f, 8.0f, 4.0f);
        Pose.ScaleMultiplier = FVector(1.12f, 0.90f, 1.0f);
        Pose.Color = FLinearColor(1.0f, 0.92f, 0.35f);
        break;
    case EGreyboxAnimationCue::HeavyWindup:
        Pose.LocationOffset = FVector(-18.0f, 0.0f, -8.0f);
        Pose.RotationOffset = FRotator(18.0f, -12.0f, 14.0f);
        Pose.ScaleMultiplier = FVector(1.25f, 1.25f, 0.82f);
        Pose.Color = FLinearColor(1.0f, 0.18f, 0.02f);
        break;
    case EGreyboxAnimationCue::HeavyStrike:
        Pose.LocationOffset = FVector(35.0f, 0.0f, 4.0f);
        Pose.RotationOffset = FRotator(-28.0f, 15.0f, -14.0f);
        Pose.ScaleMultiplier = FVector(1.30f, 1.10f, 0.92f);
        Pose.Color = FLinearColor(1.0f, 0.42f, 0.02f);
        break;
    case EGreyboxAnimationCue::ProjectileWindup:
        Pose.LocationOffset = FVector(-4.0f, 0.0f, 10.0f);
        Pose.RotationOffset = FRotator(-5.0f, 22.0f * Wave, 0.0f);
        Pose.ScaleMultiplier = FVector(0.82f, 0.82f, 1.22f);
        Pose.Color = FLinearColor(0.42f, 0.10f, 1.0f);
        break;
    case EGreyboxAnimationCue::ProjectileRelease:
        Pose.LocationOffset = FVector(10.0f, 0.0f, 14.0f);
        Pose.RotationOffset = FRotator(-12.0f, 0.0f, 0.0f);
        Pose.ScaleMultiplier = FVector(1.08f, 1.08f, 1.18f);
        Pose.Color = FLinearColor(0.68f, 0.38f, 1.0f);
        break;
    case EGreyboxAnimationCue::DashWindup:
        Pose.LocationOffset = FVector(-18.0f, 0.0f, -12.0f);
        Pose.RotationOffset = FRotator(20.0f, 0.0f, 0.0f);
        Pose.ScaleMultiplier = FVector(1.20f, 1.10f, 0.68f);
        Pose.Color = FLinearColor(0.02f, 0.82f, 1.0f);
        break;
    case EGreyboxAnimationCue::DashBurst:
        Pose.LocationOffset = FVector(36.0f, 0.0f, -3.0f);
        Pose.RotationOffset = FRotator(-25.0f, 0.0f, 0.0f);
        Pose.ScaleMultiplier = FVector(1.45f, 0.78f, 0.72f);
        Pose.Color = FLinearColor(0.35f, 1.0f, 1.0f);
        break;
    case EGreyboxAnimationCue::InterruptWindup:
        Pose.LocationOffset = FVector(-12.0f, -7.0f, -5.0f);
        Pose.RotationOffset = FRotator(8.0f, -14.0f, 25.0f);
        Pose.ScaleMultiplier = FVector(0.90f, 1.20f, 0.85f);
        Pose.Color = FLinearColor(1.0f, 0.06f, 0.62f);
        break;
    case EGreyboxAnimationCue::InterruptStrike:
        Pose.LocationOffset = FVector(28.0f, 8.0f, 0.0f);
        Pose.RotationOffset = FRotator(-16.0f, 12.0f, -30.0f);
        Pose.ScaleMultiplier = FVector(1.25f, 0.80f, 0.95f);
        Pose.Color = FLinearColor(1.0f, 0.32f, 0.72f);
        break;
    case EGreyboxAnimationCue::Block:
        Pose.LocationOffset = FVector(-8.0f, 0.0f, -4.0f);
        Pose.RotationOffset = FRotator(-8.0f, 0.0f, 0.0f);
        Pose.ScaleMultiplier = FVector(0.75f, 1.35f, 0.90f);
        Pose.Color = FLinearColor(0.08f, 0.38f, 1.0f);
        break;
    case EGreyboxAnimationCue::Dodge:
    {
        float Side = 0.0f;
        if (Action == EGreyboxPresentationAction::DodgeLeft)
        {
            Side = -1.0f;
        }
        else if (Action == EGreyboxPresentationAction::DodgeRight)
        {
            Side = 1.0f;
        }
        else if (Action == EGreyboxPresentationAction::Dodge)
        {
            Side = FMath::IsNearlyZero(Right) ? 1.0f : FMath::Sign(Right);
        }
        const bool bBackward =
            Action == EGreyboxPresentationAction::DodgeBackward;
        Pose.LocationOffset = FVector(
            bBackward ? -14.0f : -4.0f,
            16.0f * Side,
            -10.0f
        );
        Pose.RotationOffset = FRotator(
            bBackward ? 22.0f : 4.0f,
            0.0f,
            -30.0f * Side
        );
        Pose.ScaleMultiplier = FVector(1.20f, 1.15f, 0.65f);
        Pose.Color = FLinearColor(0.18f, 0.92f, 1.0f);
        break;
    }
    case EGreyboxAnimationCue::Heal:
        Pose.LocationOffset.Z = 7.0f + 3.0f * Wave;
        Pose.RotationOffset.Yaw = 8.0f * Wave;
        Pose.ScaleMultiplier = FVector(1.08f, 1.08f, 1.08f);
        Pose.Color = FLinearColor(0.12f, 1.0f, 0.24f);
        break;
    case EGreyboxAnimationCue::HitReaction:
        Pose.LocationOffset = FVector(-13.0f * Alpha, 0.0f, -3.0f * Alpha);
        Pose.RotationOffset = FRotator(
            8.0f * Alpha,
            0.0f,
            -22.0f * Alpha
        );
        Pose.ScaleMultiplier = FVector(
            1.0f - 0.08f * Alpha,
            1.0f + 0.08f * Alpha,
            1.0f - 0.10f * Alpha
        );
        Pose.Color = FLinearColor::White;
        break;
    case EGreyboxAnimationCue::Death:
        Pose.LocationOffset = FVector(0.0f, 18.0f * Alpha, -55.0f * Alpha);
        Pose.RotationOffset = FRotator(0.0f, 0.0f, 88.0f * Alpha);
        Pose.ScaleMultiplier = FVector(
            1.0f - 0.08f * Alpha,
            1.0f,
            1.0f - 0.10f * Alpha
        );
        Pose.Color = BlendColor(Pose.Color, FLinearColor(0.12f, 0.12f, 0.12f), Alpha);
        break;
    default:
        break;
    }

    if (bPredictionCue && IsAttackAction(Action)
        && Cue != EGreyboxAnimationCue::Death
        && Cue != EGreyboxAnimationCue::HitReaction)
    {
        Pose.Color = BlendColor(
            Pose.Color,
            FLinearColor(0.78f, 0.18f, 1.0f),
            0.28f + 0.08f * FMath::Abs(Wave)
        );
    }
    return Pose;
}

FLinearColor AdaptiveGreyboxPresentation::ApplyFeedbackColor(
    const FLinearColor& BaseColor,
    const bool bHitFlash,
    const bool bInvulnerable,
    const float PulseAlpha
)
{
    if (bHitFlash)
    {
        return FLinearColor::White;
    }
    if (!bInvulnerable)
    {
        return SanitizeColor(BaseColor);
    }

    const float SafePulse = FMath::IsFinite(PulseAlpha)
        ? FMath::Clamp(PulseAlpha, 0.0f, 1.0f)
        : 0.0f;
    return BlendColor(
        BaseColor,
        FLinearColor(0.42f, 1.0f, 1.0f),
        0.30f + 0.45f * SafePulse
    );
}
