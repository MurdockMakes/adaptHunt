#include "Components/HealthComponent.h"

#include "adaptHunt.h"
#include "GameFramework/Actor.h"

UHealthComponent::UHealthComponent()
    : MaxHealth(100.0f)
    , CurrentHealth(100.0f)
    , DamageReduction(0.0f)
    , bInvulnerable(false)
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    MaxHealth = FMath::IsFinite(MaxHealth)
        ? FMath::Max(1.0f, MaxHealth)
        : 100.0f;
    CurrentHealth = MaxHealth;
    DamageReduction = FMath::IsFinite(DamageReduction)
        ? FMath::Clamp(DamageReduction, 0.0f, 1.0f)
        : 0.0f;

    if (AActor* Owner = GetOwner())
    {
        Owner->OnTakeAnyDamage.AddDynamic(
            this,
            &UHealthComponent::HandleOwnerTakeAnyDamage
        );
    }
}

void UHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AActor* Owner = GetOwner())
    {
        Owner->OnTakeAnyDamage.RemoveDynamic(
            this,
            &UHealthComponent::HandleOwnerTakeAnyDamage
        );
    }

    Super::EndPlay(EndPlayReason);
}

float UHealthComponent::GetCurrentHealth() const
{
    return CurrentHealth;
}

float UHealthComponent::GetMaxHealth() const
{
    return MaxHealth;
}

float UHealthComponent::GetNormalizedHealth() const
{
    return FMath::IsFinite(CurrentHealth) && FMath::IsFinite(MaxHealth)
        && MaxHealth > 0.0f
        ? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f)
        : 0.0f;
}

float UHealthComponent::GetDamageReduction() const
{
    return DamageReduction;
}

bool UHealthComponent::IsDead() const
{
    return CurrentHealth <= 0.0f;
}

bool UHealthComponent::IsInvulnerable() const
{
    return bInvulnerable;
}

void UHealthComponent::SetDamageReduction(const float NewDamageReduction)
{
    DamageReduction = FMath::IsFinite(NewDamageReduction)
        ? FMath::Clamp(NewDamageReduction, 0.0f, 1.0f)
        : 0.0f;
}

void UHealthComponent::SetInvulnerable(const bool bNewInvulnerable)
{
    bInvulnerable = bNewInvulnerable;
}

void UHealthComponent::SetMaxHealth(const float NewMaxHealth)
{
    MaxHealth = FMath::IsFinite(NewMaxHealth)
        ? FMath::Max(1.0f, NewMaxHealth)
        : 100.0f;
    SetCurrentHealth(MaxHealth, nullptr);
}

float UHealthComponent::ApplyDamage(
    const float DamageAmount,
    AActor* DamageCauser
)
{
    if (!FMath::IsFinite(DamageAmount) || DamageAmount <= 0.0f
        || IsDead() || bInvulnerable)
    {
        return 0.0f;
    }

    const float MitigatedDamage =
        DamageAmount * (1.0f - DamageReduction);
    if (!FMath::IsFinite(MitigatedDamage) || MitigatedDamage <= 0.0f)
    {
        return 0.0f;
    }

    const float OldHealth = CurrentHealth;
    SetCurrentHealth(CurrentHealth - MitigatedDamage, DamageCauser);
    return OldHealth - CurrentHealth;
}

float UHealthComponent::Heal(const float HealAmount)
{
    if (!FMath::IsFinite(HealAmount) || HealAmount <= 0.0f || IsDead())
    {
        return 0.0f;
    }

    const float OldHealth = CurrentHealth;
    SetCurrentHealth(CurrentHealth + HealAmount, nullptr);
    return CurrentHealth - OldHealth;
}

void UHealthComponent::ResetHealth()
{
    MaxHealth = FMath::IsFinite(MaxHealth)
        ? FMath::Max(1.0f, MaxHealth)
        : 100.0f;
    DamageReduction = 0.0f;
    bInvulnerable = false;
    SetCurrentHealth(MaxHealth, nullptr);
}

void UHealthComponent::HandleOwnerTakeAnyDamage(
    AActor* DamagedActor,
    const float Damage,
    const UDamageType* DamageType,
    AController* InstigatedBy,
    AActor* DamageCauser
)
{
    ApplyDamage(Damage, DamageCauser);
}

void UHealthComponent::SetCurrentHealth(
    const float NewHealth,
    AActor* DamageCauser
)
{
    if (!FMath::IsFinite(NewHealth))
    {
        return;
    }

    const float OldHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(NewHealth, 0.0f, FMath::Max(1.0f, MaxHealth));

    if (FMath::IsNearlyEqual(OldHealth, CurrentHealth))
    {
        return;
    }

    OnHealthChanged.Broadcast(this, OldHealth, CurrentHealth);

    UE_LOG(
        LogAdaptHunt,
        Log,
        TEXT("%s health changed: %.1f -> %.1f"),
        GetOwner() ? *GetOwner()->GetName() : TEXT("Unowned HealthComponent"),
        OldHealth,
        CurrentHealth
    );

    if (OldHealth > 0.0f && IsDead())
    {
        UE_LOG(
            LogAdaptHunt,
            Log,
            TEXT("%s died."),
            GetOwner() ? *GetOwner()->GetName() : TEXT("Unowned HealthComponent")
        );
        OnDeath.Broadcast(this, DamageCauser);
    }
}
