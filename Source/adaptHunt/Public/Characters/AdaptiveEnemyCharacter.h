#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "AdaptiveEnemyCharacter.generated.h"

class UEnemyCombatComponent;
class UCombatFeedbackComponent;
class UEnemyDecisionComponent;
class UEnemyLocomotionComponent;
class UGreyboxPresentationComponent;
class UHealthComponent;
class UStaminaComponent;
class UStaticMeshComponent;

/** Greybox enemy pawn with reusable resources, combat execution, and AI. */
UCLASS()
class ADAPTHUNT_API AAdaptiveEnemyCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AAdaptiveEnemyCharacter();

    UStaticMeshComponent* GetBodyMesh() const;
    UHealthComponent* GetHealthComponent() const;
    UStaminaComponent* GetStaminaComponent() const;
    UEnemyCombatComponent* GetEnemyCombatComponent() const;
    UCombatFeedbackComponent* GetCombatFeedbackComponent() const;
    UEnemyDecisionComponent* GetEnemyDecisionComponent() const;
    UEnemyLocomotionComponent* GetEnemyLocomotionComponent() const;
    UGreyboxPresentationComponent* GetGreyboxPresentationComponent() const;

protected:
    virtual void BeginPlay() override;

private:
    /** Visible primitive only; the inherited capsule remains authoritative. */
    UPROPERTY(VisibleAnywhere, Category = "Enemy|Greybox")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, Category = "Enemy|Combat")
    TObjectPtr<UHealthComponent> HealthComponent;

    UPROPERTY(VisibleAnywhere, Category = "Enemy|Combat")
    TObjectPtr<UStaminaComponent> StaminaComponent;

    UPROPERTY(VisibleAnywhere, Category = "Enemy|Combat")
    TObjectPtr<UEnemyCombatComponent> EnemyCombatComponent;

    UPROPERTY(VisibleAnywhere, Category = "Enemy|Combat|Feedback")
    TObjectPtr<UCombatFeedbackComponent> CombatFeedbackComponent;

    /** Observes gameplay and animates only the collision-free greybox body. */
    UPROPERTY(VisibleAnywhere, Category = "Enemy|Greybox")
    TObjectPtr<UGreyboxPresentationComponent> GreyboxPresentationComponent;

    UPROPERTY(VisibleAnywhere, Category = "Enemy|AI")
    TObjectPtr<UEnemyDecisionComponent> EnemyDecisionComponent;

    UPROPERTY(VisibleAnywhere, Category = "Enemy|AI")
    TObjectPtr<UEnemyLocomotionComponent> EnemyLocomotionComponent;
};
