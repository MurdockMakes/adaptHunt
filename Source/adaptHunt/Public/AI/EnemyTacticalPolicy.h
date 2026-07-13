#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyMovementPolicy.h"
#include "Data/CombatTypes.h"

#include "EnemyTacticalPolicy.generated.h"


/** Coarse intent selected above the individual combat-action utilities. */
UENUM(BlueprintType)
enum class EEnemyTacticalState : uint8
{
    Observe,
    Approach,
    Orbit,
    Pressure,
    Retreat,
    Defend,
    Punish,
    Recover
};


/** Safe, asset-free tuning for deterministic tactical selection. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FEnemyTacticalTuning
{
    GENERATED_BODY()

    FEnemyTacticalTuning();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Resources", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LowStaminaThreshold;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Resources", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float StaminaRecoveryThreshold;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Resources", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LowHealthThreshold;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Commitment", meta = (ClampMin = "0.0"))
    float StateCommitDuration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Commitment", meta = (ClampMin = "0.0"))
    float DistanceHysteresis;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Defense", meta = (ClampMin = "0.0"))
    float DefensiveThreatDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Punish", meta = (ClampMin = "0.0"))
    float PunishWindow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Prediction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PredictionDefendConfidence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Utility", meta = (ClampMin = "0.0", ClampMax = "0.35"))
    float MaxUtilityAdjustment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Sequence", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SequenceStartChance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Sequence", meta = (ClampMin = "0.0"))
    float SequenceWindow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Sequence", meta = (ClampMin = "0.0"))
    float SequenceExitDistance;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tactics|Spacing", meta = (ClampMin = "0.0"))
    float OrbitDirectionCommitDuration;

    FEnemyTacticalTuning GetSanitized() const;
};


/** Immutable inputs for a single tactical-policy evaluation. */
struct ADAPTHUNT_API FEnemyTacticalContext
{
    float DistanceToTarget = MAX_flt;
    float RetreatDistance = 180.0f;
    float PreferredDistance = 425.0f;
    float OrbitDistance = 625.0f;
    float MeleeDistance = 190.0f;
    float EnemyHealthNormalized = 1.0f;
    float EnemyStaminaNormalized = 1.0f;
    float TimeSincePlayerAttack = MAX_flt;
    float PredictionConfidence = 0.0f;
    EPlayerCombatAction RecentPlayerAction = EPlayerCombatAction::None;
    EEnemyCombatAction RecentEnemyAction = EEnemyCombatAction::None;
    EPlayerCombatAction PredictedPlayerAction = EPlayerCombatAction::None;
    bool bHasLineOfSight = true;
    bool bPlayerAttacking = false;
    bool bTargetRecentlyHealing = false;
    bool bAttackSequenceActive = false;
    bool bLightAttackAvailable = false;
    bool bHeavyAttackAvailable = false;
    bool bProjectileAvailable = false;
    bool bDashAvailable = false;
    bool bBlockAvailable = false;
    bool bDodgeAvailable = false;
    bool bInterruptAvailable = false;

    bool HasAnyOffensiveAction() const;
    bool HasAnyDefensiveAction() const;
    bool IsActionAvailable(EEnemyCombatAction Action) const;
};


/** Runtime data for the deliberately limited two-hit close-range sequence. */
struct ADAPTHUNT_API FEnemyAttackSequenceState
{
    EEnemyCombatAction FollowUpAction = EEnemyCombatAction::None;
    double ExpiresAtTime = 0.0;

    void Start(EEnemyCombatAction InFollowUpAction, double InExpiresAtTime);
    void Reset();
    bool IsActive() const;
};


/** Side-effect-free state, spacing, weight, and sequence policy. */
struct ADAPTHUNT_API FEnemyTacticalPolicy
{
    static EEnemyTacticalState SelectState(
        const FEnemyTacticalContext& Context,
        EEnemyTacticalState CurrentState,
        float TimeInCurrentState,
        const FEnemyTacticalTuning& Tuning
    );

    static EEnemyLocomotionIntent SelectMovementIntent(
        const FEnemyTacticalContext& Context,
        EEnemyTacticalState TacticalState,
        EEnemyLocomotionIntent BaseIntent,
        bool bOrbitRight
    );

    static float GetUtilityModifier(
        EEnemyTacticalState TacticalState,
        EEnemyCombatAction Action,
        float MaximumAdjustment
    );

    static bool IsCommittedPlayerAttack(
        EPlayerCombatAction Action,
        ECombatActionPhase Phase
    );

    static EEnemyCombatAction SelectSequenceFollowUp(float RandomValue);

    static bool ShouldContinueSequence(
        const FEnemyAttackSequenceState& Sequence,
        const FEnemyTacticalContext& Context,
        EEnemyTacticalState TacticalState,
        double CurrentTime,
        const FEnemyTacticalTuning& Tuning
    );

    static bool IsProjectilePermitted(
        bool bHasLineOfSight,
        bool bExecutorAvailable
    );

    static bool IsOffensiveAction(EEnemyCombatAction Action);
    static bool IsDefensiveAction(EEnemyCombatAction Action);
};
