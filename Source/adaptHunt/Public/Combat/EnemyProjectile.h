#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "EnemyProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;

/** Lightweight C++ projectile used by the baseline enemy ranged attack. */
UCLASS()
class ADAPTHUNT_API AEnemyProjectile : public AActor
{
    GENERATED_BODY()

public:
    AEnemyProjectile();

    void InitializeProjectile(
        const FVector& Direction,
        float Damage,
        float Speed
    );

    USphereComponent* GetCollisionSphere() const;
    UProjectileMovementComponent* GetProjectileMovement() const;
    float GetDamage() const;

protected:
    virtual void BeginPlay() override;

private:
    UFUNCTION()
    void HandleHit(
        UPrimitiveComponent* HitComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        FVector NormalImpulse,
        const FHitResult& Hit
    );

    UPROPERTY(VisibleAnywhere, Category = "Projectile")
    TObjectPtr<USphereComponent> CollisionSphere;

    UPROPERTY(VisibleAnywhere, Category = "Projectile")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    UPROPERTY(VisibleInstanceOnly, Category = "Projectile")
    float DamageAmount;

    UPROPERTY(EditDefaultsOnly, Category = "Projectile", meta = (ClampMin = "0.1"))
    float MaximumLifetime;
};
