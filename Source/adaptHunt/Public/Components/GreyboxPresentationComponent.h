#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Presentation/CombatFeedbackTypes.h"
#include "Presentation/GreyboxPresentationTypes.h"

#include "GreyboxPresentationComponent.generated.h"

class UCombatComponent;
class UEnemyCombatComponent;
class UEnemyDecisionComponent;
class UHealthComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGreyboxAnimationCueEvent,
    EGreyboxAnimationCue,
    Cue
);

/**
 * Collision-free, code-driven animation and telegraphing for greybox bodies.
 *
 * The component observes authoritative locomotion, combat, resource, and
 * death state. It only changes the assigned visible mesh's relative transform
 * and material parameter, so disabling it cannot change gameplay results.
 */
UCLASS(ClassGroup = (Presentation), meta = (BlueprintSpawnableComponent))
class ADAPTHUNT_API UGreyboxPresentationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGreyboxPresentationComponent();

    /** Assigns the collision-free mesh that receives presentation offsets. */
    void SetPresentationMesh(UMeshComponent* NewPresentationMesh);

    void SetPresentationRole(EGreyboxPresentationRole NewRole);

    UFUNCTION(BlueprintCallable, Category = "Presentation|Greybox")
    void SetPresentationEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Presentation|Greybox")
    bool IsPresentationEnabled() const;

    /** Immediately restores the captured neutral transform and neutral color. */
    UFUNCTION(BlueprintCallable, Category = "Presentation|Greybox")
    void ResetPresentation();

    UFUNCTION(BlueprintPure, Category = "Presentation|Greybox")
    EGreyboxPresentationRole GetPresentationRole() const;

    UFUNCTION(BlueprintPure, Category = "Presentation|Greybox")
    EGreyboxAnimationCue GetCurrentCue() const;

    UFUNCTION(BlueprintPure, Category = "Presentation|Greybox")
    FLinearColor GetNeutralColor() const;

    UMeshComponent* GetPresentationMesh() const;

    /** Adds a short collision-free scale/color pulse for a resolved result. */
    void TriggerFeedbackCue(ECombatFeedbackCue Cue);

    ECombatFeedbackCue GetActiveFeedbackCue() const;

    /** Enables the optional prediction-informed enemy color accent. */
    UFUNCTION(BlueprintCallable, Category = "Presentation|Greybox|Debug")
    void SetPredictionDebugCueEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Presentation|Greybox|Debug")
    bool IsPredictionDebugCueEnabled() const;

    /** Optional montage/animation Blueprint hook for future skeletal bodies. */
    UPROPERTY(BlueprintAssignable, Category = "Presentation|Greybox|Events")
    FGreyboxAnimationCueEvent OnAnimationCue;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

private:
    void CacheObservedComponents();
    void BindObservedEvents();
    void UnbindObservedEvents();
    void CaptureNeutralState();
    void CreatePresentationMaterial();
    void ApplyMaterialColor(const FLinearColor& Color);
    void EmitCue(EGreyboxAnimationCue Cue);
    void UpdateCueFromState(bool bIsMoving);

    void HandlePlayerActionCommitted(
        UCombatComponent* Component,
        EPlayerCombatAction Action
    );
    void HandleEnemyActionCommitted(
        UEnemyCombatComponent* Component,
        EEnemyCombatAction Action
    );

    UFUNCTION()
    void HandleActionPhaseChanged(
        ECombatActionPhase PreviousPhase,
        ECombatActionPhase NewPhase
    );

    void HandleHealthChanged(
        UHealthComponent* Component,
        float OldHealth,
        float NewHealth
    );
    void HandleDeath(UHealthComponent* Component, AActor* DamageCauser);

    UPROPERTY(Transient)
    TObjectPtr<UMeshComponent> PresentationMesh;

    UPROPERTY(Transient)
    TObjectPtr<UCombatComponent> PlayerCombatComponent;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyDecisionComponent> EnemyDecisionComponent;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> HealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> OriginalMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> PresentationMaterial;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox")
    bool bPresentationEnabled;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox")
    EGreyboxPresentationRole PresentationRole;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox|Color")
    FLinearColor PlayerNeutralColor;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox|Color")
    FLinearColor EnemyNeutralColor;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox|Motion", meta = (ClampMin = "0.1"))
    float IdleBreathFrequency;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox|Motion", meta = (ClampMin = "0.1"))
    float MovementStepFrequency;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox|Motion", meta = (ClampMin = "0.1"))
    float TransformInterpSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox|Color", meta = (ClampMin = "0.1"))
    float ColorInterpSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox|Feedback", meta = (ClampMin = "0.0"))
    float HitFlashDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox|Feedback", meta = (ClampMin = "0.05", ClampMax = "0.5"))
    float ImpactPulseDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox|Feedback", meta = (ClampMin = "0.01"))
    float DeathFallDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Presentation|Greybox|Debug")
    bool bPredictionDebugCueEnabled;

    FTransform NeutralRelativeTransform;
    FLinearColor CurrentColor;
    ECombatActionPhase ObservedPhase;
    EGreyboxPresentationAction ObservedAction;
    EGreyboxAnimationCue CurrentCue;
    float AnimationTime;
    float MovementPhase;
    float PhaseElapsedTime;
    float RemainingHitFlashTime;
    float FeedbackPulseElapsedTime;
    float RemainingFeedbackPulseTime;
    ECombatFeedbackCue ActiveFeedbackCue;
    bool bNeutralStateCaptured;
    bool bEventsBound;
    bool bPredictionCueActive;
};
