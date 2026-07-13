#pragma once

#include "CoreMinimal.h"
#include "Combat/PlayerResponsivenessPolicy.h"
#include "Components/ActorComponent.h"
#include "Data/CombatTypes.h"
#include "TimerManager.h"

#include "CombatComponent.generated.h"

class ACharacter;
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
 * Reusable phase-driven player combat executor.
 *
 * Input commits an action exactly once. The component then owns windup,
 * active, and recovery timing, including damage, healing, blocking, dodge
 * invulnerability, interruption, and lifecycle cleanup.
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

    UFUNCTION(BlueprintCallable, Category = "Combat|Actions")
    void EnterStagger(float Duration = -1.0f);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ResetCombatState();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SetCombatEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsCombatEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Phases")
    ECombatActionPhase GetCurrentPhase() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Phases")
    bool IsMovementAllowed() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Phases")
    bool IsRotationAllowed() const;

    /** Non-damaging phase override used by console visualization commands. */
    bool DebugForceActionPhase(
        ECombatActionPhase Phase,
        float Duration = 1.0f
    );

    bool HasActivePhaseTimer() const;

    UFUNCTION(BlueprintCallable, Category = "Combat|Input Buffer")
    void ClearBufferedInput();

    UFUNCTION(BlueprintPure, Category = "Combat|Input Buffer")
    bool HasBufferedInput() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Input Buffer")
    EPlayerCombatAction GetBufferedAction() const;

    EPlayerCombatAction GetLastAction() const;
    bool IsBlocking() const;
    bool CanPerformAction(EPlayerCombatAction Action) const;
    bool IsActionOnCooldown(EPlayerCombatAction Action) const;
    float GetRemainingCooldown(EPlayerCombatAction Action) const;
    FCombatActionTiming GetActionTiming(EPlayerCombatAction Action) const;

    float GetLightAttackDamage() const;
    float GetHeavyAttackDamage() const;
    float GetLightAttackStaminaCost() const;
    float GetHeavyAttackStaminaCost() const;
    float GetDodgeStaminaCost() const;
    float GetHealAmount() const;
    float GetBlockDamageReduction() const;
    float GetInputBufferDuration() const;
    bool IsMeleeFacingAssistEnabled() const;
    float GetMeleeFacingAssistDistance() const;
    float GetMeleeFacingAssistConeHalfAngle() const;
    float GetMeleeFacingAssistMaxCorrection() const;

    /** Captures the pre-spend decision state for behavior snapshots. */
    FPlayerCombatActionEvent OnActionCommitStarted;

    /** Fired once per accepted player decision, never once per phase. */
    FPlayerCombatActionEvent OnActionCommitted;
    FBlockingChangedEvent OnBlockingChanged;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FCombatActionPhaseChangedEvent OnActionPhaseChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool TryMeleeAttack(
        EPlayerCombatAction Action,
        float StaminaCost,
        float Cooldown,
        const FCombatActionTiming& Timing
    );
    bool PerformMeleeTrace(float Damage, bool bHeavyImpact);
    bool CanCommitAction(
        EPlayerCombatAction Action,
        float StaminaCost
    ) const;
    bool CommitAction(
        EPlayerCombatAction Action,
        float StaminaCost,
        float Cooldown
    );
    bool TryBufferAction(EPlayerCombatAction Action);
    bool ExecuteBufferedAction(EPlayerCombatAction Action);
    bool CanBufferActionState(EPlayerCombatAction Action) const;
    float GetRemainingActionPhaseTime() const;
    FVector ResolveDodgeDirection(
        const ACharacter& Character,
        EPlayerCombatAction DodgeAction
    ) const;
    void ApplyMeleeFacingAssistance();
    void BeginPhasedAction(
        EPlayerCombatAction Action,
        const FCombatActionTiming& Timing
    );
    void BeginActivePhase();
    void BeginRecoveryPhase();
    void FinishCurrentAction();
    void ExecuteActiveEffect();
    void EndBlockToRecovery();
    void EndDodgeToRecovery();
    void SetActionPhase(ECombatActionPhase NewPhase);
    void SchedulePhaseTimer(
        void (UCombatComponent::*Callback)(),
        float Duration
    );
    void CancelCurrentAction(ECombatActionPhase FinalPhase);
    void ClearTemporaryStates(bool bBroadcastBlockingChange);
    void CacheOwnerComponents();
    void HandleHealthChanged(UHealthComponent*, float OldHealth, float NewHealth);
    void HandleOwnerDeath(UHealthComponent*, AActor*);
    double GetCombatTimeSeconds() const;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> HealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UStaminaComponent> StaminaComponent;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|State")
    EPlayerCombatAction LastAction;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|State")
    EPlayerCombatAction ActiveAction;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|State")
    FCombatActionRuntimeState ActionState;

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

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks")
    FCombatActionTiming LightAttackTiming;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks")
    FCombatActionTiming HeavyAttackTiming;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks", meta = (ClampMin = "0.0"))
    float MeleeReach;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks", meta = (ClampMin = "0.0"))
    float MeleeRadius;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks|Facing Assist")
    bool bEnableMeleeFacingAssist;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks|Facing Assist", meta = (ClampMin = "0.0"))
    float MeleeFacingAssistDistance;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks|Facing Assist", meta = (ClampMin = "0.0", ClampMax = "89.0"))
    float MeleeFacingAssistConeHalfAngle;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Attacks|Facing Assist", meta = (ClampMin = "0.0", ClampMax = "89.0"))
    float MeleeFacingAssistMaxCorrection;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0"))
    float BlockStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BlockDamageReduction;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0"))
    float BlockCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Block", meta = (ClampMin = "0.0"))
    float BlockRecoveryDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge", meta = (ClampMin = "0.0"))
    float DodgeStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge", meta = (ClampMin = "0.0"))
    float DodgeCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge", meta = (ClampMin = "0.0"))
    float DodgeStrength;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge", meta = (ClampMin = "0.0"))
    float DodgeInvulnerabilityDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Dodge", meta = (ClampMin = "0.0"))
    float DodgeRecoveryDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Heal", meta = (ClampMin = "0.0"))
    float HealStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Heal", meta = (ClampMin = "0.0"))
    float HealAmount;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Heal", meta = (ClampMin = "0.0"))
    float HealCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Heal")
    FCombatActionTiming HealTiming;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Timing", meta = (ClampMin = "0.0"))
    float StaggerDuration;

    /**
     * One action may be queued during this final portion of Recovery. The
     * normal commit path remains authoritative for resources and cooldowns.
     */
    UPROPERTY(EditDefaultsOnly, Category = "Combat|Input Buffer", meta = (ClampMin = "0.0", ClampMax = "0.5"))
    float InputBufferDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Debug")
    bool bDrawDebugCombat;

    TMap<EPlayerCombatAction, double> NextActionTimes;
    FPlayerCombatInputBuffer InputBuffer;
    FCombatActionTiming ActiveTiming;
    FVector PendingDodgeDirection;
    bool bCachedOrientRotationToMovement;
    bool bHasCachedRotationPolicy;
    FTimerHandle ActionPhaseTimer;
};
