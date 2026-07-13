#include "Components/StaminaComponent.h"

#include "adaptHunt.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UStaminaComponent::UStaminaComponent()
    : MaxStamina(100.0f)
    , CurrentStamina(100.0f)
    , RegenerationRate(20.0f)
    , RegenerationDelay(1.0f)
    , RegenerationInterval(0.1f)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UStaminaComponent::BeginPlay()
{
    Super::BeginPlay();
    MaxStamina = FMath::IsFinite(MaxStamina)
        ? FMath::Max(1.0f, MaxStamina)
        : 100.0f;
    RegenerationRate = FMath::IsFinite(RegenerationRate)
        ? FMath::Max(0.0f, RegenerationRate)
        : 0.0f;
    RegenerationDelay = FMath::IsFinite(RegenerationDelay)
        ? FMath::Max(0.0f, RegenerationDelay)
        : 0.0f;
    RegenerationInterval = FMath::IsFinite(RegenerationInterval)
        ? FMath::Max(0.01f, RegenerationInterval)
        : 0.1f;
    CurrentStamina = MaxStamina;
}

void UStaminaComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearRegenerationTimers();
    Super::EndPlay(EndPlayReason);
}

float UStaminaComponent::GetCurrentStamina() const
{
    return CurrentStamina;
}

float UStaminaComponent::GetMaxStamina() const
{
    return MaxStamina;
}

float UStaminaComponent::GetNormalizedStamina() const
{
    return FMath::IsFinite(CurrentStamina) && FMath::IsFinite(MaxStamina)
        && MaxStamina > 0.0f
        ? FMath::Clamp(CurrentStamina / MaxStamina, 0.0f, 1.0f)
        : 0.0f;
}

float UStaminaComponent::GetRegenerationRate() const
{
    return RegenerationRate;
}

float UStaminaComponent::GetRegenerationDelay() const
{
    return RegenerationDelay;
}

bool UStaminaComponent::IsDepleted() const
{
    return CurrentStamina <= 0.0f;
}

bool UStaminaComponent::IsRegenerating() const
{
    const UWorld* World = GetWorld();
    return World
        && World->GetTimerManager().IsTimerActive(RegenerationTimer);
}

void UStaminaComponent::SetMaxStamina(const float NewMaxStamina)
{
    ClearRegenerationTimers();
    MaxStamina = FMath::IsFinite(NewMaxStamina)
        ? FMath::Max(1.0f, NewMaxStamina)
        : 100.0f;
    SetCurrentStamina(MaxStamina);
}

bool UStaminaComponent::TryConsumeStamina(const float Cost)
{
    if (!FMath::IsFinite(Cost) || Cost < 0.0f
        || Cost > CurrentStamina)
    {
        return false;
    }

    if (FMath::IsNearlyZero(Cost))
    {
        return true;
    }

    SetCurrentStamina(CurrentStamina - Cost);
    RestartRegenerationDelay();

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("%s spent %.1f stamina; %.1f remains."),
        GetOwner() ? *GetOwner()->GetName() : TEXT("Unowned StaminaComponent"),
        Cost,
        CurrentStamina
    );

    return true;
}

float UStaminaComponent::RestoreStamina(const float Amount)
{
    if (!FMath::IsFinite(Amount) || Amount <= 0.0f)
    {
        return 0.0f;
    }

    const float OldStamina = CurrentStamina;
    SetCurrentStamina(CurrentStamina + Amount);
    return CurrentStamina - OldStamina;
}

void UStaminaComponent::ResetStamina()
{
    ClearRegenerationTimers();
    MaxStamina = FMath::IsFinite(MaxStamina)
        ? FMath::Max(1.0f, MaxStamina)
        : 100.0f;
    SetCurrentStamina(MaxStamina);
}

void UStaminaComponent::RestartRegenerationDelay()
{
    UWorld* World = GetWorld();
    if (!World || !FMath::IsFinite(RegenerationRate)
        || RegenerationRate <= 0.0f || CurrentStamina >= MaxStamina)
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();
    TimerManager.ClearTimer(RegenerationDelayTimer);
    TimerManager.ClearTimer(RegenerationTimer);

    if (RegenerationDelay <= 0.0f)
    {
        BeginRegeneration();
        return;
    }

    TimerManager.SetTimer(
        RegenerationDelayTimer,
        this,
        &UStaminaComponent::BeginRegeneration,
        RegenerationDelay,
        false
    );
}

void UStaminaComponent::BeginRegeneration()
{
    UWorld* World = GetWorld();
    if (!World || !FMath::IsFinite(RegenerationRate)
        || RegenerationRate <= 0.0f || CurrentStamina >= MaxStamina)
    {
        return;
    }

    const float Interval = FMath::Max(0.01f, RegenerationInterval);
    World->GetTimerManager().SetTimer(
        RegenerationTimer,
        this,
        &UStaminaComponent::RegenerateStamina,
        Interval,
        true
    );

    UE_LOG(
        LogAdaptHunt,
        Verbose,
        TEXT("%s began regenerating stamina."),
        GetOwner() ? *GetOwner()->GetName() : TEXT("Unowned StaminaComponent")
    );
}

void UStaminaComponent::RegenerateStamina()
{
    const float Interval = FMath::Max(0.01f, RegenerationInterval);
    RestoreStamina(RegenerationRate * Interval);

    if (CurrentStamina >= MaxStamina)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(RegenerationTimer);
        }

        UE_LOG(
            LogAdaptHunt,
            Verbose,
            TEXT("%s fully regenerated stamina."),
            GetOwner() ? *GetOwner()->GetName() : TEXT("Unowned StaminaComponent")
        );
    }
}

void UStaminaComponent::SetCurrentStamina(const float NewStamina)
{
    if (!FMath::IsFinite(NewStamina))
    {
        return;
    }

    const float OldStamina = CurrentStamina;
    CurrentStamina = FMath::Clamp(
        NewStamina,
        0.0f,
        FMath::Max(1.0f, MaxStamina)
    );

    if (FMath::IsNearlyEqual(OldStamina, CurrentStamina))
    {
        return;
    }

    OnStaminaChanged.Broadcast(this, OldStamina, CurrentStamina);

    if (OldStamina > 0.0f && IsDepleted())
    {
        OnStaminaDepleted.Broadcast(this);
    }

    if (OldStamina < MaxStamina && CurrentStamina >= MaxStamina)
    {
        OnStaminaFullyRestored.Broadcast(this);
    }
}

void UStaminaComponent::ClearRegenerationTimers()
{
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(RegenerationDelayTimer);
        TimerManager.ClearTimer(RegenerationTimer);
    }
}
