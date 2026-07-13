#include "Components/CombatFeedbackComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CombatComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/GreyboxPresentationComponent.h"
#include "Components/HealthComponent.h"
#include "GameFramework/Character.h"
#include "Game/AdaptiveHuntTuningSettings.h"


UCombatFeedbackComponent::UCombatFeedbackComponent()
    : PlayerCombatComponent(nullptr)
    , EnemyCombatComponent(nullptr)
    , PresentationComponent(nullptr)
    , HealthComponent(nullptr)
    , CameraComponent(nullptr)
    , NeutralCameraLocation(FVector::ZeroVector)
    , CurrentCameraOffset(FVector::ZeroVector)
    , ActiveCue(ECombatFeedbackCue::None)
    , RemainingCueTime(0.0f)
    , bCameraStateCaptured(false)
    , bEventsBound(false)
{
    const FAdaptiveFeedbackTuning Tuning =
        UAdaptiveHuntTuningSettings::Get().Feedback.GetSanitized();
    FeedbackDisplayDuration = Tuning.FeedbackDisplayDuration;
    bEnableKnockback = Tuning.bEnableKnockback;
    KnockbackStrength = Tuning.KnockbackStrength;
    HeavyKnockbackMultiplier = Tuning.HeavyKnockbackMultiplier;
    BlockedKnockbackMultiplier = Tuning.BlockedKnockbackMultiplier;
    KnockbackVerticalVelocity = Tuning.KnockbackVerticalVelocity;
    MaximumKnockbackStrength = Tuning.MaximumKnockbackStrength;
    CameraImpulseScale = Tuning.CameraImpulseScale;
    CameraReturnSpeed = Tuning.CameraReturnSpeed;
    MaximumCameraOffset = Tuning.MaximumCameraOffset;

    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UCombatFeedbackComponent::NotifyAttackMissed()
{
    TriggerCue(ECombatFeedbackCue::Miss);
}

void UCombatFeedbackComponent::NotifyAttackDodged(AActor*)
{
    TriggerCue(ECombatFeedbackCue::Dodged);
}

void UCombatFeedbackComponent::NotifyDamageReceived(
    AActor* AttackInstigator,
    const bool bHeavyImpact,
    const bool bBlocked
)
{
    CacheObservedComponents();
    const ECombatFeedbackCue ImpactCue = bBlocked
        ? ECombatFeedbackCue::Blocked
        : ECombatFeedbackCue::Hit;
    QueueCameraImpulse(
        AdaptiveCombatFeedback::ResolveCameraImpulse(ImpactCue)
            * (bHeavyImpact ? 1.35f : 1.0f)
    );
    if (HealthComponent && HealthComponent->IsDead())
    {
        TriggerCue(ECombatFeedbackCue::Death);
        return;
    }

    TriggerCue(ImpactCue);

    float Multiplier = bHeavyImpact
        ? FMath::Max(1.0f, HeavyKnockbackMultiplier)
        : 1.0f;
    if (bBlocked)
    {
        Multiplier *= FMath::Clamp(
            BlockedKnockbackMultiplier,
            0.0f,
            1.0f
        );
    }
    ApplyKnockback(AttackInstigator, Multiplier);
}

void UCombatFeedbackComponent::NotifyAttackConnected(
    const bool bHeavyImpact
)
{
    QueueCameraImpulse(AdaptiveCombatFeedback::ResolveCameraImpulse(
        bHeavyImpact
            ? ECombatFeedbackCue::HeavyAttack
            : ECombatFeedbackCue::LightAttack
    ) * (bHeavyImpact ? 1.20f : 0.70f));
}

void UCombatFeedbackComponent::ResetFeedback()
{
    ActiveCue = ECombatFeedbackCue::None;
    RemainingCueTime = 0.0f;
    CurrentCameraOffset = FVector::ZeroVector;
    if (CameraComponent && bCameraStateCaptured)
    {
        CameraComponent->SetRelativeLocation(NeutralCameraLocation);
    }
    if (PresentationComponent)
    {
        PresentationComponent->TriggerFeedbackCue(
            ECombatFeedbackCue::None
        );
    }
    RefreshTickState();
}

ECombatFeedbackCue UCombatFeedbackComponent::GetActiveCue() const
{
    return RemainingCueTime > 0.0f
        ? ActiveCue
        : ECombatFeedbackCue::None;
}

bool UCombatFeedbackComponent::IsCueVisible() const
{
    return GetActiveCue() != ECombatFeedbackCue::None;
}

float UCombatFeedbackComponent::GetKnockbackStrength() const
{
    return FMath::IsFinite(KnockbackStrength)
        ? FMath::Max(0.0f, KnockbackStrength)
        : 0.0f;
}

float UCombatFeedbackComponent::GetHeavyKnockbackMultiplier() const
{
    return FMath::IsFinite(HeavyKnockbackMultiplier)
        ? FMath::Clamp(HeavyKnockbackMultiplier, 1.0f, 3.0f)
        : 1.0f;
}

float UCombatFeedbackComponent::GetBlockedKnockbackMultiplier() const
{
    return FMath::IsFinite(BlockedKnockbackMultiplier)
        ? FMath::Clamp(BlockedKnockbackMultiplier, 0.0f, 1.0f)
        : 0.0f;
}

float UCombatFeedbackComponent::GetMaximumKnockbackStrength() const
{
    return FMath::IsFinite(MaximumKnockbackStrength)
        ? FMath::Max(0.0f, MaximumKnockbackStrength)
        : 0.0f;
}

float UCombatFeedbackComponent::GetFeedbackDisplayDuration() const
{
    return FMath::IsFinite(FeedbackDisplayDuration)
        ? FMath::Clamp(FeedbackDisplayDuration, 0.05f, 1.0f)
        : 0.38f;
}

bool UCombatFeedbackComponent::IsKnockbackEnabled() const
{
    return bEnableKnockback;
}

void UCombatFeedbackComponent::BeginPlay()
{
    Super::BeginPlay();
    CacheObservedComponents();
    BindObservedEvents();
    if (CameraComponent)
    {
        NeutralCameraLocation = CameraComponent->GetRelativeLocation();
        bCameraStateCaptured = true;
    }
    ResetFeedback();
}

void UCombatFeedbackComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    UnbindObservedEvents();
    ResetFeedback();
    Super::EndPlay(EndPlayReason);
}

void UCombatFeedbackComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f)
    {
        return;
    }

    RemainingCueTime = FMath::Max(0.0f, RemainingCueTime - DeltaTime);
    if (RemainingCueTime <= 0.0f)
    {
        ActiveCue = ECombatFeedbackCue::None;
    }

    if (CameraComponent && bCameraStateCaptured)
    {
        CurrentCameraOffset = FMath::VInterpTo(
            CurrentCameraOffset,
            FVector::ZeroVector,
            DeltaTime,
            FMath::Max(0.1f, CameraReturnSpeed)
        );
        if (CurrentCameraOffset.SizeSquared() < 0.0025f)
        {
            CurrentCameraOffset = FVector::ZeroVector;
        }
        CameraComponent->SetRelativeLocation(
            NeutralCameraLocation + CurrentCameraOffset
        );
    }
    RefreshTickState();
}

void UCombatFeedbackComponent::CacheObservedComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    if (!PlayerCombatComponent)
    {
        PlayerCombatComponent = Owner->FindComponentByClass<UCombatComponent>();
    }
    if (!EnemyCombatComponent)
    {
        EnemyCombatComponent =
            Owner->FindComponentByClass<UEnemyCombatComponent>();
    }
    if (!PresentationComponent)
    {
        PresentationComponent =
            Owner->FindComponentByClass<UGreyboxPresentationComponent>();
    }
    if (!HealthComponent)
    {
        HealthComponent = Owner->FindComponentByClass<UHealthComponent>();
    }
    if (!CameraComponent)
    {
        CameraComponent = Owner->FindComponentByClass<UCameraComponent>();
    }
}

void UCombatFeedbackComponent::BindObservedEvents()
{
    if (bEventsBound)
    {
        return;
    }
    if (PlayerCombatComponent)
    {
        PlayerCombatComponent->OnActionCommitted.AddUObject(
            this,
            &UCombatFeedbackComponent::HandlePlayerActionCommitted
        );
    }
    if (EnemyCombatComponent)
    {
        EnemyCombatComponent->OnActionCommitted.AddUObject(
            this,
            &UCombatFeedbackComponent::HandleEnemyActionCommitted
        );
    }
    if (HealthComponent)
    {
        HealthComponent->OnDeath.AddUObject(
            this,
            &UCombatFeedbackComponent::HandleDeath
        );
    }
    bEventsBound = true;
}

void UCombatFeedbackComponent::UnbindObservedEvents()
{
    if (!bEventsBound)
    {
        return;
    }
    if (PlayerCombatComponent)
    {
        PlayerCombatComponent->OnActionCommitted.RemoveAll(this);
    }
    if (EnemyCombatComponent)
    {
        EnemyCombatComponent->OnActionCommitted.RemoveAll(this);
    }
    if (HealthComponent)
    {
        HealthComponent->OnDeath.RemoveAll(this);
    }
    bEventsBound = false;
}

void UCombatFeedbackComponent::TriggerCue(
    const ECombatFeedbackCue Cue
)
{
    if (Cue == ECombatFeedbackCue::None)
    {
        ResetFeedback();
        return;
    }
    if (RemainingCueTime > 0.0f
        && AdaptiveCombatFeedback::GetCuePriority(Cue)
            < AdaptiveCombatFeedback::GetCuePriority(ActiveCue))
    {
        return;
    }

    ActiveCue = Cue;
    RemainingCueTime = GetFeedbackDisplayDuration();
    if (PresentationComponent)
    {
        PresentationComponent->TriggerFeedbackCue(Cue);
    }
    RefreshTickState();
}

void UCombatFeedbackComponent::QueueCameraImpulse(
    const FVector& LocalOffset
)
{
    if (!CameraComponent || !bCameraStateCaptured
        || LocalOffset.ContainsNaN())
    {
        return;
    }
    CurrentCameraOffset += LocalOffset
        * (FMath::IsFinite(CameraImpulseScale)
            ? FMath::Max(0.0f, CameraImpulseScale)
            : 0.0f);
    CurrentCameraOffset = CurrentCameraOffset.GetClampedToMaxSize(
        FMath::IsFinite(MaximumCameraOffset)
            ? FMath::Max(0.0f, MaximumCameraOffset)
            : 0.0f
    );
    CameraComponent->SetRelativeLocation(
        NeutralCameraLocation + CurrentCameraOffset
    );
    RefreshTickState();
}

void UCombatFeedbackComponent::ApplyKnockback(
    AActor* AttackInstigator,
    const float Multiplier
)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!bEnableKnockback || !Character || !IsValid(AttackInstigator)
        || Character == AttackInstigator || (HealthComponent
            && HealthComponent->IsDead()))
    {
        return;
    }

    const FVector Velocity =
        AdaptiveCombatFeedback::ResolveKnockbackVelocity(
            Character->GetActorLocation(),
            AttackInstigator->GetActorLocation(),
            Character->GetActorForwardVector(),
            GetKnockbackStrength() * FMath::Max(0.0f, Multiplier),
            KnockbackVerticalVelocity,
            GetMaximumKnockbackStrength()
        );
    Character->LaunchCharacter(Velocity, true, false);
}

void UCombatFeedbackComponent::RefreshTickState()
{
    SetComponentTickEnabled(
        RemainingCueTime > 0.0f
        || !CurrentCameraOffset.IsNearlyZero(0.05f)
    );
}

void UCombatFeedbackComponent::HandlePlayerActionCommitted(
    UCombatComponent*,
    const EPlayerCombatAction Action
)
{
    switch (Action)
    {
    case EPlayerCombatAction::LightAttack:
        QueueCameraImpulse(AdaptiveCombatFeedback::ResolveCameraImpulse(
            ECombatFeedbackCue::LightAttack
        ));
        break;
    case EPlayerCombatAction::HeavyAttack:
        QueueCameraImpulse(AdaptiveCombatFeedback::ResolveCameraImpulse(
            ECombatFeedbackCue::HeavyAttack
        ));
        break;
    case EPlayerCombatAction::DodgeLeft:
    case EPlayerCombatAction::DodgeRight:
    case EPlayerCombatAction::DodgeBackward:
    {
        TriggerCue(ECombatFeedbackCue::Dodged);
        FVector Impulse = AdaptiveCombatFeedback::ResolveCameraImpulse(
            ECombatFeedbackCue::Dodged
        );
        if (Action == EPlayerCombatAction::DodgeLeft)
        {
            Impulse.Y *= -1.0f;
        }
        else if (Action == EPlayerCombatAction::DodgeBackward)
        {
            Impulse = FVector(5.0f, 0.0f, -2.5f);
        }
        QueueCameraImpulse(Impulse);
        break;
    }
    default:
        break;
    }
}

void UCombatFeedbackComponent::HandleEnemyActionCommitted(
    UEnemyCombatComponent*,
    const EEnemyCombatAction Action
)
{
    if (Action == EEnemyCombatAction::Dodge)
    {
        TriggerCue(ECombatFeedbackCue::Dodged);
    }
}

void UCombatFeedbackComponent::HandleDeath(UHealthComponent*, AActor*)
{
    TriggerCue(ECombatFeedbackCue::Death);
}
