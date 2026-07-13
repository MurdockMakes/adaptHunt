#include "Combat/EnemyProjectile.h"

#include "AI/AdaptiveCounterOutcome.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AEnemyProjectile::AEnemyProjectile()
    : DamageAmount(12.0f)
    , SourceCombatComponent(nullptr)
    , SourceActionId(0)
    , bOutcomeReported(false)
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
    const float Speed,
    UEnemyCombatComponent* NewSourceCombatComponent,
    const int32 NewSourceActionId
)
{
    DamageAmount = FMath::Max(0.0f, Damage);
    const float SafeSpeed = FMath::Max(1.0f, Speed);
    ProjectileMovement->InitialSpeed = SafeSpeed;
    ProjectileMovement->MaxSpeed = SafeSpeed;
    ProjectileMovement->Velocity = Direction.GetSafeNormal() * SafeSpeed;
    SourceCombatComponent = NewSourceCombatComponent;
    SourceActionId = FMath::Max(0, NewSourceActionId);
}

void AEnemyProjectile::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    ReportOutcome(EAdaptiveCounterOutcomeResult::Missed);
    Super::EndPlay(EndPlayReason);
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
        UHealthComponent* TargetHealth =
            OtherActor->FindComponentByClass<UHealthComponent>();
        UCombatFeedbackComponent* TargetFeedback =
            OtherActor->FindComponentByClass<UCombatFeedbackComponent>();
        UCombatFeedbackComponent* OwnerFeedback = GetOwner()
            ? GetOwner()->FindComponentByClass<UCombatFeedbackComponent>()
            : nullptr;
        if (TargetHealth && TargetHealth->IsInvulnerable())
        {
            ReportOutcome(EAdaptiveCounterOutcomeResult::Dodged);
            UGameplayStatics::ApplyDamage(
                OtherActor,
                DamageAmount,
                GetInstigatorController(),
                this,
                nullptr
            );
            if (TargetFeedback)
            {
                TargetFeedback->NotifyAttackDodged(GetOwner());
            }
            if (OwnerFeedback)
            {
                OwnerFeedback->NotifyAttackMissed();
            }
        }
        else
        {
            const bool bBlocked = TargetHealth
                && TargetHealth->GetDamageReduction() > 0.0f;
            ReportOutcome(TargetHealth
                ? (bBlocked
                    ? EAdaptiveCounterOutcomeResult::Blocked
                    : EAdaptiveCounterOutcomeResult::Hit)
                : EAdaptiveCounterOutcomeResult::Missed);
            UGameplayStatics::ApplyDamage(
                OtherActor,
                DamageAmount,
                GetInstigatorController(),
                this,
                nullptr
            );
            if (TargetFeedback)
            {
                TargetFeedback->NotifyDamageReceived(
                    GetOwner(),
                    false,
                    bBlocked
                );
            }
            if (OwnerFeedback && TargetHealth)
            {
                OwnerFeedback->NotifyAttackConnected(false);
            }
            else if (OwnerFeedback)
            {
                OwnerFeedback->NotifyAttackMissed();
            }
        }
    }
    else if (UCombatFeedbackComponent* OwnerFeedback = GetOwner()
        ? GetOwner()->FindComponentByClass<UCombatFeedbackComponent>()
        : nullptr)
    {
        ReportOutcome(EAdaptiveCounterOutcomeResult::Missed);
        OwnerFeedback->NotifyAttackMissed();
    }
    else
    {
        ReportOutcome(EAdaptiveCounterOutcomeResult::Missed);
    }

    Destroy();
}

void AEnemyProjectile::ReportOutcome(
    const EAdaptiveCounterOutcomeResult Result
)
{
    if (bOutcomeReported)
    {
        return;
    }
    bOutcomeReported = true;
    if (SourceCombatComponent && SourceActionId > 0)
    {
        SourceCombatComponent->ReportProjectileOutcome(
            SourceActionId,
            Result
        );
    }
}
