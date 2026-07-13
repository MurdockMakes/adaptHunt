#pragma once

#include "CoreMinimal.h"
#include "Data/CombatTypes.h"

#include "GreyboxPresentationTypes.generated.h"


/** Identifies which neutral palette a greybox presentation should use. */
UENUM(BlueprintType)
enum class EGreyboxPresentationRole : uint8
{
    Player UMETA(DisplayName = "Player"),
    Enemy UMETA(DisplayName = "Enemy")
};


/** Action vocabulary used only by the presentation layer. */
UENUM(BlueprintType)
enum class EGreyboxPresentationAction : uint8
{
    None UMETA(DisplayName = "None"),
    LightAttack UMETA(DisplayName = "Light Attack"),
    HeavyAttack UMETA(DisplayName = "Heavy Attack"),
    ProjectileAttack UMETA(DisplayName = "Projectile Attack"),
    DashAttack UMETA(DisplayName = "Dash Attack"),
    InterruptAttack UMETA(DisplayName = "Interrupt Attack"),
    Block UMETA(DisplayName = "Block"),
    DodgeLeft UMETA(DisplayName = "Dodge Left"),
    DodgeRight UMETA(DisplayName = "Dodge Right"),
    DodgeBackward UMETA(DisplayName = "Dodge Backward"),
    Dodge UMETA(DisplayName = "Dodge"),
    Heal UMETA(DisplayName = "Heal")
};


/**
 * High-level hooks that a future skeletal mesh or montage Blueprint can
 * consume without making animation authoritative for gameplay.
 */
UENUM(BlueprintType)
enum class EGreyboxAnimationCue : uint8
{
    Neutral UMETA(DisplayName = "Neutral"),
    Locomotion UMETA(DisplayName = "Locomotion"),
    LightWindup UMETA(DisplayName = "Light Windup"),
    LightStrike UMETA(DisplayName = "Light Strike"),
    HeavyWindup UMETA(DisplayName = "Heavy Windup"),
    HeavyStrike UMETA(DisplayName = "Heavy Strike"),
    ProjectileWindup UMETA(DisplayName = "Projectile Windup"),
    ProjectileRelease UMETA(DisplayName = "Projectile Release"),
    DashWindup UMETA(DisplayName = "Dash Windup"),
    DashBurst UMETA(DisplayName = "Dash Burst"),
    InterruptWindup UMETA(DisplayName = "Interrupt Windup"),
    InterruptStrike UMETA(DisplayName = "Interrupt Strike"),
    Block UMETA(DisplayName = "Block"),
    Dodge UMETA(DisplayName = "Dodge"),
    Heal UMETA(DisplayName = "Heal"),
    HitReaction UMETA(DisplayName = "Hit Reaction"),
    Death UMETA(DisplayName = "Death")
};


/** A relative, collision-free pose evaluated from authoritative gameplay. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FGreyboxPresentationPose
{
    GENERATED_BODY()

    FGreyboxPresentationPose()
        : LocationOffset(FVector::ZeroVector)
        , RotationOffset(FRotator::ZeroRotator)
        , ScaleMultiplier(FVector::OneVector)
        , Color(FLinearColor::White)
    {
    }

    UPROPERTY(BlueprintReadOnly, Category = "Presentation")
    FVector LocationOffset;

    UPROPERTY(BlueprintReadOnly, Category = "Presentation")
    FRotator RotationOffset;

    UPROPERTY(BlueprintReadOnly, Category = "Presentation")
    FVector ScaleMultiplier;

    UPROPERTY(BlueprintReadOnly, Category = "Presentation")
    FLinearColor Color;
};


/** Side-effect-free presentation policy used by runtime code and tests. */
namespace AdaptiveGreyboxPresentation
{
ADAPTHUNT_API EGreyboxPresentationAction FromPlayerAction(
    EPlayerCombatAction Action
);

ADAPTHUNT_API EGreyboxPresentationAction FromEnemyAction(
    EEnemyCombatAction Action
);

ADAPTHUNT_API EGreyboxAnimationCue ResolveCue(
    ECombatActionPhase Phase,
    EGreyboxPresentationAction Action,
    bool bIsMoving
);

ADAPTHUNT_API bool IsAttackAction(EGreyboxPresentationAction Action);

/**
 * Evaluates a visible-mesh offset only. Inputs are sanitized and cannot
 * affect collision, movement, phase timing, or damage.
 */
ADAPTHUNT_API FGreyboxPresentationPose EvaluatePose(
    EGreyboxAnimationCue Cue,
    EGreyboxPresentationAction Action,
    const FLinearColor& NeutralColor,
    float NormalizedForwardSpeed,
    float NormalizedRightSpeed,
    float MotionWave,
    float StateAlpha,
    bool bPredictionCue
);

ADAPTHUNT_API FLinearColor ApplyFeedbackColor(
    const FLinearColor& BaseColor,
    bool bHitFlash,
    bool bInvulnerable,
    float PulseAlpha
);
}
