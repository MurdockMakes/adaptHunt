#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "AdaptivePlayerCharacter.generated.h"

class UCameraComponent;
class UCombatComponent;
class UCombatFeedbackComponent;
class UCombatSnapshotComponent;
class UGreyboxPresentationComponent;
class UHealthComponent;
class UInputAction;
class UInputMappingContext;
class UPlayerBehaviorTrackerComponent;
class USpringArmComponent;
class UStaminaComponent;
class UStaticMeshComponent;
struct FInputActionValue;

/**
 * C++-owned third-person player pawn for the adaptive combat prototype.
 *
 * Locomotion, camera, and input stay on the pawn. Reusable health and stamina
 * state live in actor components so the future enemy can share them.
 */
UCLASS()
class ADAPTHUNT_API AAdaptivePlayerCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AAdaptivePlayerCharacter();

    virtual void SetupPlayerInputComponent(
        UInputComponent* PlayerInputComponent
    ) override;

    UCameraComponent* GetFollowCamera() const;
    USpringArmComponent* GetCameraBoom() const;
    UStaticMeshComponent* GetBodyMesh() const;
    UHealthComponent* GetHealthComponent() const;
    UStaminaComponent* GetStaminaComponent() const;
    UCombatComponent* GetCombatComponent() const;
    UCombatFeedbackComponent* GetCombatFeedbackComponent() const;
    UGreyboxPresentationComponent* GetGreyboxPresentationComponent() const;
    UCombatSnapshotComponent* GetCombatSnapshotComponent() const;
    UPlayerBehaviorTrackerComponent* GetBehaviorTrackerComponent() const;
    const UInputMappingContext* GetDefaultMappingContext() const;
    float GetCameraPositionLagSpeed() const;
    float GetCameraRotationSmoothingSpeed() const;

    /** Deterministic resource and combat verification hooks. */
    UFUNCTION(Exec)
    void DebugDamageSelf(float DamageAmount = 25.0f);

    UFUNCTION(Exec)
    void DebugSpendStamina(float Cost = 25.0f);

    UFUNCTION(Exec)
    void DebugResetResources();

    UFUNCTION(Exec)
    void DebugLightAttack();

    UFUNCTION(Exec)
    void DebugHeavyAttack();

    UFUNCTION(Exec)
    void DebugToggleBlock();

    UFUNCTION(Exec)
    void DebugDodgeLeft();

    UFUNCTION(Exec)
    void DebugDodgeRight();

    UFUNCTION(Exec)
    void DebugDodgeBackward();

    UFUNCTION(Exec)
    void DebugHeal();

    UFUNCTION(Exec)
    void DebugSpawnCombatDummy(float Distance = 250.0f);

    /** Captures and logs every current learning feature. */
    UFUNCTION(Exec)
    void DebugCombatSnapshot();

    /** Logs all labeled player decisions in the match dataset. */
    UFUNCTION(Exec)
    void DebugBehaviorDataset();

    UFUNCTION(Exec)
    void DebugResetBehaviorDataset();

    UFUNCTION(Exec)
    void DebugToggleAdaptiveHUD();

    /** Match-complete action; ignored during an active match. */
    UFUNCTION(Exec)
    void RestartMatch();

protected:
    virtual void BeginPlay() override;

private:
    void ApplyResponsivenessTuning();
    void MoveForward(const FInputActionValue& Value);
    void MoveRight(const FInputActionValue& Value);
    void LookYaw(const FInputActionValue& Value);
    void LookPitch(const FInputActionValue& Value);
    void LightAttack();
    void HeavyAttack();
    void StartBlock();
    void StopBlock();
    void DodgeLeft();
    void DodgeRight();
    void DodgeBackward();
    void Heal();

    UPROPERTY(VisibleAnywhere, Category = "Character|Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, Category = "Character|Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    /** Visible primitive only; the inherited capsule remains authoritative. */
    UPROPERTY(VisibleAnywhere, Category = "Character|Greybox")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, Category = "Character|Combat")
    TObjectPtr<UHealthComponent> HealthComponent;

    UPROPERTY(VisibleAnywhere, Category = "Character|Combat")
    TObjectPtr<UStaminaComponent> StaminaComponent;

    UPROPERTY(VisibleAnywhere, Category = "Character|Combat")
    TObjectPtr<UCombatComponent> CombatComponent;

    UPROPERTY(VisibleAnywhere, Category = "Character|Combat|Feedback")
    TObjectPtr<UCombatFeedbackComponent> CombatFeedbackComponent;

    /** Observes gameplay and animates only the collision-free greybox body. */
    UPROPERTY(VisibleAnywhere, Category = "Character|Greybox")
    TObjectPtr<UGreyboxPresentationComponent> GreyboxPresentationComponent;

    UPROPERTY(VisibleAnywhere, Category = "Character|Machine Learning")
    TObjectPtr<UCombatSnapshotComponent> CombatSnapshotComponent;

    UPROPERTY(VisibleAnywhere, Category = "Character|Machine Learning")
    TObjectPtr<UPlayerBehaviorTrackerComponent> BehaviorTrackerComponent;

    /**
     * Runtime-created input data keeps this milestone playable without
     * Blueprint or Content Browser input assets.
     */
    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputMappingContext> DefaultMappingContext;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> MoveForwardAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> MoveRightAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> LookYawAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> LookPitchAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> LightAttackAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> HeavyAttackAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> BlockAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> DodgeLeftAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> DodgeRightAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> DodgeBackwardAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> HealAction;

    UPROPERTY(VisibleAnywhere, Category = "Input")
    TObjectPtr<UInputAction> RestartMatchAction;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Movement", meta = (ClampMin = "0.0"))
    float MovementAcceleration;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Movement", meta = (ClampMin = "0.0"))
    float WalkingBrakingDeceleration;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Movement", meta = (ClampMin = "0.0"))
    float WalkingGroundFriction;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Movement", meta = (ClampMin = "0.0"))
    float WalkingBrakingFriction;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Movement", meta = (ClampMin = "0.0"))
    float MovementRotationRateYaw;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Movement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MovementAirControl;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Movement", meta = (ClampMin = "0.0"))
    float MinimumAnalogWalkSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Camera")
    bool bEnableCameraPositionLag;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Camera", meta = (ClampMin = "0.0"))
    float CameraPositionLagSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Camera", meta = (ClampMin = "0.0"))
    float CameraPositionLagMaxDistance;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Camera")
    bool bEnableCameraRotationSmoothing;

    UPROPERTY(EditDefaultsOnly, Category = "Character|Responsiveness|Camera", meta = (ClampMin = "0.0"))
    float CameraRotationSmoothingSpeed;
};
