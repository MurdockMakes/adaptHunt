#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CombatTargetDummy.generated.h"

class UCapsuleComponent;
class UHealthComponent;
class UStaticMeshComponent;

/** Inert, damageable target used to verify player combat before Milestone 5. */
UCLASS()
class ADAPTHUNT_API ACombatTargetDummy : public AActor
{
    GENERATED_BODY()

public:
    ACombatTargetDummy();

    UHealthComponent* GetHealthComponent() const;
    UCapsuleComponent* GetCollisionCapsule() const;

private:
    UPROPERTY(VisibleAnywhere, Category = "Dummy")
    TObjectPtr<UCapsuleComponent> CollisionCapsule;

    UPROPERTY(VisibleAnywhere, Category = "Dummy")
    TObjectPtr<UStaticMeshComponent> BodyMesh;

    UPROPERTY(VisibleAnywhere, Category = "Dummy")
    TObjectPtr<UHealthComponent> HealthComponent;
};
