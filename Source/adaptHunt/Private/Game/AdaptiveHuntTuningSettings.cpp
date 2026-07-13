#include "Game/AdaptiveHuntTuningSettings.h"

namespace
{
float SanitizeMinimum(
    const float Value,
    const float Minimum,
    const float Fallback
)
{
    return FMath::IsFinite(Value)
        ? FMath::Max(Minimum, Value)
        : Fallback;
}

float SanitizeRange(
    const float Value,
    const float Minimum,
    const float Maximum,
    const float Fallback
)
{
    return FMath::IsFinite(Value)
        ? FMath::Clamp(Value, Minimum, Maximum)
        : Fallback;
}

float SanitizeFinite(const float Value, const float Fallback)
{
    return FMath::IsFinite(Value) ? Value : Fallback;
}

FCombatActionTiming SanitizeTiming(
    const FCombatActionTiming& Timing,
    const FCombatActionTiming& Fallback
)
{
    return FCombatActionTiming(
        SanitizeMinimum(
            Timing.WindupDuration,
            0.0f,
            Fallback.WindupDuration
        ),
        SanitizeMinimum(
            Timing.ActiveDuration,
            0.0f,
            Fallback.ActiveDuration
        ),
        SanitizeMinimum(
            Timing.RecoveryDuration,
            0.0f,
            Fallback.RecoveryDuration
        )
    );
}
}


FAdaptiveMovementTuning::FAdaptiveMovementTuning()
    : PlayerMaxWalkSpeed(500.0f)
    , PlayerJumpVelocity(520.0f)
    , PlayerAcceleration(2600.0f)
    , PlayerBrakingDeceleration(2400.0f)
    , PlayerGroundFriction(8.0f)
    , PlayerBrakingFriction(6.0f)
    , PlayerRotationRateYaw(900.0f)
    , PlayerAirControl(0.4f)
    , PlayerMinimumAnalogWalkSpeed(8.0f)
    , EnemyPreferredRange(425.0f)
    , EnemyRetreatRange(180.0f)
    , EnemyOrbitRange(625.0f)
    , EnemyAcceptanceRadius(65.0f)
    , EnemyAcceleration(1600.0f)
    , EnemyTurnSpeed(540.0f)
    , EnemyMovementSpeed(360.0f)
    , EnemyNavigationRefreshInterval(0.25f)
    , EnemyOrbitStepDistance(190.0f)
    , EnemyCollisionProbeDistance(110.0f)
    , EnemyStuckTimeout(0.85f)
    , EnemyMinimumProgressDistance(10.0f)
    , EnemyStuckRecoveryDuration(0.4f)
{
}

FAdaptiveMovementTuning FAdaptiveMovementTuning::GetSanitized() const
{
    FAdaptiveMovementTuning Result;
    Result.PlayerMaxWalkSpeed = SanitizeMinimum(
        PlayerMaxWalkSpeed,
        0.0f,
        Result.PlayerMaxWalkSpeed
    );
    Result.PlayerJumpVelocity = SanitizeMinimum(
        PlayerJumpVelocity,
        0.0f,
        Result.PlayerJumpVelocity
    );
    Result.PlayerAcceleration = SanitizeMinimum(
        PlayerAcceleration,
        0.0f,
        Result.PlayerAcceleration
    );
    Result.PlayerBrakingDeceleration = SanitizeMinimum(
        PlayerBrakingDeceleration,
        0.0f,
        Result.PlayerBrakingDeceleration
    );
    Result.PlayerGroundFriction = SanitizeMinimum(
        PlayerGroundFriction,
        0.0f,
        Result.PlayerGroundFriction
    );
    Result.PlayerBrakingFriction = SanitizeMinimum(
        PlayerBrakingFriction,
        0.0f,
        Result.PlayerBrakingFriction
    );
    Result.PlayerRotationRateYaw = SanitizeMinimum(
        PlayerRotationRateYaw,
        0.0f,
        Result.PlayerRotationRateYaw
    );
    Result.PlayerAirControl = SanitizeRange(
        PlayerAirControl,
        0.0f,
        1.0f,
        Result.PlayerAirControl
    );
    Result.PlayerMinimumAnalogWalkSpeed = FMath::Min(
        Result.PlayerMaxWalkSpeed,
        SanitizeMinimum(
            PlayerMinimumAnalogWalkSpeed,
            0.0f,
            Result.PlayerMinimumAnalogWalkSpeed
        )
    );

    Result.EnemyRetreatRange = SanitizeMinimum(
        EnemyRetreatRange,
        0.0f,
        Result.EnemyRetreatRange
    );
    Result.EnemyPreferredRange = FMath::Max(
        Result.EnemyRetreatRange + 1.0f,
        SanitizeMinimum(
            EnemyPreferredRange,
            0.0f,
            Result.EnemyPreferredRange
        )
    );
    Result.EnemyOrbitRange = FMath::Max(
        Result.EnemyPreferredRange + 1.0f,
        SanitizeMinimum(
            EnemyOrbitRange,
            0.0f,
            Result.EnemyOrbitRange
        )
    );
    Result.EnemyAcceptanceRadius = SanitizeMinimum(
        EnemyAcceptanceRadius,
        0.0f,
        Result.EnemyAcceptanceRadius
    );
    Result.EnemyAcceleration = SanitizeMinimum(
        EnemyAcceleration,
        0.0f,
        Result.EnemyAcceleration
    );
    Result.EnemyTurnSpeed = SanitizeMinimum(
        EnemyTurnSpeed,
        0.0f,
        Result.EnemyTurnSpeed
    );
    Result.EnemyMovementSpeed = SanitizeMinimum(
        EnemyMovementSpeed,
        0.0f,
        Result.EnemyMovementSpeed
    );
    Result.EnemyNavigationRefreshInterval = SanitizeMinimum(
        EnemyNavigationRefreshInterval,
        0.02f,
        Result.EnemyNavigationRefreshInterval
    );
    Result.EnemyOrbitStepDistance = SanitizeMinimum(
        EnemyOrbitStepDistance,
        0.0f,
        Result.EnemyOrbitStepDistance
    );
    Result.EnemyCollisionProbeDistance = SanitizeMinimum(
        EnemyCollisionProbeDistance,
        1.0f,
        Result.EnemyCollisionProbeDistance
    );
    Result.EnemyStuckTimeout = SanitizeMinimum(
        EnemyStuckTimeout,
        0.05f,
        Result.EnemyStuckTimeout
    );
    Result.EnemyMinimumProgressDistance = SanitizeMinimum(
        EnemyMinimumProgressDistance,
        0.0f,
        Result.EnemyMinimumProgressDistance
    );
    Result.EnemyStuckRecoveryDuration = SanitizeMinimum(
        EnemyStuckRecoveryDuration,
        0.05f,
        Result.EnemyStuckRecoveryDuration
    );
    return Result;
}


FAdaptiveActionTimingTuning::FAdaptiveActionTimingTuning()
    : PlayerLightAttack(0.12f, 0.08f, 0.25f)
    , PlayerHeavyAttack(0.35f, 0.12f, 0.5f)
    , PlayerHeal(0.4f, 0.05f, 0.35f)
    , PlayerBlockRecoveryDuration(0.15f)
    , PlayerDodgeInvulnerabilityDuration(0.25f)
    , PlayerDodgeRecoveryDuration(0.2f)
    , PlayerStaggerDuration(0.3f)
    , PlayerInputBufferDuration(0.14f)
    , EnemyLightAttack(0.2f, 0.08f, 0.3f)
    , EnemyHeavyAttack(0.55f, 0.12f, 0.55f)
    , EnemyProjectileAttack(0.45f, 0.05f, 0.4f)
    , EnemyDashAttack(0.4f, 0.15f, 0.55f)
    , EnemyInterruptHeal(0.25f, 0.08f, 0.4f)
    , EnemyBlockDuration(0.8f)
    , EnemyBlockRecoveryDuration(0.25f)
    , EnemyDodgeInvulnerabilityDuration(0.2f)
    , EnemyDodgeRecoveryDuration(0.25f)
    , EnemyStaggerDuration(0.3f)
{
}

FAdaptiveActionTimingTuning
FAdaptiveActionTimingTuning::GetSanitized() const
{
    FAdaptiveActionTimingTuning Result;
    Result.PlayerLightAttack = SanitizeTiming(
        PlayerLightAttack,
        Result.PlayerLightAttack
    );
    Result.PlayerHeavyAttack = SanitizeTiming(
        PlayerHeavyAttack,
        Result.PlayerHeavyAttack
    );
    Result.PlayerHeal = SanitizeTiming(PlayerHeal, Result.PlayerHeal);
    Result.PlayerBlockRecoveryDuration = SanitizeMinimum(
        PlayerBlockRecoveryDuration,
        0.0f,
        Result.PlayerBlockRecoveryDuration
    );
    Result.PlayerDodgeInvulnerabilityDuration = SanitizeMinimum(
        PlayerDodgeInvulnerabilityDuration,
        0.0f,
        Result.PlayerDodgeInvulnerabilityDuration
    );
    Result.PlayerDodgeRecoveryDuration = SanitizeMinimum(
        PlayerDodgeRecoveryDuration,
        0.0f,
        Result.PlayerDodgeRecoveryDuration
    );
    Result.PlayerStaggerDuration = SanitizeMinimum(
        PlayerStaggerDuration,
        0.0f,
        Result.PlayerStaggerDuration
    );
    Result.PlayerInputBufferDuration = SanitizeRange(
        PlayerInputBufferDuration,
        0.0f,
        0.5f,
        Result.PlayerInputBufferDuration
    );
    Result.EnemyLightAttack = SanitizeTiming(
        EnemyLightAttack,
        Result.EnemyLightAttack
    );
    Result.EnemyHeavyAttack = SanitizeTiming(
        EnemyHeavyAttack,
        Result.EnemyHeavyAttack
    );
    Result.EnemyProjectileAttack = SanitizeTiming(
        EnemyProjectileAttack,
        Result.EnemyProjectileAttack
    );
    Result.EnemyDashAttack = SanitizeTiming(
        EnemyDashAttack,
        Result.EnemyDashAttack
    );
    Result.EnemyInterruptHeal = SanitizeTiming(
        EnemyInterruptHeal,
        Result.EnemyInterruptHeal
    );
    Result.EnemyBlockDuration = SanitizeMinimum(
        EnemyBlockDuration,
        0.0f,
        Result.EnemyBlockDuration
    );
    Result.EnemyBlockRecoveryDuration = SanitizeMinimum(
        EnemyBlockRecoveryDuration,
        0.0f,
        Result.EnemyBlockRecoveryDuration
    );
    Result.EnemyDodgeInvulnerabilityDuration = SanitizeMinimum(
        EnemyDodgeInvulnerabilityDuration,
        0.0f,
        Result.EnemyDodgeInvulnerabilityDuration
    );
    Result.EnemyDodgeRecoveryDuration = SanitizeMinimum(
        EnemyDodgeRecoveryDuration,
        0.0f,
        Result.EnemyDodgeRecoveryDuration
    );
    Result.EnemyStaggerDuration = SanitizeMinimum(
        EnemyStaggerDuration,
        0.0f,
        Result.EnemyStaggerDuration
    );
    return Result;
}


FAdaptiveCameraTuning::FAdaptiveCameraTuning()
    : bEnablePositionLag(true)
    , PositionLagSpeed(18.0f)
    , PositionLagMaxDistance(25.0f)
    , bEnableRotationSmoothing(true)
    , RotationSmoothingSpeed(20.0f)
    , TargetArmLength(450.0f)
    , BoomHeight(70.0f)
    , SocketSideOffset(55.0f)
    , SocketHeightOffset(15.0f)
{
}

FAdaptiveCameraTuning FAdaptiveCameraTuning::GetSanitized() const
{
    FAdaptiveCameraTuning Result;
    Result.bEnablePositionLag = bEnablePositionLag;
    Result.PositionLagSpeed = SanitizeMinimum(
        PositionLagSpeed,
        0.0f,
        Result.PositionLagSpeed
    );
    Result.PositionLagMaxDistance = SanitizeMinimum(
        PositionLagMaxDistance,
        0.0f,
        Result.PositionLagMaxDistance
    );
    Result.bEnableRotationSmoothing = bEnableRotationSmoothing;
    Result.RotationSmoothingSpeed = SanitizeMinimum(
        RotationSmoothingSpeed,
        0.0f,
        Result.RotationSmoothingSpeed
    );
    Result.TargetArmLength = SanitizeMinimum(
        TargetArmLength,
        0.0f,
        Result.TargetArmLength
    );
    Result.BoomHeight = SanitizeFinite(BoomHeight, Result.BoomHeight);
    Result.SocketSideOffset = SanitizeFinite(
        SocketSideOffset,
        Result.SocketSideOffset
    );
    Result.SocketHeightOffset = SanitizeFinite(
        SocketHeightOffset,
        Result.SocketHeightOffset
    );
    return Result;
}


FAdaptiveFeedbackTuning::FAdaptiveFeedbackTuning()
    : FeedbackDisplayDuration(0.38f)
    , bEnableKnockback(true)
    , KnockbackStrength(220.0f)
    , HeavyKnockbackMultiplier(1.55f)
    , BlockedKnockbackMultiplier(0.32f)
    , KnockbackVerticalVelocity(35.0f)
    , MaximumKnockbackStrength(420.0f)
    , CameraImpulseScale(1.0f)
    , CameraReturnSpeed(18.0f)
    , MaximumCameraOffset(18.0f)
    , IdleBreathFrequency(1.25f)
    , MovementStepFrequency(7.5f)
    , TransformInterpSpeed(14.0f)
    , ColorInterpSpeed(18.0f)
    , HitFlashDuration(0.12f)
    , ImpactPulseDuration(0.16f)
    , DeathFallDuration(0.48f)
{
}

FAdaptiveFeedbackTuning FAdaptiveFeedbackTuning::GetSanitized() const
{
    FAdaptiveFeedbackTuning Result;
    Result.FeedbackDisplayDuration = SanitizeRange(
        FeedbackDisplayDuration,
        0.05f,
        1.0f,
        Result.FeedbackDisplayDuration
    );
    Result.bEnableKnockback = bEnableKnockback;
    Result.KnockbackStrength = SanitizeMinimum(
        KnockbackStrength,
        0.0f,
        Result.KnockbackStrength
    );
    Result.HeavyKnockbackMultiplier = SanitizeRange(
        HeavyKnockbackMultiplier,
        1.0f,
        3.0f,
        Result.HeavyKnockbackMultiplier
    );
    Result.BlockedKnockbackMultiplier = SanitizeRange(
        BlockedKnockbackMultiplier,
        0.0f,
        1.0f,
        Result.BlockedKnockbackMultiplier
    );
    Result.KnockbackVerticalVelocity = SanitizeMinimum(
        KnockbackVerticalVelocity,
        0.0f,
        Result.KnockbackVerticalVelocity
    );
    Result.MaximumKnockbackStrength = FMath::Max(
        Result.KnockbackStrength,
        SanitizeMinimum(
            MaximumKnockbackStrength,
            0.0f,
            Result.MaximumKnockbackStrength
        )
    );
    Result.CameraImpulseScale = SanitizeMinimum(
        CameraImpulseScale,
        0.0f,
        Result.CameraImpulseScale
    );
    Result.CameraReturnSpeed = SanitizeMinimum(
        CameraReturnSpeed,
        0.1f,
        Result.CameraReturnSpeed
    );
    Result.MaximumCameraOffset = SanitizeMinimum(
        MaximumCameraOffset,
        0.0f,
        Result.MaximumCameraOffset
    );
    Result.IdleBreathFrequency = SanitizeMinimum(
        IdleBreathFrequency,
        0.1f,
        Result.IdleBreathFrequency
    );
    Result.MovementStepFrequency = SanitizeMinimum(
        MovementStepFrequency,
        0.1f,
        Result.MovementStepFrequency
    );
    Result.TransformInterpSpeed = SanitizeMinimum(
        TransformInterpSpeed,
        0.1f,
        Result.TransformInterpSpeed
    );
    Result.ColorInterpSpeed = SanitizeMinimum(
        ColorInterpSpeed,
        0.1f,
        Result.ColorInterpSpeed
    );
    Result.HitFlashDuration = SanitizeMinimum(
        HitFlashDuration,
        0.0f,
        Result.HitFlashDuration
    );
    Result.ImpactPulseDuration = SanitizeRange(
        ImpactPulseDuration,
        0.05f,
        0.5f,
        Result.ImpactPulseDuration
    );
    Result.DeathFallDuration = SanitizeMinimum(
        DeathFallDuration,
        0.01f,
        Result.DeathFallDuration
    );
    return Result;
}


FAdaptiveTacticalRuntimeTuning::FAdaptiveTacticalRuntimeTuning()
    : EvaluationInterval(0.1f)
    , CombatDecisionInterval(0.85f)
    , RangedCombatDistance(425.0f)
    , DashCombatDistance(900.0f)
    , InterruptDistance(260.0f)
    , RandomSeed(22022)
{
}

FAdaptiveTacticalRuntimeTuning
FAdaptiveTacticalRuntimeTuning::GetSanitized() const
{
    FAdaptiveTacticalRuntimeTuning Result;
    Result.EvaluationInterval = SanitizeMinimum(
        EvaluationInterval,
        0.02f,
        Result.EvaluationInterval
    );
    Result.CombatDecisionInterval = SanitizeMinimum(
        CombatDecisionInterval,
        0.1f,
        Result.CombatDecisionInterval
    );
    Result.RangedCombatDistance = SanitizeMinimum(
        RangedCombatDistance,
        0.0f,
        Result.RangedCombatDistance
    );
    Result.DashCombatDistance = SanitizeMinimum(
        DashCombatDistance,
        0.0f,
        Result.DashCombatDistance
    );
    Result.InterruptDistance = SanitizeMinimum(
        InterruptDistance,
        0.0f,
        Result.InterruptDistance
    );
    Result.RandomSeed = RandomSeed;
    Result.Policy = Policy.GetSanitized();
    Result.Selection = Selection.GetSanitized();
    Result.Repetition = Repetition.GetSanitized();
    Result.RecentOutcome = RecentOutcome.GetSanitized();
    Result.UrgentReactions = UrgentReactions.GetSanitized();
    return Result;
}


FAdaptiveLearningTuning::FAdaptiveLearningTuning()
    : PredictionInfluence(0.40f)
    , MinimumPredictionConfidence(0.40f)
    , PredictionApplicationChance(0.80f)
    , MinimumPredictionSupportingSamples(2)
    , MinimumTrainingSamples(3)
    , RetrainSampleInterval(3)
    , bPersistPlayerPatterns(true)
    , MaximumPersistentSamples(128)
    , RecentActionWindow(6)
    , MinimumRepeatedLightAttacks(3)
    , MaximumLightAttackPatternAdjustment(0.55f)
    , RandomSeed(1337)
{
}

FAdaptiveLearningTuning FAdaptiveLearningTuning::GetSanitized() const
{
    FAdaptiveLearningTuning Result;
    Result.PredictionInfluence = SanitizeRange(
        PredictionInfluence,
        0.0f,
        1.0f,
        Result.PredictionInfluence
    );
    Result.MinimumPredictionConfidence = SanitizeRange(
        MinimumPredictionConfidence,
        0.0f,
        1.0f,
        Result.MinimumPredictionConfidence
    );
    Result.PredictionApplicationChance = SanitizeRange(
        PredictionApplicationChance,
        0.0f,
        1.0f,
        Result.PredictionApplicationChance
    );
    Result.MinimumPredictionSupportingSamples = FMath::Max(
        1,
        MinimumPredictionSupportingSamples
    );
    Result.MinimumTrainingSamples = FMath::Max(1, MinimumTrainingSamples);
    Result.RetrainSampleInterval = FMath::Max(1, RetrainSampleInterval);
    Result.bPersistPlayerPatterns = bPersistPlayerPatterns;
    Result.MaximumPersistentSamples = FMath::Clamp(
        MaximumPersistentSamples,
        16,
        512
    );
    Result.RecentActionWindow = FMath::Clamp(RecentActionWindow, 3, 16);
    Result.MinimumRepeatedLightAttacks = FMath::Clamp(
        MinimumRepeatedLightAttacks,
        2,
        Result.RecentActionWindow
    );
    Result.MaximumLightAttackPatternAdjustment = SanitizeRange(
        MaximumLightAttackPatternAdjustment,
        0.0f,
        0.65f,
        Result.MaximumLightAttackPatternAdjustment
    );
    Result.RandomSeed = RandomSeed;
    Result.Profile = Profile.GetSanitized();
    Result.Outcome = Outcome.GetSanitized();
    return Result;
}


FAdaptiveMatchFlowTuning::FAdaptiveMatchFlowTuning()
    : IntermissionDuration(3.0f)
    , PreRoundCountdownDuration(2.0f)
{
}

FAdaptiveMatchFlowTuning FAdaptiveMatchFlowTuning::GetSanitized() const
{
    FAdaptiveMatchFlowTuning Result;
    Result.IntermissionDuration = SanitizeMinimum(
        IntermissionDuration,
        0.0f,
        Result.IntermissionDuration
    );
    Result.PreRoundCountdownDuration = SanitizeMinimum(
        PreRoundCountdownDuration,
        0.0f,
        Result.PreRoundCountdownDuration
    );
    return Result;
}


UAdaptiveHuntTuningSettings::UAdaptiveHuntTuningSettings()
{
}

const UAdaptiveHuntTuningSettings& UAdaptiveHuntTuningSettings::Get()
{
    return *GetDefault<UAdaptiveHuntTuningSettings>();
}

FName UAdaptiveHuntTuningSettings::GetCategoryName() const
{
    return TEXT("Game");
}
