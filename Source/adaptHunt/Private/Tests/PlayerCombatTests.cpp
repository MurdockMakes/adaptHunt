#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Characters/AdaptivePlayerCharacter.h"
#include "Components/CombatComponent.h"
#include "Components/HealthComponent.h"
#include "Debug/CombatTargetDummy.h"
#include "InputMappingContext.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptivePlayerCombatDefaultsTest,
    "adaptHunt.Milestone4.PlayerCombatDefaults",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptivePlayerCombatDefaultsTest::RunTest(const FString& Parameters)
{
    const AAdaptivePlayerCharacter* Character =
        GetDefault<AAdaptivePlayerCharacter>();
    TestNotNull(TEXT("The player character CDO exists"), Character);
    if (!Character)
    {
        return false;
    }

    const UCombatComponent* Combat = Character->GetCombatComponent();
    TestNotNull(TEXT("The player owns a combat component"), Combat);
    if (!Combat)
    {
        return false;
    }

    TestFalse(
        TEXT("Combat actions do not require per-frame Tick"),
        Combat->PrimaryComponentTick.bCanEverTick
    );
    TestTrue(
        TEXT("Heavy attacks deal more damage than light attacks"),
        Combat->GetHeavyAttackDamage() > Combat->GetLightAttackDamage()
    );
    TestTrue(
        TEXT("Heavy attacks cost more stamina than light attacks"),
        Combat->GetHeavyAttackStaminaCost()
            > Combat->GetLightAttackStaminaCost()
    );
    TestTrue(
        TEXT("Directional dodges consume stamina"),
        Combat->GetDodgeStaminaCost() > 0.0f
    );
    TestTrue(
        TEXT("Healing restores a configured amount"),
        Combat->GetHealAmount() > 0.0f
    );
    TestTrue(
        TEXT("Blocking prevents some but not all damage"),
        Combat->GetBlockDamageReduction() > 0.0f
            && Combat->GetBlockDamageReduction() < 1.0f
    );

    const UInputMappingContext* MappingContext =
        Character->GetDefaultMappingContext();
    TestNotNull(TEXT("The player input context exists"), MappingContext);
    if (MappingContext)
    {
        TestEqual(
            TEXT("Seven combat actions have keyboard/mouse and gamepad keys"),
            MappingContext->GetMappings().Num(),
            26
        );
    }

    const ACombatTargetDummy* Dummy = GetDefault<ACombatTargetDummy>();
    TestNotNull(TEXT("The combat target dummy CDO exists"), Dummy);
    TestNotNull(
        TEXT("The dummy is damageable"),
        Dummy ? Dummy->GetHealthComponent() : nullptr
    );
    TestNotNull(
        TEXT("The dummy has pawn-query collision"),
        Dummy ? Dummy->GetCollisionCapsule() : nullptr
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAdaptiveCombatDefenseTest,
    "adaptHunt.Milestone4.DamageMitigation",
    EAutomationTestFlags::EditorContext
        | EAutomationTestFlags::ClientContext
        | EAutomationTestFlags::EngineFilter
)

bool FAdaptiveCombatDefenseTest::RunTest(const FString& Parameters)
{
    UHealthComponent* Health = NewObject<UHealthComponent>();
    TestNotNull(TEXT("A health component can be created"), Health);
    if (!Health)
    {
        return false;
    }

    Health->SetDamageReduction(0.65f);
    TestTrue(
        TEXT("Blocking-style mitigation reduces incoming damage"),
        FMath::IsNearlyEqual(Health->ApplyDamage(40.0f), 14.0f)
    );
    TestTrue(
        TEXT("Mitigated damage changes health by the applied amount"),
        FMath::IsNearlyEqual(Health->GetCurrentHealth(), 86.0f)
    );

    Health->SetInvulnerable(true);
    TestTrue(
        TEXT("Dodge-style invulnerability rejects damage"),
        FMath::IsNearlyZero(Health->ApplyDamage(100.0f))
    );
    TestTrue(
        TEXT("Rejected damage does not change health"),
        FMath::IsNearlyEqual(Health->GetCurrentHealth(), 86.0f)
    );

    Health->ResetHealth();
    TestTrue(
        TEXT("A round reset clears mitigation"),
        FMath::IsNearlyZero(Health->GetDamageReduction())
    );
    TestFalse(
        TEXT("A round reset clears invulnerability"),
        Health->IsInvulnerable()
    );

    return true;
}

#endif
