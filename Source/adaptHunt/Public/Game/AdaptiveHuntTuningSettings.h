#pragma once

#include "CoreMinimal.h"
#include "AI/AdaptiveCounterOutcome.h"
#include "AI/AdaptiveTacticalProfile.h"
#include "AI/EnemyDecisionPolicy.h"
#include "AI/EnemyTacticalPolicy.h"
#include "Data/CombatTypes.h"
#include "Engine/DeveloperSettings.h"

#include "AdaptiveHuntTuningSettings.generated.h"


/** Player and enemy locomotion defaults used by the native vertical slice. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveMovementTuning
{
    GENERATED_BODY()

    FAdaptiveMovementTuning();

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerMaxWalkSpeed;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerJumpVelocity;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerAcceleration;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerBrakingDeceleration;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerGroundFriction;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerBrakingFriction;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerRotationRateYaw;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PlayerAirControl;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerMinimumAnalogWalkSpeed;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Spacing", meta = (ClampMin = "0.0"))
    float EnemyPreferredRange;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Spacing", meta = (ClampMin = "0.0"))
    float EnemyRetreatRange;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Spacing", meta = (ClampMin = "0.0"))
    float EnemyOrbitRange;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Navigation", meta = (ClampMin = "0.0"))
    float EnemyAcceptanceRadius;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Movement", meta = (ClampMin = "0.0"))
    float EnemyAcceleration;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Movement", meta = (ClampMin = "0.0"))
    float EnemyTurnSpeed;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Movement", meta = (ClampMin = "0.0"))
    float EnemyMovementSpeed;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Navigation", meta = (ClampMin = "0.02"))
    float EnemyNavigationRefreshInterval;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Navigation", meta = (ClampMin = "0.0"))
    float EnemyOrbitStepDistance;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Fallback", meta = (ClampMin = "1.0"))
    float EnemyCollisionProbeDistance;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Recovery", meta = (ClampMin = "0.05"))
    float EnemyStuckTimeout;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Recovery", meta = (ClampMin = "0.0"))
    float EnemyMinimumProgressDistance;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy|Recovery", meta = (ClampMin = "0.05"))
    float EnemyStuckRecoveryDuration;

    FAdaptiveMovementTuning GetSanitized() const;
};


/** Timings only; damage, resources, cooldowns, and execution remain owned by combat components. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveActionTimingTuning
{
    GENERATED_BODY()

    FAdaptiveActionTimingTuning();

    UPROPERTY(EditAnywhere, Config, Category = "Player")
    FCombatActionTiming PlayerLightAttack;

    UPROPERTY(EditAnywhere, Config, Category = "Player")
    FCombatActionTiming PlayerHeavyAttack;

    UPROPERTY(EditAnywhere, Config, Category = "Player")
    FCombatActionTiming PlayerHeal;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerBlockRecoveryDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerDodgeInvulnerabilityDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerDodgeRecoveryDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0"))
    float PlayerStaggerDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Player", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float PlayerInputBufferDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy")
    FCombatActionTiming EnemyLightAttack;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy")
    FCombatActionTiming EnemyHeavyAttack;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy")
    FCombatActionTiming EnemyProjectileAttack;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy")
    FCombatActionTiming EnemyDashAttack;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy")
    FCombatActionTiming EnemyInterruptHeal;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy", meta = (ClampMin = "0.0"))
    float EnemyBlockDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy", meta = (ClampMin = "0.0"))
    float EnemyBlockRecoveryDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy", meta = (ClampMin = "0.0"))
    float EnemyDodgeInvulnerabilityDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy", meta = (ClampMin = "0.0"))
    float EnemyDodgeRecoveryDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Enemy", meta = (ClampMin = "0.0"))
    float EnemyStaggerDuration;

    FAdaptiveActionTimingTuning GetSanitized() const;
};


/** Spring-arm values kept separate from pawn input and camera ownership. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveCameraTuning
{
    GENERATED_BODY()

    FAdaptiveCameraTuning();

    UPROPERTY(EditAnywhere, Config, Category = "Spring Arm")
    bool bEnablePositionLag;

    UPROPERTY(EditAnywhere, Config, Category = "Spring Arm", meta = (ClampMin = "0.0"))
    float PositionLagSpeed;

    UPROPERTY(EditAnywhere, Config, Category = "Spring Arm", meta = (ClampMin = "0.0"))
    float PositionLagMaxDistance;

    UPROPERTY(EditAnywhere, Config, Category = "Spring Arm")
    bool bEnableRotationSmoothing;

    UPROPERTY(EditAnywhere, Config, Category = "Spring Arm", meta = (ClampMin = "0.0"))
    float RotationSmoothingSpeed;

    UPROPERTY(EditAnywhere, Config, Category = "Framing", meta = (ClampMin = "0.0"))
    float TargetArmLength;

    UPROPERTY(EditAnywhere, Config, Category = "Framing")
    float BoomHeight;

    UPROPERTY(EditAnywhere, Config, Category = "Framing")
    float SocketSideOffset;

    UPROPERTY(EditAnywhere, Config, Category = "Framing")
    float SocketHeightOffset;

    FAdaptiveCameraTuning GetSanitized() const;
};


/** Readability, knockback, camera impulse, and procedural presentation defaults. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveFeedbackTuning
{
    GENERATED_BODY()

    FAdaptiveFeedbackTuning();

    UPROPERTY(EditAnywhere, Config, Category = "Cues", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float FeedbackDisplayDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Knockback")
    bool bEnableKnockback;

    UPROPERTY(EditAnywhere, Config, Category = "Knockback", meta = (ClampMin = "0.0"))
    float KnockbackStrength;

    UPROPERTY(EditAnywhere, Config, Category = "Knockback", meta = (ClampMin = "1.0", ClampMax = "3.0"))
    float HeavyKnockbackMultiplier;

    UPROPERTY(EditAnywhere, Config, Category = "Knockback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlockedKnockbackMultiplier;

    UPROPERTY(EditAnywhere, Config, Category = "Knockback", meta = (ClampMin = "0.0"))
    float KnockbackVerticalVelocity;

    UPROPERTY(EditAnywhere, Config, Category = "Knockback", meta = (ClampMin = "0.0"))
    float MaximumKnockbackStrength;

    UPROPERTY(EditAnywhere, Config, Category = "Camera", meta = (ClampMin = "0.0"))
    float CameraImpulseScale;

    UPROPERTY(EditAnywhere, Config, Category = "Camera", meta = (ClampMin = "0.1"))
    float CameraReturnSpeed;

    UPROPERTY(EditAnywhere, Config, Category = "Camera", meta = (ClampMin = "0.0"))
    float MaximumCameraOffset;

    UPROPERTY(EditAnywhere, Config, Category = "Presentation|Motion", meta = (ClampMin = "0.1"))
    float IdleBreathFrequency;

    UPROPERTY(EditAnywhere, Config, Category = "Presentation|Motion", meta = (ClampMin = "0.1"))
    float MovementStepFrequency;

    UPROPERTY(EditAnywhere, Config, Category = "Presentation|Motion", meta = (ClampMin = "0.1"))
    float TransformInterpSpeed;

    UPROPERTY(EditAnywhere, Config, Category = "Presentation|Color", meta = (ClampMin = "0.1"))
    float ColorInterpSpeed;

    UPROPERTY(EditAnywhere, Config, Category = "Presentation|Cues", meta = (ClampMin = "0.0"))
    float HitFlashDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Presentation|Cues", meta = (ClampMin = "0.05", ClampMax = "0.5"))
    float ImpactPulseDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Presentation|Cues", meta = (ClampMin = "0.01"))
    float DeathFallDuration;

    FAdaptiveFeedbackTuning GetSanitized() const;
};


/** Tactical evaluation cadence, range gates, and deterministic policy tuning. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveTacticalRuntimeTuning
{
    GENERATED_BODY()

    FAdaptiveTacticalRuntimeTuning();

    UPROPERTY(EditAnywhere, Config, Category = "Cadence", meta = (ClampMin = "0.02"))
    float EvaluationInterval;

    UPROPERTY(EditAnywhere, Config, Category = "Cadence", meta = (ClampMin = "0.1"))
    float CombatDecisionInterval;

    UPROPERTY(EditAnywhere, Config, Category = "Range Gates", meta = (ClampMin = "0.0"))
    float RangedCombatDistance;

    UPROPERTY(EditAnywhere, Config, Category = "Range Gates", meta = (ClampMin = "0.0"))
    float DashCombatDistance;

    UPROPERTY(EditAnywhere, Config, Category = "Range Gates", meta = (ClampMin = "0.0"))
    float InterruptDistance;

    UPROPERTY(EditAnywhere, Config, Category = "Determinism")
    int32 RandomSeed;

    UPROPERTY(EditAnywhere, Config, Category = "Policy")
    FEnemyTacticalTuning Policy;

    UPROPERTY(EditAnywhere, Config, Category = "Selection")
    FEnemyActionSelectionTuning Selection;

    UPROPERTY(EditAnywhere, Config, Category = "Repetition")
    FEnemyActionRepetitionTuning Repetition;

    UPROPERTY(EditAnywhere, Config, Category = "Recent Outcome")
    FEnemyRecentOutcomeTuning RecentOutcome;

    UPROPERTY(EditAnywhere, Config, Category = "Urgent Reactions")
    FEnemyUrgentDecisionTuning UrgentReactions;

    FAdaptiveTacticalRuntimeTuning GetSanitized() const;
};


/** Predictor gate, explainable profile, and secondary outcome-learning tuning. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveLearningTuning
{
    GENERATED_BODY()

    FAdaptiveLearningTuning();

    UPROPERTY(EditAnywhere, Config, Category = "Prediction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PredictionInfluence;

    UPROPERTY(EditAnywhere, Config, Category = "Prediction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumPredictionConfidence;

    UPROPERTY(EditAnywhere, Config, Category = "Prediction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PredictionApplicationChance;

    UPROPERTY(EditAnywhere, Config, Category = "Prediction", meta = (ClampMin = "1"))
    int32 MinimumPredictionSupportingSamples;

    UPROPERTY(EditAnywhere, Config, Category = "Training", meta = (ClampMin = "1"))
    int32 MinimumTrainingSamples;

    UPROPERTY(EditAnywhere, Config, Category = "Training", meta = (ClampMin = "1"))
    int32 RetrainSampleInterval;

    /** Keeps only compact committed-action patterns in the local save slot. */
    UPROPERTY(EditAnywhere, Config, Category = "Persistence")
    bool bPersistPlayerPatterns;

    /** FIFO cap prevents old play styles from dominating forever. */
    UPROPERTY(EditAnywhere, Config, Category = "Persistence", meta = (ClampMin = "16", ClampMax = "512"))
    int32 MaximumPersistentSamples;

    UPROPERTY(EditAnywhere, Config, Category = "Pattern Response", meta = (ClampMin = "3", ClampMax = "16"))
    int32 RecentActionWindow;

    UPROPERTY(EditAnywhere, Config, Category = "Pattern Response", meta = (ClampMin = "2", ClampMax = "16"))
    int32 MinimumRepeatedLightAttacks;

    UPROPERTY(EditAnywhere, Config, Category = "Pattern Response", meta = (ClampMin = "0.0", ClampMax = "0.65"))
    float MaximumLightAttackPatternAdjustment;

    UPROPERTY(EditAnywhere, Config, Category = "Determinism")
    int32 RandomSeed;

    UPROPERTY(EditAnywhere, Config, Category = "Profile")
    FAdaptiveTacticalProfileTuning Profile;

    UPROPERTY(EditAnywhere, Config, Category = "Outcome")
    FAdaptiveCounterEffectivenessTuning Outcome;

    FAdaptiveLearningTuning GetSanitized() const;
};


/** Match presentation timing; progression rules remain fixed at three rounds. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FAdaptiveMatchFlowTuning
{
    GENERATED_BODY()

    FAdaptiveMatchFlowTuning();

    UPROPERTY(EditAnywhere, Config, Category = "Timing", meta = (ClampMin = "0.0"))
    float IntermissionDuration;

    UPROPERTY(EditAnywhere, Config, Category = "Timing", meta = (ClampMin = "0.0"))
    float PreRoundCountdownDuration;

    FAdaptiveMatchFlowTuning GetSanitized() const;
};


/**
 * Asset-free project-wide tuning catalog.
 *
 * Values are editable under Project Settings > Game > Adaptive Hunt while
 * native constructors retain these safe C++ defaults when no config exists.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Adaptive Hunt"))
class ADAPTHUNT_API UAdaptiveHuntTuningSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UAdaptiveHuntTuningSettings();

    static const UAdaptiveHuntTuningSettings& Get();

    virtual FName GetCategoryName() const override;

    UPROPERTY(EditAnywhere, Config, Category = "Movement")
    FAdaptiveMovementTuning Movement;

    UPROPERTY(EditAnywhere, Config, Category = "Action Timing")
    FAdaptiveActionTimingTuning ActionTiming;

    UPROPERTY(EditAnywhere, Config, Category = "Camera")
    FAdaptiveCameraTuning Camera;

    UPROPERTY(EditAnywhere, Config, Category = "Feedback")
    FAdaptiveFeedbackTuning Feedback;

    UPROPERTY(EditAnywhere, Config, Category = "Tactical AI")
    FAdaptiveTacticalRuntimeTuning Tactical;

    UPROPERTY(EditAnywhere, Config, Category = "Adaptation")
    FAdaptiveLearningTuning Adaptation;

    UPROPERTY(EditAnywhere, Config, Category = "Match Flow")
    FAdaptiveMatchFlowTuning MatchFlow;
};
