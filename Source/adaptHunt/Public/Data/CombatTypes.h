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


/** Small value-validation helpers shared by gameplay, learning, and tests. */
namespace AdaptiveCombat
{
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
