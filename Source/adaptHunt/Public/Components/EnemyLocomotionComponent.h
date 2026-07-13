#pragma once

#include "CoreMinimal.h"
#include "AI/EnemyMovementPolicy.h"
#include "Components/ActorComponent.h"

#include "EnemyLocomotionComponent.generated.h"

class ACharacter;
class UEnemyCombatComponent;
class UHealthComponent;

/**
 * Executes enemy movement intent without selecting combat or tactical actions.
 *
 * Navigation is preferred when usable data exists. Otherwise the component
 * drives CharacterMovement directly; its capsule sweep and slide keep the
 * greybox fallback collision aware.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class ADAPTHUNT_API UEnemyLocomotionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEnemyLocomotionComponent();

    void RequestMovementIntent(
        EEnemyLocomotionIntent NewIntent,
        AActor* NewTargetActor
    );

    UFUNCTION(BlueprintCallable, Category = "AI|Locomotion")
    void ClearMovementIntent();

    /** Clears transient navigation, velocity, and stuck-recovery state. */
    UFUNCTION(BlueprintCallable, Category = "AI|Locomotion")
    void ResetLocomotionState();

    UFUNCTION(BlueprintCallable, Category = "AI|Locomotion")
    void SetLocomotionEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category = "AI|Locomotion")
    bool IsLocomotionEnabled() const;

    UFUNCTION(BlueprintPure, Category = "AI|Locomotion")
    EEnemyLocomotionIntent GetActiveIntent() const;

    UFUNCTION(BlueprintPure, Category = "AI|Locomotion")
    bool IsUsingNavigation() const;

    /** Safe for worlds with no navigation system or built nav data. */
    bool HasUsableNavigationData() const;

    float GetPreferredRange() const;
    float GetRetreatRange() const;
    float GetOrbitRange() const;
    float GetAcceptanceRadius() const;
    float GetMovementAcceleration() const;
    float GetTurnSpeed() const;
    float GetMovementSpeed() const;
    int32 GetStuckRecoveryCount() const;

    /** Pure guards used by the runtime driver and automation tests. */
    static bool CanAttemptNavigation(
        bool bHasAIController,
        bool bHasNavigationData
    );
    static bool ShouldTriggerStuckRecovery(
        float TimeWithoutProgress,
        float DistanceMoved,
        float StuckTimeout,
        float MinimumProgressDistance
    );

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

private:
    void CacheOwnerComponents();
    void ApplyMovementSettings();
    void EnsureController();
    bool CanDriveLocomotion() const;
    void FaceTarget(float DeltaTime) const;
    FVector ComputeDesiredDirection() const;
    FVector ComputeNavigationDestination() const;
    FVector ComputeCollisionAwareDirection(
        const FVector& DesiredDirection
    ) const;
    bool TryRequestNavigation(double CurrentTime);
    void ApplyDirectMovement(const FVector& DesiredDirection);
    void StopDrivenMovement(bool bStopVelocity);
    void ResetProgressTracking();
    void UpdateStuckRecovery(double CurrentTime);
    void BeginStuckRecovery(double CurrentTime);
    double GetLocomotionTimeSeconds() const;

    UPROPERTY(Transient)
    TObjectPtr<ACharacter> OwnerCharacter;

    UPROPERTY(Transient)
    TObjectPtr<AActor> TargetActor;

    UPROPERTY(Transient)
    TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

    UPROPERTY(Transient)
    TObjectPtr<UHealthComponent> HealthComponent;

    /** Desired center of the enemy's normal fighting band. */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Spacing", meta = (ClampMin = "0.0"))
    float PreferredRange;

    /** Below this distance the brain requests a retreat. */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Spacing", meta = (ClampMin = "0.0"))
    float RetreatRange;

    /** Above this distance the brain requests an approach. */
    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Spacing", meta = (ClampMin = "0.0"))
    float OrbitRange;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Navigation", meta = (ClampMin = "0.0"))
    float AcceptanceRadius;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Movement", meta = (ClampMin = "0.0"))
    float MovementAcceleration;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Movement", meta = (ClampMin = "0.0"))
    float TurnSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Movement", meta = (ClampMin = "0.0"))
    float MovementSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Navigation", meta = (ClampMin = "0.02"))
    float NavigationRefreshInterval;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Navigation", meta = (ClampMin = "0.0"))
    float OrbitStepDistance;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Fallback", meta = (ClampMin = "1.0"))
    float CollisionProbeDistance;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Recovery", meta = (ClampMin = "0.05"))
    float StuckTimeout;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Recovery", meta = (ClampMin = "0.0"))
    float MinimumProgressDistance;

    UPROPERTY(EditDefaultsOnly, Category = "AI|Locomotion|Recovery", meta = (ClampMin = "0.05"))
    float StuckRecoveryDuration;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Locomotion|State")
    EEnemyLocomotionIntent ActiveIntent;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Locomotion|State")
    bool bUsingNavigation;

    UPROPERTY(VisibleInstanceOnly, Category = "AI|Locomotion|State")
    int32 StuckRecoveryCount;

    bool bLocomotionEnabled;
    bool bWasTemporarilySuppressed;
    bool bRecoveryRight;
    FVector LastProgressLocation;
    double LastProgressTime;
    double RecoveryEndTime;
    double NextNavigationRequestTime;
};
