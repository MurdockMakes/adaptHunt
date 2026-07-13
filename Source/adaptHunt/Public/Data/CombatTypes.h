#pragma once

#include "CoreMinimal.h"

#include "CombatTypes.generated.h"


UENUM(BlueprintType)
enum class EPlayerCombatAction : uint8
{
    None UMETA(DisplayName = "None"),

    LightAttack UMETA(DisplayName = "Light Attack"),
    HeavyAttack UMETA(DisplayName = "Heavy Attack"),

    DodgeLeft UMETA(DisplayName = "Dodge Left"),
    DodgeRight UMETA(DisplayName = "Dodge Right"),
    DodgeBackward UMETA(DisplayName = "Dodge Backward"),

    Block UMETA(DisplayName = "Block"),
    Heal UMETA(DisplayName = "Heal")
};


UENUM(BlueprintType)
enum class EEnemyCombatAction : uint8
{
    None UMETA(DisplayName = "None"),

    MoveTowardPlayer UMETA(DisplayName = "Move Toward Player"),
    MoveAwayFromPlayer UMETA(DisplayName = "Move Away From Player"),

    StrafeLeft UMETA(DisplayName = "Strafe Left"),
    StrafeRight UMETA(DisplayName = "Strafe Right"),

    LightAttack UMETA(DisplayName = "Light Attack"),
    HeavyAttack UMETA(DisplayName = "Heavy Attack"),

    ProjectileAttack UMETA(DisplayName = "Projectile Attack"),
    DashAttack UMETA(DisplayName = "Dash Attack"),

    Block UMETA(DisplayName = "Block"),
    Dodge UMETA(DisplayName = "Dodge"),

    InterruptHeal UMETA(DisplayName = "Interrupt Heal")
};


UENUM(BlueprintType)
enum class ECombatDistanceCategory : uint8
{
    Close UMETA(DisplayName = "Close"),
    Medium UMETA(DisplayName = "Medium"),
    Far UMETA(DisplayName = "Far")
};


UENUM(BlueprintType)
enum class ERelativePlayerPosition : uint8
{
    Front UMETA(DisplayName = "Front"),
    Left UMETA(DisplayName = "Left"),
    Right UMETA(DisplayName = "Right"),
    Behind UMETA(DisplayName = "Behind")
};


/** Shared lifecycle used by player and enemy combat executors. */
UENUM(BlueprintType)
enum class ECombatActionPhase : uint8
{
    Idle UMETA(DisplayName = "Idle"),
    Windup UMETA(DisplayName = "Windup"),
    Active UMETA(DisplayName = "Active"),
    Recovery UMETA(DisplayName = "Recovery"),
    Blocking UMETA(DisplayName = "Blocking"),
    Dodging UMETA(DisplayName = "Dodging"),
    Staggered UMETA(DisplayName = "Staggered"),
    Dead UMETA(DisplayName = "Dead")
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FCombatActionPhaseChangedEvent,
    ECombatActionPhase,
    PreviousPhase,
    ECombatActionPhase,
    NewPhase
);


/** Configurable timing for a conventional windup/active/recovery action. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FCombatActionTiming
{
    GENERATED_BODY()

    FCombatActionTiming()
        : WindupDuration(0.1f)
        , ActiveDuration(0.05f)
        , RecoveryDuration(0.2f)
    {
    }

    FCombatActionTiming(
        const float InWindupDuration,
        const float InActiveDuration,
        const float InRecoveryDuration
    )
        : WindupDuration(InWindupDuration)
        , ActiveDuration(InActiveDuration)
        , RecoveryDuration(InRecoveryDuration)
    {
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Timing", meta = (ClampMin = "0.0"))
    float WindupDuration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Timing", meta = (ClampMin = "0.0"))
    float ActiveDuration;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Timing", meta = (ClampMin = "0.0"))
    float RecoveryDuration;

    FCombatActionTiming GetSanitized() const
    {
        return FCombatActionTiming(
            FMath::IsFinite(WindupDuration)
                ? FMath::Max(0.0f, WindupDuration)
                : 0.0f,
            FMath::IsFinite(ActiveDuration)
                ? FMath::Max(0.0f, ActiveDuration)
                : 0.0f,
            FMath::IsFinite(RecoveryDuration)
                ? FMath::Max(0.0f, RecoveryDuration)
                : 0.0f
        );
    }

    float GetTotalDuration() const
    {
        const FCombatActionTiming SafeTiming = GetSanitized();
        return SafeTiming.WindupDuration
            + SafeTiming.ActiveDuration
            + SafeTiming.RecoveryDuration;
    }
};


/**
 * Small deterministic guard shared by both executors. It gives every commit a
 * new generation and allows its active effect to be consumed at most once.
 */
USTRUCT()
struct ADAPTHUNT_API FCombatActionRuntimeState
{
    GENERATED_BODY()

    FCombatActionRuntimeState()
        : Phase(ECombatActionPhase::Idle)
        , Generation(0)
        , bActiveEffectConsumed(false)
    {
    }

    ECombatActionPhase Begin(const ECombatActionPhase InitialPhase)
    {
        ++Generation;
        bActiveEffectConsumed = false;
        Phase = InitialPhase;
        return Phase;
    }

    ECombatActionPhase TransitionTo(const ECombatActionPhase NewPhase)
    {
        Phase = NewPhase;
        return Phase;
    }

    void Reset(const ECombatActionPhase NewPhase = ECombatActionPhase::Idle)
    {
        ++Generation;
        bActiveEffectConsumed = false;
        Phase = NewPhase;
    }

    bool TryConsumeActiveEffect()
    {
        if (Phase != ECombatActionPhase::Active
            || bActiveEffectConsumed)
        {
            return false;
        }

        bActiveEffectConsumed = true;
        return true;
    }

    ECombatActionPhase GetPhase() const
    {
        return Phase;
    }

    uint32 GetGeneration() const
    {
        return Generation;
    }

private:
    UPROPERTY()
    ECombatActionPhase Phase;

    UPROPERTY()
    uint32 Generation;

    UPROPERTY()
    bool bActiveEffectConsumed;
};


/** Small value-validation helpers shared by gameplay, learning, and tests. */
namespace AdaptiveCombat
{
inline bool IsCommittedPhase(const ECombatActionPhase Phase)
{
    return Phase != ECombatActionPhase::Idle
        && Phase != ECombatActionPhase::Dead;
}

inline bool IsMovementAllowed(const ECombatActionPhase Phase)
{
    return Phase == ECombatActionPhase::Idle
        || Phase == ECombatActionPhase::Dodging;
}

inline bool IsRotationAllowed(const ECombatActionPhase Phase)
{
    return Phase == ECombatActionPhase::Idle
        || Phase == ECombatActionPhase::Windup
        || Phase == ECombatActionPhase::Recovery
        || Phase == ECombatActionPhase::Blocking;
}

inline bool IsTrackablePlayerAction(const EPlayerCombatAction Action)
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

inline bool IsKnownPlayerAction(const EPlayerCombatAction Action)
{
    return Action == EPlayerCombatAction::None
        || IsTrackablePlayerAction(Action);
}

inline bool IsKnownEnemyAction(const EEnemyCombatAction Action)
{
    switch (Action)
    {
    case EEnemyCombatAction::None:
    case EEnemyCombatAction::MoveTowardPlayer:
    case EEnemyCombatAction::MoveAwayFromPlayer:
    case EEnemyCombatAction::StrafeLeft:
    case EEnemyCombatAction::StrafeRight:
    case EEnemyCombatAction::LightAttack:
    case EEnemyCombatAction::HeavyAttack:
    case EEnemyCombatAction::ProjectileAttack:
    case EEnemyCombatAction::DashAttack:
    case EEnemyCombatAction::Block:
    case EEnemyCombatAction::Dodge:
    case EEnemyCombatAction::InterruptHeal:
        return true;
    default:
        return false;
    }
}

inline bool IsUtilityEnemyAction(const EEnemyCombatAction Action)
{
    return Action == EEnemyCombatAction::LightAttack
        || Action == EEnemyCombatAction::HeavyAttack
        || Action == EEnemyCombatAction::ProjectileAttack
        || Action == EEnemyCombatAction::DashAttack
        || Action == EEnemyCombatAction::Block
        || Action == EEnemyCombatAction::Dodge
        || Action == EEnemyCombatAction::InterruptHeal;
}

inline bool IsKnownDistanceCategory(const ECombatDistanceCategory Category)
{
    return Category == ECombatDistanceCategory::Close
        || Category == ECombatDistanceCategory::Medium
        || Category == ECombatDistanceCategory::Far;
}

inline bool IsKnownRelativePosition(const ERelativePlayerPosition Position)
{
    return Position == ERelativePlayerPosition::Front
        || Position == ERelativePlayerPosition::Left
        || Position == ERelativePlayerPosition::Right
        || Position == ERelativePlayerPosition::Behind;
}
}
