#include "Components/EnemyLocomotionComponent.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/HealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/AdaptiveHuntTuningSettings.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

UEnemyLocomotionComponent::UEnemyLocomotionComponent()
    : OwnerCharacter(nullptr)
    , TargetActor(nullptr)
    , EnemyCombatComponent(nullptr)
    , HealthComponent(nullptr)
    , ActiveIntent(EEnemyLocomotionIntent::None)
    , bUsingNavigation(false)
    , StuckRecoveryCount(0)
    , bLocomotionEnabled(true)
    , bWasTemporarilySuppressed(false)
    , bRecoveryRight(false)
    , LastProgressLocation(FVector::ZeroVector)
    , LastProgressTime(0.0)
    , RecoveryEndTime(0.0)
    , NextNavigationRequestTime(0.0)
{
    const FAdaptiveMovementTuning Tuning =
        UAdaptiveHuntTuningSettings::Get().Movement.GetSanitized();
    PreferredRange = Tuning.EnemyPreferredRange;
    RetreatRange = Tuning.EnemyRetreatRange;
    OrbitRange = Tuning.EnemyOrbitRange;
    AcceptanceRadius = Tuning.EnemyAcceptanceRadius;
    MovementAcceleration = Tuning.EnemyAcceleration;
    TurnSpeed = Tuning.EnemyTurnSpeed;
    MovementSpeed = Tuning.EnemyMovementSpeed;
    NavigationRefreshInterval = Tuning.EnemyNavigationRefreshInterval;
    OrbitStepDistance = Tuning.EnemyOrbitStepDistance;
    CollisionProbeDistance = Tuning.EnemyCollisionProbeDistance;
    StuckTimeout = Tuning.EnemyStuckTimeout;
    MinimumProgressDistance = Tuning.EnemyMinimumProgressDistance;
    StuckRecoveryDuration = Tuning.EnemyStuckRecoveryDuration;

    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEnemyLocomotionComponent::BeginPlay()
{
    Super::BeginPlay();
    CacheOwnerComponents();
    ApplyMovementSettings();
    EnsureController();
    ResetProgressTracking();
}

void UEnemyLocomotionComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    StopDrivenMovement(true);
    TargetActor = nullptr;
    ActiveIntent = EEnemyLocomotionIntent::None;
    SetComponentTickEnabled(false);
    Super::EndPlay(EndPlayReason);
}

void UEnemyLocomotionComponent::TickComponent(
    const float DeltaTime,
    const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    CacheOwnerComponents();
    if (!OwnerCharacter || !IsValid(TargetActor)
        || !FEnemyMovementPolicy::IsMovementIntent(ActiveIntent))
    {
        ClearMovementIntent();
        return;
    }

    FaceTarget(DeltaTime);
    if (!CanDriveLocomotion())
    {
        if (!bWasTemporarilySuppressed)
        {
            const bool bPreserveActionVelocity = EnemyCombatComponent
                && EnemyCombatComponent->ShouldPreserveCommittedMovement();
            StopDrivenMovement(!bPreserveActionVelocity);
        }
        bWasTemporarilySuppressed = true;
        ResetProgressTracking();
        return;
    }

    bWasTemporarilySuppressed = false;
    const double CurrentTime = GetLocomotionTimeSeconds();
    UpdateStuckRecovery(CurrentTime);
    if (CurrentTime < RecoveryEndTime)
    {
        const FVector ToTarget = (
            TargetActor->GetActorLocation()
            - OwnerCharacter->GetActorLocation()
        ).GetSafeNormal2D();
        const FVector Right = FVector::CrossProduct(
            FVector::UpVector,
            ToTarget
        ).GetSafeNormal2D();
        const FVector RecoveryDirection = (
            (bRecoveryRight ? Right : -Right) - ToTarget * 0.35f
        ).GetSafeNormal2D();
        ApplyDirectMovement(RecoveryDirection);
        return;
    }

    if (CurrentTime >= NextNavigationRequestTime
        && TryRequestNavigation(CurrentTime))
    {
        return;
    }
    if (bUsingNavigation)
    {
        return;
    }

    ApplyDirectMovement(ComputeDesiredDirection());
}

void UEnemyLocomotionComponent::RequestMovementIntent(
    const EEnemyLocomotionIntent NewIntent,
    AActor* NewTargetActor
)
{
    if (!FEnemyMovementPolicy::IsMovementIntent(NewIntent)
        || !IsValid(NewTargetActor) || NewTargetActor == GetOwner())
    {
        ClearMovementIntent();
        return;
    }

    CacheOwnerComponents();
    EnsureController();
    const bool bRequestChanged = ActiveIntent != NewIntent
        || TargetActor != NewTargetActor;
    ActiveIntent = NewIntent;
    TargetActor = NewTargetActor;
    if (bRequestChanged)
    {
        StopDrivenMovement(false);
        ResetProgressTracking();
        NextNavigationRequestTime = 0.0;
    }
    if (bLocomotionEnabled)
    {
        SetComponentTickEnabled(true);
    }
}

void UEnemyLocomotionComponent::ClearMovementIntent()
{
    StopDrivenMovement(true);
    ActiveIntent = EEnemyLocomotionIntent::None;
    TargetActor = nullptr;
    RecoveryEndTime = 0.0;
    bWasTemporarilySuppressed = false;
    SetComponentTickEnabled(false);
}

void UEnemyLocomotionComponent::ResetLocomotionState()
{
    ClearMovementIntent();
    StuckRecoveryCount = 0;
    bRecoveryRight = false;
    NextNavigationRequestTime = 0.0;
    ResetProgressTracking();
}

void UEnemyLocomotionComponent::SetLocomotionEnabled(const bool bEnabled)
{
    bLocomotionEnabled = bEnabled;
    if (!bEnabled)
    {
        ClearMovementIntent();
    }
    else if (FEnemyMovementPolicy::IsMovementIntent(ActiveIntent)
        && IsValid(TargetActor))
    {
        SetComponentTickEnabled(true);
    }
}

bool UEnemyLocomotionComponent::IsLocomotionEnabled() const
{
    return bLocomotionEnabled;
}

EEnemyLocomotionIntent UEnemyLocomotionComponent::GetActiveIntent() const
{
    return ActiveIntent;
}

bool UEnemyLocomotionComponent::IsUsingNavigation() const
{
    return bUsingNavigation;
}

bool UEnemyLocomotionComponent::HasUsableNavigationData() const
{
    const ACharacter* Character = OwnerCharacter
        ? OwnerCharacter.Get()
        : Cast<ACharacter>(GetOwner());
    const AAIController* AIController = Character
        ? Cast<AAIController>(Character->GetController())
        : nullptr;
    const UWorld* World = GetWorld();
    const UCharacterMovementComponent* Movement = Character
        ? Character->GetCharacterMovement()
        : nullptr;
    if (!CanAttemptNavigation(AIController != nullptr, World != nullptr)
        || !Movement)
    {
        return false;
    }

    const UNavigationSystemV1* NavigationSystem =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    return NavigationSystem
        && NavigationSystem->GetNavDataForProps(
            Movement->GetNavAgentPropertiesRef(),
            Character->GetActorLocation()
        ) != nullptr;
}

float UEnemyLocomotionComponent::GetPreferredRange() const
{
    return PreferredRange;
}

float UEnemyLocomotionComponent::GetRetreatRange() const
{
    return RetreatRange;
}

float UEnemyLocomotionComponent::GetOrbitRange() const
{
    return OrbitRange;
}

float UEnemyLocomotionComponent::GetAcceptanceRadius() const
{
    return AcceptanceRadius;
}

float UEnemyLocomotionComponent::GetMovementAcceleration() const
{
    return MovementAcceleration;
}

float UEnemyLocomotionComponent::GetTurnSpeed() const
{
    return TurnSpeed;
}

float UEnemyLocomotionComponent::GetMovementSpeed() const
{
    return MovementSpeed;
}

int32 UEnemyLocomotionComponent::GetStuckRecoveryCount() const
{
    return StuckRecoveryCount;
}

bool UEnemyLocomotionComponent::CanAttemptNavigation(
    const bool bHasAIController,
    const bool bHasNavigationData
)
{
    return bHasAIController && bHasNavigationData;
}

bool UEnemyLocomotionComponent::ShouldTriggerStuckRecovery(
    const float TimeWithoutProgress,
    const float DistanceMoved,
    const float InStuckTimeout,
    const float InMinimumProgressDistance
)
{
    if (!FMath::IsFinite(TimeWithoutProgress)
        || !FMath::IsFinite(DistanceMoved)
        || !FMath::IsFinite(InStuckTimeout)
        || !FMath::IsFinite(InMinimumProgressDistance))
    {
        return false;
    }

    const float SafeTimeout = FMath::Max(0.05f, InStuckTimeout);
    const float SafeMinimumProgress = FMath::Max(
        0.0f,
        InMinimumProgressDistance
    );
    return TimeWithoutProgress >= SafeTimeout
        && FMath::Max(0.0f, DistanceMoved) < SafeMinimumProgress;
}

void UEnemyLocomotionComponent::CacheOwnerComponents()
{
    if (!OwnerCharacter)
    {
        OwnerCharacter = Cast<ACharacter>(GetOwner());
    }
    AActor* Owner = GetOwner();
    if (Owner && !EnemyCombatComponent)
    {
        EnemyCombatComponent =
            Owner->FindComponentByClass<UEnemyCombatComponent>();
    }
    if (Owner && !HealthComponent)
    {
        HealthComponent = Owner->FindComponentByClass<UHealthComponent>();
    }
}

void UEnemyLocomotionComponent::ApplyMovementSettings()
{
    UCharacterMovementComponent* Movement = OwnerCharacter
        ? OwnerCharacter->GetCharacterMovement()
        : nullptr;
    if (!Movement)
    {
        return;
    }

    Movement->MaxWalkSpeed = FMath::Max(0.0f, MovementSpeed);
    Movement->MaxAcceleration = FMath::Max(0.0f, MovementAcceleration);
    Movement->bOrientRotationToMovement = false;
}

void UEnemyLocomotionComponent::EnsureController()
{
    if (OwnerCharacter && OwnerCharacter->HasAuthority()
        && !OwnerCharacter->GetController())
    {
        OwnerCharacter->SpawnDefaultController();
    }
}

bool UEnemyLocomotionComponent::CanDriveLocomotion() const
{
    return bLocomotionEnabled && OwnerCharacter && IsValid(TargetActor)
        && (!HealthComponent || !HealthComponent->IsDead())
        && (!EnemyCombatComponent
            || (EnemyCombatComponent->IsCombatEnabled()
                && !EnemyCombatComponent->IsCommittedActionActive()));
}

void UEnemyLocomotionComponent::FaceTarget(const float DeltaTime) const
{
    if (!OwnerCharacter || !TargetActor || TurnSpeed <= 0.0f)
    {
        return;
    }
    if (EnemyCombatComponent
        && !EnemyCombatComponent->IsRotationAllowed())
    {
        return;
    }

    const FVector ToTarget = TargetActor->GetActorLocation()
        - OwnerCharacter->GetActorLocation();
    if (ToTarget.IsNearlyZero())
    {
        return;
    }

    const FRotator DesiredRotation(0.0f, ToTarget.Rotation().Yaw, 0.0f);
    OwnerCharacter->SetActorRotation(FMath::RInterpConstantTo(
        OwnerCharacter->GetActorRotation(),
        DesiredRotation,
        FMath::Max(0.0f, DeltaTime),
        FMath::Max(0.0f, TurnSpeed)
    ));
}

FVector UEnemyLocomotionComponent::ComputeDesiredDirection() const
{
    if (!OwnerCharacter || !TargetActor)
    {
        return FVector::ZeroVector;
    }

    const FVector ToTargetVector = TargetActor->GetActorLocation()
        - OwnerCharacter->GetActorLocation();
    const float Distance = ToTargetVector.Size2D();
    const FVector ToTarget = ToTargetVector.GetSafeNormal2D();
    const FVector Right = FVector::CrossProduct(
        FVector::UpVector,
        ToTarget
    ).GetSafeNormal2D();

    switch (ActiveIntent)
    {
    case EEnemyLocomotionIntent::Approach:
        return ToTarget;
    case EEnemyLocomotionIntent::Retreat:
        return -ToTarget;
    case EEnemyLocomotionIntent::OrbitLeft:
    case EEnemyLocomotionIntent::OrbitRight:
    {
        const FVector Tangent = ActiveIntent
                == EEnemyLocomotionIntent::OrbitRight
            ? Right
            : -Right;
        const float RangeCorrection = FMath::Clamp(
            (Distance - FMath::Max(0.0f, PreferredRange))
                / FMath::Max(1.0f, PreferredRange * 0.35f),
            -1.0f,
            1.0f
        );
        return (Tangent + ToTarget * RangeCorrection * 0.65f)
            .GetSafeNormal2D();
    }
    default:
        return FVector::ZeroVector;
    }
}

FVector UEnemyLocomotionComponent::ComputeNavigationDestination() const
{
    if (!OwnerCharacter || !TargetActor)
    {
        return FVector::ZeroVector;
    }

    const FVector OwnerLocation = OwnerCharacter->GetActorLocation();
    const FVector TargetLocation = TargetActor->GetActorLocation();
    const FVector ToTarget = (TargetLocation - OwnerLocation)
        .GetSafeNormal2D();
    const FVector RadialFromTarget = -ToTarget;
    if (ActiveIntent == EEnemyLocomotionIntent::Approach
        || ActiveIntent == EEnemyLocomotionIntent::Retreat)
    {
        return TargetLocation
            + RadialFromTarget * FMath::Max(0.0f, PreferredRange);
    }

    const FVector Right = FVector::CrossProduct(
        FVector::UpVector,
        ToTarget
    ).GetSafeNormal2D();
    const FVector Tangent = ActiveIntent == EEnemyLocomotionIntent::OrbitRight
        ? Right
        : -Right;
    return TargetLocation
        + RadialFromTarget * FMath::Max(0.0f, PreferredRange)
        + Tangent * FMath::Max(AcceptanceRadius * 2.0f, OrbitStepDistance);
}

FVector UEnemyLocomotionComponent::ComputeCollisionAwareDirection(
    const FVector& DesiredDirection
) const
{
    const FVector SafeDirection = DesiredDirection.GetSafeNormal2D();
    const UCapsuleComponent* Capsule = OwnerCharacter
        ? OwnerCharacter->GetCapsuleComponent()
        : nullptr;
    UWorld* World = GetWorld();
    if (!Capsule || !World || SafeDirection.IsNearlyZero())
    {
        return SafeDirection;
    }

    FCollisionQueryParams QueryParams(
        SCENE_QUERY_STAT(EnemyDirectMovementProbe),
        false,
        OwnerCharacter
    );
    QueryParams.AddIgnoredActor(TargetActor);
    const float Radius = FMath::Max(
        1.0f,
        Capsule->GetScaledCapsuleRadius() * 0.85f
    );
    const float HalfHeight = FMath::Max(
        Radius,
        Capsule->GetScaledCapsuleHalfHeight() * 0.9f
    );
    const FVector Start = OwnerCharacter->GetActorLocation();
    const FVector End = Start
        + SafeDirection * FMath::Max(1.0f, CollisionProbeDistance);
    FHitResult Hit;
    if (!World->SweepSingleByChannel(
        Hit,
        Start,
        End,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeCapsule(Radius, HalfHeight),
        QueryParams
    ))
    {
        return SafeDirection;
    }

    return FVector::VectorPlaneProject(SafeDirection, Hit.Normal)
        .GetSafeNormal2D();
}

bool UEnemyLocomotionComponent::TryRequestNavigation(
    const double CurrentTime
)
{
    NextNavigationRequestTime = CurrentTime
        + FMath::Max(0.02f, NavigationRefreshInterval);
    AAIController* AIController = OwnerCharacter
        ? Cast<AAIController>(OwnerCharacter->GetController())
        : nullptr;
    if (!AIController || !HasUsableNavigationData())
    {
        if (bUsingNavigation && AIController)
        {
            AIController->StopMovement();
        }
        bUsingNavigation = false;
        return false;
    }

    const EPathFollowingRequestResult::Type Result =
        AIController->MoveToLocation(
            ComputeNavigationDestination(),
            FMath::Max(0.0f, AcceptanceRadius),
            false,
            true,
            true,
            true,
            nullptr,
            true
        );
    bUsingNavigation = Result != EPathFollowingRequestResult::Failed;
    return bUsingNavigation;
}

void UEnemyLocomotionComponent::ApplyDirectMovement(
    const FVector& DesiredDirection
)
{
    if (!OwnerCharacter)
    {
        return;
    }

    if (bUsingNavigation)
    {
        if (AAIController* AIController = Cast<AAIController>(
            OwnerCharacter->GetController()
        ))
        {
            AIController->StopMovement();
        }
        bUsingNavigation = false;
    }
    const FVector SafeDirection = ComputeCollisionAwareDirection(
        DesiredDirection
    );
    if (!SafeDirection.IsNearlyZero())
    {
        OwnerCharacter->AddMovementInput(SafeDirection, 1.0f, true);
    }
}

void UEnemyLocomotionComponent::StopDrivenMovement(
    const bool bStopVelocity
)
{
    if (OwnerCharacter)
    {
        if (AAIController* AIController = Cast<AAIController>(
            OwnerCharacter->GetController()
        ))
        {
            AIController->StopMovement();
        }
        if (bStopVelocity && OwnerCharacter->GetCharacterMovement())
        {
            OwnerCharacter->GetCharacterMovement()->StopMovementImmediately();
        }
    }
    bUsingNavigation = false;
}

void UEnemyLocomotionComponent::ResetProgressTracking()
{
    LastProgressLocation = OwnerCharacter
        ? OwnerCharacter->GetActorLocation()
        : FVector::ZeroVector;
    LastProgressTime = GetLocomotionTimeSeconds();
}

void UEnemyLocomotionComponent::UpdateStuckRecovery(
    const double CurrentTime
)
{
    if (!OwnerCharacter || CurrentTime < RecoveryEndTime)
    {
        return;
    }

    const float DistanceMoved = FVector::Dist2D(
        OwnerCharacter->GetActorLocation(),
        LastProgressLocation
    );
    if (DistanceMoved >= FMath::Max(0.0f, MinimumProgressDistance))
    {
        LastProgressLocation = OwnerCharacter->GetActorLocation();
        LastProgressTime = CurrentTime;
        return;
    }

    if (ShouldTriggerStuckRecovery(
        static_cast<float>(CurrentTime - LastProgressTime),
        DistanceMoved,
        StuckTimeout,
        MinimumProgressDistance
    ))
    {
        BeginStuckRecovery(CurrentTime);
    }
}

void UEnemyLocomotionComponent::BeginStuckRecovery(
    const double CurrentTime
)
{
    ++StuckRecoveryCount;
    bRecoveryRight = !bRecoveryRight;
    RecoveryEndTime = CurrentTime
        + FMath::Max(0.05f, StuckRecoveryDuration);
    StopDrivenMovement(false);
    LastProgressLocation = OwnerCharacter
        ? OwnerCharacter->GetActorLocation()
        : FVector::ZeroVector;
    LastProgressTime = CurrentTime;
    NextNavigationRequestTime = RecoveryEndTime;
}

double UEnemyLocomotionComponent::GetLocomotionTimeSeconds() const
{
    const UWorld* World = GetWorld();
    return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}
