#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "Presentation/CombatFeedbackTypes.h"

#include "CombatFeedbackComponent.generated.h"

class UCameraComponent;
class UCombatComponent;
class UEnemyCombatComponent;
class UGreyboxPresentationComponent;
class UHealthComponent;


/**
 * Cosmetic combat feedback plus explicitly bounded physical knockback.
 *
 * The component observes committed actions and receives their resolved result.
 * It never owns damage, phases, cooldowns, input buffering, decisions, or round
 * timers. Camera/body impulses decay with Tick and therefore require no timer
 * dilation or hit stop.
 */
UCLASS(ClassGroup = (Presentation), meta = (BlueprintSpawnableComponent))
class ADAPTHUNT_API UCombatFeedbackComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatFeedbackComponent();

    void NotifyAttackMissed();
    void NotifyAttackDodged(AActor* AttackInstigator);
    void NotifyDamageReceived(
        AActor* AttackInstigator,
        bool bHeavyImpact,
        bool bBlocked
    );
    void NotifyAttackConnected(bool bHeavyImpact);

    UFUNCTION(BlueprintCallable, Category = "Combat|Feedback")
    void ResetFeedback();

    UFUNCTION(BlueprintPure, Category = "Combat|Feedback")
    ECombatFeedbackCue GetActiveCue() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Feedback")
    bool IsCueVisible() const;

    float GetKnockbackStrength() const;
    float GetHeavyKnockbackMultiplier() const;
    float GetBlockedKnockbackMultiplier() const;
    float GetMaximumKnockbackStrength() const;
    float GetFeedbackDisplayDuration() const;
    bool IsKnockbackEnabled() const;

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
    void TriggerCue(ECombatFeedbackCue Cue);
    void QueueCameraImpulse(const FVector& LocalOffset);
    void ApplyKnockback(AActor* AttackInstigator, float Multiplier);
    void RefreshTickState();

    void HandlePlayerActionCommitted(
        UCombatComponent* Component,
        EPlayerCombatAction Action
    );
    void HandleEnemyActionCommitted(
        UEnemyCombatComponent* Component,
        EEnemyCombatAction Action
    );
    void HandleDeath(UHealthComponent* Component, AActor* DamageCauser);

    UPROPERTY(Transient)
    TObjectPtr<UCombatComponent> PlayerCombatComponent;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

    UPROPERTY(Transient)
    TObjectPtr<UGreyboxPresentationComponent> PresentationComponent;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> HealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UCameraComponent> CameraComponent;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float FeedbackDisplayDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Knockback")
    bool bEnableKnockback;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Knockback", meta = (ClampMin = "0.0"))
    float KnockbackStrength;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Knockback", meta = (ClampMin = "1.0", ClampMax = "3.0"))
    float HeavyKnockbackMultiplier;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Knockback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlockedKnockbackMultiplier;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Knockback", meta = (ClampMin = "0.0"))
    float KnockbackVerticalVelocity;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Knockback", meta = (ClampMin = "0.0"))
    float MaximumKnockbackStrength;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Camera", meta = (ClampMin = "0.0"))
    float CameraImpulseScale;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Camera", meta = (ClampMin = "0.1"))
    float CameraReturnSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Feedback|Camera", meta = (ClampMin = "0.0"))
    float MaximumCameraOffset;

    FVector NeutralCameraLocation;
    FVector CurrentCameraOffset;
    ECombatFeedbackCue ActiveCue;
    float RemainingCueTime;
    bool bCameraStateCaptured;
    bool bEventsBound;
};
