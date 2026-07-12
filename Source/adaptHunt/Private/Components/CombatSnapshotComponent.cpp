#include "Components/CombatSnapshotComponent.h"

#include "adaptHunt.h"
#include "Components/CombatComponent.h"
#include "Components/EnemyCombatComponent.h"
#include "Components/HealthComponent.h"
#include "Components/StaminaComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
bool IsPlayerAttack(const EPlayerCombatAction Action)
{
    return Action == EPlayerCombatAction::LightAttack
        || Action == EPlayerCombatAction::HeavyAttack;
}

bool IsEnemyAttack(const EEnemyCombatAction Action)
{
    return Action == EEnemyCombatAction::LightAttack
        || Action == EEnemyCombatAction::HeavyAttack
        || Action == EEnemyCombatAction::ProjectileAttack
        || Action == EEnemyCombatAction::DashAttack
        || Action == EEnemyCombatAction::InterruptHeal;
}
}

UCombatSnapshotComponent::UCombatSnapshotComponent()
    : OpponentActor(nullptr)
    , PlayerHealthComponent(nullptr)
    , PlayerStaminaComponent(nullptr)
    , PlayerCombatComponent(nullptr)
    , EnemyHealthComponent(nullptr)
    , EnemyStaminaComponent(nullptr)
    , EnemyCombatComponent(nullptr)
    , CloseDistanceThreshold(300.0f)
    , FarDistanceThreshold(700.0f)
    , EnemyAttackWindow(0.5f)
    , RoundNumber(1)
    , PreviousPlayerAction(EPlayerCombatAction::None)
    , PreviousEnemyAction(EEnemyCombatAction::None)
    , TrackingStartTime(0.0)
    , LastPlayerAttackTime(0.0)
    , LastEnemyAttackTime(0.0)
    , bHasPlayerAttack(false)
    , bHasEnemyAttack(false)
    , bHasPendingPlayerActionSnapshot(false)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCombatSnapshotComponent::BeginPlay()
{
    Super::BeginPlay();

    TrackingStartTime = GetSnapshotTimeSeconds();
    LastPlayerAttackTime = TrackingStartTime;
    LastEnemyAttackTime = TrackingStartTime;

    CachePlayerComponents();
    if (PlayerCombatComponent)
    {
        PlayerCombatComponent->OnActionCommitStarted.RemoveAll(this);
        PlayerCombatComponent->OnActionCommitStarted.AddUObject(
            this,
            &UCombatSnapshotComponent::HandlePlayerActionCommitStarted
        );
        PlayerCombatComponent->OnActionCommitted.RemoveAll(this);
        PlayerCombatComponent->OnActionCommitted.AddUObject(
            this,
            &UCombatSnapshotComponent::HandlePlayerAction
        );
        PreviousPlayerAction = PlayerCombatComponent->GetLastAction();
    }

    BindOpponentCombat();

    if (!PlayerHealthComponent || !PlayerStaminaComponent
        || !PlayerCombatComponent)
    {
        UE_LOG(
            LogAdaptHunt,
            Error,
            TEXT("%s requires player health, stamina, and combat components."),
            GetOwner() ? *GetOwner()->GetName() : TEXT("CombatSnapshotComponent")
        );
    }
}

void UCombatSnapshotComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason
)
{
    if (PlayerCombatComponent)
    {
        PlayerCombatComponent->OnActionCommitStarted.RemoveAll(this);
        PlayerCombatComponent->OnActionCommitted.RemoveAll(this);
    }
    if (EnemyCombatComponent)
    {
        EnemyCombatComponent->OnActionCommitted.RemoveAll(this);
    }

    Super::EndPlay(EndPlayReason);
}

void UCombatSnapshotComponent::SetOpponentActor(AActor* NewOpponentActor)
{
    if (OpponentActor == NewOpponentActor && EnemyCombatComponent)
    {
        return;
    }

    if (EnemyCombatComponent)
    {
        EnemyCombatComponent->OnActionCommitted.RemoveAll(this);
    }

    OpponentActor = NewOpponentActor;
    EnemyHealthComponent = nullptr;
    EnemyStaminaComponent = nullptr;
    EnemyCombatComponent = nullptr;
    PreviousEnemyAction = EEnemyCombatAction::None;
    bHasEnemyAttack = false;
    LastEnemyAttackTime = GetSnapshotTimeSeconds();

    BindOpponentCombat();
}

AActor* UCombatSnapshotComponent::GetOpponentActor() const
{
    return OpponentActor;
}

void UCombatSnapshotComponent::SetRoundNumber(const int32 NewRoundNumber)
{
    RoundNumber = FMath::Max(1, NewRoundNumber);
}

int32 UCombatSnapshotComponent::GetRoundNumber() const
{
    return RoundNumber;
}

void UCombatSnapshotComponent::ResetRoundState()
{
    PreviousPlayerAction = EPlayerCombatAction::None;
    PreviousEnemyAction = EEnemyCombatAction::None;
    TrackingStartTime = GetSnapshotTimeSeconds();
    LastPlayerAttackTime = TrackingStartTime;
    LastEnemyAttackTime = TrackingStartTime;
    bHasPlayerAttack = false;
    bHasEnemyAttack = false;
    bHasPendingPlayerActionSnapshot = false;
    PendingPlayerActionSnapshot = FCombatSnapshot();
}

FCombatSnapshot UCombatSnapshotComponent::CaptureSnapshot()
{
    CachePlayerComponents();
    if (OpponentActor && !EnemyCombatComponent)
    {
        BindOpponentCombat();
    }

    FCombatSnapshot Snapshot;
    const double CurrentTime = GetSnapshotTimeSeconds();
    Snapshot.CaptureTimeSeconds = static_cast<float>(CurrentTime);
    Snapshot.RoundNumber = RoundNumber;

    if (PlayerHealthComponent)
    {
        Snapshot.PlayerHealthNormalized = FMath::Clamp(
            PlayerHealthComponent->GetNormalizedHealth(),
            0.0f,
            1.0f
        );
    }
    if (EnemyHealthComponent)
    {
        Snapshot.EnemyHealthNormalized = FMath::Clamp(
            EnemyHealthComponent->GetNormalizedHealth(),
            0.0f,
            1.0f
        );
    }
    if (PlayerStaminaComponent)
    {
        Snapshot.PlayerStaminaNormalized = FMath::Clamp(
            PlayerStaminaComponent->GetNormalizedStamina(),
            0.0f,
            1.0f
        );
    }
    if (EnemyStaminaComponent)
    {
        Snapshot.EnemyStaminaNormalized = FMath::Clamp(
            EnemyStaminaComponent->GetNormalizedStamina(),
            0.0f,
            1.0f
        );
    }

    AActor* PlayerActor = GetOwner();
    if (PlayerActor && OpponentActor)
    {
        const float Distance = FVector::Dist2D(
            PlayerActor->GetActorLocation(),
            OpponentActor->GetActorLocation()
        );
        Snapshot.DistanceCategory = ClassifyDistance(
            Distance,
            CloseDistanceThreshold,
            FarDistanceThreshold
        );
        Snapshot.RelativePlayerPosition = ClassifyRelativePosition(
            PlayerActor->GetActorLocation(),
            OpponentActor->GetActorLocation(),
            OpponentActor->GetActorForwardVector()
        );
    }

    Snapshot.PreviousPlayerAction = PreviousPlayerAction;
    Snapshot.PreviousEnemyAction = PreviousEnemyAction;
    Snapshot.TimeSincePlayerAttack = static_cast<float>(FMath::Max(
        0.0,
        CurrentTime - (
            bHasPlayerAttack ? LastPlayerAttackTime : TrackingStartTime
        )
    ));
    Snapshot.TimeSinceEnemyAttack = static_cast<float>(FMath::Max(
        0.0,
        CurrentTime - (
            bHasEnemyAttack ? LastEnemyAttackTime : TrackingStartTime
        )
    ));

    Snapshot.bHealAvailable = PlayerCombatComponent
        && PlayerCombatComponent->CanPerformAction(EPlayerCombatAction::Heal);
    Snapshot.bDodgeAvailable = PlayerCombatComponent
        && (
            PlayerCombatComponent->CanPerformAction(
                EPlayerCombatAction::DodgeLeft
            )
            || PlayerCombatComponent->CanPerformAction(
                EPlayerCombatAction::DodgeRight
            )
            || PlayerCombatComponent->CanPerformAction(
                EPlayerCombatAction::DodgeBackward
            )
        );
    Snapshot.bEnemyAttacking = bHasEnemyAttack
        && CurrentTime - LastEnemyAttackTime
            <= static_cast<double>(FMath::Max(0.0f, EnemyAttackWindow));

    return Snapshot;
}

void UCombatSnapshotComponent::LogSnapshot()
{
    const FCombatSnapshot Snapshot = CaptureSnapshot();
    const UEnum* PlayerActionEnum = StaticEnum<EPlayerCombatAction>();
    const UEnum* EnemyActionEnum = StaticEnum<EEnemyCombatAction>();
    const UEnum* DistanceEnum = StaticEnum<ECombatDistanceCategory>();
    const UEnum* PositionEnum = StaticEnum<ERelativePlayerPosition>();

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("Snapshot R%d t=%.2f | HP P/E %.2f/%.2f | STA P/E %.2f/%.2f | distance=%s position=%s | previous P/E=%s/%s | since attack P/E=%.2f/%.2f | heal=%s dodge=%s enemyAttacking=%s"),
        Snapshot.RoundNumber,
        Snapshot.CaptureTimeSeconds,
        Snapshot.PlayerHealthNormalized,
        Snapshot.EnemyHealthNormalized,
        Snapshot.PlayerStaminaNormalized,
        Snapshot.EnemyStaminaNormalized,
        DistanceEnum
            ? *DistanceEnum->GetNameStringByValue(
                static_cast<int64>(Snapshot.DistanceCategory)
            )
            : TEXT("Unknown"),
        PositionEnum
            ? *PositionEnum->GetNameStringByValue(
                static_cast<int64>(Snapshot.RelativePlayerPosition)
            )
            : TEXT("Unknown"),
        PlayerActionEnum
            ? *PlayerActionEnum->GetNameStringByValue(
                static_cast<int64>(Snapshot.PreviousPlayerAction)
            )
            : TEXT("Unknown"),
        EnemyActionEnum
            ? *EnemyActionEnum->GetNameStringByValue(
                static_cast<int64>(Snapshot.PreviousEnemyAction)
            )
            : TEXT("Unknown"),
        Snapshot.TimeSincePlayerAttack,
        Snapshot.TimeSinceEnemyAttack,
        Snapshot.bHealAvailable ? TEXT("true") : TEXT("false"),
        Snapshot.bDodgeAvailable ? TEXT("true") : TEXT("false"),
        Snapshot.bEnemyAttacking ? TEXT("true") : TEXT("false")
    );
}

float UCombatSnapshotComponent::GetCloseDistanceThreshold() const
{
    return CloseDistanceThreshold;
}

float UCombatSnapshotComponent::GetFarDistanceThreshold() const
{
    return FarDistanceThreshold;
}

float UCombatSnapshotComponent::GetEnemyAttackWindow() const
{
    return EnemyAttackWindow;
}

ECombatDistanceCategory UCombatSnapshotComponent::ClassifyDistance(
    const float Distance,
    const float CloseThreshold,
    const float FarThreshold
)
{
    const float SafeCloseThreshold = FMath::IsFinite(CloseThreshold)
        ? FMath::Max(0.0f, CloseThreshold)
        : 0.0f;
    const float SafeFarThreshold = FMath::IsFinite(FarThreshold)
        ? FMath::Max(SafeCloseThreshold, FarThreshold)
        : SafeCloseThreshold;
    if (!FMath::IsFinite(Distance))
    {
        return ECombatDistanceCategory::Far;
    }
    const float SafeDistance = FMath::Max(0.0f, Distance);

    if (SafeDistance <= SafeCloseThreshold)
    {
        return ECombatDistanceCategory::Close;
    }
    if (SafeDistance >= SafeFarThreshold)
    {
        return ECombatDistanceCategory::Far;
    }
    return ECombatDistanceCategory::Medium;
}

ERelativePlayerPosition UCombatSnapshotComponent::ClassifyRelativePosition(
    const FVector& PlayerLocation,
    const FVector& EnemyLocation,
    const FVector& EnemyForward
)
{
    if (PlayerLocation.ContainsNaN() || EnemyLocation.ContainsNaN()
        || EnemyForward.ContainsNaN())
    {
        return ERelativePlayerPosition::Front;
    }

    const FVector ToPlayer = (
        PlayerLocation - EnemyLocation
    ).GetSafeNormal2D();
    const FVector Forward = EnemyForward.GetSafeNormal2D();
    if (ToPlayer.IsNearlyZero() || Forward.IsNearlyZero())
    {
        return ERelativePlayerPosition::Front;
    }

    const FVector Right = FVector::CrossProduct(
        FVector::UpVector,
        Forward
    ).GetSafeNormal();
    const float ForwardDot = FVector::DotProduct(Forward, ToPlayer);
    const float RightDot = FVector::DotProduct(Right, ToPlayer);

    if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
    {
        return ForwardDot >= 0.0f
            ? ERelativePlayerPosition::Front
            : ERelativePlayerPosition::Behind;
    }

    return RightDot >= 0.0f
        ? ERelativePlayerPosition::Right
        : ERelativePlayerPosition::Left;
}

void UCombatSnapshotComponent::CachePlayerComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    if (!PlayerHealthComponent)
    {
        PlayerHealthComponent = Owner->FindComponentByClass<UHealthComponent>();
    }
    if (!PlayerStaminaComponent)
    {
        PlayerStaminaComponent = Owner->FindComponentByClass<UStaminaComponent>();
    }
    if (!PlayerCombatComponent)
    {
        PlayerCombatComponent = Owner->FindComponentByClass<UCombatComponent>();
    }
}

void UCombatSnapshotComponent::BindOpponentCombat()
{
    if (!OpponentActor)
    {
        return;
    }

    EnemyHealthComponent =
        OpponentActor->FindComponentByClass<UHealthComponent>();
    EnemyStaminaComponent =
        OpponentActor->FindComponentByClass<UStaminaComponent>();
    EnemyCombatComponent =
        OpponentActor->FindComponentByClass<UEnemyCombatComponent>();

    if (EnemyCombatComponent)
    {
        EnemyCombatComponent->OnActionCommitted.RemoveAll(this);
        EnemyCombatComponent->OnActionCommitted.AddUObject(
            this,
            &UCombatSnapshotComponent::HandleEnemyAction
        );
        PreviousEnemyAction = EnemyCombatComponent->GetLastAction();
    }
}

void UCombatSnapshotComponent::HandlePlayerAction(
    UCombatComponent*,
    const EPlayerCombatAction Action
)
{
    if (!bHasPendingPlayerActionSnapshot)
    {
        PendingPlayerActionSnapshot = CaptureSnapshot();
    }

    OnPlayerActionObserved.Broadcast(PendingPlayerActionSnapshot, Action);
    bHasPendingPlayerActionSnapshot = false;

    PreviousPlayerAction = Action;
    if (IsPlayerAttack(Action))
    {
        LastPlayerAttackTime = GetSnapshotTimeSeconds();
        bHasPlayerAttack = true;
    }
}

void UCombatSnapshotComponent::HandlePlayerActionCommitStarted(
    UCombatComponent*,
    const EPlayerCombatAction
)
{
    PendingPlayerActionSnapshot = CaptureSnapshot();
    bHasPendingPlayerActionSnapshot = true;
}

void UCombatSnapshotComponent::HandleEnemyAction(
    UEnemyCombatComponent*,
    const EEnemyCombatAction Action
)
{
    PreviousEnemyAction = Action;
    if (IsEnemyAttack(Action))
    {
        LastEnemyAttackTime = GetSnapshotTimeSeconds();
        bHasEnemyAttack = true;
    }
}

double UCombatSnapshotComponent::GetSnapshotTimeSeconds() const
{
    const UWorld* World = GetWorld();
    return World ? static_cast<double>(World->GetTimeSeconds()) : 0.0;
}
