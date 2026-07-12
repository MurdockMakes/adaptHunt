#include "Combat/EnemyProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AEnemyProjectile::AEnemyProjectile()
    : DamageAmount(12.0f)
    , MaximumLifetime(5.0f)
{
    PrimaryActorTick.bCanEverTick = false;

    CollisionSphere = CreateDefaultSubobject<USphereComponent>(
        TEXT("CollisionSphere")
    );
    SetRootComponent(CollisionSphere);
    CollisionSphere->InitSphereRadius(14.0f);
    CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    CollisionSphere->SetNotifyRigidBodyCollision(true);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(
        TEXT("ProjectileMovement")
    );
    ProjectileMovement->UpdatedComponent = CollisionSphere;
    ProjectileMovement->InitialSpeed = 1100.0f;
    ProjectileMovement->MaxSpeed = 1100.0f;
    ProjectileMovement->ProjectileGravityScale = 0.0f;
    ProjectileMovement->bRotationFollowsVelocity = true;
}

void AEnemyProjectile::BeginPlay()
{
    Super::BeginPlay();

    CollisionSphere->OnComponentHit.AddDynamic(
        this,
        &AEnemyProjectile::HandleHit
    );
    SetLifeSpan(FMath::Max(0.1f, MaximumLifetime));
}

void AEnemyProjectile::InitializeProjectile(
    const FVector& Direction,
    const float Damage,
    const float Speed
)
{
    DamageAmount = FMath::Max(0.0f, Damage);
    const float SafeSpeed = FMath::Max(1.0f, Speed);
    ProjectileMovement->InitialSpeed = SafeSpeed;
    ProjectileMovement->MaxSpeed = SafeSpeed;
    ProjectileMovement->Velocity = Direction.GetSafeNormal() * SafeSpeed;
}

USphereComponent* AEnemyProjectile::GetCollisionSphere() const
{
    return CollisionSphere;
}

UProjectileMovementComponent*
AEnemyProjectile::GetProjectileMovement() const
{
    return ProjectileMovement;
}

float AEnemyProjectile::GetDamage() const
{
    return DamageAmount;
}

void AEnemyProjectile::HandleHit(
    UPrimitiveComponent*,
    AActor* OtherActor,
    UPrimitiveComponent*,
    FVector,
    const FHitResult&
)
{
    if (OtherActor && OtherActor != this && OtherActor != GetOwner())
    {
        UGameplayStatics::ApplyDamage(
            OtherActor,
            DamageAmount,
            GetInstigatorController(),
            this,
            nullptr
        );
    }

    Destroy();
}
