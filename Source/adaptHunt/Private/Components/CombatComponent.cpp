#include "Components/CombatComponent.h"

#include "adaptHunt.h"
#include "Camera/CameraComponent.h"
#include "Components/CombatFeedbackComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/StaminaComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/AdaptiveHuntTuningSettings.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"

namespace
{
constexpr double InputBufferTimerToleranceSeconds = 1.0 / 30.0;

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
    , ActiveAction(EPlayerCombatAction::None)
    , bBlocking(false)
    , bCombatEnabled(true)
    , LightAttackDamage(20.0f)
    , HeavyAttackDamage(40.0f)
    , LightAttackStaminaCost(15.0f)
    , HeavyAttackStaminaCost(30.0f)
    , LightAttackCooldown(0.45f)
    , HeavyAttackCooldown(0.9f)
    , MeleeReach(175.0f)
    , MeleeRadius(55.0f)
    , bEnableMeleeFacingAssist(true)
    , MeleeFacingAssistDistance(260.0f)
    , MeleeFacingAssistConeHalfAngle(45.0f)
    , MeleeFacingAssistMaxCorrection(18.0f)
    , BlockStaminaCost(10.0f)
    , BlockDamageReduction(0.65f)
    , BlockCooldown(0.35f)
    , DodgeStaminaCost(25.0f)
    , DodgeCooldown(1.0f)
    , DodgeStrength(700.0f)
    , HealStaminaCost(20.0f)
    , HealAmount(30.0f)
    , HealCooldown(5.0f)
    , bDrawDebugCombat(true)
    , ActiveTiming(0.0f, 0.0f, 0.0f)
    , PendingDodgeDirection(FVector::ZeroVector)
    , bCachedOrientRotationToMovement(false)
    , bHasCachedRotationPolicy(false)
{
    const FAdaptiveActionTimingTuning Tuning =
        UAdaptiveHuntTuningSettings::Get().ActionTiming.GetSanitized();
    const FAdaptiveCombatBalanceTuning CombatBalance =
        UAdaptiveHuntTuningSettings::Get().CombatBalance.GetSanitized();
    LightAttackDamage = CombatBalance.PlayerLightAttackDamage;
    HeavyAttackDamage = CombatBalance.PlayerHeavyAttackDamage;
    LightAttackTiming = Tuning.PlayerLightAttack;
    HeavyAttackTiming = Tuning.PlayerHeavyAttack;
    BlockRecoveryDuration = Tuning.PlayerBlockRecoveryDuration;
    DodgeInvulnerabilityDuration =
        Tuning.PlayerDodgeInvulnerabilityDuration;
    DodgeRecoveryDuration = Tuning.PlayerDodgeRecoveryDuration;
    HealTiming = Tuning.PlayerHeal;
    StaggerDuration = Tuning.PlayerStaggerDuration;
    InputBufferDuration = Tuning.PlayerInputBufferDuration;

    PrimaryComponentTick.bCanEverTick = false;
}

void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    CacheOwnerComponents();

    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (const UCharacterMovementComponent* Movement =
            Character->GetCharacterMovement())
        {
            bCachedOrientRotationToMovement =
                Movement->bOrientRotationToMovement;
            bHasCachedRotationPolicy = true;
        }
    }

    if (HealthComponent)
    {
        HealthComponent->OnHealthChanged.AddUObject(
            this,
            &UCombatComponent::HandleHealthChanged
        );
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
        HealthComponent->OnHealthChanged.RemoveAll(this);
        HealthComponent->OnDeath.RemoveAll(this);
    }
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ActionPhaseTimer);
    }

    ClearTemporaryStates(false);
    ClearBufferedInput();
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Movement =
            Character->GetCharacterMovement();
            Movement && bHasCachedRotationPolicy)
        {
            Movement->bOrientRotationToMovement =
                bCachedOrientRotationToMovement;
        }
    }
    ActiveAction = EPlayerCombatAction::None;
    ActionState.Reset(ECombatActionPhase::Idle);
    Super::EndPlay(EndPlayReason);
}

bool UCombatComponent::TryLightAttack()
{
    return TryMeleeAttack(
        EPlayerCombatAction::LightAttack,
        LightAttackStaminaCost,
        LightAttackCooldown,
        LightAttackTiming
    );
}

bool UCombatComponent::TryHeavyAttack()
{
    return TryMeleeAttack(
        EPlayerCombatAction::HeavyAttack,
        HeavyAttackStaminaCost,
        HeavyAttackCooldown,
        HeavyAttackTiming
    );
}

bool UCombatComponent::TryStartBlock()
{
    CacheOwnerComponents();
    if (bBlocking)
    {
        return false;
    }
    if (!CommitAction(
        EPlayerCombatAction::Block,
        BlockStaminaCost,
        BlockCooldown
    ))
    {
        return TryBufferAction(EPlayerCombatAction::Block);
    }

    ActiveAction = EPlayerCombatAction::Block;
    ActiveTiming = FCombatActionTiming(
        0.0f,
        0.0f,
        BlockRecoveryDuration
    ).GetSanitized();
    ActionState.Begin(ActionState.GetPhase());
    SetActionPhase(ECombatActionPhase::Blocking);

    bBlocking = true;
    HealthComponent->SetDamageReduction(BlockDamageReduction);
    OnBlockingChanged.Broadcast(this, true);

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("%s entered the Blocking phase (%.0f%% damage reduction)."),
        *GetOwner()->GetName(),
        BlockDamageReduction * 100.0f
    );
    return true;
}

void UCombatComponent::StopBlock()
{
    if (InputBuffer.GetAction() == EPlayerCombatAction::Block)
    {
        ClearBufferedInput();
    }
    if (bBlocking && GetCurrentPhase() == ECombatActionPhase::Blocking)
    {
        EndBlockToRecovery();
    }
}

bool UCombatComponent::TryDodge(
    const EPlayerCombatAction DodgeAction
)
{
    CacheOwnerComponents();
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !IsDirectionalDodge(DodgeAction))
    {
        return false;
    }

    if (!CommitAction(DodgeAction, DodgeStaminaCost, DodgeCooldown))
    {
        return TryBufferAction(DodgeAction);
    }

    ActiveAction = DodgeAction;
    ActiveTiming = FCombatActionTiming(
        0.0f,
        DodgeInvulnerabilityDuration,
        DodgeRecoveryDuration
    ).GetSanitized();
    PendingDodgeDirection = ResolveDodgeDirection(
        *Character,
        DodgeAction
    );
    ActionState.Begin(ActionState.GetPhase());
    SetActionPhase(ECombatActionPhase::Dodging);

    Character->LaunchCharacter(
        PendingDodgeDirection * FMath::Max(0.0f, DodgeStrength),
        true,
        false
    );
    HealthComponent->SetInvulnerable(true);
    SchedulePhaseTimer(
        &UCombatComponent::EndDodgeToRecovery,
        ActiveTiming.ActiveDuration
    );

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("%s entered the Dodging phase in direction %d."),
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
        )
    {
        return false;
    }
    if (!CommitAction(
        EPlayerCombatAction::Heal,
        HealStaminaCost,
        HealCooldown
    ))
    {
        return TryBufferAction(EPlayerCombatAction::Heal);
    }

    BeginPhasedAction(EPlayerCombatAction::Heal, HealTiming);
    return true;
}

void UCombatComponent::EnterStagger(const float Duration)
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
        &UCombatComponent::FinishCurrentAction,
        SafeDuration
    );
}

void UCombatComponent::ResetCombatState()
{
    CancelCurrentAction(ECombatActionPhase::Idle);
    LastAction = EPlayerCombatAction::None;
    NextActionTimes.Reset();
}

void UCombatComponent::SetCombatEnabled(const bool bEnabled)
{
    bCombatEnabled = bEnabled;
    if (!bCombatEnabled)
    {
        ClearBufferedInput();
        const ECombatActionPhase FinalPhase = HealthComponent
            && HealthComponent->IsDead()
            ? ECombatActionPhase::Dead
            : ECombatActionPhase::Idle;
        CancelCurrentAction(FinalPhase);
    }
}

bool UCombatComponent::IsCombatEnabled() const
{
    return bCombatEnabled;
}

ECombatActionPhase UCombatComponent::GetCurrentPhase() const
{
    return ActionState.GetPhase();
}

bool UCombatComponent::IsMovementAllowed() const
{
    return AdaptiveCombat::IsMovementAllowed(GetCurrentPhase());
}

bool UCombatComponent::IsRotationAllowed() const
{
    return AdaptiveCombat::IsRotationAllowed(GetCurrentPhase());
}

bool UCombatComponent::DebugForceActionPhase(
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
        OnBlockingChanged.Broadcast(this, true);
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
            &UCombatComponent::FinishCurrentAction,
            Duration
        );
    }
    return true;
}

bool UCombatComponent::HasActivePhaseTimer() const
{
    const UWorld* World = GetWorld();
    return World
        && World->GetTimerManager().IsTimerActive(ActionPhaseTimer);
}

void UCombatComponent::ClearBufferedInput()
{
    InputBuffer.Clear();
}

bool UCombatComponent::HasBufferedInput() const
{
    return InputBuffer.HasAction();
}

EPlayerCombatAction UCombatComponent::GetBufferedAction() const
{
    return InputBuffer.GetAction();
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

FCombatActionTiming UCombatComponent::GetActionTiming(
    const EPlayerCombatAction Action
) const
{
    switch (Action)
    {
    case EPlayerCombatAction::LightAttack:
        return LightAttackTiming.GetSanitized();
    case EPlayerCombatAction::HeavyAttack:
        return HeavyAttackTiming.GetSanitized();
    case EPlayerCombatAction::Heal:
        return HealTiming.GetSanitized();
    default:
        return FCombatActionTiming(0.0f, 0.0f, 0.0f);
    }
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

float UCombatComponent::GetInputBufferDuration() const
{
    return AdaptivePlayerResponsiveness::SanitizeInputBufferDuration(
        InputBufferDuration
    );
}

bool UCombatComponent::IsMeleeFacingAssistEnabled() const
{
    return bEnableMeleeFacingAssist;
}

float UCombatComponent::GetMeleeFacingAssistDistance() const
{
    return FMath::Max(0.0f, MeleeFacingAssistDistance);
}

float UCombatComponent::GetMeleeFacingAssistConeHalfAngle() const
{
    return FMath::Clamp(MeleeFacingAssistConeHalfAngle, 0.0f, 89.0f);
}

float UCombatComponent::GetMeleeFacingAssistMaxCorrection() const
{
    return FMath::Clamp(
        MeleeFacingAssistMaxCorrection,
        0.0f,
        GetMeleeFacingAssistConeHalfAngle()
    );
}

bool UCombatComponent::TryMeleeAttack(
    const EPlayerCombatAction Action,
    const float StaminaCost,
    const float Cooldown,
    const FCombatActionTiming& Timing
)
{
    CacheOwnerComponents();
    if (!CommitAction(Action, StaminaCost, Cooldown))
    {
        return TryBufferAction(Action);
    }

    ApplyMeleeFacingAssistance();
    BeginPhasedAction(Action, Timing);
    return true;
}

bool UCombatComponent::PerformMeleeTrace(
    const float Damage,
    const bool bHeavyImpact
)
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
        UHealthComponent* TargetHealth =
            HitActor->FindComponentByClass<UHealthComponent>();
        UCombatFeedbackComponent* TargetFeedback =
            HitActor->FindComponentByClass<UCombatFeedbackComponent>();
        UCombatFeedbackComponent* OwnerFeedback =
            Owner->FindComponentByClass<UCombatFeedbackComponent>();
        UEnemyCombatComponent* TargetEnemyCombat =
            HitActor->FindComponentByClass<UEnemyCombatComponent>();
        if (TargetHealth && TargetHealth->IsInvulnerable())
        {
            if (TargetEnemyCombat)
            {
                TargetEnemyCombat->ReportDefensiveOutcome(
                    EAdaptiveCounterOutcomeResult::Dodged
                );
            }
            UGameplayStatics::ApplyDamage(
                HitActor,
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
            if (bBlocked && TargetEnemyCombat)
            {
                TargetEnemyCombat->ReportDefensiveOutcome(
                    EAdaptiveCounterOutcomeResult::Blocked
                );
            }
            UGameplayStatics::ApplyDamage(
                HitActor,
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
    }
    else if (UCombatFeedbackComponent* OwnerFeedback =
        Owner->FindComponentByClass<UCombatFeedbackComponent>())
    {
        OwnerFeedback->NotifyAttackMissed();
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
    return bCombatEnabled && HealthComponent && StaminaComponent
        && !HealthComponent->IsDead() && !bBlocking
        && GetCurrentPhase() == ECombatActionPhase::Idle
        && StaminaCost >= 0.0f
        && StaminaComponent->GetCurrentStamina() >= StaminaCost
        && !IsActionOnCooldown(Action);
}

bool UCombatComponent::CommitAction(
    const EPlayerCombatAction Action,
    const float StaminaCost,
    const float Cooldown
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

    const double NextActionTime = GetCombatTimeSeconds()
        + FMath::Max(0.0f, Cooldown);
    if (IsDirectionalDodge(Action))
    {
        NextActionTimes.Add(EPlayerCombatAction::DodgeLeft, NextActionTime);
        NextActionTimes.Add(EPlayerCombatAction::DodgeRight, NextActionTime);
        NextActionTimes.Add(
            EPlayerCombatAction::DodgeBackward,
            NextActionTime
        );
    }
    else
    {
        NextActionTimes.Add(Action, NextActionTime);
    }

    LastAction = Action;
    OnActionCommitted.Broadcast(this, Action);
    return true;
}

bool UCombatComponent::TryBufferAction(
    const EPlayerCombatAction Action
)
{
    CacheOwnerComponents();
    const float RemainingPhaseTime = GetRemainingActionPhaseTime();
    const bool bOwnerAlive = HealthComponent
        && !HealthComponent->IsDead();
    if (!AdaptivePlayerResponsiveness::IsBufferableAction(Action)
        || !CanBufferActionState(Action)
        || !AdaptivePlayerResponsiveness::CanBufferDuringRecovery(
            GetCurrentPhase(),
            RemainingPhaseTime,
            GetInputBufferDuration(),
            bCombatEnabled,
            bOwnerAlive
        ))
    {
        return false;
    }

    const double ExpirationTime = GetCombatTimeSeconds()
        + static_cast<double>(GetInputBufferDuration())
        + InputBufferTimerToleranceSeconds;
    const bool bAlreadyBuffered = InputBuffer.HasAction();
    if (!InputBuffer.TryStore(Action, ExpirationTime))
    {
        return false;
    }

    if (!bAlreadyBuffered)
    {
        UE_LOG(
            LogAdaptHunt,
            Verbose,
            TEXT("%s buffered player action %d for the end of Recovery."),
            GetOwner() ? *GetOwner()->GetName() : TEXT("Player"),
            static_cast<int32>(Action)
        );
    }
    return true;
}

bool UCombatComponent::ExecuteBufferedAction(
    const EPlayerCombatAction Action
)
{
    switch (Action)
    {
    case EPlayerCombatAction::LightAttack:
        return TryLightAttack();
    case EPlayerCombatAction::HeavyAttack:
        return TryHeavyAttack();
    case EPlayerCombatAction::DodgeLeft:
    case EPlayerCombatAction::DodgeRight:
    case EPlayerCombatAction::DodgeBackward:
        return TryDodge(Action);
    case EPlayerCombatAction::Block:
        return TryStartBlock();
    case EPlayerCombatAction::Heal:
        return TryHeal();
    default:
        return false;
    }
}

bool UCombatComponent::CanBufferActionState(
    const EPlayerCombatAction Action
) const
{
    if (!HealthComponent || !StaminaComponent || bBlocking)
    {
        return false;
    }

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
        StaminaCost = BlockStaminaCost;
        break;
    case EPlayerCombatAction::Heal:
        if (HealthComponent->GetCurrentHealth()
            >= HealthComponent->GetMaxHealth())
        {
            return false;
        }
        StaminaCost = HealStaminaCost;
        break;
    default:
        return false;
    }

    return StaminaCost >= 0.0f
        && StaminaComponent->GetCurrentStamina() >= StaminaCost;
}

float UCombatComponent::GetRemainingActionPhaseTime() const
{
    if (UWorld* World = GetWorld())
    {
        return World->GetTimerManager().GetTimerRemaining(ActionPhaseTimer);
    }
    return -1.0f;
}

FVector UCombatComponent::ResolveDodgeDirection(
    const ACharacter& Character,
    const EPlayerCombatAction DodgeAction
) const
{
    FVector CameraForward = FVector::ZeroVector;
    FVector CameraRight = FVector::ZeroVector;
    bool bHasCameraBasis = false;
    if (const UCameraComponent* Camera =
        Character.FindComponentByClass<UCameraComponent>())
    {
        CameraForward = Camera->GetForwardVector();
        CameraRight = Camera->GetRightVector();
        bHasCameraBasis = true;
    }
    else if (const AController* Controller = Character.GetController())
    {
        const FRotator YawRotation(
            0.0f,
            Controller->GetControlRotation().Yaw,
            0.0f
        );
        CameraForward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        CameraRight = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        bHasCameraBasis = true;
    }

    return AdaptivePlayerResponsiveness::ResolveDodgeDirection(
        DodgeAction,
        Character.GetActorForwardVector(),
        Character.GetActorRightVector(),
        CameraForward,
        CameraRight,
        bHasCameraBasis
    );
}

void UCombatComponent::ApplyMeleeFacingAssistance()
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    const float AssistDistance = GetMeleeFacingAssistDistance();
    if (!bEnableMeleeFacingAssist || !Owner || !World
        || AssistDistance <= 0.0f)
    {
        return;
    }

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(PlayerMeleeFacingAssist),
        false,
        Owner
    );
    TArray<FOverlapResult> Overlaps;
    World->OverlapMultiByObjectType(
        Overlaps,
        Owner->GetActorLocation(),
        FQuat::Identity,
        FCollisionObjectQueryParams(ECC_Pawn),
        FCollisionShape::MakeSphere(AssistDistance),
        QueryParams
    );

    bool bFoundTarget = false;
    float BestScore = TNumericLimits<float>::Max();
    float BestYawCorrection = 0.0f;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Candidate = Overlap.GetActor();
        UHealthComponent* CandidateHealth = Candidate
            ? Candidate->FindComponentByClass<UHealthComponent>()
            : nullptr;
        if (!Candidate || Candidate == Owner || !CandidateHealth
            || CandidateHealth->IsDead())
        {
            continue;
        }

        const FVector ToTarget = Candidate->GetActorLocation()
            - Owner->GetActorLocation();
        const float Distance = FVector(ToTarget.X, ToTarget.Y, 0.0f).Size();
        if (Distance > AssistDistance)
        {
            continue;
        }

        float RawYawCorrection = 0.0f;
        if (!AdaptivePlayerResponsiveness::ComputeMeleeFacingCorrection(
            Owner->GetActorForwardVector(),
            ToTarget,
            GetMeleeFacingAssistConeHalfAngle(),
            GetMeleeFacingAssistConeHalfAngle(),
            RawYawCorrection
        ))
        {
            continue;
        }

        const float Score = FMath::Abs(RawYawCorrection)
            + Distance * 0.0001f;
        if (!bFoundTarget || Score < BestScore)
        {
            bFoundTarget = true;
            BestScore = Score;
            BestYawCorrection = FMath::Clamp(
                RawYawCorrection,
                -GetMeleeFacingAssistMaxCorrection(),
                GetMeleeFacingAssistMaxCorrection()
            );
        }
    }

    if (bFoundTarget && !FMath::IsNearlyZero(BestYawCorrection))
    {
        FRotator AssistedRotation = Owner->GetActorRotation();
        AssistedRotation.Yaw = FRotator::NormalizeAxis(
            AssistedRotation.Yaw + BestYawCorrection
        );
        Owner->SetActorRotation(AssistedRotation);
    }
}

void UCombatComponent::BeginPhasedAction(
    const EPlayerCombatAction Action,
    const FCombatActionTiming& Timing
)
{
    ActiveAction = Action;
    ActiveTiming = Timing.GetSanitized();
    ActionState.Begin(ActionState.GetPhase());
    SetActionPhase(ECombatActionPhase::Windup);
    SchedulePhaseTimer(
        &UCombatComponent::BeginActivePhase,
        ActiveTiming.WindupDuration
    );
}

void UCombatComponent::BeginActivePhase()
{
    if (ActiveAction == EPlayerCombatAction::None
        || GetCurrentPhase() != ECombatActionPhase::Windup)
    {
        CancelCurrentAction(ECombatActionPhase::Idle);
        return;
    }

    SetActionPhase(ECombatActionPhase::Active);
    ExecuteActiveEffect();
    SchedulePhaseTimer(
        &UCombatComponent::BeginRecoveryPhase,
        ActiveTiming.ActiveDuration
    );
}

void UCombatComponent::BeginRecoveryPhase()
{
    if (GetCurrentPhase() != ECombatActionPhase::Active)
    {
        return;
    }

    SetActionPhase(ECombatActionPhase::Recovery);
    SchedulePhaseTimer(
        &UCombatComponent::FinishCurrentAction,
        ActiveTiming.RecoveryDuration
    );
}

void UCombatComponent::FinishCurrentAction()
{
    if (GetCurrentPhase() == ECombatActionPhase::Dead)
    {
        return;
    }

    const EPlayerCombatAction BufferedAction =
        InputBuffer.ConsumeIfValid(GetCombatTimeSeconds());
    ClearTemporaryStates(true);
    ActiveAction = EPlayerCombatAction::None;
    ActiveTiming = FCombatActionTiming(0.0f, 0.0f, 0.0f);
    PendingDodgeDirection = FVector::ZeroVector;
    ActionState.Reset(ActionState.GetPhase());
    SetActionPhase(ECombatActionPhase::Idle);

    if (BufferedAction != EPlayerCombatAction::None)
    {
        ExecuteBufferedAction(BufferedAction);
    }
}

void UCombatComponent::ExecuteActiveEffect()
{
    if (!ActionState.TryConsumeActiveEffect())
    {
        return;
    }

    switch (ActiveAction)
    {
    case EPlayerCombatAction::LightAttack:
        PerformMeleeTrace(LightAttackDamage, false);
        break;
    case EPlayerCombatAction::HeavyAttack:
        PerformMeleeTrace(HeavyAttackDamage, true);
        break;
    case EPlayerCombatAction::Heal:
        if (HealthComponent)
        {
            const float RestoredHealth = HealthComponent->Heal(HealAmount);
            UE_LOG(
                LogAdaptHunt,
                Log,
                TEXT("%s restored %.1f health during the Active phase."),
                GetOwner() ? *GetOwner()->GetName() : TEXT("Player"),
                RestoredHealth
            );
        }
        break;
    default:
        break;
    }
}

void UCombatComponent::EndBlockToRecovery()
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
    SetActionPhase(ECombatActionPhase::Recovery);
    SchedulePhaseTimer(
        &UCombatComponent::FinishCurrentAction,
        ActiveTiming.RecoveryDuration
    );
}

void UCombatComponent::EndDodgeToRecovery()
{
    if (GetCurrentPhase() != ECombatActionPhase::Dodging)
    {
        return;
    }

    if (HealthComponent)
    {
        HealthComponent->SetInvulnerable(false);
    }
    SetActionPhase(ECombatActionPhase::Recovery);
    SchedulePhaseTimer(
        &UCombatComponent::FinishCurrentAction,
        ActiveTiming.RecoveryDuration
    );
}

void UCombatComponent::SetActionPhase(
    const ECombatActionPhase NewPhase
)
{
    const ECombatActionPhase PreviousPhase = ActionState.GetPhase();
    if (PreviousPhase == NewPhase)
    {
        return;
    }

    ActionState.TransitionTo(NewPhase);
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Movement =
            Character->GetCharacterMovement())
        {
            if (!AdaptiveCombat::IsMovementAllowed(NewPhase))
            {
                Movement->StopMovementImmediately();
            }
            if (bHasCachedRotationPolicy)
            {
                Movement->bOrientRotationToMovement =
                    AdaptiveCombat::IsRotationAllowed(NewPhase)
                    && bCachedOrientRotationToMovement;
            }
        }
    }
    OnActionPhaseChanged.Broadcast(PreviousPhase, NewPhase);
}

void UCombatComponent::SchedulePhaseTimer(
    void (UCombatComponent::*Callback)(),
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

void UCombatComponent::CancelCurrentAction(
    const ECombatActionPhase FinalPhase
)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ActionPhaseTimer);
    }
    ClearTemporaryStates(true);
    ClearBufferedInput();
    ActiveAction = EPlayerCombatAction::None;
    ActiveTiming = FCombatActionTiming(0.0f, 0.0f, 0.0f);
    PendingDodgeDirection = FVector::ZeroVector;
    ActionState.Reset(ActionState.GetPhase());
    SetActionPhase(FinalPhase);
}

void UCombatComponent::ClearTemporaryStates(
    const bool bBroadcastBlockingChange
)
{
    const bool bWasBlocking = bBlocking;
    bBlocking = false;
    if (HealthComponent)
    {
        HealthComponent->SetDamageReduction(0.0f);
        HealthComponent->SetInvulnerable(false);
    }
    if (bWasBlocking && bBroadcastBlockingChange)
    {
        OnBlockingChanged.Broadcast(this, false);
    }
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

void UCombatComponent::HandleHealthChanged(
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

void UCombatComponent::HandleOwnerDeath(UHealthComponent*, AActor*)
{
    CancelCurrentAction(ECombatActionPhase::Dead);
}

double UCombatComponent::GetCombatTimeSeconds() const
{
    const UWorld* World = GetWorld();
    return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}
