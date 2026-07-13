#pragma once

#include "CoreMinimal.h"

#include "CombatFeedbackTypes.generated.h"


/** Short-lived, presentation-only results shown by the greybox slice. */
UENUM(BlueprintType)
enum class ECombatFeedbackCue : uint8
{
    None UMETA(DisplayName = "None"),
    LightAttack UMETA(DisplayName = "Light Attack"),
    HeavyAttack UMETA(DisplayName = "Heavy Attack"),
    Hit UMETA(DisplayName = "Hit"),
    Blocked UMETA(DisplayName = "Blocked"),
    Dodged UMETA(DisplayName = "Dodged"),
    Miss UMETA(DisplayName = "Miss"),
    Death UMETA(DisplayName = "Death")
};


/** Collision-free scale and color overlay evaluated for one impact pulse. */
USTRUCT(BlueprintType)
struct ADAPTHUNT_API FCombatFeedbackPulse
{
    GENERATED_BODY()

    FCombatFeedbackPulse()
        : ScaleMultiplier(FVector::OneVector)
        , TintColor(FLinearColor::White)
        , TintStrength(0.0f)
    {
    }

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Feedback")
    FVector ScaleMultiplier;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Feedback")
    FLinearColor TintColor;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Feedback")
    float TintStrength;
};


/** Pure feedback math shared by runtime code and deterministic tests. */
namespace AdaptiveCombatFeedback
{
ADAPTHUNT_API int32 GetCuePriority(ECombatFeedbackCue Cue);

ADAPTHUNT_API FCombatFeedbackPulse EvaluatePulse(
    ECombatFeedbackCue Cue,
    float NormalizedElapsedTime
);

ADAPTHUNT_API FVector ResolveKnockbackVelocity(
    const FVector& TargetLocation,
    const FVector& InstigatorLocation,
    const FVector& FallbackForward,
    float HorizontalStrength,
    float VerticalVelocity,
    float MaximumHorizontalStrength
);

ADAPTHUNT_API FVector ResolveCameraImpulse(ECombatFeedbackCue Cue);

ADAPTHUNT_API FString GetCueLabel(ECombatFeedbackCue Cue);

ADAPTHUNT_API FLinearColor GetCueColor(ECombatFeedbackCue Cue);
}
