#include "Components/CombatComponent.h"

#include "adaptHunt.h"
#include "Components/HealthComponent.h"
#include "Components/StaminaComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"

namespace
{
bool IsDirectionalDodge(const EPlayerCombatAction Action)
{
    return Action == EPlayerCombatAction::DodgeLeft
        || Action == EPlayerCombatAction::DodgeRight
        || Action == EPlayerCombatAction::DodgeBackward;
}
}

UCombatComponent::UCombatComponent()
    : HealthComponent(nullptr)
    , StaminaComponent(nullptr)
    , LastAction(EPlayerCombatAction::None)
    , bBlocking(false)
    , bCombatEnabled(true)
    , LightAttackDamage(20.0f)
    , HeavyAttackDamage(40.0f)
    , LightAttackStaminaCost(15.0f)
    , HeavyAttackStaminaCost(30.0f)
    , LightAttackCooldown(0.45f)
    , HeavyAttackCooldown(1.0f)
    , MeleeReach(175.0f)
    , MeleeRadius(55.0f)
    , BlockStaminaCost(10.0f)
    , BlockDamageReduction(0.65f)
    , BlockCooldown(0.35f)
    , DodgeStaminaCost(25.0f)
    , DodgeCooldown(1.0f)
    , DodgeStrength(700.0f)
    , DodgeInvulnerabilityDuration(0.25f)
    , HealStaminaCost(20.0f)
    , HealAmount(30.0f)
    , HealCooldown(5.0f)
    , GlobalActionRecovery(0.25f)
    , bDrawDebugCombat(true)
    , NextGlobalActionTime(0.0)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    CacheOwnerComponents();

    if (HealthComponent)
    {
        HealthComponent->OnDeath.AddUObject(
            this,
            &UCombatComponent::HandleOwnerDeath
        );
    }

    if (!HealthComponent || !StaminaComponent)
    {
        UE_LOG(
            LogAdaptHunt,
            Error,
            TEXT("%s requires health and stamina components."),
            GetOwner() ? *GetOwner()->GetName() : TEXT("CombatComponent")
        );
    }
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HealthComponent)
    {
        HealthComponent->OnDeath.RemoveAll(this);
        HealthComponent->SetDamageReduction(0.0f);
        HealthComponent->SetInvulnerable(false);
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DodgeInvulnerabilityTimer);
    }

    Super::EndPlay(EndPlayReason);
}

bool UCombatComponent::TryLightAttack()
{
    return TryMeleeAttack(
        EPlayerCombatAction::LightAttack,
        LightAttackDamage,
        LightAttackStaminaCost,
        LightAttackCooldown
    );
}

bool UCombatComponent::TryHeavyAttack()
{
    return TryMeleeAttack(
        EPlayerCombatAction::HeavyAttack,
        HeavyAttackDamage,
        HeavyAttackStaminaCost,
        HeavyAttackCooldown
    );
}

bool UCombatComponent::TryStartBlock()
{
    CacheOwnerComponents();
    if (bBlocking || !CanCommitAction(
        EPlayerCombatAction::Block,
        BlockStaminaCost
    ))
    {
        return false;
    }

    if (!CommitAction(
        EPlayerCombatAction::Block,
        BlockStaminaCost,
        BlockCooldown,
        false
    ))
    {
        return false;
    }

    bBlocking = true;
    HealthComponent->SetDamageReduction(BlockDamageReduction);
    OnBlockingChanged.Broadcast(this, true);

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("%s started blocking (%.0f%% damage reduction)."),
        *GetOwner()->GetName(),
        BlockDamageReduction * 100.0f
    );
    return true;
}

void UCombatComponent::StopBlock()
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
    OnBlockingChanged.Broadcast(this, false);

    UE_LOG(
        LogAdaptHunt,
        Verbose,
        TEXT("%s stopped blocking."),
        GetOwner() ? *GetOwner()->GetName() : TEXT("CombatComponent")
    );
}

bool UCombatComponent::TryDodge(const EPlayerCombatAction DodgeAction)
{
    CacheOwnerComponents();
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !IsDirectionalDodge(DodgeAction)
        || !CanCommitAction(DodgeAction, DodgeStaminaCost))
    {
        return false;
    }

    FVector DodgeDirection = FVector::ZeroVector;
    if (DodgeAction == EPlayerCombatAction::DodgeLeft)
    {
        DodgeDirection = -Character->GetActorRightVector();
    }
    else if (DodgeAction == EPlayerCombatAction::DodgeRight)
    {
        DodgeDirection = Character->GetActorRightVector();
    }
    else
    {
        DodgeDirection = -Character->GetActorForwardVector();
    }

    if (!CommitAction(
        DodgeAction,
        DodgeStaminaCost,
        DodgeCooldown
    ))
    {
        return false;
    }

    Character->LaunchCharacter(
        DodgeDirection.GetSafeNormal() * DodgeStrength,
        true,
        false
    );

    HealthComponent->SetInvulnerable(true);
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DodgeInvulnerabilityTimer);
        World->GetTimerManager().SetTimer(
            DodgeInvulnerabilityTimer,
            this,
            &UCombatComponent::EndDodgeInvulnerability,
            FMath::Max(0.01f, DodgeInvulnerabilityDuration),
            false
        );
    }

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("%s dodged in direction %d."),
        *Character->GetName(),
        static_cast<int32>(DodgeAction)
    );
    return true;
}

bool UCombatComponent::TryHeal()
{
    CacheOwnerComponents();
    if (!HealthComponent
        || HealthComponent->GetCurrentHealth()
            >= HealthComponent->GetMaxHealth()
        || !CanCommitAction(EPlayerCombatAction::Heal, HealStaminaCost))
    {
        return false;
    }

    if (!CommitAction(
        EPlayerCombatAction::Heal,
        HealStaminaCost,
        HealCooldown
    ))
    {
        return false;
    }

    const float RestoredHealth = HealthComponent->Heal(HealAmount);
    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("%s healed %.1f health."),
        *GetOwner()->GetName(),
        RestoredHealth
    );
    return RestoredHealth > 0.0f;
}

void UCombatComponent::ResetCombatState()
{
    StopBlock();
    LastAction = EPlayerCombatAction::None;
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

void UCombatComponent::SetCombatEnabled(const bool bEnabled)
{
    bCombatEnabled = bEnabled;
    if (!bCombatEnabled)
    {
        StopBlock();
    }
}

bool UCombatComponent::IsCombatEnabled() const
{
    return bCombatEnabled;
}

EPlayerCombatAction UCombatComponent::GetLastAction() const
{
    return LastAction;
}

bool UCombatComponent::IsBlocking() const
{
    return bBlocking;
}

bool UCombatComponent::CanPerformAction(
    const EPlayerCombatAction Action
) const
{
    float StaminaCost = 0.0f;
    switch (Action)
    {
    case EPlayerCombatAction::LightAttack:
        StaminaCost = LightAttackStaminaCost;
        break;
    case EPlayerCombatAction::HeavyAttack:
        StaminaCost = HeavyAttackStaminaCost;
        break;
    case EPlayerCombatAction::DodgeLeft:
    case EPlayerCombatAction::DodgeRight:
    case EPlayerCombatAction::DodgeBackward:
        StaminaCost = DodgeStaminaCost;
        break;
    case EPlayerCombatAction::Block:
        if (bBlocking)
        {
            return false;
        }
        StaminaCost = BlockStaminaCost;
        break;
    case EPlayerCombatAction::Heal:
        if (!HealthComponent
            || HealthComponent->GetCurrentHealth()
                >= HealthComponent->GetMaxHealth())
        {
            return false;
        }
        StaminaCost = HealStaminaCost;
        break;
    default:
        return false;
    }

    return CanCommitAction(Action, StaminaCost);
}

bool UCombatComponent::IsActionOnCooldown(
    const EPlayerCombatAction Action
) const
{
    return GetRemainingCooldown(Action) > 0.0f;
}

float UCombatComponent::GetRemainingCooldown(
    const EPlayerCombatAction Action
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

float UCombatComponent::GetLightAttackDamage() const
{
    return LightAttackDamage;
}

float UCombatComponent::GetHeavyAttackDamage() const
{
    return HeavyAttackDamage;
}

float UCombatComponent::GetLightAttackStaminaCost() const
{
    return LightAttackStaminaCost;
}

float UCombatComponent::GetHeavyAttackStaminaCost() const
{
    return HeavyAttackStaminaCost;
}

float UCombatComponent::GetDodgeStaminaCost() const
{
    return DodgeStaminaCost;
}

float UCombatComponent::GetHealAmount() const
{
    return HealAmount;
}

float UCombatComponent::GetBlockDamageReduction() const
{
    return BlockDamageReduction;
}

bool UCombatComponent::TryMeleeAttack(
    const EPlayerCombatAction Action,
    const float Damage,
    const float StaminaCost,
    const float Cooldown
)
{
    CacheOwnerComponents();
    if (!CommitAction(Action, StaminaCost, Cooldown))
    {
        return false;
    }

    const bool bHitTarget = PerformMeleeTrace(Damage);
    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("%s performed %s for %.1f damage%s."),
        *GetOwner()->GetName(),
        Action == EPlayerCombatAction::LightAttack
            ? TEXT("LightAttack")
            : TEXT("HeavyAttack"),
        Damage,
        bHitTarget ? TEXT(" and hit a target") : TEXT("")
    );
    return true;
}

bool UCombatComponent::PerformMeleeTrace(const float Damage)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World)
    {
        return false;
    }

    const FVector Start = Owner->GetActorLocation()
        + Owner->GetActorForwardVector() * 50.0f;
    const FVector End = Start
        + Owner->GetActorForwardVector() * MeleeReach;

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(PlayerMeleeAttack),
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
        FCollisionShape::MakeSphere(MeleeRadius),
        QueryParams
    );

    AActor* HitActor = nullptr;
    for (const FHitResult& Hit : Hits)
    {
        if (Hit.GetActor() && Hit.GetActor() != Owner)
        {
            HitActor = Hit.GetActor();
            break;
        }
    }

    if (HitActor)
    {
        UGameplayStatics::ApplyDamage(
            HitActor,
            Damage,
            Owner->GetInstigatorController(),
            Owner,
            nullptr
        );
    }

    if (bDrawDebugCombat)
    {
        const FColor DebugColor = HitActor ? FColor::Green : FColor::Red;
        DrawDebugLine(World, Start, End, DebugColor, false, 0.75f, 0, 2.0f);
        DrawDebugSphere(
            World,
            End,
            MeleeRadius,
            16,
            DebugColor,
            false,
            0.75f
        );
    }

    return HitActor != nullptr;
}

bool UCombatComponent::CanCommitAction(
    const EPlayerCombatAction Action,
    const float StaminaCost
) const
{
    if (!bCombatEnabled || !HealthComponent || !StaminaComponent
        || HealthComponent->IsDead() || bBlocking
        || GetCombatTimeSeconds() < NextGlobalActionTime
        || IsActionOnCooldown(Action))
    {
        return false;
    }

    return StaminaCost >= 0.0f
        && StaminaComponent->GetCurrentStamina() >= StaminaCost;
}

bool UCombatComponent::CommitAction(
    const EPlayerCombatAction Action,
    const float StaminaCost,
    const float Cooldown,
    const bool bUseGlobalRecovery
)
{
    if (!CanCommitAction(Action, StaminaCost))
    {
        return false;
    }

    OnActionCommitStarted.Broadcast(this, Action);

    if (!StaminaComponent->TryConsumeStamina(StaminaCost))
    {
        return false;
    }

    const double CurrentTime = GetCombatTimeSeconds();
    const double NextActionTime = CurrentTime + FMath::Max(0.0f, Cooldown);
    if (IsDirectionalDodge(Action))
    {
        NextActionTimes.Add(EPlayerCombatAction::DodgeLeft, NextActionTime);
        NextActionTimes.Add(EPlayerCombatAction::DodgeRight, NextActionTime);
        NextActionTimes.Add(EPlayerCombatAction::DodgeBackward, NextActionTime);
    }
    else
    {
        NextActionTimes.Add(Action, NextActionTime);
    }

    if (bUseGlobalRecovery)
    {
        NextGlobalActionTime = CurrentTime
            + FMath::Max(0.0f, GlobalActionRecovery);
    }

    LastAction = Action;
    OnActionCommitted.Broadcast(this, Action);
    return true;
}

void UCombatComponent::CacheOwnerComponents()
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

void UCombatComponent::EndDodgeInvulnerability()
{
    if (HealthComponent)
    {
        HealthComponent->SetInvulnerable(false);
    }
}

void UCombatComponent::HandleOwnerDeath(UHealthComponent*, AActor*)
{
    StopBlock();
    if (HealthComponent)
    {
        HealthComponent->SetInvulnerable(false);
    }
}

double UCombatComponent::GetCombatTimeSeconds() const
{
    const UWorld* World = GetWorld();
    return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}
