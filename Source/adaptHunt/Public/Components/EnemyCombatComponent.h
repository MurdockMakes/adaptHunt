#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "TimerManager.h"

#include "EnemyCombatComponent.generated.h"

class UEnemyCombatComponent;
class UHealthComponent;
class UStaminaComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(
    FEnemyCombatActionEvent,
    UEnemyCombatComponent*,
    EEnemyCombatAction
);

/**
 * Executes enemy actions without deciding when they should be used.
 *
 * Keeping action execution separate from baseline decision making lets later
 * utility and prediction systems select actions without owning damage,
 * cooldown, stamina, projectile, or defensive-state details.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ADAPTHUNT_API UEnemyCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEnemyCombatComponent();

    UFUNCTION(BlueprintCallable, Category = "Combat|Enemy")
    bool TryExecuteAction(EEnemyCombatAction Action, AActor* TargetActor);

    void CommitMovementAction(EEnemyCombatAction Action);

    UFUNCTION(BlueprintCallable, Category = "Combat|Enemy")
    void StopBlock();

    UFUNCTION(BlueprintCallable, Category = "Combat|Enemy")
    void ResetCombatState();

    UFUNCTION(BlueprintCallable, Category = "Combat|Enemy")
    void SetCombatEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Combat|Enemy")
    bool IsCombatEnabled() const;

    EEnemyCombatAction GetLastAction() const;
    bool IsBlocking() const;
    bool IsActionOnCooldown(EEnemyCombatAction Action) const;
    float GetRemainingCooldown(EEnemyCombatAction Action) const;
    bool SupportsAction(EEnemyCombatAction Action) const;

    float GetLightAttackDamage() const;
    float GetHeavyAttackDamage() const;
    float GetProjectileDamage() const;
    float GetDashDamage() const;
    float GetInterruptDamage() const;
    float GetMeleeRange() const;
    float GetProjectileSpeed() const;

    FEnemyCombatActionEvent OnActionCommitted;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool TryMeleeAttack(
        EEnemyCombatAction Action,
        AActor* TargetActor,
        float Damage,
        float StaminaCost,
        float Cooldown,
        float Range
    );
    bool TryProjectileAttack(AActor* TargetActor);
    bool TryDashAttack(AActor* TargetActor);
    bool TryStartBlock();
    bool TryDodge(AActor* TargetActor);
    bool TryInterruptHeal(AActor* TargetActor);
    bool ApplyAttackDamage(AActor* TargetActor, float Damage, float Range);
    bool CanCommitAction(EEnemyCombatAction Action, float StaminaCost) const;
    bool CommitAction(
        EEnemyCombatAction Action,
        float StaminaCost,
        float Cooldown
    );
    void CacheOwnerComponents();
    void EndDodgeInvulnerability();
    void HandleOwnerDeath(UHealthComponent*, AActor*);
    double GetCombatTimeSeconds() const;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> HealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UStaminaComponent> StaminaComponent;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|Enemy|State")
    EEnemyCombatAction LastAction;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|Enemy|State")
    bool bBlocking;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|Enemy|State")
    bool bCombatEnabled;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float LightAttackDamage;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float HeavyAttackDamage;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float ProjectileDamage;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float DashDamage;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float InterruptDamage;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float LightAttackStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float HeavyAttackStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float ProjectileStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float DashStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float LightAttackCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float HeavyAttackCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float ProjectileCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float DashCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float InterruptCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float MeleeRange;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float DashHitRange;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks", meta = (ClampMin = "0.0"))
    float AttackRadius;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Projectile", meta = (ClampMin = "1.0"))
    float ProjectileSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Dash", meta = (ClampMin = "0.0"))
    float DashStrength;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float BlockStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlockDamageReduction;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.01"))
    float BlockDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float BlockCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float DodgeStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float DodgeCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float DodgeStrength;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.01"))
    float DodgeInvulnerabilityDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Timing", meta = (ClampMin = "0.0"))
    float GlobalActionRecovery;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Debug")
    bool bDrawDebugCombat;

    TMap<EEnemyCombatAction, double> NextActionTimes;
    double NextGlobalActionTime;
    bool bDodgeRightNext;
    FTimerHandle BlockTimer;
    FTimerHandle DodgeInvulnerabilityTimer;
};
