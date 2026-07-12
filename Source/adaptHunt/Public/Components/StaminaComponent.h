#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"

#include "StaminaComponent.generated.h"

class UStaminaComponent;

DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FStaminaChangedEvent,
    UStaminaComponent*,
    float,
    float
);
DECLARE_MULTICAST_DELEGATE_OneParam(
    FStaminaThresholdEvent,
    UStaminaComponent*
);

/**
 * Reusable action resource with delayed, timer-driven regeneration.
 *
 * Successful spending restarts the regeneration delay. The component never
 * ticks, so it has no per-frame cost while stamina is full.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ADAPTHUNT_API UStaminaComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UStaminaComponent();

    float GetCurrentStamina() const;
    float GetMaxStamina() const;
    float GetNormalizedStamina() const;
    float GetRegenerationRate() const;
    float GetRegenerationDelay() const;
    bool IsDepleted() const;
    bool IsRegenerating() const;

    /** Spends exactly Cost when affordable; otherwise leaves stamina unchanged. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Stamina")
    bool TryConsumeStamina(float Cost);

    /** Restores stamina immediately and returns the amount actually restored. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Stamina")
    float RestoreStamina(float Amount);

    /** Restores full stamina and cancels pending regeneration timers. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Stamina")
    void ResetStamina();

    FStaminaChangedEvent OnStaminaChanged;
    FStaminaThresholdEvent OnStaminaDepleted;
    FStaminaThresholdEvent OnStaminaFullyRestored;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void RestartRegenerationDelay();
    void BeginRegeneration();
    void RegenerateStamina();
    void SetCurrentStamina(float NewStamina);
    void ClearRegenerationTimers();

    UPROPERTY(EditDefaultsOnly, Category = "Stamina", meta = (ClampMin = "1.0"))
    float MaxStamina;

    UPROPERTY(VisibleInstanceOnly, Category = "Stamina")
    float CurrentStamina;

    UPROPERTY(EditDefaultsOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
    float RegenerationRate;

    UPROPERTY(EditDefaultsOnly, Category = "Stamina", meta = (ClampMin = "0.0"))
    float RegenerationDelay;

    UPROPERTY(EditDefaultsOnly, Category = "Stamina", meta = (ClampMin = "0.01"))
    float RegenerationInterval;

    FTimerHandle RegenerationDelayTimer;
    FTimerHandle RegenerationTimer;
};
