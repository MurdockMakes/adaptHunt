#pragma once

#include "CoreMinimal.h"
#include "AI/AdaptiveCounterOutcome.h"
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

DECLARE_MULTICAST_DELEGATE_OneParam(
    FEnemyCombatActionResolvedEvent,
    const FEnemyCombatActionOutcome&
);

/** Executes phase-driven enemy actions without deciding when to use them. */
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
    void EnterStagger(float Duration = -1.0f);

    /** Cancels target-dependent and temporary state without erasing cooldowns. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Enemy")
    void HandleTargetLost();

    UFUNCTION(BlueprintCallable, Category = "Combat|Enemy")
    void ResetCombatState();

    UFUNCTION(BlueprintCallable, Category = "Combat|Enemy")
    void SetCombatEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "Combat|Enemy")
    bool IsCombatEnabled() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Enemy|Phases")
    ECombatActionPhase GetCurrentPhase() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Enemy|Phases")
    bool IsMovementAllowed() const;

    UFUNCTION(BlueprintPure, Category = "Combat|Enemy|Phases")
    bool IsRotationAllowed() const;

    /** Non-damaging phase override used by console visualization commands. */
    bool DebugForceActionPhase(
        ECombatActionPhase Phase,
        float Duration = 1.0f
    );

    bool HasActivePhaseTimer() const;

    EEnemyCombatAction GetLastAction() const;
    bool IsBlocking() const;
    bool IsCommittedActionActive() const;
    bool ShouldPreserveCommittedMovement() const;
    bool IsActionOnCooldown(EEnemyCombatAction Action) const;
    float GetRemainingCooldown(EEnemyCombatAction Action) const;
    bool SupportsAction(EEnemyCombatAction Action) const;
    bool CanExecuteAction(EEnemyCombatAction Action) const;
    float GetActionStaminaCost(EEnemyCombatAction Action) const;
    FCombatActionTiming GetActionTiming(EEnemyCombatAction Action) const;

    float GetLightAttackDamage() const;
    float GetHeavyAttackDamage() const;
    float GetProjectileDamage() const;
    float GetDashDamage() const;
    float GetInterruptDamage() const;
    float GetMeleeRange() const;
    float GetProjectileSpeed() const;
    int32 GetLastCommittedOutcomeId() const;

    /** Projectile and opposing executors report raw results without context. */
    void ReportProjectileOutcome(
        int32 ActionId,
        EAdaptiveCounterOutcomeResult Result
    );
    void ReportDefensiveOutcome(EAdaptiveCounterOutcomeResult Result);

    /** Fired once per accepted enemy decision, never once per phase. */
    FEnemyCombatActionEvent OnActionCommitted;

    /** Fired at most once for every accepted utility-action ID. */
    FEnemyCombatActionResolvedEvent OnActionResolved;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Enemy|Events")
    FCombatActionPhaseChangedEvent OnActionPhaseChanged;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool TryMeleeAttack(
        EEnemyCombatAction Action,
        AActor* TargetActor,
        float StaminaCost,
        float Cooldown,
        const FCombatActionTiming& Timing
    );
    bool TryProjectileAttack(AActor* TargetActor);
    bool TryDashAttack(AActor* TargetActor);
    bool TryStartBlock();
    bool TryDodge(AActor* TargetActor);
    bool TryInterruptHeal(AActor* TargetActor);
    EAdaptiveCounterOutcomeResult ApplyAttackDamage(
        AActor* TargetActor,
        float Damage,
        float Range,
        bool bHeavyImpact
    );
    bool SpawnProjectile(AActor* TargetActor, int32 ActionId);
    bool CanCommitAction(EEnemyCombatAction Action, float StaminaCost) const;
    bool CommitAction(
        EEnemyCombatAction Action,
        float StaminaCost,
        float Cooldown
    );
    void BeginPhasedAction(
        EEnemyCombatAction Action,
        AActor* TargetActor,
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
        void (UEnemyCombatComponent::*Callback)(),
        float Duration
    );
    bool ResolveOutcome(
        int32 ActionId,
        EAdaptiveCounterOutcomeResult Result
    );
    void ResolveAllPendingOutcomes(
        EAdaptiveCounterOutcomeResult Result
    );
    void ResolveCanceledActiveOutcome();
    void CancelCurrentAction(ECombatActionPhase FinalPhase);
    void ClearTemporaryStates();
    void SetPendingTarget(AActor* TargetActor);
    void ClearPendingTarget();
    void CacheOwnerComponents();
    void HandleHealthChanged(UHealthComponent*, float OldHealth, float NewHealth);
    void HandleOwnerDeath(UHealthComponent*, AActor*);

    UFUNCTION()
    void HandlePendingTargetDestroyed(AActor* DestroyedActor);

    double GetCombatTimeSeconds() const;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> HealthComponent;

    UPROPERTY(Transient)
    TObjectPtr<UStaminaComponent> StaminaComponent;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|Enemy|State")
    EEnemyCombatAction LastAction;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|Enemy|State")
    EEnemyCombatAction ActiveAction;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|Enemy|State")
    FCombatActionRuntimeState ActionState;

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

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks")
    FCombatActionTiming LightAttackTiming;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks")
    FCombatActionTiming HeavyAttackTiming;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Projectile")
    FCombatActionTiming ProjectileTiming;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Dash")
    FCombatActionTiming DashTiming;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Attacks")
    FCombatActionTiming InterruptTiming;

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

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float BlockDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float BlockRecoveryDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float BlockCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float DodgeStaminaCost;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float DodgeCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float DodgeStrength;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float DodgeInvulnerabilityDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Defense", meta = (ClampMin = "0.0"))
    float DodgeRecoveryDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Timing", meta = (ClampMin = "0.0"))
    float StaggerDuration;

    UPROPERTY(EditDefaultsOnly, Category = "Combat|Enemy|Debug")
    bool bDrawDebugCombat;

    TMap<EEnemyCombatAction, double> NextActionTimes;
    TMap<int32, EEnemyCombatAction> PendingOutcomeActions;
    FCombatActionTiming ActiveTiming;
    TWeakObjectPtr<AActor> PendingTargetActor;
    int32 NextOutcomeActionId;
    int32 LastCommittedOutcomeId;
    int32 ActiveOutcomeActionId;
    bool bActiveFrameReached;
    bool bDodgeRightNext;
    FTimerHandle ActionPhaseTimer;
};
