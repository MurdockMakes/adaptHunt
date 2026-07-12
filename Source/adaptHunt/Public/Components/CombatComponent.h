#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "TimerManager.h"

#include "CombatComponent.generated.h"

class UCombatComponent;
class UHealthComponent;
class UStaminaComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(
    FPlayerCombatActionEvent,
    UCombatComponent*,
    EPlayerCombatAction
);
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FBlockingChangedEvent,
    UCombatComponent*,
    bool
);

/**
 * Reusable, action-level player combat logic.
 *
 * The character owns input and locomotion. This component owns combat costs,
 * cooldowns, damage traces, blocking, dodging, healing, and action events so
 * behavior tracking can observe it in a later milestone without knowing input.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ADAPTHUNT_API UCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatComponent();

    UFUNCTION(BlueprintCallable, Category = "Combat|Actions")
    bool TryLightAttack();

    UFUNCTION(BlueprintCallable, Category = "Combat|Actions")
    bool TryHeavyAttack();

    UFUNCTION(BlueprintCallable, Category = "Combat|Actions")
    bool TryStartBlock();

    UFUNCTION(BlueprintCallable, Category = "Combat|Actions")
    void StopBlock();

    UFUNCTION(BlueprintCallable, Category = "Combat|Actions")
    bool TryDodge(EPlayerCombatAction DodgeAction);

    UFUNCTION(BlueprintCallable, Category = "Combat|Actions")
    bool TryHeal();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ResetCombatState();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SetCombatEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsCombatEnabled() const;

    EPlayerCombatAction GetLastAction() const;
    bool IsBlocking() const;
    /** Read-only availability query used by combat-state snapshots and UI. */
    bool CanPerformAction(EPlayerCombatAction Action) const;
    bool IsActionOnCooldown(EPlayerCombatAction Action) const;
    float GetRemainingCooldown(EPlayerCombatAction Action) const;

    float GetLightAttackDamage() const;
    float GetHeavyAttackDamage() const;
    float GetLightAttackStaminaCost() const;
    float GetHeavyAttackStaminaCost() const;
    float GetDodgeStaminaCost() const;
    float GetHealAmount() const;
    float GetBlockDamageReduction() const;

    /**
     * Fired after an action has passed validation but before it spends
     * resources or changes combat state. Snapshot capture uses this hook so
     * training features describe the decision state, not its aftermath.
     */
    FPlayerCombatActionEvent OnActionCommitStarted;

    /** Fired after all action state and resource changes are committed. */
    FPlayerCombatActionEvent OnActionCommitted;
    FBlockingChangedEvent OnBlockingChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool TryMeleeAttack(
        EPlayerCombatAction Action,
        float Damage,
        float StaminaCost,
        float Cooldown
    );
    bool PerformMeleeTrace(float Damage);
    bool CanCommitAction(
        EPlayerCombatAction Action,
        float StaminaCost
    ) const;
    bool CommitAction(
        EPlayerCombatAction Action,
        float StaminaCost,
        float Cooldown,
        bool bUseGlobalRecovery = true
    );
    void CacheOwnerComponents();
    void EndDodgeInvulnerability();
    void HandleOwnerDeath(UHealthComponent*, AActor*);
    double GetCombatTimeSeconds() const;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> HealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UStaminaComponent> StaminaComponent;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|State")
    EPlayerCombatAction LastAction;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|State")
    bool bBlocking;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|State")
    bool bCombatEnabled;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks", meta = (ClampMin = "0.0"))
    float LightAttackDamage;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks", meta = (ClampMin = "0.0"))
    float HeavyAttackDamage;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks", meta = (ClampMin = "0.0"))
    float LightAttackStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks", meta = (ClampMin = "0.0"))
    float HeavyAttackStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks", meta = (ClampMin = "0.0"))
    float LightAttackCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks", meta = (ClampMin = "0.0"))
    float HeavyAttackCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks", meta = (ClampMin = "0.0"))
    float MeleeReach;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks", meta = (ClampMin = "0.0"))
    float MeleeRadius;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0"))
    float BlockStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlockDamageReduction;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0"))
    float BlockCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge", meta = (ClampMin = "0.0"))
    float DodgeStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge", meta = (ClampMin = "0.0"))
    float DodgeCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge", meta = (ClampMin = "0.0"))
    float DodgeStrength;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge", meta = (ClampMin = "0.0"))
    float DodgeInvulnerabilityDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Heal", meta = (ClampMin = "0.0"))
    float HealStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Heal", meta = (ClampMin = "0.0"))
    float HealAmount;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Heal", meta = (ClampMin = "0.0"))
    float HealCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Timing", meta = (ClampMin = "0.0"))
    float GlobalActionRecovery;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Debug")
    bool bDrawDebugCombat;

    TMap<EPlayerCombatAction, double> NextActionTimes;
    double NextGlobalActionTime;
    FTimerHandle DodgeInvulnerabilityTimer;
};
