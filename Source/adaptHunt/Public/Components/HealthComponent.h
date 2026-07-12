#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "HealthComponent.generated.h"

class AController;
class UDamageType;
class UHealthComponent;

DECLARE_MULTICAST_DELEGATE_ThreeParams(
    FHealthChangedEvent,
    UHealthComponent*,
    float,
    float
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FDeathEvent,
    UHealthComponent*,
    AActor*
);

/**
 * Reusable health state that listens to its owner's Unreal damage events.
 *
 * Attacks can use UGameplayStatics::ApplyDamage without knowing which actor
 * class owns this component. Direct ApplyDamage calls remain available for
 * deterministic tests and non-actor damage sources.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ADAPTHUNT_API UHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UHealthComponent();

    float GetCurrentHealth() const;
    float GetMaxHealth() const;
    float GetNormalizedHealth() const;
    float GetDamageReduction() const;
    bool IsDead() const;
    bool IsInvulnerable() const;

    /**
     * Sets the fraction of incoming damage prevented, clamped to [0, 1].
     * Combat states such as blocking own the policy; health owns the math.
     */
    void SetDamageReduction(float NewDamageReduction);

    /** Dodge and future combat states can temporarily reject all damage. */
    void SetInvulnerable(bool bNewInvulnerable);

    /** Returns the amount of health actually removed. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Health")
    float ApplyDamage(float DamageAmount, AActor* DamageCauser = nullptr);

    /** Returns the amount of health actually restored. Dead actors stay dead. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Health")
    float Heal(float HealAmount);

    /** Restores full health and clears the dead state for a new test or round. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Health")
    void ResetHealth();

    FHealthChangedEvent OnHealthChanged;
    FDeathEvent OnDeath;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleOwnerTakeAnyDamage(
        AActor* DamagedActor,
        float Damage,
        const UDamageType* DamageType,
        AController* InstigatedBy,
        AActor* DamageCauser
    );

    void SetCurrentHealth(float NewHealth, AActor* DamageCauser);

    UPROPERTY(EditDefaultsOnly, Category = "Health", meta = (ClampMin = "1.0"))
    float MaxHealth;

    UPROPERTY(VisibleInstanceOnly, Category = "Health")
    float CurrentHealth;

    UPROPERTY(VisibleInstanceOnly, Category = "Health")
    float DamageReduction;

    UPROPERTY(VisibleInstanceOnly, Category = "Health")
    bool bInvulnerable;
};
