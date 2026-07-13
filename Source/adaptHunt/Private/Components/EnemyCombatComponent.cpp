#include "Components/EnemyCombatComponent.h"

#include "Combat/EnemyProjectile.h"
#include "Components/CombatComponent.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/HealthComponent.h"
#include "Components/StaminaComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/AdaptiveHuntTuningSettings.h"
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
    , ActiveAction(EEnemyCombatAction::None)
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
    , BlockCooldown(2.0f)
    , DodgeStaminaCost(20.0f)
    , DodgeCooldown(1.8f)
    , DodgeStrength(650.0f)
    , bDrawDebugCombat(true)
    , ActiveTiming(0.0f, 0.0f, 0.0f)
    , NextOutcomeActionId(1)
    , LastCommittedOutcomeId(0)
    , ActiveOutcomeActionId(0)
    , bActiveFrameReached(false)
    , bDodgeRightNext(false)
{
    const FAdaptiveActionTimingTuning Tuning =
        UAdaptiveHuntTuningSettings::Get().ActionTiming.GetSanitized();
    LightAttackTiming = Tuning.EnemyLightAttack;
    HeavyAttackTiming = Tuning.EnemyHeavyAttack;
    ProjectileTiming = Tuning.EnemyProjectileAttack;
    DashTiming = Tuning.EnemyDashAttack;
    InterruptTiming = Tuning.EnemyInterruptHeal;
    BlockDuration = Tuning.EnemyBlockDuration;
    BlockRecoveryDuration = Tuning.EnemyBlockRecoveryDuration;
    DodgeInvulnerabilityDuration =
        Tuning.EnemyDodgeInvulnerabilityDuration;
    DodgeRecoveryDuration = Tuning.EnemyDodgeRecoveryDuration;
    StaggerDuration = Tuning.EnemyStaggerDuration;

    PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    CacheOwnerComponents();

    if (HealthComponent)
    {
        HealthComponent->OnHealthChanged.AddUObject(
            this,
            &UEnemyCombatComponent::HandleHealthChanged
        );
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
        HealthComponent->OnHealthChanged.RemoveAll(this);
        HealthComponent->OnDeath.RemoveAll(this);
    }
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ActionPhaseTimer);
    }

    ResolveCanceledActiveOutcome();
    ResolveAllPendingOutcomes(EAdaptiveCounterOutcomeResult::Missed);
    ClearPendingTarget();
    ClearTemporaryStates();
    ActiveAction = EEnemyCombatAction::None;
    ActionState.Reset(ECombatActionPhase::Idle);
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
        if (GetCurrentPhase() != ECombatActionPhase::Idle)
        {
            return false;
        }
        CommitMovementAction(Action);
        return true;
    case EEnemyCombatAction::LightAttack:
        return TryMeleeAttack(
            Action,
            TargetActor,
            LightAttackStaminaCost,
            LightAttackCooldown,
            LightAttackTiming
        );
    case EEnemyCombatAction::HeavyAttack:
        return TryMeleeAttack(
            Action,
            TargetActor,
            HeavyAttackStaminaCost,
            HeavyAttackCooldown,
            HeavyAttackTiming
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
    if (!IsMovementAction(Action) || LastAction == Action
        || GetCurrentPhase() != ECombatActionPhase::Idle)
    {
        return;
    }

    LastAction = Action;
    OnActionCommitted.Broadcast(this, Action);
}

void UEnemyCombatComponent::StopBlock()
{
    if (bBlocking && GetCurrentPhase() == ECombatActionPhase::Blocking)
    {
        EndBlockToRecovery();
    }
}

void UEnemyCombatComponent::EnterStagger(const float Duration)
{
    CacheOwnerComponents();
    if (!HealthComponent || HealthComponent->IsDead())
    {
        return;
    }

    CancelCurrentAction(ECombatActionPhase::Staggered);
    const float SafeDuration = FMath::IsFinite(Duration) && Duration >= 0.0f
        ? Duration
        : StaggerDuration;
    SchedulePhaseTimer(
        &UEnemyCombatComponent::FinishCurrentAction,
        SafeDuration
    );
}

void UEnemyCombatComponent::HandleTargetLost()
{
    const ECombatActionPhase FinalPhase = HealthComponent
        && HealthComponent->IsDead()
        ? ECombatActionPhase::Dead
        : ECombatActionPhase::Idle;
    CancelCurrentAction(FinalPhase);
    ResolveAllPendingOutcomes(EAdaptiveCounterOutcomeResult::Missed);
}

void UEnemyCombatComponent::ResetCombatState()
{
    CancelCurrentAction(ECombatActionPhase::Idle);
    ResolveAllPendingOutcomes(EAdaptiveCounterOutcomeResult::Missed);
    LastAction = EEnemyCombatAction::None;
    NextActionTimes.Reset();
}

void UEnemyCombatComponent::SetCombatEnabled(const bool bEnabled)
{
    bCombatEnabled = bEnabled;
    if (!bCombatEnabled)
    {
        const ECombatActionPhase FinalPhase = HealthComponent
            && HealthComponent->IsDead()
            ? ECombatActionPhase::Dead
            : ECombatActionPhase::Idle;
        CancelCurrentAction(FinalPhase);
        ResolveAllPendingOutcomes(EAdaptiveCounterOutcomeResult::Missed);
    }
}

bool UEnemyCombatComponent::IsCombatEnabled() const
{
    return bCombatEnabled;
}

ECombatActionPhase UEnemyCombatComponent::GetCurrentPhase() const
{
    return ActionState.GetPhase();
}

bool UEnemyCombatComponent::IsMovementAllowed() const
{
    return AdaptiveCombat::IsMovementAllowed(GetCurrentPhase());
}

bool UEnemyCombatComponent::IsRotationAllowed() const
{
    return AdaptiveCombat::IsRotationAllowed(GetCurrentPhase());
}

bool UEnemyCombatComponent::DebugForceActionPhase(
    const ECombatActionPhase Phase,
    const float Duration
)
{
    if (static_cast<uint8>(Phase)
        > static_cast<uint8>(ECombatActionPhase::Dead))
    {
        return false;
    }

    CancelCurrentAction(Phase);
    bCombatEnabled = Phase != ECombatActionPhase::Dead;
    if (Phase == ECombatActionPhase::Blocking)
    {
        bBlocking = true;
        if (HealthComponent)
        {
            HealthComponent->SetDamageReduction(BlockDamageReduction);
        }
    }
    else if (Phase == ECombatActionPhase::Dodging && HealthComponent)
    {
        HealthComponent->SetInvulnerable(true);
    }

    if (Phase != ECombatActionPhase::Idle
        && Phase != ECombatActionPhase::Dead
        && FMath::IsFinite(Duration) && Duration > 0.0f)
    {
        SchedulePhaseTimer(
            &UEnemyCombatComponent::FinishCurrentAction,
            Duration
        );
    }
    return true;
}

bool UEnemyCombatComponent::HasActivePhaseTimer() const
{
    const UWorld* World = GetWorld();
    return World
        && World->GetTimerManager().IsTimerActive(ActionPhaseTimer);
}

EEnemyCombatAction UEnemyCombatComponent::GetLastAction() const
{
    return LastAction;
}

bool UEnemyCombatComponent::IsBlocking() const
{
    return bBlocking;
}

bool UEnemyCombatComponent::IsCommittedActionActive() const
{
    return AdaptiveCombat::IsCommittedPhase(GetCurrentPhase());
}

bool UEnemyCombatComponent::ShouldPreserveCommittedMovement() const
{
    return GetCurrentPhase() == ECombatActionPhase::Dodging
        || (GetCurrentPhase() == ECombatActionPhase::Active
            && ActiveAction == EEnemyCombatAction::DashAttack);
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

bool UEnemyCombatComponent::CanExecuteAction(
    const EEnemyCombatAction Action
) const
{
    if (!SupportsAction(Action))
    {
        return false;
    }
    if (IsMovementAction(Action))
    {
        return bCombatEnabled
            && GetCurrentPhase() == ECombatActionPhase::Idle;
    }
    return CanCommitAction(Action, GetActionStaminaCost(Action));
}

float UEnemyCombatComponent::GetActionStaminaCost(
    const EEnemyCombatAction Action
) const
{
    switch (Action)
    {
    case EEnemyCombatAction::LightAttack:
        return LightAttackStaminaCost;
    case EEnemyCombatAction::HeavyAttack:
    case EEnemyCombatAction::InterruptHeal:
        return HeavyAttackStaminaCost;
    case EEnemyCombatAction::ProjectileAttack:
        return ProjectileStaminaCost;
    case EEnemyCombatAction::DashAttack:
        return DashStaminaCost;
    case EEnemyCombatAction::Block:
        return BlockStaminaCost;
    case EEnemyCombatAction::Dodge:
        return DodgeStaminaCost;
    default:
        return 0.0f;
    }
}

FCombatActionTiming UEnemyCombatComponent::GetActionTiming(
    const EEnemyCombatAction Action
) const
{
    switch (Action)
    {
    case EEnemyCombatAction::LightAttack:
        return LightAttackTiming.GetSanitized();
    case EEnemyCombatAction::HeavyAttack:
        return HeavyAttackTiming.GetSanitized();
    case EEnemyCombatAction::ProjectileAttack:
        return ProjectileTiming.GetSanitized();
    case EEnemyCombatAction::DashAttack:
        return DashTiming.GetSanitized();
    case EEnemyCombatAction::InterruptHeal:
        return InterruptTiming.GetSanitized();
    default:
        return FCombatActionTiming(0.0f, 0.0f, 0.0f);
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

int32 UEnemyCombatComponent::GetLastCommittedOutcomeId() const
{
    return LastCommittedOutcomeId;
}

void UEnemyCombatComponent::ReportProjectileOutcome(
    const int32 ActionId,
    const EAdaptiveCounterOutcomeResult Result
)
{
    ResolveOutcome(ActionId, Result);
}

void UEnemyCombatComponent::ReportDefensiveOutcome(
    const EAdaptiveCounterOutcomeResult Result
)
{
    const bool bMatchesActiveDefense =
        (ActiveAction == EEnemyCombatAction::Block
            && Result == EAdaptiveCounterOutcomeResult::Blocked)
        || (ActiveAction == EEnemyCombatAction::Dodge
            && Result == EAdaptiveCounterOutcomeResult::Dodged);
    if (bMatchesActiveDefense && bActiveFrameReached)
    {
        ResolveOutcome(ActiveOutcomeActionId, Result);
    }
}

bool UEnemyCombatComponent::TryMeleeAttack(
    const EEnemyCombatAction Action,
    AActor* TargetActor,
    const float StaminaCost,
    const float Cooldown,
    const FCombatActionTiming& Timing
)
{
    if (!IsValid(TargetActor)
        || !CommitAction(Action, StaminaCost, Cooldown))
    {
        return false;
    }

    BeginPhasedAction(Action, TargetActor, Timing);
    return true;
}

bool UEnemyCombatComponent::TryProjectileAttack(AActor* TargetActor)
{
    if (!GetOwner() || !GetWorld() || !IsValid(TargetActor)
        || !CommitAction(
            EEnemyCombatAction::ProjectileAttack,
            ProjectileStaminaCost,
            ProjectileCooldown
        ))
    {
        return false;
    }

    BeginPhasedAction(
        EEnemyCombatAction::ProjectileAttack,
        TargetActor,
        ProjectileTiming
    );
    return true;
}

bool UEnemyCombatComponent::TryDashAttack(AActor* TargetActor)
{
    if (!Cast<ACharacter>(GetOwner()) || !IsValid(TargetActor)
        || !CommitAction(
            EEnemyCombatAction::DashAttack,
            DashStaminaCost,
            DashCooldown
        ))
    {
        return false;
    }

    BeginPhasedAction(
        EEnemyCombatAction::DashAttack,
        TargetActor,
        DashTiming
    );
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

    ActiveAction = EEnemyCombatAction::Block;
    ActiveTiming = FCombatActionTiming(
        0.0f,
        BlockDuration,
        BlockRecoveryDuration
    ).GetSanitized();
    bActiveFrameReached = true;
    ActionState.Begin(ActionState.GetPhase());
    SetActionPhase(ECombatActionPhase::Blocking);
    bBlocking = true;
    HealthComponent->SetDamageReduction(BlockDamageReduction);
    SchedulePhaseTimer(
        &UEnemyCombatComponent::EndBlockToRecovery,
        ActiveTiming.ActiveDuration
    );
    return true;
}

bool UEnemyCombatComponent::TryDodge(AActor* TargetActor)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !IsValid(TargetActor)
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

    ActiveAction = EEnemyCombatAction::Dodge;
    ActiveTiming = FCombatActionTiming(
        0.0f,
        DodgeInvulnerabilityDuration,
        DodgeRecoveryDuration
    ).GetSanitized();
    bActiveFrameReached = true;
    SetPendingTarget(TargetActor);
    ActionState.Begin(ActionState.GetPhase());
    SetActionPhase(ECombatActionPhase::Dodging);
    Character->LaunchCharacter(
        DodgeDirection.GetSafeNormal() * FMath::Max(0.0f, DodgeStrength),
        true,
        false
    );
    HealthComponent->SetInvulnerable(true);
    SchedulePhaseTimer(
        &UEnemyCombatComponent::EndDodgeToRecovery,
        ActiveTiming.ActiveDuration
    );
    return true;
}

bool UEnemyCombatComponent::TryInterruptHeal(AActor* TargetActor)
{
    return TryMeleeAttack(
        EEnemyCombatAction::InterruptHeal,
        TargetActor,
        HeavyAttackStaminaCost,
        InterruptCooldown,
        InterruptTiming
    );
}

EAdaptiveCounterOutcomeResult UEnemyCombatComponent::ApplyAttackDamage(
    AActor* TargetActor,
    const float Damage,
    const float Range,
    const bool bHeavyImpact
)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !IsValid(TargetActor) || !World)
    {
        return EAdaptiveCounterOutcomeResult::Missed;
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
    EAdaptiveCounterOutcomeResult Outcome =
        EAdaptiveCounterOutcomeResult::Missed;
    for (const FHitResult& Hit : Hits)
    {
        if (Hit.GetActor() == TargetActor)
        {
            UHealthComponent* TargetHealth =
                TargetActor->FindComponentByClass<UHealthComponent>();
            UCombatFeedbackComponent* TargetFeedback =
                TargetActor->FindComponentByClass<UCombatFeedbackComponent>();
            UCombatFeedbackComponent* OwnerFeedback =
                Owner->FindComponentByClass<UCombatFeedbackComponent>();
            if (TargetHealth && TargetHealth->IsInvulnerable())
            {
                Outcome = EAdaptiveCounterOutcomeResult::Dodged;
                UGameplayStatics::ApplyDamage(
                    TargetActor,
                    Damage,
                    Owner->GetInstigatorController(),
                    Owner,
                    nullptr
                );
                if (TargetFeedback)
                {
                    TargetFeedback->NotifyAttackDodged(Owner);
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
                Outcome = bBlocked
                    ? EAdaptiveCounterOutcomeResult::Blocked
                    : EAdaptiveCounterOutcomeResult::Hit;
                UGameplayStatics::ApplyDamage(
                    TargetActor,
                    Damage,
                    Owner->GetInstigatorController(),
                    Owner,
                    nullptr
                );
                if (TargetFeedback)
                {
                    TargetFeedback->NotifyDamageReceived(
                        Owner,
                        bHeavyImpact,
                        bBlocked
                    );
                }
                if (OwnerFeedback)
                {
                    OwnerFeedback->NotifyAttackConnected(bHeavyImpact);
                }
            }
            bHitTarget = true;
            break;
        }
    }

    if (!bHitTarget)
    {
        if (UCombatFeedbackComponent* OwnerFeedback =
            Owner->FindComponentByClass<UCombatFeedbackComponent>())
        {
            OwnerFeedback->NotifyAttackMissed();
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

    return Outcome;
}

bool UEnemyCombatComponent::SpawnProjectile(
    AActor* TargetActor,
    const int32 ActionId
)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !IsValid(TargetActor) || !World)
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
            ProjectileSpeed,
            this,
            ActionId
        );
    }
    return Projectile != nullptr;
}

bool UEnemyCombatComponent::CanCommitAction(
    const EEnemyCombatAction Action,
    const float StaminaCost
) const
{
    return bCombatEnabled && HealthComponent && StaminaComponent
        && !HealthComponent->IsDead() && !bBlocking
        && GetCurrentPhase() == ECombatActionPhase::Idle
        && FMath::IsFinite(StaminaCost) && StaminaCost >= 0.0f
        && StaminaComponent->GetCurrentStamina() >= StaminaCost
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

    NextActionTimes.Add(
        Action,
        GetCombatTimeSeconds() + FMath::Max(0.0f, Cooldown)
    );
    if (NextOutcomeActionId <= 0 || NextOutcomeActionId == MAX_int32)
    {
        NextOutcomeActionId = 1;
    }
    while (PendingOutcomeActions.Contains(NextOutcomeActionId))
    {
        ++NextOutcomeActionId;
        if (NextOutcomeActionId <= 0 || NextOutcomeActionId == MAX_int32)
        {
            NextOutcomeActionId = 1;
        }
    }
    LastCommittedOutcomeId = NextOutcomeActionId++;
    ActiveOutcomeActionId = LastCommittedOutcomeId;
    bActiveFrameReached = false;
    PendingOutcomeActions.Add(ActiveOutcomeActionId, Action);
    LastAction = Action;
    OnActionCommitted.Broadcast(this, Action);
    return true;
}

void UEnemyCombatComponent::BeginPhasedAction(
    const EEnemyCombatAction Action,
    AActor* TargetActor,
    const FCombatActionTiming& Timing
)
{
    ActiveAction = Action;
    ActiveTiming = Timing.GetSanitized();
    bActiveFrameReached = false;
    SetPendingTarget(TargetActor);
    ActionState.Begin(ActionState.GetPhase());
    SetActionPhase(ECombatActionPhase::Windup);
    SchedulePhaseTimer(
        &UEnemyCombatComponent::BeginActivePhase,
        ActiveTiming.WindupDuration
    );
}

void UEnemyCombatComponent::BeginActivePhase()
{
    if (ActiveAction == EEnemyCombatAction::None
        || GetCurrentPhase() != ECombatActionPhase::Windup
        || !PendingTargetActor.IsValid())
    {
        CancelCurrentAction(ECombatActionPhase::Idle);
        return;
    }

    SetActionPhase(ECombatActionPhase::Active);
    bActiveFrameReached = true;
    ExecuteActiveEffect();
    SchedulePhaseTimer(
        &UEnemyCombatComponent::BeginRecoveryPhase,
        ActiveTiming.ActiveDuration
    );
}

void UEnemyCombatComponent::BeginRecoveryPhase()
{
    if (GetCurrentPhase() != ECombatActionPhase::Active)
    {
        return;
    }

    SetActionPhase(ECombatActionPhase::Recovery);
    SchedulePhaseTimer(
        &UEnemyCombatComponent::FinishCurrentAction,
        ActiveTiming.RecoveryDuration
    );
}

void UEnemyCombatComponent::FinishCurrentAction()
{
    if (GetCurrentPhase() == ECombatActionPhase::Dead)
    {
        return;
    }

    ClearTemporaryStates();
    ClearPendingTarget();
    ActiveAction = EEnemyCombatAction::None;
    ActiveOutcomeActionId = 0;
    bActiveFrameReached = false;
    ActiveTiming = FCombatActionTiming(0.0f, 0.0f, 0.0f);
    ActionState.Reset(ActionState.GetPhase());
    SetActionPhase(ECombatActionPhase::Idle);
}

void UEnemyCombatComponent::ExecuteActiveEffect()
{
    if (!ActionState.TryConsumeActiveEffect())
    {
        return;
    }

    AActor* TargetActor = PendingTargetActor.Get();
    if (!TargetActor)
    {
        ResolveOutcome(
            ActiveOutcomeActionId,
            EAdaptiveCounterOutcomeResult::Missed
        );
        return;
    }

    switch (ActiveAction)
    {
    case EEnemyCombatAction::LightAttack:
        ResolveOutcome(
            ActiveOutcomeActionId,
            ApplyAttackDamage(
                TargetActor,
                LightAttackDamage,
                MeleeRange,
                false
            )
        );
        break;
    case EEnemyCombatAction::HeavyAttack:
        ResolveOutcome(
            ActiveOutcomeActionId,
            ApplyAttackDamage(
                TargetActor,
                HeavyAttackDamage,
                MeleeRange,
                true
            )
        );
        break;
    case EEnemyCombatAction::ProjectileAttack:
        if (!SpawnProjectile(TargetActor, ActiveOutcomeActionId))
        {
            ResolveOutcome(
                ActiveOutcomeActionId,
                EAdaptiveCounterOutcomeResult::Missed
            );
        }
        break;
    case EEnemyCombatAction::DashAttack:
        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
            const FVector Direction = (
                TargetActor->GetActorLocation()
                - Character->GetActorLocation()
            ).GetSafeNormal2D();
            Character->LaunchCharacter(
                Direction * FMath::Max(0.0f, DashStrength),
                true,
                false
            );
            ResolveOutcome(
                ActiveOutcomeActionId,
                ApplyAttackDamage(
                    TargetActor,
                    DashDamage,
                    DashHitRange,
                    true
                )
            );
        }
        else
        {
            ResolveOutcome(
                ActiveOutcomeActionId,
                EAdaptiveCounterOutcomeResult::Missed
            );
        }
        break;
    case EEnemyCombatAction::InterruptHeal:
    {
        const UCombatComponent* TargetCombat =
            TargetActor->FindComponentByClass<UCombatComponent>();
        const ECombatActionPhase TargetPhase = TargetCombat
            ? TargetCombat->GetCurrentPhase()
            : ECombatActionPhase::Idle;
        const bool bTargetWasHealing = TargetCombat
            && TargetCombat->GetLastAction() == EPlayerCombatAction::Heal
            && (TargetPhase == ECombatActionPhase::Windup
                || TargetPhase == ECombatActionPhase::Active
                || TargetPhase == ECombatActionPhase::Recovery);
        EAdaptiveCounterOutcomeResult Result = ApplyAttackDamage(
            TargetActor,
            InterruptDamage,
            MeleeRange * 1.35f,
            true
        );
        if (bTargetWasHealing
            && Result == EAdaptiveCounterOutcomeResult::Hit)
        {
            Result = EAdaptiveCounterOutcomeResult::InterruptedHeal;
        }
        ResolveOutcome(ActiveOutcomeActionId, Result);
        break;
    }
    default:
        break;
    }
}

void UEnemyCombatComponent::EndBlockToRecovery()
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
    ResolveOutcome(
        ActiveOutcomeActionId,
        EAdaptiveCounterOutcomeResult::Missed
    );
    SetActionPhase(ECombatActionPhase::Recovery);
    SchedulePhaseTimer(
        &UEnemyCombatComponent::FinishCurrentAction,
        ActiveTiming.RecoveryDuration
    );
}

void UEnemyCombatComponent::EndDodgeToRecovery()
{
    if (GetCurrentPhase() != ECombatActionPhase::Dodging)
    {
        return;
    }

    if (HealthComponent)
    {
        HealthComponent->SetInvulnerable(false);
    }
    ResolveOutcome(
        ActiveOutcomeActionId,
        EAdaptiveCounterOutcomeResult::Missed
    );
    ClearPendingTarget();
    SetActionPhase(ECombatActionPhase::Recovery);
    SchedulePhaseTimer(
        &UEnemyCombatComponent::FinishCurrentAction,
        ActiveTiming.RecoveryDuration
    );
}

void UEnemyCombatComponent::SetActionPhase(
    const ECombatActionPhase NewPhase
)
{
    const ECombatActionPhase PreviousPhase = ActionState.GetPhase();
    if (PreviousPhase == NewPhase)
    {
        return;
    }

    ActionState.TransitionTo(NewPhase);
    if (!AdaptiveCombat::IsMovementAllowed(NewPhase))
    {
        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
            if (UCharacterMovementComponent* Movement =
                Character->GetCharacterMovement())
            {
                Movement->StopMovementImmediately();
            }
        }
    }
    OnActionPhaseChanged.Broadcast(PreviousPhase, NewPhase);
}

void UEnemyCombatComponent::SchedulePhaseTimer(
    void (UEnemyCombatComponent::*Callback)(),
    const float Duration
)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ActionPhaseTimer);
        World->GetTimerManager().SetTimer(
            ActionPhaseTimer,
            this,
            Callback,
            FMath::Max(0.001f, FMath::IsFinite(Duration) ? Duration : 0.0f),
            false
        );
    }
}

bool UEnemyCombatComponent::ResolveOutcome(
    const int32 ActionId,
    const EAdaptiveCounterOutcomeResult Result
)
{
    const EEnemyCombatAction* PendingAction =
        PendingOutcomeActions.Find(ActionId);
    if (!PendingAction)
    {
        return false;
    }

    FEnemyCombatActionOutcome Outcome;
    Outcome.ActionId = ActionId;
    Outcome.Action = *PendingAction;
    Outcome.Result = Result;
    if (!Outcome.IsValid())
    {
        return false;
    }

    PendingOutcomeActions.Remove(ActionId);
    OnActionResolved.Broadcast(Outcome);
    return true;
}

void UEnemyCombatComponent::ResolveAllPendingOutcomes(
    const EAdaptiveCounterOutcomeResult Result
)
{
    TArray<int32> ActionIds;
    PendingOutcomeActions.GetKeys(ActionIds);
    ActionIds.Sort();
    for (const int32 ActionId : ActionIds)
    {
        ResolveOutcome(ActionId, Result);
    }
}

void UEnemyCombatComponent::ResolveCanceledActiveOutcome()
{
    const EEnemyCombatAction* PendingAction =
        PendingOutcomeActions.Find(ActiveOutcomeActionId);
    if (!PendingAction)
    {
        return;
    }

    if (!bActiveFrameReached)
    {
        ResolveOutcome(
            ActiveOutcomeActionId,
            EAdaptiveCounterOutcomeResult::InvalidatedBeforeActiveFrame
        );
        return;
    }

    // A launched projectile remains authoritative until it reports impact,
    // lifetime expiry, or the round/match explicitly resolves all pending IDs.
    if (*PendingAction != EEnemyCombatAction::ProjectileAttack)
    {
        ResolveOutcome(
            ActiveOutcomeActionId,
            EAdaptiveCounterOutcomeResult::Missed
        );
    }
}

void UEnemyCombatComponent::CancelCurrentAction(
    const ECombatActionPhase FinalPhase
)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ActionPhaseTimer);
    }
    ResolveCanceledActiveOutcome();
    ClearTemporaryStates();
    ClearPendingTarget();
    ActiveAction = EEnemyCombatAction::None;
    ActiveOutcomeActionId = 0;
    bActiveFrameReached = false;
    ActiveTiming = FCombatActionTiming(0.0f, 0.0f, 0.0f);
    ActionState.Reset(ActionState.GetPhase());
    SetActionPhase(FinalPhase);
}

void UEnemyCombatComponent::ClearTemporaryStates()
{
    bBlocking = false;
    if (HealthComponent)
    {
        HealthComponent->SetDamageReduction(0.0f);
        HealthComponent->SetInvulnerable(false);
    }
}

void UEnemyCombatComponent::SetPendingTarget(AActor* TargetActor)
{
    ClearPendingTarget();
    if (IsValid(TargetActor))
    {
        PendingTargetActor = TargetActor;
        TargetActor->OnDestroyed.AddUniqueDynamic(
            this,
            &UEnemyCombatComponent::HandlePendingTargetDestroyed
        );
    }
}

void UEnemyCombatComponent::ClearPendingTarget()
{
    if (AActor* TargetActor = PendingTargetActor.Get())
    {
        TargetActor->OnDestroyed.RemoveDynamic(
            this,
            &UEnemyCombatComponent::HandlePendingTargetDestroyed
        );
    }
    PendingTargetActor.Reset();
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

void UEnemyCombatComponent::HandleHealthChanged(
    UHealthComponent*,
    const float OldHealth,
    const float NewHealth
)
{
    if (NewHealth < OldHealth && NewHealth > 0.0f)
    {
        EnterStagger(StaggerDuration);
    }
}

void UEnemyCombatComponent::HandleOwnerDeath(UHealthComponent*, AActor*)
{
    CancelCurrentAction(ECombatActionPhase::Dead);
}

void UEnemyCombatComponent::HandlePendingTargetDestroyed(AActor*)
{
    HandleTargetLost();
}

double UEnemyCombatComponent::GetCombatTimeSeconds() const
{
    const UWorld* World = GetWorld();
    return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}
