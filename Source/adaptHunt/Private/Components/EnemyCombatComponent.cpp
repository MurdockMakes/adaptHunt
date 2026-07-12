#include "Components/EnemyCombatComponent.h"

#include "Combat/EnemyProjectile.h"
#include "Components/HealthComponent.h"
#include "Components/StaminaComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

namespace
{
bool IsMovementAction(const EEnemyCombatAction Action)
{
    return Action == EEnemyCombatAction::MoveTowardPlayer
        || Action == EEnemyCombatAction::MoveAwayFromPlayer
        || Action == EEnemyCombatAction::StrafeLeft
        || Action == EEnemyCombatAction::StrafeRight;
}
}

UEnemyCombatComponent::UEnemyCombatComponent()
    : HealthComponent(nullptr)
    , StaminaComponent(nullptr)
    , LastAction(EEnemyCombatAction::None)
    , bBlocking(false)
    , bCombatEnabled(true)
    , LightAttackDamage(12.0f)
    , HeavyAttackDamage(24.0f)
    , ProjectileDamage(10.0f)
    , DashDamage(18.0f)
    , InterruptDamage(16.0f)
    , LightAttackStaminaCost(12.0f)
    , HeavyAttackStaminaCost(24.0f)
    , ProjectileStaminaCost(16.0f)
    , DashStaminaCost(25.0f)
    , LightAttackCooldown(0.65f)
    , HeavyAttackCooldown(1.25f)
    , ProjectileCooldown(1.75f)
    , DashCooldown(2.5f)
    , InterruptCooldown(1.5f)
    , MeleeRange(190.0f)
    , DashHitRange(450.0f)
    , AttackRadius(60.0f)
    , ProjectileSpeed(1100.0f)
    , DashStrength(900.0f)
    , BlockStaminaCost(10.0f)
    , BlockDamageReduction(0.55f)
    , BlockDuration(0.8f)
    , BlockCooldown(2.0f)
    , DodgeStaminaCost(20.0f)
    , DodgeCooldown(1.8f)
    , DodgeStrength(650.0f)
    , DodgeInvulnerabilityDuration(0.2f)
    , GlobalActionRecovery(0.35f)
    , bDrawDebugCombat(true)
    , NextGlobalActionTime(0.0)
    , bDodgeRightNext(false)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    CacheOwnerComponents();

    if (HealthComponent)
    {
        HealthComponent->OnDeath.AddUObject(
            this,
            &UEnemyCombatComponent::HandleOwnerDeath
        );
    }
}

void UEnemyCombatComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (HealthComponent)
    {
        HealthComponent->OnDeath.RemoveAll(this);
        HealthComponent->SetDamageReduction(0.0f);
        HealthComponent->SetInvulnerable(false);
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BlockTimer);
        World->GetTimerManager().ClearTimer(DodgeInvulnerabilityTimer);
    }

    Super::EndPlay(EndPlayReason);
}

bool UEnemyCombatComponent::TryExecuteAction(
    const EEnemyCombatAction Action,
    AActor* TargetActor
)
{
    CacheOwnerComponents();

    switch (Action)
    {
    case EEnemyCombatAction::MoveTowardPlayer:
    case EEnemyCombatAction::MoveAwayFromPlayer:
    case EEnemyCombatAction::StrafeLeft:
    case EEnemyCombatAction::StrafeRight:
        CommitMovementAction(Action);
        return true;
    case EEnemyCombatAction::LightAttack:
        return TryMeleeAttack(
            Action,
            TargetActor,
            LightAttackDamage,
            LightAttackStaminaCost,
            LightAttackCooldown,
            MeleeRange
        );
    case EEnemyCombatAction::HeavyAttack:
        return TryMeleeAttack(
            Action,
            TargetActor,
            HeavyAttackDamage,
            HeavyAttackStaminaCost,
            HeavyAttackCooldown,
            MeleeRange
        );
    case EEnemyCombatAction::ProjectileAttack:
        return TryProjectileAttack(TargetActor);
    case EEnemyCombatAction::DashAttack:
        return TryDashAttack(TargetActor);
    case EEnemyCombatAction::Block:
        return TryStartBlock();
    case EEnemyCombatAction::Dodge:
        return TryDodge(TargetActor);
    case EEnemyCombatAction::InterruptHeal:
        return TryInterruptHeal(TargetActor);
    default:
        return false;
    }
}

void UEnemyCombatComponent::CommitMovementAction(
    const EEnemyCombatAction Action
)
{
    if (!IsMovementAction(Action) || LastAction == Action)
    {
        return;
    }

    LastAction = Action;
    OnActionCommitted.Broadcast(this, Action);
}

void UEnemyCombatComponent::StopBlock()
{
    if (!bBlocking)
    {
        return;
    }

    bBlocking = false;
    if (HealthComponent)
    {
        HealthComponent->SetDamageReduction(0.0f);
    }
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(BlockTimer);
    }
}

void UEnemyCombatComponent::ResetCombatState()
{
    StopBlock();
    LastAction = EEnemyCombatAction::None;
    NextActionTimes.Reset();
    NextGlobalActionTime = 0.0;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DodgeInvulnerabilityTimer);
    }
    if (HealthComponent)
    {
        HealthComponent->SetDamageReduction(0.0f);
        HealthComponent->SetInvulnerable(false);
    }
}

void UEnemyCombatComponent::SetCombatEnabled(const bool bEnabled)
{
    bCombatEnabled = bEnabled;
    if (!bCombatEnabled)
    {
        StopBlock();
    }
}

bool UEnemyCombatComponent::IsCombatEnabled() const
{
    return bCombatEnabled;
}

EEnemyCombatAction UEnemyCombatComponent::GetLastAction() const
{
    return LastAction;
}

bool UEnemyCombatComponent::IsBlocking() const
{
    return bBlocking;
}

bool UEnemyCombatComponent::IsActionOnCooldown(
    const EEnemyCombatAction Action
) const
{
    return GetRemainingCooldown(Action) > 0.0f;
}

float UEnemyCombatComponent::GetRemainingCooldown(
    const EEnemyCombatAction Action
) const
{
    const double* NextActionTime = NextActionTimes.Find(Action);
    return NextActionTime
        ? static_cast<float>(FMath::Max(
            0.0,
            *NextActionTime - GetCombatTimeSeconds()
        ))
        : 0.0f;
}

bool UEnemyCombatComponent::SupportsAction(
    const EEnemyCombatAction Action
) const
{
    switch (Action)
    {
    case EEnemyCombatAction::MoveTowardPlayer:
    case EEnemyCombatAction::MoveAwayFromPlayer:
    case EEnemyCombatAction::StrafeLeft:
    case EEnemyCombatAction::StrafeRight:
    case EEnemyCombatAction::LightAttack:
    case EEnemyCombatAction::HeavyAttack:
    case EEnemyCombatAction::ProjectileAttack:
    case EEnemyCombatAction::DashAttack:
    case EEnemyCombatAction::Block:
    case EEnemyCombatAction::Dodge:
    case EEnemyCombatAction::InterruptHeal:
        return true;
    default:
        return false;
    }
}

float UEnemyCombatComponent::GetLightAttackDamage() const
{
    return LightAttackDamage;
}

float UEnemyCombatComponent::GetHeavyAttackDamage() const
{
    return HeavyAttackDamage;
}

float UEnemyCombatComponent::GetProjectileDamage() const
{
    return ProjectileDamage;
}

float UEnemyCombatComponent::GetDashDamage() const
{
    return DashDamage;
}

float UEnemyCombatComponent::GetInterruptDamage() const
{
    return InterruptDamage;
}

float UEnemyCombatComponent::GetMeleeRange() const
{
    return MeleeRange;
}

float UEnemyCombatComponent::GetProjectileSpeed() const
{
    return ProjectileSpeed;
}

bool UEnemyCombatComponent::TryMeleeAttack(
    const EEnemyCombatAction Action,
    AActor* TargetActor,
    const float Damage,
    const float StaminaCost,
    const float Cooldown,
    const float Range
)
{
    if (!TargetActor || !CommitAction(Action, StaminaCost, Cooldown))
    {
        return false;
    }

    ApplyAttackDamage(TargetActor, Damage, Range);
    return true;
}

bool UEnemyCombatComponent::TryProjectileAttack(AActor* TargetActor)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !TargetActor || !World
        || !CommitAction(
            EEnemyCombatAction::ProjectileAttack,
            ProjectileStaminaCost,
            ProjectileCooldown
        ))
    {
        return false;
    }

    const FVector SpawnLocation = Owner->GetActorLocation()
        + Owner->GetActorForwardVector() * 80.0f
        + FVector(0.0f, 0.0f, 35.0f);
    const FVector Direction = (
        TargetActor->GetActorLocation() - SpawnLocation
    ).GetSafeNormal();

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = Owner;
    SpawnParameters.Instigator = Cast<APawn>(Owner);
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AEnemyProjectile* Projectile = World->SpawnActor<AEnemyProjectile>(
        AEnemyProjectile::StaticClass(),
        SpawnLocation,
        Direction.Rotation(),
        SpawnParameters
    );
    if (Projectile)
    {
        Projectile->InitializeProjectile(
            Direction,
            ProjectileDamage,
            ProjectileSpeed
        );
    }

    return Projectile != nullptr;
}

bool UEnemyCombatComponent::TryDashAttack(AActor* TargetActor)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !TargetActor
        || !CommitAction(
            EEnemyCombatAction::DashAttack,
            DashStaminaCost,
            DashCooldown
        ))
    {
        return false;
    }

    const FVector Direction = (
        TargetActor->GetActorLocation() - Character->GetActorLocation()
    ).GetSafeNormal2D();
    Character->LaunchCharacter(Direction * DashStrength, true, false);
    ApplyAttackDamage(TargetActor, DashDamage, DashHitRange);
    return true;
}

bool UEnemyCombatComponent::TryStartBlock()
{
    if (bBlocking || !CommitAction(
        EEnemyCombatAction::Block,
        BlockStaminaCost,
        BlockCooldown
    ))
    {
        return false;
    }

    bBlocking = true;
    HealthComponent->SetDamageReduction(BlockDamageReduction);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            BlockTimer,
            this,
            &UEnemyCombatComponent::StopBlock,
            FMath::Max(0.01f, BlockDuration),
            false
        );
    }
    return true;
}

bool UEnemyCombatComponent::TryDodge(AActor* TargetActor)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !TargetActor
        || !CommitAction(
            EEnemyCombatAction::Dodge,
            DodgeStaminaCost,
            DodgeCooldown
        ))
    {
        return false;
    }

    const FVector ToTarget = (
        TargetActor->GetActorLocation() - Character->GetActorLocation()
    ).GetSafeNormal2D();
    const FVector Right = FVector::CrossProduct(FVector::UpVector, ToTarget);
    const FVector DodgeDirection = bDodgeRightNext ? Right : -Right;
    bDodgeRightNext = !bDodgeRightNext;
    Character->LaunchCharacter(
        DodgeDirection.GetSafeNormal() * DodgeStrength,
        true,
        false
    );

    HealthComponent->SetInvulnerable(true);
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            DodgeInvulnerabilityTimer,
            this,
            &UEnemyCombatComponent::EndDodgeInvulnerability,
            FMath::Max(0.01f, DodgeInvulnerabilityDuration),
            false
        );
    }
    return true;
}

bool UEnemyCombatComponent::TryInterruptHeal(AActor* TargetActor)
{
    return TryMeleeAttack(
        EEnemyCombatAction::InterruptHeal,
        TargetActor,
        InterruptDamage,
        HeavyAttackStaminaCost,
        InterruptCooldown,
        MeleeRange * 1.35f
    );
}

bool UEnemyCombatComponent::ApplyAttackDamage(
    AActor* TargetActor,
    const float Damage,
    const float Range
)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !TargetActor || !World)
    {
        return false;
    }

    const FVector Start = Owner->GetActorLocation();
    const FVector Direction = (
        TargetActor->GetActorLocation() - Start
    ).GetSafeNormal();
    const FVector End = Start + Direction * Range;

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(EnemyAttack),
        false,
        Owner
    );
    TArray<FHitResult> Hits;
    World->SweepMultiByObjectType(
        Hits,
        Start,
        End,
        FQuat::Identity,
        FCollisionObjectQueryParams(ECC_Pawn),
        FCollisionShape::MakeSphere(AttackRadius),
        QueryParams
    );

    bool bHitTarget = false;
    for (const FHitResult& Hit : Hits)
    {
        if (Hit.GetActor() == TargetActor)
        {
            UGameplayStatics::ApplyDamage(
                TargetActor,
                Damage,
                Owner->GetInstigatorController(),
                Owner,
                nullptr
            );
            bHitTarget = true;
            break;
        }
    }

    if (bDrawDebugCombat)
    {
        const FColor Color = bHitTarget ? FColor::Orange : FColor::Yellow;
        DrawDebugLine(World, Start, End, Color, false, 0.6f, 0, 2.0f);
        DrawDebugSphere(
            World,
            End,
            AttackRadius,
            12,
            Color,
            false,
            0.6f
        );
    }

    return bHitTarget;
}

bool UEnemyCombatComponent::CanCommitAction(
    const EEnemyCombatAction Action,
    const float StaminaCost
) const
{
    return bCombatEnabled && HealthComponent
        && StaminaComponent
        && !HealthComponent->IsDead()
        && !bBlocking
        && StaminaCost >= 0.0f
        && StaminaComponent->GetCurrentStamina() >= StaminaCost
        && GetCombatTimeSeconds() >= NextGlobalActionTime
        && !IsActionOnCooldown(Action);
}

bool UEnemyCombatComponent::CommitAction(
    const EEnemyCombatAction Action,
    const float StaminaCost,
    const float Cooldown
)
{
    if (!CanCommitAction(Action, StaminaCost)
        || !StaminaComponent->TryConsumeStamina(StaminaCost))
    {
        return false;
    }

    const double CurrentTime = GetCombatTimeSeconds();
    NextActionTimes.Add(
        Action,
        CurrentTime + FMath::Max(0.0f, Cooldown)
    );
    NextGlobalActionTime = CurrentTime
        + FMath::Max(0.0f, GlobalActionRecovery);
    LastAction = Action;
    OnActionCommitted.Broadcast(this, Action);
    return true;
}

void UEnemyCombatComponent::CacheOwnerComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    if (!HealthComponent)
    {
        HealthComponent = Owner->FindComponentByClass<UHealthComponent>();
    }
    if (!StaminaComponent)
    {
        StaminaComponent = Owner->FindComponentByClass<UStaminaComponent>();
    }
}

void UEnemyCombatComponent::EndDodgeInvulnerability()
{
    if (HealthComponent)
    {
        HealthComponent->SetInvulnerable(false);
    }
}

void UEnemyCombatComponent::HandleOwnerDeath(UHealthComponent*, AActor*)
{
    StopBlock();
    if (HealthComponent)
    {
        HealthComponent->SetInvulnerable(false);
    }
}

double UEnemyCombatComponent::GetCombatTimeSeconds() const
{
    const UWorld* World = GetWorld();
    return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}
