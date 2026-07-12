#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/HealthComponent.h"
#include "Components/StaminaComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveHealthComponentTest,
    "adaptHunt.Milestone3.HealthComponent",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveHealthComponentTest::RunTest(const FString& Parameters)
{
    UHealthComponent* Health = NewObject<UHealthComponent>();
    TestNotNull(TEXT("Health component can be created"), Health);
    if (!Health)
    {
        return false;
    }

    int32 ChangeEvents = 0;
    int32 DeathEvents = 0;
    Health->OnHealthChanged.AddLambda(
        [&ChangeEvents](UHealthComponent*, float, float)
        {
            ++ChangeEvents;
        }
    );
    Health->OnDeath.AddLambda(
        [&DeathEvents](UHealthComponent*, AActor*)
        {
            ++DeathEvents;
        }
    );

    TestTrue(
        TEXT("Health starts full"),
        FMath::IsNearlyEqual(Health->GetCurrentHealth(), 100.0f)
    );
    TestTrue(
        TEXT("Controlled damage reports the applied amount"),
        FMath::IsNearlyEqual(Health->ApplyDamage(30.0f), 30.0f)
    );
    TestTrue(
        TEXT("Controlled damage reduces health"),
        FMath::IsNearlyEqual(Health->GetCurrentHealth(), 70.0f)
    );
    TestTrue(
        TEXT("Lethal damage is clamped to remaining health"),
        FMath::IsNearlyEqual(Health->ApplyDamage(100.0f), 70.0f)
    );
    TestTrue(TEXT("Lethal damage marks the owner dead"), Health->IsDead());
    TestEqual(TEXT("Death broadcasts exactly once"), DeathEvents, 1);

    Health->ApplyDamage(10.0f);
    TestEqual(TEXT("Damage after death does not rebroadcast"), DeathEvents, 1);
    TestEqual(TEXT("Two health changes were broadcast"), ChangeEvents, 2);

    Health->ResetHealth();
    TestFalse(TEXT("Reset clears death for a new round"), Health->IsDead());
    TestTrue(
        TEXT("Reset restores maximum health"),
        FMath::IsNearlyEqual(
            Health->GetCurrentHealth(),
            Health->GetMaxHealth()
        )
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveStaminaComponentTest,
    "adaptHunt.Milestone3.StaminaComponent",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveStaminaComponentTest::RunTest(const FString& Parameters)
{
    UStaminaComponent* Stamina = NewObject<UStaminaComponent>();
    TestNotNull(TEXT("Stamina component can be created"), Stamina);
    if (!Stamina)
    {
        return false;
    }

    int32 DepletedEvents = 0;
    int32 FullyRestoredEvents = 0;
    Stamina->OnStaminaDepleted.AddLambda(
        [&DepletedEvents](UStaminaComponent*)
        {
            ++DepletedEvents;
        }
    );
    Stamina->OnStaminaFullyRestored.AddLambda(
        [&FullyRestoredEvents](UStaminaComponent*)
        {
            ++FullyRestoredEvents;
        }
    );

    TestTrue(
        TEXT("Stamina starts full"),
        FMath::IsNearlyEqual(Stamina->GetCurrentStamina(), 100.0f)
    );
    TestTrue(TEXT("Affordable stamina cost succeeds"), Stamina->TryConsumeStamina(35.0f));
    TestTrue(
        TEXT("Successful cost is deducted exactly"),
        FMath::IsNearlyEqual(Stamina->GetCurrentStamina(), 65.0f)
    );
    TestFalse(
        TEXT("Unaffordable stamina cost fails"),
        Stamina->TryConsumeStamina(70.0f)
    );
    TestTrue(
        TEXT("A failed cost does not change stamina"),
        FMath::IsNearlyEqual(Stamina->GetCurrentStamina(), 65.0f)
    );
    TestTrue(TEXT("Exact remaining cost succeeds"), Stamina->TryConsumeStamina(65.0f));
    TestTrue(TEXT("Exact remaining cost depletes stamina"), Stamina->IsDepleted());
    TestEqual(TEXT("Depletion broadcasts once"), DepletedEvents, 1);

    TestTrue(
        TEXT("The regeneration primitive restores a controlled amount"),
        FMath::IsNearlyEqual(Stamina->RestoreStamina(10.0f), 10.0f)
    );
    TestTrue(
        TEXT("Restore clamps at maximum"),
        FMath::IsNearlyEqual(Stamina->RestoreStamina(500.0f), 90.0f)
    );
    TestEqual(
        TEXT("Reaching maximum broadcasts once"),
        FullyRestoredEvents,
        1
    );

    TestTrue(
        TEXT("Regeneration rate is configured"),
        Stamina->GetRegenerationRate() > 0.0f
    );
    TestTrue(
        TEXT("Regeneration has a deliberate delay"),
        Stamina->GetRegenerationDelay() > 0.0f
    );

    const AAdaptivePlayerCharacter* Character =
        GetDefault<AAdaptivePlayerCharacter>();
    TestNotNull(
        TEXT("Player owns the reusable health component"),
        Character ? Character->GetHealthComponent() : nullptr
    );
    TestNotNull(
        TEXT("Player owns the reusable stamina component"),
        Character ? Character->GetStaminaComponent() : nullptr
    );

    return true;
}

#endif
